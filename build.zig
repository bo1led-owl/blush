const std = @import("std");

const main_flags: []const []const u8 = &.{
    "-std=iso9899:1999",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wcast-qual",
    "-Wconversion",
    "-Wsign-conversion",
};

const sanitizer_flags: []const []const u8 = &.{
    "-fsanitize=address",
    "-fsanitize=undefined",
    "-fsanitize=pointer-compare",
    "-fsanitize=pointer-subtract",
    "-fsanitize=leak",
    "-fsanitize-address-use-after-scope",
};

const files: []const []const u8 = &.{
    "src/main.c",
    "src/executor.c",
    "src/interactive.c",
    "src/dyn_string.c",
    "src/vars.c",
};

pub fn build(b: *std.Build) void {
    const link_asan = b.option(bool, "use-sanitizers", "Link asan library and use sanitizers") orelse false;

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const debug_mode = optimize == std.builtin.OptimizeMode.Debug;

    const should_strip = !debug_mode;
    const exe = b.addExecutable(.{
        .name = "blush",
        .target = target,
        .optimize = optimize,
        .strip = should_strip,
        .link_libc = true,
    });

    const flags = if (link_asan) main_flags ++ sanitizer_flags else main_flags;
    exe.addCSourceFiles(.{
        .files = files,
        .flags = flags,
    });
    if (link_asan) {
        exe.linkSystemLibrary("asan");
    }
    b.installArtifact(exe);
}
