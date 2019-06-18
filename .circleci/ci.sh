#!/usr/bin/env bash

set -ex
cd "$(dirname "$0")/.."

TENSORFLOW_BUILD_TAG="${TENSORFLOW_BUILD_TAG:-centos}"
TENSORFLOW_DOCKER_BUILD_TAG="${TENSORFLOW_DOCKER_BUILD_TAG:-latest}"
BAZEL_OUTPUT_BASE="${BAZEL_OUTPUT_BASE:-/var/tmp/bazel-output-base}"

if [[ ! -z "${BAZEL_VERSION}" ]]
then
    DOCKER_BUILD_ARGS="${DOCKER_BUILD_ARGS} --build-arg BAZEL_VERSION=${BAZEL_VERSION}"
fi

mkdir -p "${BAZEL_OUTPUT_BASE}"
mkdir -p build

if [[ -z "${BUILD_CONFIG}" ]]
then
    echo Please specify the build config as the environmental variable BUILD_CONFIG
    echo Can be one of avx, compatibility or mkl
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

if [[ -z ${TENSORFLOW_SKIP_DOCKER_BUILD} ]] ; then
    pushd .circleci
    docker build \
        ${DOCKER_BUILD_ARGS} \
        -t 794612149504.dkr.ecr.us-east-1.amazonaws.com/tensorflow-ci:${TENSORFLOW_DOCKER_BUILD_TAG} \
        .
    popd
fi

docker run -ti --rm \
    -v "$(pwd)":/tensorflow-src \
    -v "$(pwd)/build":/output-dir \
    -v "${BAZEL_OUTPUT_BASE}":/bazel-output-base      \
    -e BUILD_CONFIG=${BUILD_CONFIG}                 \
    -e TENSORFLOW_BUILD_TAG=${TENSORFLOW_BUILD_TAG} \
    794612149504.dkr.ecr.us-east-1.amazonaws.com/tensorflow-ci:${TENSORFLOW_DOCKER_BUILD_TAG} /build.sh
