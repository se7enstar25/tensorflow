"""Repository rule for def file filter autoconfiguration.

This repository reuses Bazel's VC detect mechanism to find undname.exe,
which is a tool used in def_file_filter.py.

def_file_filter.py is for filtering the DEF file for TensorFlow on Windows.
On Windows, we use a DEF file generated by Bazel to export symbols from the
tensorflow dynamic library(_pywrap_tensorflow.dll). The maximum number of
symbols that can be exported per DLL is 64K, so we have to filter some useless
symbols through this python script.

`def_file_filter_config` depends on the following environment variables:
  * `BAZEL_VC`
  * `BAZEL_VS`
  * `VS90COMNTOOLS`
  * `VS100COMNTOOLS`
  * `VS110COMNTOOLS`
  * `VS120COMNTOOLS`
  * `VS140COMNTOOLS`
"""

load("@bazel_tools//tools/cpp:windows_cc_configure.bzl", "find_vc_path")
load("@bazel_tools//tools/cpp:windows_cc_configure.bzl", "find_msvc_tool")
load("@bazel_tools//tools/cpp:lib_cc_configure.bzl", "auto_configure_fail")

def _def_file_filter_configure_impl(repository_ctx):
  if repository_ctx.os.name.lower().find("windows") == -1:
    repository_ctx.symlink(Label("//tensorflow/tools/def_file_filter:BUILD.tpl"), "BUILD")
    repository_ctx.file("def_file_filter.py", "")
    return
  vc_path = find_vc_path(repository_ctx)
  if vc_path == "visual-studio-not-found":
    auto_configure_fail("Visual C++ build tools not found on your machine")

  undname = find_msvc_tool(repository_ctx, vc_path, "undname.exe")
  if undname == None:
    auto_configure_fail("Couldn't find undname.exe under %s, please check your VC installation and set BAZEL_VC environment variable correctly." % vc_path)
  undname_bin_path = undname.replace("\\", "\\\\")

  repository_ctx.template(
    "def_file_filter.py",
    Label("//tensorflow/tools/def_file_filter:def_file_filter.py.tpl"),
    {
      "%{undname_bin_path}": undname_bin_path,
    })
  repository_ctx.symlink(Label("//tensorflow/tools/def_file_filter:BUILD.tpl"), "BUILD")


def_file_filter_configure = repository_rule(
    implementation = _def_file_filter_configure_impl,
    environ = [
        "BAZEL_VC",
        "BAZEL_VS",
        "VS90COMNTOOLS",
        "VS100COMNTOOLS",
        "VS110COMNTOOLS",
        "VS120COMNTOOLS",
        "VS140COMNTOOLS"
    ],
)
