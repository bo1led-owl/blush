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

const lto_flags: []const []const u8 = &.{
    "-flto=full",
};

const files: []const []const u8 = &.{
    "src/main.c",
    "src/executor.c",
    "src/interactive.c",
    "src/dyn_string.c",
    "src/vars.c",
    "src/alloc.c",
};

pub fn build(b: *std.Build) !void {
    const sanitize = b.option(bool, "sanitize", "Link asan library and use sanitizers") orelse false;
    const lto = b.option(bool, "lto", "Enable link-time optimizations") orelse false;

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

    var flags = try std.ArrayList([]const u8).initCapacity(b.allocator, main_flags.len);
    defer flags.deinit();
    try flags.appendSlice(main_flags);
    if (sanitize) {
        try flags.appendSlice(sanitizer_flags);
    }
    if (lto) {
        try flags.appendSlice(lto_flags);
    }

    exe.addCSourceFiles(.{
        .files = files,
        .flags = flags.items,
    });
    if (sanitize) {
        exe.linkSystemLibrary("asan");
    }
    b.installArtifact(exe);
}
