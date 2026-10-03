/* Auto-generated offsets for Pico 4 Kernels */
#include <stdint.h>
#include <string.h>

struct kernel_offsets {
    const char* fw_ver;
    uint64_t selinux_state;
};

/*=======================================================================================================================
system confirm_smartisan_version:   5.2.7-202212020445-RELEASE-user-phoenix-b2122
ro.pico.ota.version:                5.2.7-202212020445-RELEASE-user-phoenix-b2122
ro.pvr.internal.version:            c000_rf01_bv1.0.1_sv5.2.7_202212020445_phoenix_b2122_user  # '_' vs '-' ! 
========================================================================================================================*/
// Supported Pico 4 OS Versions: 5.2.0 up to 5.11.0 Chinese or Global 5.9.9.
//===========================================
// Epoch 1a layout  FW 5.2.0 - 5.6.1    EC0
// Epoch 1b layout  FW 5.7.0 - 5.7.2    EC0
// Epoch  2 layout  FW 5.8.0 - 5.8.2    F40
// Epoch  3 layout  FW 5.9.0 - 5.12.2   FC0
//===========================================
static struct kernel_offsets k_table[] = {
    // fw_ver               selinux_state
    { "5.2.0-202210211528",	0xffffff800aaae000 },
    { "5.2.1-202211111309",	0xffffff800aaae000 },
    { "5.2.7-202212020323",	0xffffff800aaae000 },
    { "5.2.7-202212020445",	0xffffff800aaae000 },

    { "5.3.0-202212230826",	0xffffff800aab1000 },
    { "5.3.1-202301051632",	0xffffff800aab1000 },
    { "5.3.1-202301051635",	0xffffff800aab1000 },
    { "5.3.1-202301051855",	0xffffff800aab1000 },
    { "5.3.2-202301071642", 0xffffff800aab1000 },
	{ "5.3.2-202301071817", 0xffffff800aab1000 },
    { "5.4.0-202302022206",	0xffffff800aab1000 },
    { "5.4.0-202302022219",	0xffffff800aab1000 },
    { "5.4.0-202302082133",	0xffffff800aab1000 },
    { "5.4.0-202302091032",	0xffffff800aab1000 },
    { "5.4.0-202302171231",	0xffffff800aab1000 },
    { "5.4.0-202302171557",	0xffffff800aab1000 },

    { "5.4.0-202302221812", 0xffffff800aab1000 },   
    { "5.5.0-202303210013",	0xffffff800aab1000 },
    { "5.5.0-202303210046",	0xffffff800aab1000 },
    { "5.5.0-202303210100",	0xffffff800aab1000 },
    { "5.5.0-202303210104",	0xffffff800aab1000 },
    { "5.5.0-202304070244",	0xffffff800aab1000 },
    { "5.5.0-202304070526",	0xffffff800aab1000 },
    { "5.5.0-202304070533",	0xffffff800aab1000 },

    { "5.6.0-202304240012",	0xffffff800aab7000 },
    { "5.6.0-202304240057",	0xffffff800aab7000 },
    { "5.6.0-202304240058",	0xffffff800aab7000 },
    { "5.6.0-202304241441",	0xffffff800aab7000 },
    { "5.6.0-202305101528",	0xffffff800aab7000 },
    { "5.6.0-202305101545",	0xffffff800aab7000 },
    { "5.6.0-202305101706",	0xffffff800aab7000 },
    { "5.6.0-202305101713",	0xffffff800aab7000 },
    { "5.6.0-202305190040",	0xffffff800aab7000 },
    { "5.6.0-202305190206",	0xffffff800aab7000 },
    { "5.6.0-202305190440",	0xffffff800aab7000 },
    { "5.6.0-202305190627",	0xffffff800aab7000 },
    { "5.6.1-202305240156",	0xffffff800aab7000 },
    { "5.6.1-202305240403",	0xffffff800aab7000 },
    
    { "5.7.0-202306271628",	0xffffff800aabb000 },
    { "5.7.0-202306271629",	0xffffff800aabb000 },
    { "5.7.0-202306271630",	0xffffff800aabb000 },
    { "5.7.0-202307110339",	0xffffff800aabb000 },
    { "5.7.0-202307111047",	0xffffff800aabb000 },
    { "5.7.0-202307190236",	0xffffff800aabb000 },
    { "5.7.0-202307190415",	0xffffff800aabb000 },
    { "5.7.1-202308041817",	0xffffff800aabb000 },
    { "5.7.1-202308041820",	0xffffff800aabb000 },
    { "5.7.1-202308041830",	0xffffff800aabb000 },

    { "5.7.2-202308222102",	0xffffff800aabb000 },
    { "5.7.2-202308222235",	0xffffff800aabb000 },
    { "5.7.2-202308222237",	0xffffff800aabb000 },

    { "5.8.0-202308311959",	0xffffff800aabb000 },
    { "5.8.0-202308312000",	0xffffff800aabb000 },
    { "5.8.0-202309122317",	0xffffff800aabb000 },
    { "5.8.0-202309122318",	0xffffff800aabb000 },
    { "5.8.0-202309150211",	0xffffff800aabb000 },
    { "5.8.0-202309150551",	0xffffff800aabb000 },
    { "5.8.0-202309201640",	0xffffff800aabb000 },
    { "5.8.0-202309201816",	0xffffff800aabb000 },
    { "5.8.0-202309201937",	0xffffff800aabb000 },
    { "5.8.2-202310121309",	0xffffff800aabb000 },
    { "5.8.2-202310121346",	0xffffff800aabb000 },
    { "5.8.2-202310121534",	0xffffff800aabb000 },
    { "5.8.2-202310121535",	0xffffff800aabb000 },
    { "5.9.0-202312191606",	0xffffff800aabb000 },
    { "5.9.0-202312191609",	0xffffff800aabb000 },
    { "5.9.0-202312300021",	0xffffff800aabb000 },
    { "5.9.0-202312300500",	0xffffff800aabb000 },
    { "5.9.0-202401102315",	0xffffff800aabb000 },
    { "5.9.0-202401102317",	0xffffff800aabb000 },
    { "5.9.0-202401110244",	0xffffff800aabb000 },
    { "5.9.0-202401110404",	0xffffff800aabb000 },
    { "5.9.1-202401171309",	0xffffff800aabb000 },
    { "5.9.1-202401171310",	0xffffff800aabb000 },
    { "5.9.1-202401171316",	0xffffff800aabb000 },
    { "5.9.1-202401171556",	0xffffff800aabb000 },
    { "5.9.2-202403020013",	0xffffff800aabb000 },
    { "5.9.2-202403020025",	0xffffff800aabb000 },
    { "5.9.2-202403020318",	0xffffff800aabb000 },
    { "5.9.2-202403020343",	0xffffff800aabb000 },
    { "5.9.8-202406140037", 0xffffff800aabb000 },
    { "5.9.8-202406140215",	0xffffff800aabb000 },
    { "5.9.9-202408300028",	0xffffff800aabb000 },
    { "5.9.9-202408300231", 0xffffff800aabb000 },
    { "5.11.0-202407301711",0xffffff800aabb000 },
    { "5.11.0-202407301738",0xffffff800aabb000 },
    { "5.11.0-202408201621",0xffffff800aabb000 },
    { "5.11.0-202408201622",0xffffff800aabb000 },
    { NULL, 0 }
};

const int NUM_KOFFSETS=(sizeof(k_table) / sizeof (struct kernel_offsets));

#define kptr_restrict               0xffffff800a018ea0
#define sysctl_perf_event_paranoid  0xffffff800a00c1d4



