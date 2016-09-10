#!/usr/bin/env bash
# Copyright 2016 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
#
# This script invokes dist_mnist.py multiple times concurrently to test the
# TensorFlow's distributed runtime over a Kubernetes (k8s) cluster with the
# grpc pods and service set up.
#
# Usage:
#    dist_mnist_test.sh [--ps-hosts <PS_HOSTS>] 
#                       [--worker-hosts <WORKER_HOSTS>]
#                       [--num-gpus <NUM_GPUS>]
#                       [--sync-replicas]
#
# --sync-replicas
#   Use the synchronized-replica mode. The parameter updates from the replicas
#   (workers) will be aggregated before applied, which avoids stale parameter
#   updates.
#
# ps-hosts/worker-hosts is the list of IP addresses or the GRPC URLs of the ps/worker of
# the worker sessions, separated with ","
# e.g., "localhost:2222,localhost:2223"
#
# --num-gpus <NUM_GPUS>:
#   Specifies the number of gpus to use
#
# NOTES: 
# If you have the error "$'\r': command not found"
# Please run the command below to remove trailing '\r' character that causes the error:
#   sed -i 's/\r$//' dist_mnist_test.sh 


# Configurations
TIMEOUT=120  # Timeout for MNIST replica sessions

# Helper functions
die() {
  echo $@
  exit 1
}

if [[ $# == "0" ]]; then
  die "Usage: $0 [--ps-hosts <PS_HOSTS>] [--worker-hosts <WORKER_HOSTS>] "\
"[--num-gpus <NUM_GPUS>] [--sync-replicas]"
fi

# Process additional input arguments
SYNC_REPLICAS=0

while true; do
  if [[ "$1" == "--ps-hosts" ]]; then
  	PS_HOSTS=$2
  elif [[ "$1" == "--worker-hosts" ]]; then
    WORKER_HOSTS=$2
  elif [[ "$1" == "--num-gpus" ]]; then
    N_GPUS=$2
  elif [[ "$1" == "--sync-replicas" ]]; then
    SYNC_REPLICAS="1"
    die "ERROR: --sync-replicas (synchronized-replicas) mode is not fully "\
"supported by this test yet."
    # TODO(cais): Remove error message once sync-replicas is fully supported
  fi
  shift 2

  if [[ -z "$1" ]]; then
    break
  fi
done

SYNC_REPLICAS_FLAG=""
if [[ ${SYNC_REPLICAS} == "1" ]]; then
  SYNC_REPLICAS_FLAG="True"
else
  SYNC_REPLICAS_FLAG="False"
fi

echo "PS_HOSTS = ${PS_HOSTS}"
echo "WORKER_HOSTS = ${WORKER_HOSTS}"
echo "NUM_GPUS = ${N_GPUS}"
echo "SYNC_REPLICAS = ${SYNC_REPLICAS}"
echo "SYNC_REPLICAS_FLAG = ${SYNC_REPLICAS_FLAG}"


# Current working directory
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY_DIR=$(dirname "${DIR}")/python

MNIST_REPLICA="${PY_DIR}/mnist_replica.py"

WKR_LOG_PREFIX="/tmp/worker"
PS_LOG_PREFIX="/tmp/ps"

# First, download the data from a single process, to avoid race-condition
# during data downloading

timeout ${TIMEOUT} python "${MNIST_REPLICA}" \
    --ps_hosts="${PS_HOSTS}" \
    --worker_hosts="${WORKER_HOSTS}" \
    --job_name="worker" \
    --task_index=0 \
    --num_gpus=${N_GPUS} \
    --sync_replicas=${SYNC_REPLICAS_FLAG} \
    --download_only || \
    die "Download-only step of MNIST replica FAILED"


# Get N_PS by PS_HOSTS
N_PS=$(echo ${PS_HOSTS} | awk -F "," '{printf NF}')
# Replace the delimiter with " "
PS_ARRAY=$(echo ${PS_HOSTS} | awk -F "," '{for(i=1;i<=NF;i++){printf $i" "}}')
# Run a number of ps in parallel. In general, we only set 1 ps.
echo "${N_PS} ps process(es) running in parallel..."

IDX=0
PS=($PS_HOSTS)
while true; do
  timeout ${TIMEOUT} python "${MNIST_REPLICA}" \
      --ps_hosts="${PS_HOSTS}" \
      --worker_hosts="${WORKER_HOSTS}" \
      --job_name="ps" \
      --task_index=${IDX} \
      --num_gpus=${N_GPUS} \
      --sync_replicas=${SYNC_REPLICAS_FLAG} \ | tee "${PS_LOG_PREFIX}${IDX}.log" &
  echo "PS ${IDX}: "
  echo "  PS HOST: ${PS_ARRAY[IDX]}"
  echo "  log file: ${PS_LOG_PREFIX}${IDX}.log"

  ((IDX++))
  if [[ "${IDX}" == "${N_PS}" ]]; then
    break
  fi
done


# Get N_WORKERS by WORKER_HOSTS
N_WORKERS=$(echo ${WORKER_HOSTS} | awk -F "," '{printf NF}')
# Replace the delimiter with " "
WORKER_ARRAY=$(echo ${WORKER_HOSTS} | awk -F "," '{for(i=1;i<=NF;i++){printf $i" "}}')
# Run a number of workers in parallel
echo "${N_WORKERS} worker process(es) running in parallel..."

INDICES=""
IDX=0
while true; do
  timeout ${TIMEOUT} python "${MNIST_REPLICA}" \
      --ps_hosts="${PS_HOSTS}" \
      --worker_hosts="${WORKER_HOSTS}" \
      --job_name="worker" \
      --task_index=${IDX} \
      --num_gpus=${N_GPUS} \
      --sync_replicas=${SYNC_REPLICAS_FLAG} \ | tee "${WKR_LOG_PREFIX}${IDX}.log" &
  echo "Worker ${IDX}: "
  echo "  WORKER HOST: ${WORKER_ARRAY[IDX]}"
  echo "  log file: ${WKR_LOG_PREFIX}${IDX}.log"

  INDICES="${INDICES} ${IDX}"

  ((IDX++))
  if [[ "${IDX}" == "${N_WORKERS}" ]]; then
    break
  fi
done




# Poll until all final validation cross entropy values become available or
# operation times out
COUNTER=0
while true; do
  ((COUNTER++))
  if [[ "${COUNTER}" -gt "${TIMEOUT}" ]]; then
    die "Reached maximum polling steps while polling for final validation "\
"cross entropies from all workers"
  fi

  N_AVAIL=0
  VAL_XENT=""
  for N in ${INDICES}; do
    if [[ ! -z $(grep "Training ends " "${WKR_LOG_PREFIX}${N}.log") ]]; then
      ((N_AVAIL++))
    fi
  done

  if [[ "${N_AVAIL}" == "${N_WORKERS}" ]]; then
    # Print out the content of the log files
    for M in ${INDICES}; do
      ORD=$(expr ${M} + 1)
      echo "==================================================="
      echo "===        Log file from worker ${ORD} / ${N_WORKERS}          ==="
      cat "${WKR_LOG_PREFIX}${M}.log"
      echo "==================================================="
      echo ""
    done

    break
  else
    sleep 1
  fi
done

# Function for getting final validation cross entropy from worker log files
get_final_val_xent() {
  echo $(cat $1 | grep "^After.*validation cross entropy = " | \
      awk '{print $NF}')
}

VAL_XENT=$(get_final_val_xent "${WKR_LOG_PREFIX}0.log")

# Sanity check on the validation entropies
# TODO(cais): In addition to this basic sanity check, we could run the training
# with 1 and 2 workers, each for a few times and use scipy.stats to do a t-test
# to verify that the 2-worker training gives significantly lower final cross
# entropy
echo "Final validation cross entropy from worker0: ${VAL_XENT}"
if [[ $(python -c "print(${VAL_XENT}>0)") != "True" ]]; then
  die "Sanity checks on the final validation cross entropy values FAILED"
fi
