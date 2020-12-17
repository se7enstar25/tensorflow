#!/bin/bash -e
# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TFLM_ROOT_DIR=${SCRIPT_DIR}/..

# The renode script for the board being emulated.
RESC_PATH=${TFLM_ROOT_DIR}/testing/bluepill.resc

# Robot file with definition of custom keywords used in test suite.
ROBOT_RESOURCE=${TFLM_ROOT_DIR}/testing/robot.resource.txt

# Renode's entrypoint for using the Robot Framework.
RENODE_TEST_SCRIPT=${TFLM_ROOT_DIR}/tools/make/downloads/renode/test.sh

if [ ! -f "${RENODE_TEST_SCRIPT}" ]; then
  echo "The renode test script: ${RENODE_TEST_SCRIPT} does not exist. Please " \
       "make sure that you have correctly installed Renode for TFLM. See " \
       "tensorflow/lite/micro/docs/renode.md for more details."
  exit 1
fi

if ! ${RENODE_TEST_SCRIPT} &> /dev/null
then
  echo "The following command failed: ${RENODE_TEST_SCRIPT}. Please " \
       "make sure that you have correctly installed Renode for TFLM. See " \
       "tensorflow/lite/micro/docs/renode.md for more details."
  exit 1
fi

exit_code=0

# Files generated by this script will go in the RESULTS_DIRECTORY. These include:
#  1. UART_LOG: Output log from the renode uart.
#  2. html and xml files generated by the Robot Framework.
#  3. ROBOT_SCRIPT: Generated test suite.
#
# Note that with the current approach (in generated ROBOT_SCRIPT), multiple test
# binaries are run in a the same test suite and UART_LOG only has logs from the last test
# binary since it is deleted prior to running each test binary. If some test fails
# the UART_LOG will be printed to console log before being deleted.
RESULTS_DIRECTORY=/tmp/renode_bluepill_logs
mkdir -p ${RESULTS_DIRECTORY}

UART_LOG=${RESULTS_DIRECTORY}/uart_log.txt

ROBOT_SCRIPT=${RESULTS_DIRECTORY}/Bluepill.robot

echo -e "*** Settings ***\n" \
        "Suite Setup                   Setup\n" \
        "Suite Teardown                Teardown\n" \
        "Test Setup                    Reset Emulation\n" \
        "Test Teardown                 Teardown With Custom Message\n" \
        "Resource                      \${RENODEKEYWORDS}\n" \
        "Resource                      ${ROBOT_RESOURCE}\n" \
        "Default Tags                  bluepill uart tensorflow arm\n" \
        "\n" \
        "*** Variables ***\n" \
        "\${RESC}                      undefined_RESC\n" \
        "\${UART_LOG}                  /tmp/uart.log\n" \
        "\${UART_LINE_ON_SUCCESS}      ~~~ALL TESTS PASSED~~~\n" \
        "\${CREATE_SNAPSHOT_ON_FAIL}   False\n" \
        "\n" \
        "*** Test Cases ***\n" \
        "Should Create Platform\n" \
        "    Create Platform\n" > $ROBOT_SCRIPT

BIN_DIR=$1
for binary in `ls $BIN_DIR/*_test`;
do
    echo -e "Should Run $(basename ${binary})\n"\
    	    "    Test Binary    @$(realpath ${binary})\n" >> $ROBOT_SCRIPT
done

ROBOT_COMMAND="${RENODE_TEST_SCRIPT} ${ROBOT_SCRIPT} \
  -r ${RESULTS_DIRECTORY} \
  --variable RESC:${RESC_PATH} \
  --variable UART_LOG:${UART_LOG}"

echo "${ROBOT_COMMAND}"

if ! ${ROBOT_COMMAND}
then
  exit_code=1
fi

exit $exit_code
