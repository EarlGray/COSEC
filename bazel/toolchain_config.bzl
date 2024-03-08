"""
"""

load(
    "@bazel_tools//tools/build_defs/cc:action_names.bzl",
    "ACTION_NAMES",
)
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
    "tool_path",
)

def _flags_for_action(actions, flags, name):
    return feature(
        name = name,
        enabled = True,
        flag_sets = [
            flag_set(
                actions = actions,
                flag_groups = [
                    flag_group(flags = flags),
                ],
            ),
        ],
    )

def _impl(ctx):
    # TODO: Linux
    crosscompile_path = "/opt/homebrew/bin"
    crosscompile_prefix = crosscompile_path + "/i686-elf-"

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        toolchain_identifier = "i686_elf-toolchain",
        host_system_name = "local",
        target_system_name = "linux",
        target_cpu = "i386",
        target_libc = "none",
        compiler = "gcc",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        tool_paths = [
            tool_path(name = tool, path = crosscompile_prefix + tool)
            for tool in ["ar", "cpp", "gcc", "ld", "nm", "objdump", "strip"]
        ],
        features = [
            _flags_for_action(
                name = "default_compile_flags",
                actions = [
                    ACTION_NAMES.c_compile,
                    ACTION_NAMES.cpp_compile,
                    ACTION_NAMES.assemble,
                    ACTION_NAMES.preprocess_assemble,
                ],
                flags = ["-nostdinc", "-Ilib/c/include", "-D_YES_IM_HERE"],
            ),
            _flags_for_action(
                name = "default_linker_flags",
                actions = [ACTION_NAMES.cpp_link_executable],
                flags = ["-nostdlib", "-static", "-fno-pie"],
            ),
        ],
    )

cc_toolchain_config = rule(
    implementation = _impl,
    attrs = {},
    provides = [CcToolchainConfigInfo],
)
