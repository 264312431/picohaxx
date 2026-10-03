//===============================================================
// crazy idea #99: embedd easily editable config section into binary
//===============================================================
#include "cfg.h"

#define CFG_GLOB(name, def, label) int g_##name = (def);
#define CFG(name, def)             int g_##name = (def);
#include "cfg.inc.h"
#undef CFG_GLOB
#undef CFG

// append cfg block to .interp
__attribute__((section(".interp"), used))
const char inter[] = "/system/bin/linker64";

__attribute__((section(".interp"), used, aligned(16)))
const char cfg_block[] =
"#==============#"
"# bincfg [TM]  #"
"#==============#"
#define CFG_GLOB(name, def, label) "#[" _CFG_STR(def) "] " label "#"
#define CFG(name, def)
#include "cfg.inc.h"
"#==============#"
"# to edit use  #"
"#[x] any char  #"
"#[ ] or space  #"
"#==============#";
#undef CFG_GLOB
#undef CFG