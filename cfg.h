#pragma once
#include "term.h"

#define CFG_STRIDE    16
#define CFG_HDR_LINES  3

// used to declare the embedded block
#define _CFG_STR_0 " "
#define _CFG_STR_1 "x"
#define _CFG_STR(x) _CFG_STR_##x

/* ------------------------------------------------------------------*/
/*  creates enum that maps each cfg item to its line index           */
/* ------------------------------------------------------------------*/
typedef enum {
_CFG_HDR = CFG_HDR_LINES-1,
        #define CFG_GLOB(name, def, label) CFG_##name,
        #define CFG(name, def)
        #include "cfg.inc.h"
        #undef CFG_GLOB
        #undef CFG
_CFG_OPT_COUNT
} cfg_opt_t;

/* ------------------------------------------------------------------*/
/*  for each cfg item, create global int g_xxx                       */
/* ------------------------------------------------------------------*/
#define CFG_GLOB(name, def, label) extern int g_##name;
#define CFG(name, def)             extern int g_##name;
#include "cfg.inc.h"
#undef CFG_GLOB
#undef CFG

/* ------------------------------------------------------------------*/
/*  ELF block                                                        */
/* ------------------------------------------------------------------*/
extern const char cfg_block[];

/* ------------------------------------------------------------------*/
/*  cfg_apply:                                                       */
/*  creates g_## globals apply the cfg Block from the binary.        */
/* ------------------------------------------------------------------*/
static inline void cfg_apply(void) {
        #define CFG_GLOB(name, def, label) g_##name = (cfg_block[CFG_##name * CFG_STRIDE + 2] != ' ');
        #define CFG(name, def)
        #include "cfg.inc.h"
        #undef CFG_GLOB
 #undef CFG
}

static inline void cfg_print_short(void) {
        printf("\n#======================#\n runtime config:\n#======================#\n");
        #define CFG(name, def)
        #define CFG_GLOB(name, def, label) printf(" %-10s  %d\n", label ":", g_##name);
        #include "cfg.inc.h"
        #undef CFG_GLOB
        #undef CFG
}

static inline void cfg_print(void) {
        int b,bright; 
        printf("\n#====================================#\n runtime config:\n#====================================#\n");
        #define CFG_GLOB(name, def, label) \
        b=(cfg_block[CFG_##name * CFG_STRIDE + 2] != ' '); \
        bright=(b==g_##name)?0:60; \
        printf(" %s%-9s   [%d]  %s(default: %d%s)" col(0) "\n", \
        g_##name?col(32):col(31), #name ":", g_##name, b?col(92):col(91),b,\
        (b != def) ? ", via binary-cfg" : "");
        #include "cfg.inc.h"
        #undef CFG_GLOB
        #undef CFG
        #define STRINGISE_IMPL(x) #x
        #define STRINGISE(x) STRINGISE_IMPL(x)
}
        