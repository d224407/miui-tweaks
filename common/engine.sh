#!/system/bin/sh
##############################################################################
# MIUI Tweaks - engine
#
# The "apply" side: config loading, shared helpers, the property-tracking
# system, the CPU/cgroup helpers, the single-tweak dispatcher used by the
# WebUI, and the three entry points (apply_early/apply_late/restore_all).
#
# This file has no tweak-specific data - what actually gets changed on the
# device lives in the sibling tweaks-*.sh files, one per section of
# config/tweaks.conf. common/load.sh sources this file plus every
# tweaks-*.sh together, and is what post-fs-data.sh, service.sh,
# uninstall.sh and the WebUI all source.
#
# MODDIR must already be set by whoever sources common/load.sh (directly
# executed scripts can derive it from their own $0; anything that sources
# load.sh instead - like the WebUI - must set MODDIR itself first, since
# $0 does not change across a `.`/source and so cannot be trusted here).
##############################################################################

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

##############################################################################
# Property tracking (set_prop/apply_prop_block/revert_props_tag) and the
# resetprop shim
##############################################################################

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

##############################################################################
# CPU / cgroup helpers
##############################################################################

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
    GMS_LOG_DISABLE)      if is_on "$val"; then tweak_gms_log_disable; else revert_gms_log_disable; fi ;;
    WIFI_QCOM_FIX)       tweak_wifi_qcom_fix; true ;;  # handles both on/off itself
    WIFI_BAND_CAPABILITY|WIFI_KEY_ARP|WIFI_KEY_NS|WIFI_KEY_MCADDR|WIFI_KEY_POWERSAVE|WIFI_KEY_RUNTIMEPM|WIFI_KEY_ROAM|WIFI_KEY_11D|WIFI_KEY_RTS|WIFI_KEY_SCANTIME|WIFI_KEY_SESSIONS|WIFI_KEY_WAKELOCK)
      tweak_wifi_qcom_fix; true ;;  # re-patches with the current flags (no-op if WIFI_QCOM_FIX is off)
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
  tweak_gms_log_disable
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
  revert_gms_log_disable
  if [ -f "$PROP_TRACK" ]; then
    sort -u "$PROP_TRACK" | while IFS= read -r name; do
      [ -n "$name" ] || continue
      resetprop --delete "$name" 2>/dev/null || resetprop -d "$name" 2>/dev/null
    done
    rm -f "$PROP_TRACK"
  fi
  log 1 "restore_all: services re-enabled, tracked properties deleted (persist.* still need a reboot to fully clear since init reapplies them)"
}
