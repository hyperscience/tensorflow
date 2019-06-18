#!/bin/bash -l

set -ex

cd /tensorflow-src

case ${BUILD_CONFIG} in
avx)
    echo 'Building for AVX'
    HS_BAZEL_OPTIONS='--copt=-mavx'
    BUILD_TAG='hs.avx'
    ;;
compatibility)
    echo 'Compatibility build (no AVX)'
    HS_BAZEL_OPTIONS=
    BUILD_TAG='hs'
    ;;
mkl)
    echo 'Building for MKL'
    HS_BAZEL_OPTIONS='--config=mkl --copt=-mavx'
    BUILD_TAG='hs.mkl.patched'
    ;;
*)
    echo "Unknown value for  BUILD_CONFIG='${BUILD_CONFIG}'"
    exit 1
    ;;
esac

source /opt/rh/devtoolset-7/enable
source /opt/tensorflow_venv/bin/activate

./configure
bazel --output_base=/bazel-output-base build -c opt ${HS_BAZEL_OPTIONS} --cxxopt="-D_GLIBCXX_USE_CXX11_ABI=0" \
        tensorflow/tools/pip_package:build_pip_package

bazel-bin/tensorflow/tools/pip_package/build_pip_package /output-dir \
    --no_tensorboard --tag ${BUILD_TAG}
