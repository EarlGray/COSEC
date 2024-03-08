cc_library(
    name = "c",
    linkstatic = True,
    srcs = glob(["src/*.c"]),
    hdrs = glob(["include/**/*.h"]),
    #include_prefix = "include",
    copts = ["-isystem lib/c/include"],
    visibility = ["//visibility:public"],
    deps = ["libc"],
    target_compatible_with = [
        "@platforms//os:linux",
    ]
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
