"""Run the options regression at CMake's C++14 language floor."""

def _cxx14_options(settings, attr):
    flags = [
        flag
        for flag in settings["//command_line_option:cxxopt"]
        if not flag.startswith("-std=") and not flag.startswith("/std:")
    ]
    return {"//command_line_option:cxxopt": flags + [attr.standard_flag]}

_cxx14_transition = transition(
    implementation = _cxx14_options,
    inputs = ["//command_line_option:cxxopt"],
    outputs = ["//command_line_option:cxxopt"],
)

def _cxx14_test_impl(ctx):
    binary = ctx.attr.binary[0][DefaultInfo]
    executable = ctx.actions.declare_file(
        ctx.label.name + (".exe" if ctx.attr.standard_flag.startswith("/") else ""),
    )
    ctx.actions.symlink(
        output = executable,
        target_file = binary.files_to_run.executable,
        is_executable = True,
    )
    return [DefaultInfo(executable = executable, runfiles = binary.default_runfiles)]

cxx14_test = rule(
    implementation = _cxx14_test_impl,
    test = True,
    attrs = {
        "binary": attr.label(mandatory = True, cfg = _cxx14_transition),
        "standard_flag": attr.string(mandatory = True),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
)
