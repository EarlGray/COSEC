# see https://bazel.build/tutorials/ccp-toolchain-config

package(default_visibility = ["//visibility:public"])
load(":toolchain_config.bzl", "cc_toolchain_config")

filegroup(name = "empty")

cc_toolchain_config(name = "i686_elf_toolchain_config")

cc_toolchain(
    name = "i686_elf_toolchain",
    toolchain_identifier = "i686_elf-toolchain",
    toolchain_config = ":i686_elf_toolchain_config",
    all_files = ":empty",
    ar_files = ":empty",
    compiler_files = ":empty",
    dwp_files = ":empty",
    linker_files = ":empty",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_param_files = 0,
)


toolchain(
    name = "cc_toolchain_i386_none",
    toolchain = ":i686_elf_toolchain",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    target_compatible_with = [
        "@platforms//cpu:i386",
        "@platforms//os:none",
    ],
)

toolchain(
    name = "cc_toolchain_i386_linux",
    toolchain = ":i686_elf_toolchain",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    target_compatible_with = [
        "@platforms//cpu:i386",
        "@platforms//os:linux",
    ],
)
