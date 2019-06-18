FROM centos:7

RUN yum update -y

RUN  yum install -y \
        https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm \
        git \
        yum-utils \
        centos-release-scl

RUN yum install -y \
        devtoolset-7 \
        java-1.8.0-openjdk \
        java-1.8.0-openjdk-devel \
        patch \
        python36 \
        python36-devel

RUN python3.6 -m ensurepip && \
        ln -s /usr/local/bin/pip3 /usr/bin/pip3 && \
        pip3 install --upgrade pip setuptools

RUN python3.6 -m venv /opt/tensorflow_venv

RUN source /opt/tensorflow_venv/bin/activate && \
    pip3 install --upgrade \
        'absl-py >= 0.1.6' \
        'astor >= 0.6.0' \
        'gast >= 0.2.0' \
        'keras_applications >= 1.0.6' \
        'keras_preprocessing >= 1.0.5' \
        'numpy >= 1.13.3' \
        'six >= 1.10.0' \
        'protobuf >= 3.6.1' \
        'tensorflow_estimator >= 1.13.0, < 1.14.0rc0' \
        'termcolor >= 1.1.0'

# Install bazel
ARG BAZEL_VERSION=0.21.0
RUN curl -L https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh > /tmp/bazel.sh && \
    chmod +x /tmp/bazel.sh && \
    /tmp/bazel.sh && \
    rm /tmp/bazel.sh

ENV PYTHON_BIN_PATH /opt/tensorflow_venv/bin/python3.6
ENV PYTHON_LIB_PATH /opt/tensorflow_venv/lib/python3.6/site-packages/
ENV TF_NEED_IGNITE false
ENV TF_ENABLE_XLA true
ENV TF_NEED_OPENCL_SYCL false
ENV TF_NEED_ROCM false
ENV TF_NEED_CUDA false
ENV TF_DOWNLOAD_CLANG false
ENV TF_NEED_MPI false
ENV CC_OPT_FLAGS -march=native
ENV TF_SET_ANDROID_WORKSPACE false
ENV TF_CPP_MIN_LOG_LEVEL 1

COPY build.sh /build.sh

VOLUME "/tensorflow-src"
VOLUME "/output-dir"
VOLUME "/bazel-output-base"
