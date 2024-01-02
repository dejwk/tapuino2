
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "gtest",
    url = "https://github.com/google/googletest/archive/v1.8.x.zip",
#    sha256 = "b58cb7547a28b2c718d1e38aee18a3659c9e3ff52440297e965f5edffe34b6d0",
#    build_file = "@//:gtest.BUILD",
    strip_prefix = "googletest-1.8.x",
)

new_local_repository(
   name = "fltk",
   path = "roo_testing/fltk-1.3.5",
   build_file = "//roo_testing:fltk.BUILD",
)

new_local_repository(
    name = "system_libs",
    # pkg-config --variable=libdir x11
    path = "/usr/lib/x86_64-linux-gnu",
    build_file_content = """
cc_library(
    name = "x11",
    srcs = [
        "libX11.so",
        "libXext.so.6",
#        "libdl.so",
        "libm.so",
#        "libpthread.so",
    ],
    linkstatic = 1,
    alwayslink = 1,
    visibility = ["//visibility:public"],
)
""",
)

http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
    strip_prefix = "gflags-2.2.2",
    urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
)

http_archive(
    name = "com_github_google_glog",
    sha256 = "21bc744fb7f2fa701ee8db339ded7dce4f975d0d55837a97be7d46e8382dea5a",
    strip_prefix = "glog-0.5.0",
    urls = ["https://github.com/google/glog/archive/v0.5.0.zip"],
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_proto",
    sha256 = "e017528fd1c91c5a33f15493e3a398181a9e821a804eb7ff5acdd1d2d6c2b18d",
    strip_prefix = "rules_proto-4.0.0-3.20.0",
    urls = [
        "https://github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0-3.20.0.tar.gz",
    ],
)
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
rules_proto_dependencies()
rules_proto_toolchains()