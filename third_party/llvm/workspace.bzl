"""Provides the repository macro to import LLVM."""

load("//third_party:repo.bzl", "tf_http_archive")

def repo(name):
    """Imports LLVM."""
    LLVM_COMMIT = "a72cd6353c455b81df75d89959fafe886addfe5e"
    LLVM_SHA256 = "26f712c37096108f3c8430cff44114f8f5fd672fcc5935814e29e11ef4fe2639"

    tf_http_archive(
        name = name,
        sha256 = LLVM_SHA256,
        strip_prefix = "llvm-project-" + LLVM_COMMIT,
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/github.com/llvm/llvm-project/archive/{commit}.tar.gz".format(commit = LLVM_COMMIT),
            "https://github.com/llvm/llvm-project/archive/{commit}.tar.gz".format(commit = LLVM_COMMIT),
        ],
        link_files = {
            "//third_party/llvm:llvm.autogenerated.BUILD": "llvm/BUILD",
            "//third_party/mlir:BUILD": "mlir/BUILD",
            "//third_party/mlir:build_defs.bzl": "mlir/build_defs.bzl",
            "//third_party/mlir:linalggen.bzl": "mlir/linalggen.bzl",
            "//third_party/mlir:tblgen.bzl": "mlir/tblgen.bzl",
            "//third_party/mlir:test.BUILD": "mlir/test/BUILD",
        },
    )
