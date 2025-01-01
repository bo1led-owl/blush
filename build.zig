const std = @import("std");

const main_flags: []const []const u8 = &.{
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wcast-qual",
    "-Wconversion",
    "-Wsign-conversion",
};
const debug_flags: []const []const u8 = &.{
    "-fsanitize=address",
    "-fsanitize=undefined",
    "-fsanitize=pointer-compare",
    "-fsanitize=pointer-subtract",
    "-fsanitize=leak",
    "-fsanitize-address-use-after-scope",
};
const release_flags: []const []const u8 = &.{};

const files: []const []const u8 = &.{
    "src/main.c",
    "src/executor.c",
    "src/interactive.c",
    "src/arena.c",
    "src/dyn_string.c",
};

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const debug_mode = optimize == std.builtin.OptimizeMode.Debug;

    const should_strip = !debug_mode;
    const exe = b.addExecutable(.{
        .name = "csh",
        .target = target,
        .optimize = optimize,
        .strip = should_strip,
        .link_libc = true,
    });
    const flags = if (debug_mode) main_flags ++ debug_flags else main_flags ++ release_flags;
    exe.addCSourceFiles(.{
        .files = files,
        .flags = flags,
    });
    if (debug_mode) {
        exe.linkSystemLibrary("asan");
    }
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
