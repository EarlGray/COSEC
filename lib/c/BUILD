cc_library(
    name = "c",
    linkstatic = True,
    srcs = glob([
        "src/*.c",
        "*.S",
    ]),
    hdrs = glob(["include/**/*.h"]),
    #copts = ["-nostdinc -isystem lib/c/include"],
    visibility = ["//visibility:public"],
    local_defines = ["LINUX=1"],        # TODO: make this configurable
    target_compatible_with = [
        "@platforms//os:linux",
    ]
)

filegroup(
    name = "include",

)

cc_import(
    name = "libc",
    hdrs = glob(["include/**/*.h"]),
    static_library = "libc.a",
)

cc_binary(
    name = "sh",
    srcs = ["test/sh.c"],
    deps = [":c"],
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:i386",
    ],
)

cc_binary(
    name = "hello",
    srcs = ["test/hello.c"],
    deps = [":c"],
    #copts = [
    #    "-nostdinc",
    #    "-isystem lib/c/include",
    #],
    #linkopts = [
    #    "-ffreestanding",
    #    "-nostdlib",
    #    "-static",
    #    "-fno-pie",
    #],
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:i386",
    ],
)
