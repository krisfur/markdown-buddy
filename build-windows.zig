const std = @import("std");

const backend_object_names = [_][]const u8{
    "markdown_buddy_backend-main.obj",
    "markdown_buddy_backend-runtime-internal.obj",
    "markdown_buddy_backend-runtime-default_temp_allocator_arena.obj",
    "markdown_buddy_backend-runtime-random_generator_chacha8.obj",
    "markdown_buddy_backend-runtime-random_generator_chacha8_simd128.obj",
    "markdown_buddy_backend-runtime-heap_allocator.obj",
    "markdown_buddy_backend-runtime-udivmod128.obj",
    "markdown_buddy_backend-runtime-default_temporary_allocator.obj",
    "markdown_buddy_backend-runtime-error_checks.obj",
    "markdown_buddy_backend-builtin.obj",
    "markdown_buddy_backend-runtime-print.obj",
    "markdown_buddy_backend-runtime-core_builtin.obj",
    "markdown_buddy_backend-runtime-os_specific_windows.obj",
    "markdown_buddy_backend-sanitizer.obj",
    "markdown_buddy_backend-runtime-core.obj",
    "markdown_buddy_backend-runtime-procs.obj",
    "markdown_buddy_backend-mem.obj",
    "markdown_buddy_backend-runtime-procs_windows_amd64.obj",
    "markdown_buddy_backend-runtime-heap_allocator_windows.obj",
    "markdown_buddy_backend-runtime-os_specific.obj",
};

fn winTarget(b: *std.Build) std.Build.ResolvedTarget {
    return b.resolveTargetQuery(.{
        .cpu_arch = .x86_64,
        .os_tag = .windows,
        .abi = .gnu,
    });
}

fn backendObjectPath(b: *std.Build, name: []const u8) std.Build.LazyPath {
    return b.path(b.fmt("frontend-win/.build/{s}", .{name}));
}

pub fn build(b: *std.Build) void {
    const target = winTarget(b);
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const prepare = b.addSystemCommand(&.{ "mkdir", "-p", "dist", "frontend-win/.build" });

    const odin = b.addSystemCommand(&.{
        "odin",
        "build",
        "backend-odin/src",
        "-build-mode:obj",
        "-no-entry-point",
        "-target:windows_amd64",
        "-out:frontend-win/.build/markdown_buddy_backend.obj",
    });
    odin.step.dependOn(&prepare.step);

    const generated = b.addWriteFiles();
    const shim_source = generated.add("fltused.c", "int _fltused = 1;\n");

    const fltused = b.addObject(.{
        .name = "fltused",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    fltused.root_module.addCSourceFile(.{ .file = shim_source, .flags = &.{} });

    const exe = b.addExecutable(.{
        .name = "markdown-buddy-win",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .link_libcpp = true,
        }),
        .win32_manifest = b.path("frontend-win/resources/app.manifest"),
    });
    exe.root_module.addCSourceFile(.{
        .file = b.path("frontend-win/src/main.cpp"),
        .flags = &.{"-std=c++20"},
    });
    exe.root_module.addIncludePath(b.path("backend-odin/include"));
    exe.root_module.linkSystemLibrary("ole32", .{});
    exe.root_module.linkSystemLibrary("uuid", .{});
    exe.root_module.linkSystemLibrary("comctl32", .{});
    exe.root_module.linkSystemLibrary("shell32", .{});
    exe.root_module.linkSystemLibrary("gdi32", .{});
    exe.root_module.linkSystemLibrary("msimg32", .{});
    exe.subsystem = .Windows;

    const install_exe = b.addInstallArtifact(exe, .{
        .dest_dir = .{ .override = .prefix },
        .dest_sub_path = "markdown-buddy-win.exe",
    });

    const link_dll = b.addSystemCommand(&.{
        "zig",
        "c++",
        "-target",
        "x86_64-windows-gnu",
        "-shared",
    });
    link_dll.step.dependOn(&prepare.step);
    link_dll.step.dependOn(&odin.step);
    link_dll.step.dependOn(&fltused.step);
    for (backend_object_names) |name| {
        link_dll.addFileArg(backendObjectPath(b, name));
    }
    link_dll.addFileArg(fltused.getEmittedBin());
    link_dll.addArgs(&.{
        "frontend-win/resources/markdown_buddy.def",
        "-lbcrypt",
        "-o",
        "dist/markdown_buddy.dll",
    });

    const windows = b.step("windows", "Build Windows frontend and backend");
    windows.dependOn(&install_exe.step);
    windows.dependOn(&link_dll.step);
}
