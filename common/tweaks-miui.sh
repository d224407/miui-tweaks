#!/system/bin/sh
##############################################################################
# MIUI: services, system properties, CPU/scheduling, misc
# Matches the "MIUI - ..." sections of config/tweaks.conf.
##############################################################################

############################################################################
# MIUI: services
############################################################################

MIUI_SERVICE_LIST="com.miui.systemAdSolution
com.miui.analytics
com.xiaomi.joyose/.smartop.gamebooster.receiver.BoostRequestReceiver
com.xiaomi.joyose/.smartop.SmartOpService
com.xiaomi.joyose.sysbase.MetokClService
com.miui.daemon/.performance.cloudcontrol.CloudControlSyncService
com.miui.daemon/.performance.statistics.services.GraphicDumpService
com.miui.daemon/.performance.statistics.services.AtraceDumpService
com.miui.daemon/.performance.SysoptService
com.miui.daemon/.performance.MiuiPerfService
com.miui.daemon/.performance.server.ExecutorService
com.miui.daemon/.mqsas.jobs.EventUploadService
com.miui.daemon/.mqsas.jobs.FileUploadService
com.miui.daemon/.mqsas.jobs.HeartBeatUploadService
com.miui.daemon/.mqsas.providers.MQSProvider
com.miui.daemon/.performance.provider.PerfTurboProvider
com.miui.daemon/.performance.system.am.SysoptjobService
com.miui.daemon/.performance.system.am.MemCompactService
com.miui.daemon/.performance.statistics.services.FreeFragDumpService
com.miui.daemon/.performance.statistics.services.DefragService
com.miui.daemon/.performance.statistics.services.MeminfoService
com.miui.daemon/.performance.statistics.services.IonService
com.miui.daemon/.performance.statistics.services.GcBoosterService
com.miui.daemon/.mqsas.OmniTestReceiver"

tweak_miui_services() {
  is_on "$MIUI_SERVICES" || return 0
  for s in $MIUI_SERVICE_LIST; do pm disable "$s" >/dev/null 2>&1; done
  log 1 "miui_services: disabled"
}

restore_miui_services() {
  for s in $MIUI_SERVICE_LIST; do pm enable "$s" >/dev/null 2>&1; done
}

MISC_KILL_LIST="statsd traced cnss_diag tcpdump ipacm-diag ramdump subsystem_ramdump charge_logger com.miui.daemon miuibooster"

tweak_misc_kill_services() {
  is_on "$MISC_KILL_SERVICES" || return 0
  for s in $MISC_KILL_LIST; do terminate_service "$s"; done
  log 1 "misc_kill_services: done"
}

revert_misc_kill_services() {
  # Best effort - most of these restart on their own via init/watchdog even
  # without this; "start" nudges the ones that do not.
  for s in $MISC_KILL_LIST; do start "$s" 2>/dev/null; done
  log 1 "misc_kill_services: reverted (best effort - some restart via init automatically)"
}
############################################################################
# System properties (deduped: shared between MIUI+GMS originals)
############################################################################

tweak_sys_log_props() {
  is_on "$SYS_LOG_PROPS" || return 0
  local props="vidc.debug.level 0
vendor.vidc.debug.level 0
vendor.swvdec.log.level 0
persist.vendor.dpm.loglevel 0
persist.vendor.dpmhalservice.loglevel 0
persist.debug.sf.statistics 0
debug.sf.enable_egl_image_tracker 0
debug.mdpcomp.logs 0
persist.ims.disableDebugLogs 1
persist.ims.disableADBLogs 1
persist.ims.disableQXDMLogs 1
persist.ims.disableIMSLogs 1
persist.sys.lmk.reportkills false
config.disable_rtt true
db.log.slow_query_threshold 0
debug.egl.profiler 0
debug.enable.gamed 0
debug.enable.wl_log 0
debug.hwc.otf 0
debug.hwc_dump_en 0
debug.sf.dump 0
debug.sf.ddms 0
debug.sf.recomputecrop 0
debug.sf.showupdates 0
debug.sf.showcpu 0
debug.sf.showbackground 0
debug.sf.showfps 0
debug.atrace.tags.enableflags 0
debugtool.anrhistory 0
logcat.live disable
net.ipv4.tcp_no_metrics_save 1
media.stagefright.log-uri 0
persist.android.strictmode 0
ro.config.nocheckin 1
rw.logger 0
persist.debug.wfd.enable 0
persist.data.qmi.adb_logmask 0
persist.debug.sensors.hal 0
persist.oem.dump 0
log.redirect-stdio false
libc.debug.malloc 0
persist.vendor.ssr.enable_ramdumps 0"

  apply_prop_block "SYS_LOG_PROPS" "$props"
  log 1 "sys_log_props: applied"
}

tweak_sys_dalvik_props() {
  is_on "$SYS_DALVIK_PROPS" || return 0
  local props="dalvik.vm.minidebuginfo false
dalvik.vm.dex2oat-minidebuginfo false
dalvik.vm.check-dex-sum false
dalvik.vm.checkjni false
dalvik.vm.verify-bytecode false
dalvik.gc.type generational_cc
dalvik.vm.dex2oat-swap true
dalvik.vm.dex2oat-resolve-startup-strings true
dalvik.vm.systemservercompilerfilter speed-profile
dalvik.vm.systemuicompilerfilter speed-profile
dalvik.vm.usap_pool_enabled true
persist.sys.usap_pool_enabled true
persist.device_config.runtime_native.usap_pool_enabled true"
  # JIT is deliberately left on.

  apply_prop_block "SYS_DALVIK_PROPS" "$props"
  log 1 "sys_dalvik_props: applied"
}

############################################################################
# MIUI: CPU / scheduling (higher risk - off by default)
############################################################################

tweak_cpu_pin() {
  is_on "$CPU_PIN" || return 0
  rebuild_process_scan_cache
  for proc in zygote usap surfaceflinger system_server composer; do
    pin_proc_on_perf "$proc"
    change_task_cgroup "$proc" "foreground" "cpuset"
    change_task_nice "$proc" "-20"
  done
  local top="$(pm resolve-activity -a android.intent.action.MAIN -c android.intent.category.HOME | grep packageName | head -n1 | cut -d= -f2) com.android.systemui"
  for proc in $top; do
    pin_proc_on_perf "$proc"
    change_task_cgroup "$proc" "top-app" "cpuset"
    change_task_nice "$proc" "-20"
  done
  for proc in logd statsd tombstoned incidentd; do
    change_task_cgroup "$proc" "background" "cpuset"
    change_task_nice "$proc" "5"
  done
  log 1 "cpu_pin: applied (risk: battery/heat)"
}

revert_cpu_pin() {
  # Cgroup placement is not restored (Android's own ActivityManager
  # reassigns it on the next app-switch/lifecycle event); nice value is
  # restored immediately since nothing else resets that on its own.
  rebuild_process_scan_cache
  for proc in zygote usap surfaceflinger system_server composer logd statsd tombstoned incidentd; do
    change_task_nice "$proc" "0"
  done
  local top="$(pm resolve-activity -a android.intent.action.MAIN -c android.intent.category.HOME | grep packageName | head -n1 | cut -d= -f2) com.android.systemui"
  for proc in $top; do change_task_nice "$proc" "0"; done
  log 1 "cpu_pin: reverted (nice reset to 0, cgroup normalizes on next app switch)"
}

tweak_cpu_core_hardcode() {
  is_on "$CPU_CORE_HARDCODE" || return 0
  # Assumes an 8-core chip with cores 6-7 as the "big" cluster.
  # Wrong on other layouts - verify against your own CPU before enabling.
  set_prop CPU_CORE_HARDCODE persist.sys.miui.sf_cores 6
  set_prop CPU_CORE_HARDCODE persist.sys.miui_animator_sched.bigcores 6-7
  log 1 "cpu_core_hardcode: applied (risk: wrong on non-8-core chips)"
}

revert_cpu_core_hardcode() {
  revert_props_tag CPU_CORE_HARDCODE
  log 1 "cpu_core_hardcode: reverted"
}

tweak_fixed_perf_mode() {
  is_on "$FIXED_PERF_MODE" || return 0
  cmd power set-fixed-performance-mode-enabled true
  log 1 "fixed_perf_mode: applied (risk: battery/heat)"
}

revert_fixed_perf_mode() {
  cmd power set-fixed-performance-mode-enabled false
  log 1 "fixed_perf_mode: reverted"
}

tweak_thermal_override() {
  is_on "$THERMAL_OVERRIDE" || return 0
  cmd thermalservice override-status 0
  log 1 "thermal_override: applied (risk: disables overheat throttling)"
}

revert_thermal_override() {
  cmd thermalservice reset
  log 1 "thermal_override: reverted"
}
############################################################################
# MIUI: misc
############################################################################

tweak_packages_dexopt() {
  is_on "$PACKAGES_DEXOPT" || return 0
  pm compile -m speed-profile -a >/dev/null 2>&1
  pm compile -m speed-profile --secondary-dex -a >/dev/null 2>&1
  pm compile --compile-layouts -a >/dev/null 2>&1
  pm compile -m speed-profile --full -a >/dev/null 2>&1
  pm art dexopt-packages -r bg-dexopt >/dev/null 2>&1
  pm art cleanup >/dev/null 2>&1
  log 1 "packages_dexopt: done"
}

tweak_cmd_misc() {
  is_on "$CMD_MISC" || return 0
  cmd settings put system anr_debugging_mechanism 0
  cmd looper_stats disable
  cmd settings put global netstats_enabled 0
  cmd device_config put runtime_native_boot disable_lock_profiling true
  cmd device_config put runtime_native_boot iorap_readahead_enable true
  cmd settings put global fstrim_mandatory_interval 3600
  cmd activity idle-maintenance
  cmd dropbox set-rate-limit 10000
  log 1 "cmd_misc: applied"
}

revert_cmd_misc() {
  # Only the settings with an obvious, safe default are reverted;
  # fstrim/idle-maintenance/dropbox rate are one-shot actions with nothing
  # meaningful to undo.
  cmd settings put system anr_debugging_mechanism 1
  cmd looper_stats enable
  cmd settings put global netstats_enabled 1
  cmd device_config put runtime_native_boot disable_lock_profiling false
  log 1 "cmd_misc: reverted (best effort)"
}
