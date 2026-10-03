
#include "common.h"

static const char *MIUI_SERVICE_LIST[] = {
  "com.miui.systemAdSolution",
  "com.miui.analytics",
  "com.xiaomi.joyose/.smartop.gamebooster.receiver.BoostRequestReceiver",
  "com.xiaomi.joyose/.smartop.SmartOpService",
  "com.xiaomi.joyose.sysbase.MetokClService",
  "com.miui.daemon/.performance.cloudcontrol.CloudControlSyncService",
  "com.miui.daemon/.performance.statistics.services.GraphicDumpService",
  "com.miui.daemon/.performance.statistics.services.AtraceDumpService",
  "com.miui.daemon/.performance.SysoptService",
  "com.miui.daemon/.performance.MiuiPerfService",
  "com.miui.daemon/.performance.server.ExecutorService",
  "com.miui.daemon/.mqsas.jobs.EventUploadService",
  "com.miui.daemon/.mqsas.jobs.FileUploadService",
  "com.miui.daemon/.mqsas.jobs.HeartBeatUploadService",
  "com.miui.daemon/.mqsas.providers.MQSProvider",
  "com.miui.daemon/.performance.provider.PerfTurboProvider",
  "com.miui.daemon/.performance.system.am.SysoptjobService",
  "com.miui.daemon/.performance.system.am.MemCompactService",
  "com.miui.daemon/.performance.statistics.services.FreeFragDumpService",
  "com.miui.daemon/.performance.statistics.services.DefragService",
  "com.miui.daemon/.performance.statistics.services.MeminfoService",
  "com.miui.daemon/.performance.statistics.services.IonService",
  "com.miui.daemon/.performance.statistics.services.GcBoosterService",
  "com.miui.daemon/.mqsas.OmniTestReceiver",
  NULL
};

static const char *MISC_KILL_LIST[] = {
  "statsd", "traced", "cnss_diag", "tcpdump", "ipacm-diag", "ramdump",
  "subsystem_ramdump", "charge_logger", "com.miui.daemon", "miuibooster", NULL
};

static void pm_toggle_all(const char *const *list, int disable) {
  int failed = 0;
  for (int i = 0; list[i]; i++) {
    char *argv[] = { (char *)"/system/bin/pm", disable ? (char *)"disable" : (char *)"enable", (char *)list[i], NULL };
    if (mt_run(argv, 1) != 0) {
      if (disable) mt_log(3, "miui_services: failed to disable %s (not present on this ROM?)", list[i]);
      failed++;
    }
  }
  if (disable) mt_log(1, "miui_services: disabled (%d failures)", failed);
}

static void misc_kill(void) {
  for (int i = 0; MISC_KILL_LIST[i]; i++) {
    char *k1[] = { (char *)"/system/bin/killall", (char *)"-q", (char *)"-9", (char *)MISC_KILL_LIST[i], NULL };
    mt_run(k1, 1);
    char *k2[] = { (char *)"/system/bin/stop", (char *)MISC_KILL_LIST[i], NULL };
    mt_run(k2, 1);
  }
  mt_log(1, "misc_kill_services: done");
}

static void misc_kill_revert(void) {
  for (int i = 0; MISC_KILL_LIST[i]; i++) {
    char *argv[] = { (char *)"/system/bin/start", (char *)MISC_KILL_LIST[i], NULL };
    mt_run(argv, 1);
  }
  mt_log(1, "misc_kill_services: reverted (best effort - some restart via init automatically)");
}


static const char *LOG_PROPS[][2] = {
  {"vidc.debug.level","0"},{"vendor.vidc.debug.level","0"},{"vendor.swvdec.log.level","0"},
  {"persist.vendor.dpm.loglevel","0"},{"persist.vendor.dpmhalservice.loglevel","0"},
  {"persist.debug.sf.statistics","0"},{"debug.sf.enable_egl_image_tracker","0"},
  {"debug.mdpcomp.logs","0"},{"persist.ims.disableDebugLogs","1"},{"persist.ims.disableADBLogs","1"},
  {"persist.ims.disableQXDMLogs","1"},{"persist.ims.disableIMSLogs","1"},
  {"persist.sys.lmk.reportkills","false"},{"config.disable_rtt","true"},
  {"db.log.slow_query_threshold","0"},{"debug.egl.profiler","0"},{"debug.enable.gamed","0"},
  {"debug.enable.wl_log","0"},{"debug.hwc.otf","0"},{"debug.hwc_dump_en","0"},{"debug.sf.dump","0"},
  {"debug.sf.ddms","0"},{"debug.sf.recomputecrop","0"},{"debug.sf.showupdates","0"},
  {"debug.sf.showcpu","0"},{"debug.sf.showbackground","0"},{"debug.sf.showfps","0"},
  {"debug.atrace.tags.enableflags","0"},{"debugtool.anrhistory","0"},{"logcat.live","disable"},
  {"net.ipv4.tcp_no_metrics_save","1"},{"media.stagefright.log-uri","0"},
  {"persist.android.strictmode","0"},{"ro.config.nocheckin","1"},{"rw.logger","0"},
  {"persist.debug.wfd.enable","0"},{"persist.data.qmi.adb_logmask","0"},
  {"persist.debug.sensors.hal","0"},{"persist.oem.dump","0"},{"log.redirect-stdio","false"},
  {"libc.debug.malloc","0"},{"persist.vendor.ssr.enable_ramdumps","0"}, {NULL,NULL}
};

static const char *DALVIK_PROPS[][2] = {
  {"dalvik.vm.minidebuginfo","false"},{"dalvik.vm.dex2oat-minidebuginfo","false"},
  {"dalvik.vm.check-dex-sum","false"},{"dalvik.vm.checkjni","false"},
  {"dalvik.vm.verify-bytecode","false"},{"dalvik.gc.type","generational_cc"},
  {"dalvik.vm.dex2oat-swap","true"},{"dalvik.vm.dex2oat-resolve-startup-strings","true"},
  {"dalvik.vm.systemservercompilerfilter","speed-profile"},
  {"dalvik.vm.systemuicompilerfilter","speed-profile"},{"dalvik.vm.usap_pool_enabled","true"},
  {"persist.sys.usap_pool_enabled","true"},
  {"persist.device_config.runtime_native.usap_pool_enabled","true"}, {NULL,NULL}
};

static void apply_prop_table(const char *track, const char *tag, const char *const table[][2]) {
  for (int i = 0; table[i][0]; i++) mt_set_prop(track, tag, table[i][0], table[i][1]);
}

static void cmd2(const char *a, const char *b) {
  char *argv[] = { (char *)"/system/bin/cmd", (char *)a, (char *)b, NULL };
  mt_run(argv, 1);
}
static void cmd3(const char *a, const char *b, const char *c) {
  char *argv[] = { (char *)"/system/bin/cmd", (char *)a, (char *)b, (char *)c, NULL };
  mt_run(argv, 1);
}
static void cmd4(const char *a, const char *b, const char *c, const char *d) {
  char *argv[] = { (char *)"/system/bin/cmd", (char *)a, (char *)b, (char *)c, (char *)d, NULL };
  mt_run(argv, 1);
}
static void cmd5(const char *a, const char *b, const char *c, const char *d, const char *e) {
  char *argv[] = { (char *)"/system/bin/cmd", (char *)a, (char *)b, (char *)c, (char *)d, (char *)e, NULL };
  mt_run(argv, 1);
}

static void packages_dexopt(void) {
  char *a1[] = { (char *)"/system/bin/pm", (char *)"compile", (char *)"-m", (char *)"speed-profile", (char *)"-a", NULL }; mt_run(a1, 1);
  char *a2[] = { (char *)"/system/bin/pm", (char *)"compile", (char *)"-m", (char *)"speed-profile", (char *)"--secondary-dex", (char *)"-a", NULL }; mt_run(a2, 1);
  char *a3[] = { (char *)"/system/bin/pm", (char *)"compile", (char *)"--compile-layouts", (char *)"-a", NULL }; mt_run(a3, 1);
  char *a4[] = { (char *)"/system/bin/pm", (char *)"compile", (char *)"-m", (char *)"speed-profile", (char *)"--full", (char *)"-a", NULL }; mt_run(a4, 1);
  char *a5[] = { (char *)"/system/bin/pm", (char *)"art", (char *)"dexopt-packages", (char *)"-r", (char *)"bg-dexopt", NULL }; mt_run(a5, 1);
  char *a6[] = { (char *)"/system/bin/pm", (char *)"art", (char *)"cleanup", NULL }; mt_run(a6, 1);
  mt_log(1, "packages_dexopt: done");
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: miui <conf> <track> early|late|set KEY VALUE\n"); return 2; }
  const char *conf = argv[1], *track = argv[2], *mode = argv[3];

  if (strcmp(mode, "early") == 0) {
    if (mt_is_on(conf, "SYS_LOG_PROPS")) { apply_prop_table(track, "SYS_LOG_PROPS", LOG_PROPS); mt_log(1, "sys_log_props: applied"); }
    if (mt_is_on(conf, "SYS_DALVIK_PROPS")) { apply_prop_table(track, "SYS_DALVIK_PROPS", DALVIK_PROPS); mt_log(1, "sys_dalvik_props: applied"); }
    return 0;
  }

  if (strcmp(mode, "late") == 0) {
    if (mt_is_on(conf, "MIUI_SERVICES")) pm_toggle_all(MIUI_SERVICE_LIST, 1);
    if (mt_is_on(conf, "MISC_KILL_SERVICES")) misc_kill();
    if (mt_is_on(conf, "CPU_CORE_HARDCODE")) {
      mt_set_prop(track, "CPU_CORE_HARDCODE", "persist.sys.miui.sf_cores", "6");
      mt_set_prop(track, "CPU_CORE_HARDCODE", "persist.sys.miui_animator_sched.bigcores", "6-7");
      mt_log(1, "cpu_core_hardcode: applied (risk: wrong on non-8-core chips)");
    }
    if (mt_is_on(conf, "FIXED_PERF_MODE")) { cmd3("power", "set-fixed-performance-mode-enabled", "true"); mt_log(1, "fixed_perf_mode: applied (risk: battery/heat)"); }
    if (mt_is_on(conf, "THERMAL_OVERRIDE")) { cmd3("thermalservice", "override-status", "0"); mt_log(1, "thermal_override: applied (risk: disables overheat throttling)"); }
    if (mt_is_on(conf, "PACKAGES_DEXOPT")) packages_dexopt();
    if (mt_is_on(conf, "CMD_MISC")) {
      cmd5("settings", "put", "system", "anr_debugging_mechanism", "0");
      cmd2("looper_stats", "disable");
      cmd5("settings", "put", "global", "netstats_enabled", "0");
      { char *a[] = {(char*)"/system/bin/cmd",(char*)"device_config",(char*)"put",(char*)"runtime_native_boot",(char*)"disable_lock_profiling",(char*)"true",NULL}; mt_run(a,1); }
      { char *a[] = {(char*)"/system/bin/cmd",(char*)"device_config",(char*)"put",(char*)"runtime_native_boot",(char*)"iorap_readahead_enable",(char*)"true",NULL}; mt_run(a,1); }
      { char *a[] = {(char*)"/system/bin/cmd",(char*)"settings",(char*)"put",(char*)"global",(char*)"fstrim_mandatory_interval",(char*)"3600",NULL}; mt_run(a,1); }
      cmd2("activity", "idle-maintenance");
      { char *a[] = {(char*)"/system/bin/cmd",(char*)"dropbox",(char*)"set-rate-limit",(char*)"10000",NULL}; mt_run(a,1); }
      mt_log(1, "cmd_misc: applied");
    }
    return 0;
  }

  if (strcmp(mode, "set") == 0) {
    if (argc < 6) { fprintf(stderr, "usage: miui <conf> <track> set KEY VALUE\n"); return 2; }
    const char *key = argv[4], *val = argv[5];
    int on = strcmp(val, "1") == 0;
    if (strcmp(key, "MIUI_SERVICES") == 0) { pm_toggle_all(MIUI_SERVICE_LIST, on); if (!on) mt_log(1, "miui_services: re-enabled"); }
    else if (strcmp(key, "MISC_KILL_SERVICES") == 0) { if (on) misc_kill(); else misc_kill_revert(); }
    else if (strcmp(key, "SYS_LOG_PROPS") == 0) { if (on) apply_prop_table(track, "SYS_LOG_PROPS", LOG_PROPS); else mt_revert_props_tag(track, "SYS_LOG_PROPS"); }
    else if (strcmp(key, "SYS_DALVIK_PROPS") == 0) { if (on) apply_prop_table(track, "SYS_DALVIK_PROPS", DALVIK_PROPS); else mt_revert_props_tag(track, "SYS_DALVIK_PROPS"); }
    else if (strcmp(key, "CPU_CORE_HARDCODE") == 0) {
      if (on) { mt_set_prop(track, "CPU_CORE_HARDCODE", "persist.sys.miui.sf_cores", "6"); mt_set_prop(track, "CPU_CORE_HARDCODE", "persist.sys.miui_animator_sched.bigcores", "6-7"); }
      else mt_revert_props_tag(track, "CPU_CORE_HARDCODE");
    }
    else if (strcmp(key, "FIXED_PERF_MODE") == 0) cmd3("power", "set-fixed-performance-mode-enabled", on ? "true" : "false");
    else if (strcmp(key, "THERMAL_OVERRIDE") == 0) { if (on) cmd3("thermalservice", "override-status", "0"); else cmd2("thermalservice", "reset"); }
    else if (strcmp(key, "PACKAGES_DEXOPT") == 0) { if (on) packages_dexopt();  }
    else if (strcmp(key, "CMD_MISC") == 0) {
      if (on) {
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"settings",(char*)"put",(char*)"system",(char*)"anr_debugging_mechanism",(char*)"0",NULL}; mt_run(a,1); }
        cmd2("looper_stats", "disable");
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"settings",(char*)"put",(char*)"global",(char*)"netstats_enabled",(char*)"0",NULL}; mt_run(a,1); }
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"device_config",(char*)"put",(char*)"runtime_native_boot",(char*)"disable_lock_profiling",(char*)"true",NULL}; mt_run(a,1); }
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"device_config",(char*)"put",(char*)"runtime_native_boot",(char*)"iorap_readahead_enable",(char*)"true",NULL}; mt_run(a,1); }
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"settings",(char*)"put",(char*)"global",(char*)"fstrim_mandatory_interval",(char*)"3600",NULL}; mt_run(a,1); }
        cmd2("activity", "idle-maintenance");
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"dropbox",(char*)"set-rate-limit",(char*)"10000",NULL}; mt_run(a,1); }
        mt_log(1, "cmd_misc: applied");
      } else {
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"settings",(char*)"put",(char*)"system",(char*)"anr_debugging_mechanism",(char*)"1",NULL}; mt_run(a,1); }
        cmd2("looper_stats", "enable");
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"settings",(char*)"put",(char*)"global",(char*)"netstats_enabled",(char*)"1",NULL}; mt_run(a,1); }
        { char *a[] = {(char*)"/system/bin/cmd",(char*)"device_config",(char*)"put",(char*)"runtime_native_boot",(char*)"disable_lock_profiling",(char*)"false",NULL}; mt_run(a,1); }
        mt_log(1, "cmd_misc: reverted (best effort)");
      }
    }
    else { mt_log(2, "miui: unknown key %s", key); return 1; }
    return 0;
  }

  fprintf(stderr, "miui: unknown mode %s\n", mode);
  return 2;
}
