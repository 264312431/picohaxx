/*
 * default config flags, creates Globals. Also exposed through "bincfg"
 *
 * CFG_GLOB(name, default, label)
 *   name    : C identifier → g_##name, CFG_##name, cfg_t field
 *   default : 0 / 1 — Initial vakue for of  Globals AND default in the ELF-Block
 *   label   : exakt 10 chars
 */

CFG_GLOB(dbg     ,0,"dbg       ")
CFG_GLOB(dbg2    ,0,"dbg       ")
CFG_GLOB(adbd    ,1,"adbd      ")
CFG_GLOB(ftpd    ,1,"ftpd      ")
CFG_GLOB(force   ,0,"force     ")
CFG_GLOB(unroot  ,0,"unroot    ")
CFG_GLOB(cfg     ,0,"showcfg   ")
CFG_GLOB(sound   ,0,"sound     ")
CFG_GLOB(simulate,0,"simulate  ")
CFG_GLOB(dbgchild,0,"dbgchild  ")
CFG_GLOB(marathon,0,"marathon  ")



