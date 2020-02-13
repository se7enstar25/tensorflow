"""Provides a redirection point for platform specific implementations of starlark utilities."""

load(
    "//tensorflow/core/platform/default:build_config.bzl",
    _if_llvm_aarch64_available = "if_llvm_aarch64_available",
    _pyx_library = "pyx_library",
    _tf_additional_all_protos = "tf_additional_all_protos",
    _tf_additional_binary_deps = "tf_additional_binary_deps",
    _tf_additional_core_deps = "tf_additional_core_deps",
    _tf_additional_cupti_utils_cuda_deps = "tf_additional_cupti_utils_cuda_deps",
    _tf_additional_device_tracer_srcs = "tf_additional_device_tracer_srcs",
    _tf_additional_env_hdrs = "tf_additional_env_hdrs",
    _tf_additional_lib_deps = "tf_additional_lib_deps",
    _tf_additional_lib_hdrs = "tf_additional_lib_hdrs",
    _tf_additional_monitoring_hdrs = "tf_additional_monitoring_hdrs",
    _tf_additional_rpc_deps = "tf_additional_rpc_deps",
    _tf_additional_tensor_coding_deps = "tf_additional_tensor_coding_deps",
    _tf_additional_test_deps = "tf_additional_test_deps",
    _tf_additional_test_srcs = "tf_additional_test_srcs",
    _tf_fingerprint_deps = "tf_fingerprint_deps",
    _tf_google_mobile_srcs_no_runtime = "tf_google_mobile_srcs_no_runtime",
    _tf_google_mobile_srcs_only_runtime = "tf_google_mobile_srcs_only_runtime",
    _tf_jspb_proto_library = "tf_jspb_proto_library",
    _tf_kernel_tests_linkstatic = "tf_kernel_tests_linkstatic",
    _tf_lib_proto_parsing_deps = "tf_lib_proto_parsing_deps",
    _tf_logging_deps = "tf_logging_deps",
    _tf_monitoring_deps = "tf_monitoring_deps",
    _tf_platform_alias = "tf_platform_alias",
    _tf_platform_deps = "tf_platform_deps",
    _tf_portable_deps_no_runtime = "tf_portable_deps_no_runtime",
    _tf_proto_library = "tf_proto_library",
    _tf_proto_library_cc = "tf_proto_library_cc",
    _tf_proto_library_py = "tf_proto_library_py",
    _tf_protobuf_compiler_deps = "tf_protobuf_compiler_deps",
    _tf_protobuf_deps = "tf_protobuf_deps",
    _tf_protobuf_full_deps = "tf_protobuf_full_deps",
    _tf_protobuf_lite_deps = "tf_protobuf_lite_deps",
    _tf_protos_all = "tf_protos_all",
    _tf_protos_all_impl = "tf_protos_all_impl",
    _tf_protos_grappler = "tf_protos_grappler",
    _tf_protos_grappler_impl = "tf_protos_grappler_impl",
    _tf_protos_profiler_impl = "tf_protos_profiler_impl",
    _tf_py_clif_cc = "tf_py_clif_cc",
    _tf_pyclif_proto_library = "tf_pyclif_proto_library",
    _tf_windows_aware_platform_deps = "tf_windows_aware_platform_deps",
)

if_llvm_aarch64_available = _if_llvm_aarch64_available
pyx_library = _pyx_library
tf_additional_all_protos = _tf_additional_all_protos
tf_additional_binary_deps = _tf_additional_binary_deps
tf_additional_core_deps = _tf_additional_core_deps
tf_additional_cupti_utils_cuda_deps = _tf_additional_cupti_utils_cuda_deps
tf_additional_device_tracer_srcs = _tf_additional_device_tracer_srcs
tf_additional_env_hdrs = _tf_additional_env_hdrs
tf_additional_lib_deps = _tf_additional_lib_deps
tf_additional_lib_hdrs = _tf_additional_lib_hdrs
tf_additional_monitoring_hdrs = _tf_additional_monitoring_hdrs
tf_additional_rpc_deps = _tf_additional_rpc_deps
tf_additional_tensor_coding_deps = _tf_additional_tensor_coding_deps
tf_additional_test_deps = _tf_additional_test_deps
tf_additional_test_srcs = _tf_additional_test_srcs
tf_fingerprint_deps = _tf_fingerprint_deps
tf_google_mobile_srcs_no_runtime = _tf_google_mobile_srcs_no_runtime
tf_google_mobile_srcs_only_runtime = _tf_google_mobile_srcs_only_runtime
tf_jspb_proto_library = _tf_jspb_proto_library
tf_kernel_tests_linkstatic = _tf_kernel_tests_linkstatic
tf_lib_proto_parsing_deps = _tf_lib_proto_parsing_deps
tf_logging_deps = _tf_logging_deps
tf_monitoring_deps = _tf_monitoring_deps
tf_platform_alias = _tf_platform_alias
tf_platform_deps = _tf_platform_deps
tf_portable_deps_no_runtime = _tf_portable_deps_no_runtime
tf_proto_library = _tf_proto_library
tf_proto_library_cc = _tf_proto_library_cc
tf_proto_library_py = _tf_proto_library_py
tf_protobuf_compiler_deps = _tf_protobuf_compiler_deps
tf_protobuf_deps = _tf_protobuf_deps
tf_protobuf_full_deps = _tf_protobuf_full_deps
tf_protobuf_lite_deps = _tf_protobuf_lite_deps
tf_protos_all = _tf_protos_all
tf_protos_all_impl = _tf_protos_all_impl
tf_protos_grappler = _tf_protos_grappler
tf_protos_grappler_impl = _tf_protos_grappler_impl
tf_protos_profiler_impl = _tf_protos_profiler_impl
tf_py_clif_cc = _tf_py_clif_cc
tf_pyclif_proto_library = _tf_pyclif_proto_library
tf_windows_aware_platform_deps = _tf_windows_aware_platform_deps
