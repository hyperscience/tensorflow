exports_files(["LICENSE"])

load(
    "@org_tensorflow//third_party/mkl_dnn:build_defs.bzl",
    "if_mkl_open_source_only",
)

config_setting(
    name = "clang_linux_x86_64",
    values = {
        "cpu": "k8",
        "define": "using_clang=true",
    },
)

cc_library(
    name = "mkl_dnn",
    srcs = glob([
        "src/common/*.cpp",
        "src/cpu/*.cpp",
        "src/cpu/gemm/*.cpp",
    ]),
    hdrs = glob(["include/*"]),
    copts = [
        "-fexceptions",
        "-DUSE_MKL",
        "-DUSE_CBLAS",
        "-DMKLDNN_DLL",
        "-DMKLDNN_DLL_EXPORTS",
        "-DMKLDNN_THR=MKLDNN_THR_SEQ",
        "-D__STDC_CONSTANT_MACROS",
        "-D__STDC_LIMIT_MACROS",
        "-Dmkldnn_EXPORTS",
        "-D_GLIBCXX_USE_CXX11_ABI=0",
        "-std=c++11",
        "-fvisibility-inlines-hidden",
        "-Wall",
        "-Wno-unknown-pragmas",
        "-fvisibility=internal",
        "-mavx",
        "-fPIC",
        "-Wformat",
        "-Wformat-security",
        "-fstack-protector-strong",
        "-Wmissing-field-initializers",
        "-O3",
        "-DNDEBUG",
        "-std=gnu++11",
    ] + if_mkl_open_source_only([
        "-UUSE_MKL",
        "-UUSE_CBLAS",
    ]),
    includes = [
        "include",
        "src",
        "src/common",
        "src/cpu",
        "src/cpu/xbyak",
        "src/cpu/gemm",
    ],
    nocopts = "-fno-exceptions",
    visibility = ["//visibility:public"],
    deps = select({
        "@org_tensorflow//tensorflow:linux_x86_64": [
            "@mkl_linux//:mkl_headers",
            "@mkl_linux//:mkl_libs_linux",
        ],
        "@org_tensorflow//tensorflow:darwin": [
            "@mkl_darwin//:mkl_headers",
            "@mkl_darwin//:mkl_libs_darwin",
        ],
        "@org_tensorflow//tensorflow:windows": [
            "@mkl_windows//:mkl_headers",
            "@mkl_windows//:mkl_libs_windows",
        ],
        "//conditions:default": [],
    }),
)
