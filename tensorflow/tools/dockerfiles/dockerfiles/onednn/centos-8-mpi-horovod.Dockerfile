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
# ============================================================================
#
# THIS IS A GENERATED DOCKERFILE.
#
# This file was assembled from multiple pieces, whose use is documented
# throughout. Please refer to the TensorFlow dockerfiles documentation
# for more information.

ARG CENTOS_VERSION=8

FROM centos:${CENTOS_VERSION} as base

# See http://bugs.python.org/issue19846
ENV LANG C.UTF-8
ARG PYTHON=python3

RUN yum update -y && yum install -y \
    ${PYTHON} \
    ${PYTHON}-pip \
    which && \
    yum clean all


RUN ${PYTHON} -m pip --no-cache-dir install --upgrade \
    pip \
    setuptools

# Some TF tools expect a "python" binary
RUN ln -sf $(which ${PYTHON}) /usr/local/bin/python && \
    ln -sf $(which ${PYTHON}) /usr/local/bin/python3 && \
    ln -sf $(which ${PYTHON}) /usr/bin/python

# Options:
#   tensorflow
#   tensorflow-gpu
#   tf-nightly
#   tf-nightly-gpu
# Set --build-arg TF_PACKAGE_VERSION=1.11.0rc0 to install a specific version.
# Installs the latest version by default.
ARG TF_PACKAGE=tensorflow
ARG TF_PACKAGE_VERSION=
RUN python3 -m pip install --no-cache-dir ${TF_PACKAGE}${TF_PACKAGE_VERSION:+==${TF_PACKAGE_VERSION}}

RUN yum update -y && yum install -y \
    openmpi \
    openmpi-devel \
    openssh \
    openssh-server \
    which && \
    yum clean all

ENV PATH="/usr/lib64/openmpi/bin:${PATH}"

# Create a wrapper for OpenMPI to allow running as root by default
RUN mv -f $(which mpirun) /usr/bin/mpirun.real && \
    echo '#!/bin/bash' > /usr/bin/mpirun && \
    echo 'mpirun.real --allow-run-as-root "$@"' >> /usr/bin/mpirun && \
    chmod a+x /usr/bin/mpirun

# Configure OpenMPI to run good defaults:
RUN echo "btl_tcp_if_exclude = lo,docker0" >> /etc/openmpi-x86_64/openmpi-mca-params.conf

# Install OpenSSH for MPI to communicate between containers
RUN mkdir -p /var/run/sshd

# Allow OpenSSH to talk to containers without asking for confirmation
RUN cat /etc/ssh/sshd_config | grep -v StrictHostKeyChecking > /etc/ssh/sshd_config.new && \
    echo "    StrictHostKeyChecking no" >> /etc/ssh/sshd_config.new && \
    mv -f /etc/ssh/sshd_config.new /etc/ssh/sshd_config

# Install Horovod
ARG HOROVOD_WITHOUT_PYTORCH=1
ARG HOROVOD_WITHOUT_MXNET=1
ARG HOROVOD_WITH_TENSORFLOW=1
ARG HOROVOD_VERSION=

RUN yum update -y && yum install -y \
    gcc \
    gcc-c++ \
    python36-devel && \
    yum clean all

RUN ${PYTHON} -m pip install --no-cache-dir horovod${HOROVOD_VERSION:+==${HOROVOD_VERSION}}

COPY bashrc /etc/bash.bashrc
RUN chmod a+rwx /etc/bash.bashrc
