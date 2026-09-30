#!/system/bin/sh
##############################################################################
# MIUI Tweaks - shared functions
#
# Every tweak function is gated by a variable in config/tweaks.conf, so
# nothing runs unless explicitly enabled (via the WebUI or by hand).
##############################################################################

MODDIR="${0%/*}"
MODDIR="${MODDIR%/common}"
CONF="$MODDIR/config/tweaks.conf"
LOGFILE="/storage/emulated/0/Android/miui_tweaks.log"
GMSLIST="$MODDIR/gmslist.txt"

load_conf() {
  [ -f "$CONF" ] && . "$CONF"
}

log() {
  # $1=level(1 info/2 warn/3 error) $2=message
  local t=""
  case "$1" in
    1) t="INFO" ;; 2) t="WARN" ;; 3) t="ERROR" ;; *) t="?" ;;
  esac
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$t] $2" >> "$LOGFILE" 2>/dev/null
  return 0
}

is_on() {
  # $1 = value of a config var, "1" = on
  [ "$1" = "1" ]
}

PROP_TRACK="$MODDIR/config/.applied_props"

set_prop() {
  # $1=tag(config key) $2=name $3=value - sets a non-persist prop and
  # records "tag name" so it can be deleted immediately when that one
  # tweak is turned off, or on uninstall.
  resetprop -n "$2" "$3"
  echo "$1 $2" >> "$PROP_TRACK"
}

apply_prop_block() {
  # $1=tag(config key) $2 = multi-line "key value" list
  echo "$2" | while IFS= read -r line; do
    [ -n "$line" ] || continue
    resetprop $line
    echo "$1 ${line%% *}" >> "$PROP_TRACK"
  done
}

revert_props_tag() {
  # $1 = tag(config key) - deletes every prop tracked under that tag and
  # drops those lines from PROP_TRACK.
  [ -f "$PROP_TRACK" ] || return 0
  grep "^$1 " "$PROP_TRACK" | while IFS=' ' read -r _ name; do
    [ -n "$name" ] && { resetprop --delete "$name" 2>/dev/null || resetprop -d "$name" 2>/dev/null; }
  done
  grep -v "^$1 " "$PROP_TRACK" > "$PROP_TRACK.tmp" 2>/dev/null
  mv -f "$PROP_TRACK.tmp" "$PROP_TRACK" 2>/dev/null
}

setup_resetprop() {
  if ! command -v resetprop > /dev/null 2>&1; then
    if [ -f /data/adb/ksu/bin/resetprop ]; then
      alias resetprop=/data/adb/ksu/bin/resetprop
    elif [ -f /data/adb/ap/bin/resetprop ]; then
      alias resetprop=/data/adb/ap/bin/resetprop
    else
      alias resetprop=setprop
    fi
    export resetprop
  fi
}

terminate_service() {
  killall -q -9 "$1" 2>/dev/null
  stop "$1" 2>/dev/null
}

############################################################################
# CPU / cgroup helpers
############################################################################

ps_ret=""
rebuild_process_scan_cache() { ps_ret="$(ps -Ao pid,args)"; }

get_cpu_count() { grep -c ^processor /proc/cpuinfo; }

get_full_cpu_mask() {
  local n=$(get_cpu_count) mask=1 i=1
  while [ $i -lt $n ]; do mask=$((mask | (mask << 1))); i=$((i + 1)); done
  printf '%x' $mask
}

change_task_cgroup() {
  # $1 name-regex $2 cgroup $3 cpuset|stune
  for pid in $(echo "$ps_ret" | grep -i -E "$1" | awk '{print $1}'); do
    for tid in $(ls "/proc/$pid/task/" 2>/dev/null); do
      echo "$tid" > "/dev/$3/$2/tasks" 2>/dev/null
    done
  done
}

change_task_nice() {
  for pid in $(echo "$ps_ret" | grep -i -E "$1" | awk '{print $1}'); do
    for tid in $(ls "/proc/$pid/task/" 2>/dev/null); do
      renice -n "$2" -p "$tid" >/dev/null 2>&1
    done
  done
}

change_task_affinity() {
  for pid in $(echo "$ps_ret" | grep -i -E "$1" | awk '{print $1}'); do
    for tid in $(ls "/proc/$pid/task/" 2>/dev/null); do
      taskset -p "$2" "$tid" >/dev/null 2>&1
    done
  done
}

pin_proc_on_perf() {
  change_task_cgroup "$1" "" "cpuset"
  change_task_affinity "$1" "$(get_full_cpu_mask)"
}

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

tweak_legacy_mode() {
  is_on "$LEGACY_MODE" || return 0
  # Deep sysprop set carried over from GhostGMS Legacy 1.3/2.0, deduped
  # against SYS_LOG_PROPS/SYS_DALVIK_PROPS above (only new keys here).
  # persist.ims.disabled and wifi.interface=wlan0 from the source module
  # are intentionally left out - the first disables VoLTE/VoWiFi calling,
  # the second is known to bootloop some ROMs (the source module itself
  # ships that line commented out).
  local props="av.debug.disable.pers.cache true
debug.composition.type gpu
debug.kill_allocating_task 0
debug.qualcomm.sns.daemon 0
debug.qualcomm.sns.hal 0
debug.qualcomm.sns.libsensor1 0
debug.sf.disable_backpressure 1
debug.sf.gpu_comp_tiling 0
debug_test 0
hwui.use_gpu_pixel_buffers false
live.logcat disable
log.cffdump 0
log.cffdump_no_memzero 0
log.cffdump_with_ifh 0
log.dumpx 0
log.pm4 0
log.pm4mem 0
log.primitives 0
log.resolves 0
log.sc_dev 0
log.shaders 0
log_ao 0
log_audiodecnode 0
log_audiooutput 0
log_basedecnode 0
log_datapath 0
log_fps_interval 0
log_frame_info 0
log_metadatadriver 0
log_mp4dectime 0
log_mp4parsernode 0
log_omxmp4 0
log_outputnode 0
log_outputnodeinputport 0
log_playerdriver 0
log_playerengine 0
log_posttime 0
log_profile 0
log_surfaceoutput 0
log_videodecnode 0
logcast.live disable
persist.brcm.ap_crash none
persist.brcm.cp_crash none
persist.brcm.log none
persist.bt.a2dp.aac_disable true
persist.ims.enableADBLogs 0
persist.ims.enableDebugLogs 0
persist.radio.oem_socket false
persist.service.lgospd.enable 0
persist.service.pcsync.enable 0
persist.sys.composition.type gpu
persist.sys.dun.override 0
persist.sys.offlinelog.kernel 1
persist.sys.offlinelog.logcat 1
persist.sys.wfd.virtual 0
pm.sleep_mode 1
profiler.debugmonitor false
profiler.forse_disable_err_rpt 1
profiler.forse_disable_ulog 1
profiler.hung.dumpdobugreport false
profiler.launch false
ro.compcache.default 0
ro.config.ksm.support false
ro.debuggable 0
ro.egl.destroy_after_detach false
ro.kernel.android.checkjni 0
ro.kernel.checkjni 0
ro.kernel.qemu.gles 0
ro.sf.battery.log.enabled 0
ro.sf.battery_log 0
ro.telephony.call_ring.multiple false
sdm.debug.disable_inline_rotator 1
sdm.debug.disable_skip_validate 1
sys.games.gt.prof 1
sys.hwc.gpu_perf_mode 0
vendor.fm.a2dp.conc.disabled true
vendor.vidc.enc.disable_bframes 1
video.disable.ubwc 1"
  apply_prop_block "LEGACY_MODE" "$props"
  log 1 "legacy_mode: applied (78 properties)"
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

tweak_lmk_props() {
  is_on "$LMK_PROPS" || return 0
  set_prop LMK_PROPS ro.lmk.debug false
  set_prop LMK_PROPS ro.lmk.log_stats false
  log 1 "lmk_props: applied"
}

tweak_tombstone_disable() {
  is_on "$TOMBSTONE_DISABLE" || return 0
  set_prop TOMBSTONE_DISABLE tombstoned.max_tombstone_count 0
  log 1 "tombstone_disable: applied"
}

tweak_blur_disable() {
  is_on "$BLUR_DISABLE" || return 0
  set_prop BLUR_DISABLE disableBlurs true
  set_prop BLUR_DISABLE enable_blurs_on_windows 0
  set_prop BLUR_DISABLE ro.launcher.blur.appLaunch 0
  set_prop BLUR_DISABLE ro.sf.blurs_are_expensive 0
  set_prop BLUR_DISABLE ro.surface_flinger.supports_background_blur 0
  log 1 "blur_disable: applied"
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

############################################################################
# GMS: service categories
############################################################################

gms_should_disable() {
  # $1 = category token from gmslist.txt
  case "$1" in
    ads) is_on "$DISABLE_ADS" && echo 1 ;;
    tracking) is_on "$DISABLE_TRACKING" && echo 1 ;;
    analytics) is_on "$DISABLE_ANALYTICS" && echo 1 ;;
    reporting) is_on "$DISABLE_REPORTING" && echo 1 ;;
    background) is_on "$DISABLE_BACKGROUND" && echo 1 ;;
    update) is_on "$DISABLE_UPDATE" && echo 1 ;;
    location) is_on "$DISABLE_LOCATION" && echo 1 ;;
    geofence) is_on "$DISABLE_GEOFENCE" && echo 1 ;;
    nearby) is_on "$DISABLE_NEARBY" && echo 1 ;;
    cast) is_on "$DISABLE_CAST" && echo 1 ;;
    discovery) is_on "$DISABLE_DISCOVERY" && echo 1 ;;
    sync) is_on "$DISABLE_SYNC" && echo 1 ;;
    cloud) is_on "$DISABLE_CLOUD" && echo 1 ;;
    auth) is_on "$DISABLE_AUTH" && echo 1 ;;
    wallet) is_on "$DISABLE_WALLET" && echo 1 ;;
    payment) is_on "$DISABLE_PAYMENT" && echo 1 ;;
    wear) is_on "$DISABLE_WEAR" && echo 1 ;;
    fitness) is_on "$DISABLE_FITNESS" && echo 1 ;;
    core|essential) echo 0 ;;
    *) echo 0 ;;
  esac
}

tweak_gms_services() {
  [ -f "$GMSLIST" ] || return 0
  while IFS='|' read -r svc cat || [ -n "$svc" ]; do
    case "$svc" in ''|'#'*) continue ;; esac
    [ -z "$cat" ] && continue
    if [ "$(gms_should_disable "$cat")" = "1" ]; then
      pm disable "$svc" >/dev/null 2>&1
    else
      pm enable "$svc" >/dev/null 2>&1
    fi
  done < "$GMSLIST"
  log 1 "gms_services: applied per category config"
}

gms_apply_category() {
  # $1 = category token, $2 = "1" disable / "0" enable - only touches the
  # services in that one category, for an instant per-switch toggle.
  [ -f "$GMSLIST" ] || return 0
  local action="pm enable"
  [ "$2" = "1" ] && action="pm disable"
  while IFS='|' read -r svc cat || [ -n "$svc" ]; do
    case "$svc" in ''|'#'*) continue ;; esac
    [ "$cat" = "$1" ] && $action "$svc" >/dev/null 2>&1
  done < "$GMSLIST"
  log 1 "gms category $1: $([ "$2" = "1" ] && echo disabled || echo enabled)"
}

restore_gms_services() {
  [ -f "$GMSLIST" ] || return 0
  while IFS='|' read -r svc cat || [ -n "$svc" ]; do
    case "$svc" in ''|'#'*) continue ;; esac
    [ -z "$svc" ] && continue
    pm enable "$svc" >/dev/null 2>&1
  done < "$GMSLIST"
}

############################################################################
# Wi-Fi: Qualcomm WCNSS config patch
# Reduces qcom_rx_wakelock wakeups / Wi-Fi battery drain (hostArpOffload,
# hostNsOffload being the main ones). Generates a patched WCNSS_qcom_cfg.ini
# as a module overlay at post-fs-data, so it takes effect after the next
# reboot. Turning the tweak off removes the overlay.
############################################################################

WIFI_CACHE="$MODDIR/config/.wifi_cfg_paths"

wifi_find_cfg() {
  if [ -s "$WIFI_CACHE" ]; then cat "$WIFI_CACHE"; return; fi
  find /system /vendor -name WCNSS_qcom_cfg.ini 2>/dev/null | while read -r f; do
    [ -f "$f" ] && [ ! -L "$f" ] && echo "$f"
  done | sort -u | tee "$WIFI_CACHE"
}

wifi_overlay_path() {
  case "$1" in
    /vendor/*) echo "$MODDIR/system$1" ;;
    *)         echo "$MODDIR$1" ;;
  esac
}

wifi_set_key() {
  # $1=file $2=key $3=value (replace if present, append if not)
  if grep -q "^[[:space:]]*$2=" "$1"; then
    sed -i "s|^[[:space:]]*$2=.*|$2=$3|" "$1"
  else
    [ -n "$(tail -c1 "$1")" ] && echo >> "$1"
    echo "$2=$3" >> "$1"
  fi
}

tweak_wifi_qcom_fix() {
  local cfg dst src found=0
  # Nothing to clean up if the tweak is off and was never applied
  is_on "$WIFI_QCOM_FIX" || [ -s "$WIFI_CACHE" ] || return 0
  for cfg in $(wifi_find_cfg); do
    dst="$(wifi_overlay_path "$cfg")"
    if ! is_on "$WIFI_QCOM_FIX"; then
      rm -f "$dst"
      continue
    fi
    found=1
    src="$cfg"
    [ -f "/sbin/.magisk/mirror$cfg" ] && src="/sbin/.magisk/mirror$cfg"
    mkdir -p "$(dirname "$dst")"
    cp -f "$src" "$dst" || continue
    wifi_set_key "$dst" hostArpOffload 0
    wifi_set_key "$dst" hostNsOffload 0
    wifi_set_key "$dst" gMCAddrListEnable 1
    wifi_set_key "$dst" gEnablePowerSaveOffload 5
    wifi_set_key "$dst" gRuntimePM 1
    wifi_set_key "$dst" RoamRssiDiff 3
    wifi_set_key "$dst" g11dSupportEnabled 0
    wifi_set_key "$dst" RTSThreshold 1048576
    wifi_set_key "$dst" gActiveMaxChannelTime 40
    wifi_set_key "$dst" gActiveMinChannelTime 20
    wifi_set_key "$dst" gMaxConcurrentActiveSessions 2
    wifi_set_key "$dst" rx_wakelock_timeout 0
    # WIFI_BAND_CAPABILITY: 0=leave as-is (auto, both bands), 1=2.4GHz
    # only, 2=5GHz only. Only written when explicitly set to 1 or 2.
    case "$WIFI_BAND_CAPABILITY" in
      1|2) wifi_set_key "$dst" BandCapability "$WIFI_BAND_CAPABILITY" ;;
    esac
    chmod 644 "$dst"
    chown --reference="$cfg" "$dst" 2>/dev/null
    chcon --reference="$cfg" "$dst" 2>/dev/null || chcon u:object_r:vendor_configs_file:s0 "$dst" 2>/dev/null
    log 1 "wifi_qcom_fix: patched overlay for $cfg (reboot to take effect)"
  done
  is_on "$WIFI_QCOM_FIX" && [ "$found" = 0 ] && log 2 "wifi_qcom_fix: WCNSS_qcom_cfg.ini not found (non-Qualcomm device?)"
  return 0
}

############################################################################
# Single-tweak dispatch - used by the WebUI so flipping one switch applies
# or fully reverts just that tweak immediately, without waiting for a
# reboot or a full apply_early/apply_late pass.
############################################################################

GMS_CATEGORY_KEYS="DISABLE_ADS:ads DISABLE_TRACKING:tracking DISABLE_ANALYTICS:analytics DISABLE_REPORTING:reporting DISABLE_BACKGROUND:background DISABLE_UPDATE:update DISABLE_LOCATION:location DISABLE_GEOFENCE:geofence DISABLE_NEARBY:nearby DISABLE_CAST:cast DISABLE_DISCOVERY:discovery DISABLE_SYNC:sync DISABLE_CLOUD:cloud DISABLE_AUTH:auth DISABLE_WALLET:wallet DISABLE_PAYMENT:payment DISABLE_WEAR:wear DISABLE_FITNESS:fitness"

run_single() {
  # $1 = config key, $2 = new value ("0" or "1")
  load_conf
  setup_resetprop
  local key="$1" val="$2"

  for pair in $GMS_CATEGORY_KEYS; do
    if [ "${pair%%:*}" = "$key" ]; then
      gms_apply_category "${pair#*:}" "$val"
      return 0
    fi
  done

  case "$key" in
    MIUI_SERVICES)      if is_on "$val"; then tweak_miui_services; else restore_miui_services; fi ;;
    MISC_KILL_SERVICES) if is_on "$val"; then tweak_misc_kill_services; else revert_misc_kill_services; fi ;;
    SYS_LOG_PROPS)       if is_on "$val"; then tweak_sys_log_props; else revert_props_tag SYS_LOG_PROPS; fi ;;
    SYS_DALVIK_PROPS)    if is_on "$val"; then tweak_sys_dalvik_props; else revert_props_tag SYS_DALVIK_PROPS; fi ;;
    CPU_PIN)             if is_on "$val"; then tweak_cpu_pin; else revert_cpu_pin; fi ;;
    CPU_CORE_HARDCODE)   if is_on "$val"; then tweak_cpu_core_hardcode; else revert_cpu_core_hardcode; fi ;;
    FIXED_PERF_MODE)     if is_on "$val"; then tweak_fixed_perf_mode; else revert_fixed_perf_mode; fi ;;
    THERMAL_OVERRIDE)    if is_on "$val"; then tweak_thermal_override; else revert_thermal_override; fi ;;
    PACKAGES_DEXOPT)     is_on "$val" && tweak_packages_dexopt; true ;;  # one-shot, nothing to revert
    CMD_MISC)            if is_on "$val"; then tweak_cmd_misc; else revert_cmd_misc; fi ;;
    LMK_PROPS)           if is_on "$val"; then tweak_lmk_props; else revert_props_tag LMK_PROPS; fi ;;
    TOMBSTONE_DISABLE)   if is_on "$val"; then tweak_tombstone_disable; else revert_props_tag TOMBSTONE_DISABLE; fi ;;
    BLUR_DISABLE)        if is_on "$val"; then tweak_blur_disable; else revert_props_tag BLUR_DISABLE; fi ;;
    LEGACY_MODE)          if is_on "$val"; then tweak_legacy_mode; else revert_props_tag LEGACY_MODE; fi ;;
    WIFI_QCOM_FIX)       tweak_wifi_qcom_fix; true ;;  # handles both on/off itself
    WIFI_BAND_CAPABILITY) tweak_wifi_qcom_fix; true ;;  # re-patches with the new band value (no-op if WIFI_QCOM_FIX is off)
    *) log 2 "run_single: unknown key $key" ;;
  esac
  return 0
}

############################################################################
# Entry points
############################################################################

apply_early() {
  # Runs at post-fs-data: properties only (fast, no wait for boot)
  load_conf
  setup_resetprop
  tweak_sys_log_props
  tweak_sys_dalvik_props
  tweak_lmk_props
  tweak_tombstone_disable
  tweak_blur_disable
  tweak_legacy_mode
  tweak_wifi_qcom_fix
}

apply_late() {
  # Runs at late boot: services, GMS categories, CPU, dexopt
  load_conf
  setup_resetprop
  log 1 "[START] apply_late"
  tweak_miui_services
  tweak_misc_kill_services
  tweak_gms_services
  tweak_cpu_pin
  tweak_cpu_core_hardcode
  tweak_fixed_perf_mode
  tweak_thermal_override
  tweak_packages_dexopt
  tweak_cmd_misc
  log 1 "[END] apply_late"
}

restore_all() {
  load_conf
  restore_miui_services
  restore_gms_services
  if [ -f "$PROP_TRACK" ]; then
    sort -u "$PROP_TRACK" | while IFS= read -r name; do
      [ -n "$name" ] || continue
      resetprop --delete "$name" 2>/dev/null || resetprop -d "$name" 2>/dev/null
    done
    rm -f "$PROP_TRACK"
  fi
  log 1 "restore_all: services re-enabled, tracked properties deleted (persist.* still need a reboot to fully clear since init reapplies them)"
}
