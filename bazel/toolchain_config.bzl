load("@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl", "tool_path")

def _impl(ctx):
    crosscompile_path = "/opt/homebrew/bin"
    crosscompile_prefix = crosscompile_path + "/i686-elf-"
    tool_paths = \
        [tool_path(name=tool, path=crosscompile_prefix + tool)
             for tool in ["ar", "cpp", "gcc", "ld", "nm", "objdump", "strip"]]
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
        tool_paths = tool_paths,
    )

cc_toolchain_config = rule(
    implementation = _impl,
    attrs = {},
    provides = [CcToolchainConfigInfo],
)
