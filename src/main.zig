const std = @import("std");
const readline = @cImport({
    @cInclude("stdio.h");
    @cInclude("stdlib.h");
    @cInclude("readline/readline.h");
    @cInclude("readline/history.h");
});
const glob = @cImport({
    @cInclude("glob.h");
});

const shellErr = error{ExitShell};

const cmd_list = [_][]const u8{
    "exit", "cd", "pwd", "type", "echo",
    "builtin", "clear", 
};

const ParsedCommand = struct {
    command: []const u8,
    argv: []const []const u8,
};

pub fn main() !void {
    // Global allocator
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const gpa_allocator = gpa.allocator();

    const historyPath = try getHistoryFile(gpa_allocator);
    defer gpa_allocator.free(historyPath);

    if (historyPath.len > 0) {
        _ = readline.read_history(historyPath.ptr);
    }

    while (true) {
        // Allocator for prompts and commands
        var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
        defer arena.deinit();
        const allocator = arena.allocator();

        const prompt = displayPrompt(allocator) catch "$ ";

        const line = readline.readline(prompt.ptr);

        if (line == null) {
            break;
        }
        defer readline.free(line);

        const input = std.mem.span(line);
        if (input.len == 0) continue;

        readline.add_history(line);

        handleCommand(input, allocator) catch |err| {
            if (err == shellErr.ExitShell) {
                break;
            }
        };
    }

    if (historyPath.len > 0) {
        _ = readline.write_history(historyPath.ptr);
    }
}

fn expandArgs(allocator: std.mem.Allocator, arg: []const u8, argsList: *std.ArrayList([]const u8)) !void {
    if (hasWildcard(arg)) {
        try argsList.append(allocator, arg);
        return;
    }

    var glob_result: glob.glob_t = undefined;

    const pattern = try allocator.dupeZ(u8, arg);

    const flags = glob.GLOB_NOCHECK | glob.GLOB_TILDE;

    if (glob.glob(pattern.ptr, flags, null, &glob_result) == 0) {
        defer glob.globfree(&glob_result);

        var i: usize = 0;
        while(i < glob_result.gl_pathc) : (i += 1) {
            const cString = glob_result.gl_pathv[i];
            const  zigSlice = std.mem.span(cString);
            const entry = try allocator.dupeZ(u8, zigSlice);
            try argsList.append(allocator, entry);
        }
    } else {
        std.debug.print("DEBUG: glob failed or no match for '{s}'\n", .{arg});
        try argsList.append(allocator, arg);
    }
}

// Parser Logic
fn parseCommand(input: []const u8, allocator: std.mem.Allocator) !ParsedCommand {
    var parts = std.mem.tokenizeAny(u8, input, " \t\r\n");

    var argvList = std.ArrayListUnmanaged([]const u8){};
    errdefer argvList.deinit(allocator);

    while (parts.next()) |part| {
        try expandArgs(allocator, part, &argvList);
    }

    if (argvList.items.len == 0)
    return error.EmptyCommand;

    const argv = try argvList.toOwnedSlice(allocator);

    return ParsedCommand{
        .command = argv[0],
        .argv = argv,
    };
}

fn handleCommand(input: []const u8, allocator: std.mem.Allocator) !void {
    const parsed = parseCommand(input, allocator) catch |err| {
        if (err == error.EmptyCommand) return;
        return err;
    };
    defer allocator.free(parsed.argv);

    const cmd = parsed.command;

    if (std.mem.eql(u8, cmd, "exit")) {
        return shellErr.ExitShell;
    } else if (std.mem.eql(u8, cmd, "echo")) {
        echoCommand(parsed.argv);
    } else if (std.mem.eql(u8, cmd, "pwd")) {
        try pwdCommand(allocator);
    } else if (std.mem.eql(u8, cmd, "clear")) {
        clearCommand();
    } else if (std.mem.eql(u8, cmd, "cd")) {
        const target = if (parsed.argv.len > 1) parsed.argv[1] else "";
        try cdCommand(target);
    } else if (std.mem.eql(u8, cmd, "builtin")) {
        builtinCommand();
    } else if (std.mem.eql(u8, cmd, "type")) {
        typeCommand(parsed.argv);
    } else {
        executeCommand(allocator, parsed.argv) catch |err| {
            switch (err) {
                error.FileNotFound => {},
                error.AccessDenied => {},
                else => std.debug.print("exec error: {s}\n", .{@errorName(err)}),
            }
        };
    }
}

// Commands
fn echoCommand(argv: []const []const u8) void {
    if (argv.len <= 1) {
        std.debug.print("\n", .{});
        return;
    }

    for (argv[1..], 0..) |arg, i| {
        if (i > 0) std.debug.print(" ", .{});
        std.debug.print("{s}", .{arg});
    }
    std.debug.print("\n", .{});
}

fn pwdCommand(allocator: std.mem.Allocator) !void {
    const cwd = try std.fs.cwd().realpathAlloc(allocator, ".");
    defer allocator.free(cwd);
    std.debug.print("{s}\n", .{cwd});
}

fn clearCommand() void {
    std.debug.print("\x1B[2J\x1B[H", .{});
}

fn cdCommand(target: []const u8) !void {
    const path =
    if (target.len == 0 or std.mem.eql(u8, target, "~"))
        std.posix.getenv("HOME") orelse {
            std.debug.print("cd: HOME not set\n", .{});
            return error.HomeNotSet;
        }
        else
        target;

    std.posix.chdir(path) catch {
        std.debug.print("cd: {s}: No such directory\n", .{path});
    };
}

fn builtinCommand() void {
    for (cmd_list) |cmd| {
        std.debug.print("{s}\n", .{cmd});
    }
}

fn typeCommand(argv: []const []const u8) void {
    if (argv.len < 2) return;

    for (cmd_list) |builtin| {
        if (std.mem.eql(u8, argv[1], builtin)) {
            std.debug.print("{s} is a builtin\n", .{builtin});
            return;
        }
    }

    std.debug.print("{s} not found\n", .{argv[1]});
}


fn executeCommand(allocator: std.mem.Allocator, argv: []const []const u8) !void {
    const cmd = argv[0];

    // Direct path
    if (std.mem.indexOfScalar(u8, cmd, '/')) |_| {
        try spawnAndWait(allocator, argv);
        return;
    }

    const path = std.posix.getenv("PATH") orelse "/bin:/usr/bin";
    var dirs = std.mem.splitScalar(u8, path, ':');

    while (dirs.next()) |dir| {
        const full = std.fs.path.join(allocator, &.{ dir, cmd }) catch continue;
        defer allocator.free(full);

        if (spawnAndWait(allocator, argv)) {
            return;
        } else |_| {}
    }

    std.debug.print("{s}: command not found\n", .{cmd});
}

fn spawnAndWait(allocator: std.mem.Allocator, argv: []const []const u8) !void {
    var child = std.process.Child.init(argv, allocator);
    child.stdin_behavior = .Ignore;
    child.stdout_behavior = .Inherit;
    child.stderr_behavior = .Inherit;

    try child.spawn();
    _ = try child.wait();
}

// Helpers
fn displayPrompt(allocator: std.mem.Allocator) ![:0]const u8 {

    // To display prompt
    const user = std.posix.getenv("USER") orelse "user";
    var hostname_buf: [64]u8 = undefined;
    const hostname = try std.posix.gethostname(&hostname_buf);

    const cwd_path = try std.fs.cwd().realpathAlloc(allocator, ".");
    const dir_name = std.fs.path.basename(cwd_path);


    return std.fmt.allocPrintSentinel(allocator, "[\x1b[33m{s}\x1b[0m\x1b[31m@\x1b[0m\x1b[92m{s}\x1b[0m \x1b[34m{s}\x1b[0m]$ ", .{ user, hostname, dir_name }, 0);
}

fn getHistoryFile(allocator: std.mem.Allocator) ![:0]const u8 {
    const home = std.posix.getenv("HOME") orelse ".";

    const raw_path = try std.fs.path.join(allocator, &[_][]const u8{ home, ".fastsh_history" });
    
    defer allocator.free(raw_path);

    return try allocator.dupeZ(u8, raw_path);
}

fn hasWildcard(s: []const u8) bool {
    for (s) |char| {
        if (char == '*' or char == '?' or char == ']') return true;
    }
    return false;
}
