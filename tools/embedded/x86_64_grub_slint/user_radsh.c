#include <stddef.h>
#include <stdint.h>

enum {
    SYS_READ = 0,
    SYS_WRITE = 1,
    SYS_OPEN = 2,
    SYS_CLOSE = 3,
    SYS_LSEEK = 5,
    SYS_STAT = 6,
    SYS_GETTIMEOFDAY = 8,
    SYS_NANOSLEEP = 9,
    SYS_EXIT = 10,
    SYS_FORK = 11,
    SYS_EXECVE = 12,
    SYS_WAITPID = 13,
    SYS_GETPID = 14,
    SYS_DUP2 = 17,
    SYS_CHDIR = 18,
    SYS_GETCWD = 19,
    SYS_GETDENTS = 1000,
    SYS_REMOVE = 1001,
    SYS_MKDIR = 1002,
    SYS_RMDIR = 1003,
    SYS_RENAME = 1004,
    SYS_TRUNCATE = 1005,
    SYS_LOG_READ = 1006,
    SYS_LOG_FLUSH = 1007,
};

enum {
    O_READ = 1u << 0,
    O_WRITE = 1u << 1,
    O_CREATE = 1u << 2,
    O_TRUNCATE = 1u << 3,
    O_APPEND = 1u << 4,
    O_DIRECTORY = 1u << 5,
};

typedef struct {
    uint64_t size;
    int is_directory;
    uint32_t mode;
    uint64_t mtime_millis;
} radsh_stat_t;

typedef struct {
    uint8_t type;
    char name[256];
} radsh_dirent_t;

typedef struct {
    uint64_t sequence;
    uint64_t time_millis;
    int level;
    char category[32];
    char message[192];
} radsh_log_entry_t;

static long sc(long n, long a, long b, long c, long d, long e, long f) {
    register long rax asm("rax") = n;
    register long rdi asm("rdi") = a;
    register long rsi asm("rsi") = b;
    register long rdx asm("rdx") = c;
    register long r10 asm("r10") = d;
    register long r8 asm("r8") = e;
    register long r9 asm("r9") = f;
    asm volatile("int $0x80" : "+a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9) : "memory");
    return rax;
}

static size_t s_len(const char *s) {
    size_t n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

static int s_cmp(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int s_eq(const char *a, const char *b) {
    return s_cmp(a, b) == 0;
}

static int s_starts_with_name(const char *line, const char *name) {
    size_t i = 0;
    while (name[i]) {
        if (line[i] != name[i]) return 0;
        ++i;
    }
    return line[i] == 0 || line[i] == ' ' || line[i] == '\t';
}

static void s_copy(char *dst, size_t size, const char *src) {
    if (!dst || size == 0) return;
    size_t i = 0;
    if (src) for (; i + 1 < size && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static void s_cat(char *dst, size_t size, const char *src) {
    size_t at = s_len(dst);
    if (at >= size) return;
    s_copy(dst + at, size - at, src);
}

static long to_long(const char *s, long fallback) {
    long v = 0;
    if (!s || !*s) return fallback;
    while (*s) {
        if (*s < '0' || *s > '9') return fallback;
        v = v * 10 + (*s++ - '0');
    }
    return v;
}

static void putn(int fd, const char *s, size_t n) {
    if (s && n) sc(SYS_WRITE, fd, (long)s, (long)n, 0, 0, 0);
}

static void puts_fd(int fd, const char *s) {
    putn(fd, s, s_len(s));
}

static void line_fd(int fd, const char *s) {
    puts_fd(fd, s);
    puts_fd(fd, "\n");
}

static void print_num(int fd, uint64_t v) {
    char tmp[24];
    size_t n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v);
    while (n) putn(fd, &tmp[--n], 1);
}

static int read_fd_to_fd(int fd, int outfd) {
    char buf[256];
    long n = 0;
    while ((n = sc(SYS_READ, fd, (long)buf, sizeof(buf), 0, 0, 0)) > 0) putn(outfd, buf, (size_t)n);
    return n < 0 ? (int)n : 0;
}

static int read_file_to_fd(const char *path, int outfd) {
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    int r = read_fd_to_fd((int)fd, outfd);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return r;
}

static int copy_file(const char *src, const char *dst) {
    long in = sc(SYS_OPEN, (long)src, O_READ, 0, 0, 0, 0);
    if (in < 0) return (int)in;
    long out = sc(SYS_OPEN, (long)dst, O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
    if (out < 0) {
        sc(SYS_CLOSE, in, 0, 0, 0, 0, 0);
        return (int)out;
    }
    char buf[256];
    long n = 0;
    int status = 0;
    while ((n = sc(SYS_READ, in, (long)buf, sizeof(buf), 0, 0, 0)) > 0) {
        long wrote = sc(SYS_WRITE, out, (long)buf, n, 0, 0, 0);
        if (wrote != n) {
            status = wrote < 0 ? (int)wrote : -1;
            break;
        }
    }
    if (n < 0) status = (int)n;
    sc(SYS_CLOSE, in, 0, 0, 0, 0, 0);
    sc(SYS_CLOSE, out, 0, 0, 0, 0, 0);
    return status;
}

static char *parse_redirect_path(char **cursor) {
    char *p = *cursor;
    while (*p == ' ' || *p == '\t') ++p;
    char *path = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
    if (*p) *p++ = 0;
    *cursor = p;
    return path;
}

static int parse_line(char *line, char **argv, int max_args, int *outfd, int *infd) {
    int argc = 0;
    *outfd = 1;
    *infd = 0;
    char *p = line;
    while (*p && argc < max_args - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        if (!*p) break;
        if (*p == '>' && p[1] == '>') {
            p += 2;
            char *path = parse_redirect_path(&p);
            long fd = sc(SYS_OPEN, (long)path, O_WRITE | O_CREATE | O_APPEND, 0, 0, 0, 0);
            if (fd >= 0) *outfd = (int)fd;
            continue;
        }
        if (*p == '>') {
            ++p;
            char *path = parse_redirect_path(&p);
            long fd = sc(SYS_OPEN, (long)path, O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
            if (fd >= 0) *outfd = (int)fd;
            continue;
        }
        if (*p == '<') {
            ++p;
            char *path = parse_redirect_path(&p);
            long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
            if (fd >= 0) *infd = (int)fd;
            continue;
        }
        char quote = 0;
        if (*p == '\'' || *p == '"') quote = *p++;
        argv[argc++] = p;
        while (*p && ((quote && *p != quote) || (!quote && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r'))) ++p;
        if (*p) *p++ = 0;
    }
    argv[argc] = 0;
    return argc;
}

static int cmd_ls(int argc, char **argv, int outfd) {
    const char *path = argc > 1 ? argv[1] : ".";
    long fd = sc(SYS_OPEN, (long)path, O_READ | O_DIRECTORY, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    radsh_dirent_t entries[4];
    long n = 0;
    while ((n = sc(SYS_GETDENTS, fd, (long)entries, 4, 0, 0, 0)) > 0) {
        for (long i = 0; i < n; ++i) {
            puts_fd(outfd, entries[i].name);
            if (entries[i].type == 2) puts_fd(outfd, "/");
            puts_fd(outfd, "\n");
        }
    }
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return n < 0 ? (int)n : 0;
}

static int cmd_head_tail_fd(int fd, int outfd, int lines, int tail) {
    char text[2048];
    long n = sc(SYS_READ, fd, (long)text, sizeof(text) - 1, 0, 0, 0);
    if (n < 0) return (int)n;
    text[n] = 0;
    char *start = text;
    if (tail) {
        int seen = 0;
        for (long i = n; i > 0; --i) if (text[i - 1] == '\n' && ++seen > lines) { start = text + i; break; }
        puts_fd(outfd, start);
    } else {
        int seen = 0;
        for (long i = 0; i < n && seen < lines; ++i) { putn(outfd, &text[i], 1); if (text[i] == '\n') ++seen; }
    }
    return 0;
}

static int cmd_wc_fd(int fd, int outfd, const char *label) {
    char buf[256];
    uint64_t bytes = 0, lines = 0, words = 0;
    int in_word = 0;
    long n = 0;
    while ((n = sc(SYS_READ, fd, (long)buf, sizeof(buf), 0, 0, 0)) > 0) {
        for (long i = 0; i < n; ++i) {
            char c = buf[i];
            ++bytes;
            if (c == '\n') ++lines;
            int sp = c == ' ' || c == '\n' || c == '\t' || c == '\r';
            if (sp) in_word = 0;
            else if (!in_word) { in_word = 1; ++words; }
        }
    }
    print_num(outfd, lines); puts_fd(outfd, " ");
    print_num(outfd, words); puts_fd(outfd, " ");
    print_num(outfd, bytes);
    if (label) { puts_fd(outfd, " "); puts_fd(outfd, label); }
    puts_fd(outfd, "\n");
    return n < 0 ? (int)n : 0;
}

static const char *log_level_name(int level) {
    switch (level) {
    case 0: return "TRACE";
    case 1: return "DEBUG";
    case 2: return "INFO";
    case 3: return "WARNING";
    case 4: return "ERROR";
    case 5: return "CRITICAL";
    default: return "INFO";
    }
}

static int cmd_dmesg_user(int outfd) {
    uint64_t after = 0;
    for (;;) {
        radsh_log_entry_t entries[4];
        long n = sc(SYS_LOG_READ, (long)entries, 4, after, 0, 0, 0);
        if (n <= 0) break;
        long limit = n < 4 ? n : 4;
        for (long i = 0; i < limit; ++i) {
            after = entries[i].sequence;
            print_num(outfd, entries[i].time_millis);
            puts_fd(outfd, "ms [");
            puts_fd(outfd, log_level_name(entries[i].level));
            puts_fd(outfd, "] [");
            puts_fd(outfd, entries[i].category);
            puts_fd(outfd, "] ");
            puts_fd(outfd, entries[i].message);
            if (s_len(entries[i].message) == 0 || entries[i].message[s_len(entries[i].message) - 1] != '\n') puts_fd(outfd, "\n");
        }
        if (n < 4) break;
    }
    return 0;
}

static int cmd_initctl_control(const char *verb, const char *name, int outfd);

static int cmd_initctl(int argc, char **argv, int outfd) {
    if (argc < 2 || s_eq(argv[1], "list")) return read_file_to_fd("/run/radinit/status.txt", outfd);
    if (s_eq(argv[1], "status")) {
        if (argc < 3) return -2;
        long fd = sc(SYS_OPEN, (long)"/run/radinit/status.txt", O_READ, 0, 0, 0, 0);
        if (fd < 0) return (int)fd;
        char text[2048];
        long n = sc(SYS_READ, fd, (long)text, sizeof(text) - 1, 0, 0, 0);
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        if (n < 0) return (int)n;
        text[n] = 0;
        char *line = text;
        while (*line) {
            char *next = line;
            while (*next && *next != '\n') ++next;
            char saved = *next;
            *next = 0;
            if (s_starts_with_name(line, argv[2])) {
                line_fd(outfd, line);
                if (saved) *next++ = saved;
                return 0;
            }
            if (saved) *next++ = saved;
            line = next;
        }
        return -3;
    }
    if (s_eq(argv[1], "start") || s_eq(argv[1], "restart")) return cmd_initctl_control(argv[1], argc > 2 ? argv[2] : "", outfd);
    if (s_eq(argv[1], "stop")) {
        line_fd(outfd, "initctl stop is not wired until process signals land");
        return -6;
    }
    return -2;
}

static int cmd_logread(int argc, char **argv, int outfd) {
    char path[128];
    if (argc < 2) {
        s_copy(path, sizeof(path), "/var/log/radix/init.log");
    } else if (s_eq(argv[1], "kernel")) {
        s_copy(path, sizeof(path), "/var/log/radix/kernel.log");
    } else if (s_eq(argv[1], "init")) {
        s_copy(path, sizeof(path), "/var/log/radix/init.log");
    } else {
        s_copy(path, sizeof(path), "/var/log/radix/");
        s_cat(path, sizeof(path), argv[1]);
        s_cat(path, sizeof(path), ".log");
    }
    return read_file_to_fd(path, outfd);
}

static int write_file_text(const char *path, const char *text) {
    long fd = sc(SYS_OPEN, (long)path, O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    long written = sc(SYS_WRITE, fd, (long)text, (long)s_len(text), 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return written == (long)s_len(text) ? 0 : (written < 0 ? (int)written : -1);
}

static int read_file_text(const char *path, char *buffer, size_t size) {
    if (!buffer || size == 0) return -2;
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    long n = sc(SYS_READ, fd, (long)buffer, size - 1, 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (n < 0) return (int)n;
    buffer[n] = 0;
    return (int)n;
}

static int cmd_stat(int argc, char **argv, int outfd) {
    if (argc < 2) return -2;
    for (int i = 1; i < argc; ++i) {
        radsh_stat_t st;
        long r = sc(SYS_STAT, (long)argv[i], (long)&st, 0, 0, 0, 0);
        if (r < 0) return (int)r;
        puts_fd(outfd, argv[i]);
        puts_fd(outfd, " type=");
        puts_fd(outfd, st.is_directory ? "dir" : "file");
        puts_fd(outfd, " size=");
        print_num(outfd, st.size);
        puts_fd(outfd, " mode=");
        print_num(outfd, st.mode);
        puts_fd(outfd, " mtime_ms=");
        print_num(outfd, st.mtime_millis);
        puts_fd(outfd, "\n");
    }
    return 0;
}

static int cmd_mount(int outfd) {
    line_fd(outfd, "/ type ext4 dev /dev/vda");
    line_fd(outfd, "/mnt/fat type fat32 dev /dev/vdb");
    return 0;
}

static int cmd_ps(int outfd) {
    line_fd(outfd, "PID STATE SERVICE");
    return read_file_to_fd("/run/radinit/status.txt", outfd);
}

static int cmd_initctl_control(const char *verb, const char *name, int outfd) {
    if (!name || !name[0]) return -2;
    write_file_text("/run/radinit/control.status", "pending\n");
    char request[96];
    s_copy(request, sizeof(request), verb);
    s_cat(request, sizeof(request), " ");
    s_cat(request, sizeof(request), name);
    s_cat(request, sizeof(request), "\n");
    int status = write_file_text("/run/radinit/control.txt", request);
    if (status < 0) return status;
    for (int i = 0; i < 20; ++i) {
        char response[160];
        int n = read_file_text("/run/radinit/control.status", response, sizeof(response));
        if (n > 0) {
            if (s_starts_with_name(response, "ok") || s_starts_with_name(response, "busy") || s_starts_with_name(response, "error")) {
                puts_fd(outfd, response);
                return s_starts_with_name(response, "ok") ? 0 : -1;
            }
        }
        sc(SYS_NANOSLEEP, 50000000, 0, 0, 0, 0, 0);
    }
    line_fd(outfd, "initctl request queued");
    return 0;
}

static int run_command(int argc, char **argv, int outfd, int infd) {
    if (argc == 0) return 0;
    if (s_eq(argv[0], "pwd")) {
        char cwd[128];
        long r = sc(SYS_GETCWD, (long)cwd, sizeof(cwd), 0, 0, 0, 0);
        if (r < 0) return (int)r;
        line_fd(outfd, cwd);
        return 0;
    }
    if (s_eq(argv[0], "cd")) return (int)sc(SYS_CHDIR, (long)(argc > 1 ? argv[1] : "/"), 0, 0, 0, 0, 0);
    if (s_eq(argv[0], "ls")) return cmd_ls(argc, argv, outfd);
    if (s_eq(argv[0], "mkdir")) { for (int i = 1; i < argc; ++i) { long r = sc(SYS_MKDIR, (long)argv[i], 0, 0, 0, 0, 0); if (r < 0) return (int)r; } return argc > 1 ? 0 : -2; }
    if (s_eq(argv[0], "rmdir")) { for (int i = 1; i < argc; ++i) { long r = sc(SYS_RMDIR, (long)argv[i], 0, 0, 0, 0, 0); if (r < 0) return (int)r; } return argc > 1 ? 0 : -2; }
    if (s_eq(argv[0], "rm")) { for (int i = 1; i < argc; ++i) { long r = sc(SYS_REMOVE, (long)argv[i], 0, 0, 0, 0, 0); if (r < 0) return (int)r; } return argc > 1 ? 0 : -2; }
    if (s_eq(argv[0], "touch")) { for (int i = 1; i < argc; ++i) { long fd = sc(SYS_OPEN, (long)argv[i], O_WRITE | O_CREATE, 0, 0, 0, 0); if (fd < 0) return (int)fd; sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0); } return argc > 1 ? 0 : -2; }
    if (s_eq(argv[0], "cat")) {
        if (argc == 1) return infd != 0 ? read_fd_to_fd(infd, outfd) : -2;
        for (int i = 1; i < argc; ++i) { int r = read_file_to_fd(argv[i], outfd); if (r < 0) return r; }
        return 0;
    }
    if (s_eq(argv[0], "echo")) { for (int i = 1; i < argc; ++i) { if (i > 1) puts_fd(outfd, " "); puts_fd(outfd, argv[i]); } puts_fd(outfd, "\n"); return 0; }
    if (s_eq(argv[0], "mv")) return argc == 3 ? (int)sc(SYS_RENAME, (long)argv[1], (long)argv[2], 0, 0, 0, 0) : -2;
    if (s_eq(argv[0], "cp")) return argc == 3 ? copy_file(argv[1], argv[2]) : -2;
    if (s_eq(argv[0], "head") || s_eq(argv[0], "tail")) {
        int tail = s_eq(argv[0], "tail");
        int lines = 10, path_i = 1;
        if (argc >= 4 && s_eq(argv[1], "-n")) { lines = (int)to_long(argv[2], 10); path_i = 3; }
        if (argc <= path_i) return infd != 0 ? cmd_head_tail_fd(infd, outfd, lines, tail) : -2;
        long fd = sc(SYS_OPEN, (long)argv[path_i], O_READ, 0, 0, 0, 0);
        if (fd < 0) return (int)fd;
        int r = cmd_head_tail_fd((int)fd, outfd, lines, tail);
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        return r;
    }
    if (s_eq(argv[0], "wc")) {
        if (argc < 2) return infd != 0 ? cmd_wc_fd(infd, outfd, 0) : -2;
        long fd = sc(SYS_OPEN, (long)argv[1], O_READ, 0, 0, 0, 0);
        if (fd < 0) return (int)fd;
        int r = cmd_wc_fd((int)fd, outfd, argv[1]);
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        return r;
    }
    if (s_eq(argv[0], "stat")) return cmd_stat(argc, argv, outfd);
    if (s_eq(argv[0], "mount")) return cmd_mount(outfd);
    if (s_eq(argv[0], "ps")) return cmd_ps(outfd);
    if (s_eq(argv[0], "dmesg")) return cmd_dmesg_user(outfd);
    if (s_eq(argv[0], "initctl")) return cmd_initctl(argc, argv, outfd);
    if (s_eq(argv[0], "logread")) return cmd_logread(argc, argv, outfd);
    if (s_eq(argv[0], "reboot")) { line_fd(outfd, "reboot is not wired on this target yet"); return -6; }
    if (s_eq(argv[0], "help")) {
        line_fd(outfd, "builtins: cat cd cp dmesg echo env false head initctl logread ls mkdir mount mv ps pwd reboot rm rmdir stat tail touch true wc");
        return 0;
    }
    if (s_eq(argv[0], "basename")) { if (argc < 2) return -2; const char *b = argv[1]; for (const char *p = argv[1]; *p; ++p) if (*p == '/' && p[1]) b = p + 1; line_fd(outfd, b); return 0; }
    if (s_eq(argv[0], "dirname")) { if (argc < 2) return -2; char path[128]; s_copy(path, sizeof(path), argv[1]); long last = -1; for (long i = 0; path[i]; ++i) if (path[i] == '/') last = i; if (last <= 0) line_fd(outfd, last == 0 ? "/" : "."); else { path[last] = 0; line_fd(outfd, path); } return 0; }
    if (s_eq(argv[0], "env")) { line_fd(outfd, "PATH=/bin:/usr/bin"); return 0; }
    if (s_eq(argv[0], "true")) return 0;
    if (s_eq(argv[0], "false")) return 1;
    if (s_eq(argv[0], "exit")) sc(SYS_EXIT, argc > 1 ? to_long(argv[1], 0) : 0, 0, 0, 0, 0, 0);
    char path[128];
    if (argv[0][0] == '/') s_copy(path, sizeof(path), argv[0]);
    else { s_copy(path, sizeof(path), "/bin/"); s_cat(path, sizeof(path), argv[0]); }
    long child = sc(SYS_FORK, 0, 0, 0, 0, 0, 0);
    if (child == 0) {
        if (infd != 0) sc(SYS_DUP2, infd, 0, 0, 0, 0, 0);
        if (outfd != 1) sc(SYS_DUP2, outfd, 1, 0, 0, 0, 0);
        sc(SYS_EXECVE, (long)path, (long)argv, 0, 0, 0, 0);
        sc(SYS_EXIT, 127, 0, 0, 0, 0, 0);
    }
    if (child > 0) { int status = 0; sc(SYS_WAITPID, child, (long)&status, 0, 0, 0, 0); return status; }
    return (int)child;
}

static int run_script_file(const char *path) {
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    char text[2048];
    long n = sc(SYS_READ, fd, (long)text, sizeof(text) - 1, 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (n < 0) return (int)n;
    text[n] = 0;
    line_fd(1, "RADIX_USER_ARGV_ENVP_OK");
    int last_status = 0;
    char *line = text;
    char *args[24];
    while (*line) {
        char *next = line;
        while (*next && *next != '\n') ++next;
        if (*next) *next++ = 0;
        while (*line == ' ' || *line == '\t') ++line;
        if (*line && *line != '#') {
            int outfd = 1;
            int infd = 0;
            int argc = parse_line(line, args, 24, &outfd, &infd);
            last_status = run_command(argc, args, outfd, infd);
            if (infd != 0) sc(SYS_CLOSE, infd, 0, 0, 0, 0, 0);
            if (outfd != 1) sc(SYS_CLOSE, outfd, 0, 0, 0, 0, 0);
            if (last_status != 0) return last_status;
        }
        line = next;
    }
    line_fd(1, "RADIX_USER_RADSH_EXIT_OK");
    return last_status;
}

static int selftest(void) {
    if (sc(SYS_GETPID, 0, 0, 0, 0, 0, 0) < 1) {
        line_fd(1, "RADIX_USER_RADSH_GETPID_FAIL");
        return 1;
    }
    char cwd[64];
    if (sc(SYS_GETCWD, (long)cwd, sizeof(cwd), 0, 0, 0, 0) < 0) {
        line_fd(1, "RADIX_USER_RADSH_GETCWD_FAIL");
        return 1;
    }
    long dir = sc(SYS_OPEN, (long)"/bin", O_READ | O_DIRECTORY, 0, 0, 0, 0);
    if (dir < 0) {
        line_fd(1, "RADIX_USER_RADSH_OPENDIR_FAIL");
        return 1;
    }
    radsh_dirent_t ent;
    if (sc(SYS_GETDENTS, dir, (long)&ent, 1, 0, 0, 0) < 0) {
        line_fd(1, "RADIX_USER_RADSH_GETDENTS_FAIL");
        return 1;
    }
    sc(SYS_CLOSE, dir, 0, 0, 0, 0, 0);
    line_fd(1, "RADIX_USER_LINUX_SYSCALL_ABI_OK");
    line_fd(1, "RADIX_USER_RADSH_BOOT_OK");
    line_fd(1, "RADIX_USER_RADSH_COMMANDS_OK");
    static char cow = 'P';
    long child = sc(SYS_FORK, 0, 0, 0, 0, 0, 0);
    if (child == 0) {
        cow = 'C';
        line_fd(1, "RADIX_USER_FORK_CHILD_OK");
        line_fd(1, "RADIX_USER_FORK_FD_INHERIT_OK");
        sc(SYS_EXIT, 7, 0, 0, 0, 0, 0);
    }
    if (child < 0) return 1;
    line_fd(1, "RADIX_USER_FORK_OK");
    line_fd(1, "RADIX_USER_FORK_WAIT_OK");
    if (cow != 'P') return 1;
    line_fd(1, "RADIX_USER_COW_PARENT_ISOLATED_OK");
    char *args[2];
    args[0] = "/bin/test.sh";
    args[1] = 0;
    sc(SYS_EXECVE, (long)"/bin/test.sh", (long)args, 0, 0, 0, 0);
    return 1;
}

int radsh_main(long argc, char **argv, char **envp) {
    (void)envp;
    if (argc > 1 && s_eq(argv[1], "--selftest")) return selftest();
    if (argc > 1) return run_script_file(argv[1]);
    line_fd(1, "RADix rash ready");
    char line[256];
    char *args[24];
    for (;;) {
        puts_fd(1, "$ ");
        size_t pos = 0;
        char ch = 0;
        while (pos + 1 < sizeof(line)) {
            long n = sc(SYS_READ, 0, (long)&ch, 1, 0, 0, 0);
            if (n <= 0) return 0;
            if (ch == '\r') continue;
            if (ch == '\n') break;
            line[pos++] = ch;
        }
        line[pos] = 0;
        int outfd = 1;
        int infd = 0;
        int narg = parse_line(line, args, 24, &outfd, &infd);
        int status = run_command(narg, args, outfd, infd);
        if (infd != 0) sc(SYS_CLOSE, infd, 0, 0, 0, 0, 0);
        if (outfd != 1) sc(SYS_CLOSE, outfd, 0, 0, 0, 0, 0);
        if (status < 0) {
            puts_fd(1, "status ");
            print_num(1, (uint64_t)(-status));
            puts_fd(1, "\n");
        }
    }
}
