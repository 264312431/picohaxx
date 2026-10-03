// =============================================================================
// bundle.h
// easily bundle files. fileless access/exec possible through memfd.
// =============================================================================
#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#ifndef __NR_memfd_create
#  if defined(__aarch64__)
#    define __NR_memfd_create 279
#  elif defined(__arm__)
#    define __NR_memfd_create 385
#  else
#    error "Unsupported architecture"
#  endif
#endif
#ifndef MFD_CLOEXEC
#  define MFD_CLOEXEC 0x0001U
#endif

// ── write payload into a fresh memfd, return fd or -1 ────────────────
static inline int _bundle_memfd(const char *tag,
                                const unsigned char *data, size_t len)
{
    int fd = (int)syscall(__NR_memfd_create, tag, 0);
    if (fd < 0) return -1;
    if (write(fd, data, len) != (ssize_t)len) { close(fd); return -1; }
    return fd;
}

// ── argv builder ──────────────────────────────────────────────────────
#define _BUNDLE_MKARGV(name, argv0, ap_in)                                      \
    __extension__ ({                                                            \
        if (!(argv0)) (argv0) = #name;                                          \
        va_list _ap; va_copy(_ap, (ap_in));                                     \
        size_t _n = 1;                                                          \
        while (va_arg(_ap, const char *)) ++_n;                                 \
        va_end(_ap);                                                            \
        char **_argv = malloc((_n + 1) * sizeof(char *));                       \
        if (_argv) {                                                            \
            _argv[0] = (char *)(argv0);                                         \
            va_copy(_ap, (ap_in));                                              \
            size_t _i = 1; const char *_a;                                      \
            while ((_a = va_arg(_ap, const char *))) _argv[_i++] = (char *)_a;  \
            va_end(_ap); _argv[_i] = NULL;                                      \
        }                                                                       \
        _argv;                                                                  \
    })

// ════════════════════════════════════════════════════════════════════
// GENERIC SPAWN / SYSTEM / FEXEC — cgroup escape + double-fork
// ════════════════════════════════════════════════════════════════════
static inline void _escape_cgroups_self(void)
{
    pid_t pid = getpid();
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", pid);

    const char *targets[] = {
        "/acct/tasks",
        "/acct/cgroup.procs",
        "/dev/cpuctl/tasks",
        "/dev/cpuset/tasks",
        "/dev/stune/tasks",
        "/sys/fs/cgroup/cgroup.procs"
    };

    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++) {
        int fd = open(targets[i], O_WRONLY);
        if (fd >= 0) {
            write(fd, pid_str, strlen(pid_str));
            close(fd);
        }
    }
}

// ── dospawn: double-fork, returns grandchild pid ────────────────────
//    filename: path to binary (memfd `/proc/self/fd/N` or disk path)
//    argv[]: NULL-terminated argv array
static inline int dospawn_internal(const char *filename, char *const argv[])
{
    if (!filename || !argv) { errno = EINVAL; return -1; }

    int pfd[2];
    if (pipe(pfd) != 0) return -1;

    pid_t outer = fork();
    if (outer < 0) {
        close(pfd[0]); close(pfd[1]);
        return -1;
    }

    if (outer == 0) {
        // outer child
        close(pfd[0]);
        setsid();

        pid_t inner = fork();
        if (inner < 0) {
            int e = errno;
            pid_t fail_val = -1;
            write(pfd[1], &fail_val, sizeof(pid_t));
            close(pfd[1]);
            _exit(e); 
        }

        if (inner == 0) {
            // grandchild: runs in background, reparented to init
            close(pfd[1]);
            _escape_cgroups_self();

            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                if (devnull > STDERR_FILENO) close(devnull);
            }

            execv(filename, argv);
            _exit(errno);
        }

        // outer: send grandchild pid to parent, then exit
        write(pfd[1], &inner, sizeof(pid_t));
        close(pfd[1]);
        _exit(0);
    }

    // parent
    close(pfd[1]);
    pid_t grandchild = -1;
    read(pfd[0], &grandchild, sizeof(pid_t));
    close(pfd[0]);

    int status;
    waitpid(outer, &status, 0);

    if (grandchild < 0) {
        errno = ECHILD;
        return -1;
    }
    return (int)grandchild;
}

// ── dosystem: fire-and-forget ──────────────────────────────────────
static inline void dosystem_internal(const char *filename, char *const argv[])
{
    if (!filename || !argv) return;

    if (fork() == 0) {
        setsid();
        _escape_cgroups_self();

        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }

        execv(filename, argv);
        _exit(errno);
    }
}

// ── dofexec: replace current process ────────────────────────────────
static inline int dofexec_internal(const char *filename, char *const argv[])
{
    if (!filename || !argv) { errno = EINVAL; return -1; }
    execv(filename, argv);
    return -1;
}

// ════════════════════════════════════════════════════════════════════
// BUNDLE(name, file)
//
//   Embeds 'file' at compile time, accessible via bundle_file(name).
//   Provides three inline wrappers:
//
//   int  spawn_<name>(char *argv0, ...)   — double-fork, returns grandchild pid
//   void system_<name>(char *argv0, ...)  — fire-and-forget
//   int  fexec_<name>(char *argv0, ...)   — replace current process
//
//   argv0 == NULL  →  auto-filled with #name
//   Variadic list is NULL-terminated.
// ════════════════════════════════════════════════════════════════════
#define BUNDLE(name, file)                                                      \
    __asm__(                                                                    \
        ".global _bundle_bytes_" #name "\n"                                     \
        ".global _bundle_len_"   #name "\n"                                     \
        ".balign 16\n"                                                          \
        "_bundle_bytes_" #name ":\n"                                            \
        ".incbin \"" file "\"\n"                                                \
        "_bundle_end_"  #name ":\n"                                             \
        "_bundle_len_"  #name ":\n"                                             \
        ".quad _bundle_end_" #name " - _bundle_bytes_" #name "\n"               \
    );                                                                          \
    extern unsigned char _bundle_bytes_##name[];                          \
    extern unsigned long _bundle_len_##name;                              \
                                                                                \
    /* Memfd-based: create on-demand, cache fd + path */                        \
    static int  _bundle_fd_##name = -1;                                         \
    static char _bundle_path_##name[256] = {0};                                 \
                                                                                \
    static inline  char *bundle_file_##name(void)                               \
    {                                                                           \
        if (_bundle_fd_##name < 0) {                                            \
            _bundle_fd_##name = _bundle_memfd(#name,                            \
                _bundle_bytes_##name, (size_t)_bundle_len_##name);              \
            if (_bundle_fd_##name < 0) return NULL;                             \
        }                                                                       \
        if (!_bundle_path_##name[0]) {                                          \
            snprintf(_bundle_path_##name, sizeof(_bundle_path_##name),          \
                "/proc/self/fd/%d", _bundle_fd_##name);                         \
        }                                                                       \
        return _bundle_path_##name;                                             \
    }                                                                           \
                                                                                \
    /* spawn_name: double-fork wrapper */                                       \
    static inline int spawn_##name(const char *argv0, ...)                      \
    {                                                                           \
        va_list ap; va_start(ap, argv0);                                        \
        char **argv = _BUNDLE_MKARGV(name, argv0, ap);                          \
        va_end(ap);                                                             \
        if (!argv) return -1;                                                   \
        const char *path = bundle_file_##name();                                \
        if (!path) { free(argv); return -1; }                                   \
        int pid = dospawn_internal(path, argv);                                 \
        free(argv);                                                             \
        return pid;                                                             \
    }                                                                           \
                                                                                \
    /* system_name: fire-and-forget wrapper */                                  \
    static inline void system_##name(const char *argv0, ...)                    \
    {                                                                           \
        va_list ap; va_start(ap, argv0);                                        \
        char **argv = _BUNDLE_MKARGV(name, argv0, ap);                          \
        va_end(ap);                                                             \
        if (!argv) return;                                                      \
        const char *path = bundle_file_##name();                                \
        if (path) dosystem_internal(path, argv);                                \
        free(argv);                                                             \
    }                                                                           \
                                                                                \
    /* fexec_name: replace process wrapper */                                   \
    static inline int fexec_##name(const char *argv0, ...)                      \
    {                                                                           \
        va_list ap; va_start(ap, argv0);                                        \
        char **argv = _BUNDLE_MKARGV(name, argv0, ap);                          \
        va_end(ap);                                                             \
        if (!argv) { errno = ENOMEM; return -1; }                               \
        const char *path = bundle_file_##name();                                \
        if (!path) { free(argv); errno = ENOENT; return -1; }                   \
        int ret = dofexec_internal(path, argv);                                 \
        free(argv);                                                             \
        return ret;                                                             \
    }

// ══════════════════════════════════════════════════════════════════════════
// DEPLOYED_BUNDLE(name, file, default_path)
//
//   Lazy-deploy variant: embedded binary is written to disk on first access.
//   Accessible via bundle_file(name), just like BUNDLE().
//
//   ** IMPORTANT: Choose EITHER BUNDLE() OR DEPLOYED_BUNDLE() per binary,
//                 not both. They're mutually exclusive. **
//
//   Lazy deployment logic:
//     1. On first bundle_file(name) call: stat() the path
//     2. If missing or size differs: write + chmod 0700
//     3. Cache path, reuse for subsequent calls in same process
//
//   Provides same three wrappers as BUNDLE():
//   - spawn_<name>(char *argv0, ...)
//   - system_<name>(char *argv0, ...)
//   - fexec_<name>(char *argv0, ...)
// ══════════════════════════════════════════════════════════════════════════
#define DEPLOYED_BUNDLE(name, file, default_path)                               \
    /* Embed binary at compile time (same as BUNDLE) */                         \
    __asm__(                                                                    \
        ".global _bundle_bytes_" #name "\n"                                     \
        ".global _bundle_len_"   #name "\n"                                     \
        ".balign 16\n"                                                          \
        "_bundle_bytes_" #name ":\n"                                            \
        ".incbin \"" file "\"\n"                                                \
        "_bundle_end_"  #name ":\n"                                             \
        "_bundle_len_"  #name ":\n"                                             \
        ".quad _bundle_end_" #name " - _bundle_bytes_" #name "\n"               \
    );                                                                          \
    extern const unsigned char _bundle_bytes_##name[];                          \
    extern const unsigned long _bundle_len_##name;                              \
                                                                                \
    /* Lazy-deploy state */                                                     \
    static int _deployed_##name = 0;  /* 0=not yet, 1=ok, -1=failed */          \
    static char _deployed_path_##name[256] = {0};                               \
                                                                                \
    /* Resolve path: provided > default_path > auto */                          \
    static inline const char *_bundle_deploy_path_##name(const char *path,      \
                                                         char *buf, size_t sz)  \
    {                                                                           \
        if (path) return path;                                                  \
        if (default_path) return default_path;                                  \
        snprintf(buf, sz, "/data/local/tmp/.%s", #name);                        \
        return buf;                                                             \
    }                                                                           \
                                                                                \
    /* Lazy deploy: size check + write on demand */                             \
    static inline int _bundle_ensure_deployed_##name(const char *deploy_path)   \
    {                                                                           \
        if (_deployed_##name == 1) return 0;   /* already deployed in run */    \
        if (_deployed_##name == -1) return -1; /* prev attempt failed */        \
                                                                                \
        char _pb[256];                                                          \
        deploy_path = _bundle_deploy_path_##name(deploy_path, _pb, sizeof(_pb));\
        if (!deploy_path) { _deployed_##name = -1; return -1; }                 \
                                                                                \
        struct stat st;                                                         \
        if (stat(deploy_path, &st) == 0) {                                      \
            if (st.st_size == (off_t)_bundle_len_##name) {                      \
                /* Size matches; assume OK */                                   \
                _deployed_##name = 1;                                           \
                strncpy(_deployed_path_##name, deploy_path,                     \
                    sizeof(_deployed_path_##name) - 1);                         \
                return 0;                                                       \
            }                                                                   \
        }                                                                       \
                                                                                \
        /* Write (or re-write) */                                               \
        int fd = open(deploy_path, O_WRONLY | O_CREAT | O_TRUNC, 0700);         \
        if (fd < 0) { _deployed_##name = -1; return -1; }                       \
                                                                                \
        const unsigned char *p = _bundle_bytes_##name;                          \
        size_t rem = (size_t)_bundle_len_##name;                                \
        while (rem > 0) {                                                       \
            ssize_t w = write(fd, p, rem);                                      \
            if (w < 0) { close(fd); _deployed_##name = -1; return -1; }         \
            p += w; rem -= (size_t)w;                                           \
        }                                                                       \
        close(fd);                                                              \
                                                                                \
        if (chmod(deploy_path, 0700) != 0) {                                    \
            _deployed_##name = -1;                                              \
            return -1;                                                          \
        }                                                                       \
                                                                                \
        _deployed_##name = 1;                                                   \
        strncpy(_deployed_path_##name, deploy_path,                             \
            sizeof(_deployed_path_##name) - 1);                                 \
        return 0;                                                               \
    }                                                                           \
                                                                                \
    /* deploy_file: lazy deploy + return path */                                \
    static inline const char *deploy_file_##name(const char *deploy_path)       \
    {                                                                           \
        if (_bundle_ensure_deployed_##name(deploy_path) != 0) return NULL;      \
        return _deployed_path_##name;                                           \
    }                                                                           \
                                                                                \
    /* spawn_name: double-fork wrapper */                                       \
    static inline int spawn_##name(const char *argv0, ...)                      \
    {                                                                           \
        va_list ap; va_start(ap, argv0);                                        \
        char **argv = _BUNDLE_MKARGV(name, argv0, ap);                          \
        va_end(ap);                                                             \
        if (!argv) return -1;                                                   \
        const char *path = deploy_file_##name(NULL);                            \
        if (!path) { free(argv); return -1; }                                   \
        int pid = dospawn_internal(path, argv);                                 \
        free(argv);                                                             \
        return pid;                                                             \
    }                                                                           \
                                                                                \
    /* system_name: fire-and-forget wrapper */                                  \
    static inline void system_##name(const char *argv0, ...)                    \
    {                                                                           \
        va_list ap; va_start(ap, argv0);                                        \
        char **argv = _BUNDLE_MKARGV(name, argv0, ap);                          \
        va_end(ap);                                                             \
        if (!argv) return;                                                      \
        const char *path = deploy_file_##name(NULL);                            \
        if (path) dosystem_internal(path, argv);                                \
        free(argv);                                                             \
    }                                                                           \
                                                                                \
    /* fexec_name: replace process wrapper */                                   \
    static inline int fexec_##name(const char *argv0, ...)                      \
    {                                                                           \
        va_list ap; va_start(ap, argv0);                                        \
        char **argv = _BUNDLE_MKARGV(name, argv0, ap);                          \
        va_end(ap);                                                             \
        if (!argv) { errno = ENOMEM; return -1; }                               \
        const char *path = deploy_file_##name(NULL);                            \
        if (!path) { free(argv); errno = ENOENT; return -1; }                   \
        int ret = dofexec_internal(path, argv);                                 \
        free(argv);                                                             \
        return ret;                                                             \
    }                                                                           

#define bundle_file(bundle) bundle_file_##bundle()                              
#define deploy_file(bundle) deploy_file_##bundle(0)                             
