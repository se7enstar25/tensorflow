load(
    "//tensorflow/lite:build_def.bzl",
    "tflite_cc_library_with_c_headers_test",
    "tflite_copts",
)
load("//tensorflow:tensorflow.default.bzl", "get_compatible_with_portable")

package(
    # copybara:uncomment default_applicable_licenses = ["//tensorflow:license"],
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],
)

tflite_cc_library_with_c_headers_test(
    name = "profiler",
    hdrs = ["profiler.h"],
    compatible_with = get_compatible_with_portable(),
    copts = tflite_copts(),
    deps = [":telemetry_setting"],
)

tflite_cc_library_with_c_headers_test(
    name = "telemetry_setting",
    hdrs = ["telemetry_setting.h"],
    compatible_with = get_compatible_with_portable(),
    copts = tflite_copts(),
)
