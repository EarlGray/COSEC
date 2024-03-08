cc_library(
    name = "c",
    srcs = glob([ "src/*.c", "*.S", ]),
    hdrs = glob(["include/**/*.h"]),
    local_defines = ["LINUX=1"],        # TODO: make this configurable
    linkstatic = True,
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:i386",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "cosec",
    srcs = glob([ "src/*.c", "*.S", ]),
    hdrs = glob(["include/**/*.h"]),
    local_defines = ["COSEC=1"],
    linkstatic = True,
    target_compatible_with = [
        "@platforms//os:cosec",
        "@platforms//cpu:i386",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "kosec",
    srcs = glob([ "src/*.c", "*.S", ]),
    hdrs = glob(["include/**/*.h"]),
    local_defines = ["COSEC_KERN=1"],
    linkstatic = True,
    target_compatible_with = [
        "@platforms//os:none",
        "@platforms//cpu:i386",
    ],
    visibility = ["//visibility:public"],
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
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:i386",
    ],
)
