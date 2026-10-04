#!/system/bin/sh

CONF="$MODDIR/config/tweaks.conf"
LOGFILE="/storage/emulated/0/Android/miui_tweaks.log"
GMSLIST="$MODDIR/gmslist.txt"
PROP_TRACK="$MODDIR/config/.applied_props"

. "$MODDIR/common/arch.sh"

load_conf() {
  [ -f "$CONF" ] && . "$CONF"
}

log() {
  local t=""
  case "$1" in
    1) t="INFO" ;; 2) t="WARN" ;; 3) t="ERROR" ;; *) t="?" ;;
  esac
  echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$t] $2" >> "$LOGFILE" 2>/dev/null
  return 0
}

is_on() {
  [ "$1" = "1" ]
}

resetprop_bin() {
  if command -v resetprop > /dev/null 2>&1; then echo "resetprop"; return; fi
  for p in /data/adb/ksu/bin/resetprop /data/adb/ap/bin/resetprop; do
    [ -f "$p" ] && { echo "$p"; return; }
  done
  echo "setprop"
}

run_tool() {
  local tool="$1"; shift
  local p="$(bin_path "$tool")"
  if [ -z "$p" ]; then
    log 3 "engine: no $tool binary for this ABI (ro.product.cpu.abi=$(getprop ro.product.cpu.abi))"
    return 1
  fi
  "$p" "$CONF" "$PROP_TRACK" "$@"
}

terminate_service() {
  killall -q -9 "$1" 2>/dev/null
  stop "$1" 2>/dev/null
}


ps_ret=""
rebuild_process_scan_cache() { ps_ret="$(ps -Ao pid,args)"; }

get_cpu_count() { grep -c ^processor /proc/cpuinfo; }

get_full_cpu_mask() {
  local n=$(get_cpu_count) mask=1 i=1
  while [ $i -lt $n ]; do mask=$((mask | (mask << 1))); i=$((i + 1)); done
  printf '%x' $mask
}

change_task_cgroup() {
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
  rebuild_process_scan_cache
  for proc in zygote usap surfaceflinger system_server composer logd statsd tombstoned incidentd; do
    change_task_nice "$proc" "0"
  done
  local top="$(pm resolve-activity -a android.intent.action.MAIN -c android.intent.category.HOME | grep packageName | head -n1 | cut -d= -f2) com.android.systemui"
  for proc in $top; do change_task_nice "$proc" "0"; done
  log 1 "cpu_pin: reverted (nice reset to 0, cgroup normalizes on next app switch)"
}


GMS_CATEGORY_KEYS="DISABLE_ADS:ads DISABLE_TRACKING:tracking DISABLE_ANALYTICS:analytics DISABLE_REPORTING:reporting DISABLE_BACKGROUND:background DISABLE_UPDATE:update DISABLE_LOCATION:location DISABLE_GEOFENCE:geofence DISABLE_NEARBY:nearby DISABLE_CAST:cast DISABLE_DISCOVERY:discovery DISABLE_SYNC:sync DISABLE_CLOUD:cloud DISABLE_AUTH:auth DISABLE_WALLET:wallet DISABLE_PAYMENT:payment DISABLE_WEAR:wear DISABLE_FITNESS:fitness"

run_single() {
  load_conf
  local key="$1" val="$2"

  for pair in $GMS_CATEGORY_KEYS; do
    if [ "${pair%%:*}" = "$key" ]; then
      run_tool gms "$GMSLIST" category "${pair#*:}" "$val"
      return 0
    fi
  done

  case "$key" in
    MIUI_SERVICES|MISC_KILL_SERVICES|SYS_LOG_PROPS|SYS_DALVIK_PROPS|CPU_CORE_HARDCODE|FIXED_PERF_MODE|THERMAL_OVERRIDE|PACKAGES_DEXOPT|CMD_MISC)
      run_tool miui set "$key" "$val" ;;
    LMK_PROPS|TOMBSTONE_DISABLE|BLUR_DISABLE)
      run_tool shared set "$key" "$val" ;;
    LEGACY_MODE)
      run_tool legacy set "$key" "$val" ;;
    GMS_MASTER|GMS_LOG_DISABLE|DISABLE_DROIDGUARD)
      run_tool gms "$GMSLIST" set "$key" "$val" ;;
    CPU_PIN)
      if is_on "$val"; then tweak_cpu_pin; else revert_cpu_pin; fi ;;
    WIFI_QCOM_FIX)
      run_tool wifi "$MODDIR" set "$key" "$val" ;;
    GMS_DOZE)
      run_tool doze "$MODDIR" set "$key" "$val" ;;
    SYSBIN_MASTER|STUB_LOG|STUB_TRACED|STUB_DEBUG|STUB_BUGREPORT|STUB_NETDIAG)
      run_tool sysbin "$MODDIR" set "$key" "$val" ;;
    *) log 2 "run_single: unknown key $key" ;;
  esac
  return 0
}


apply_early() {
  load_conf
  run_tool shared early
  run_tool miui early
  run_tool legacy early
  run_tool wifi "$MODDIR" early
  run_tool sysbin "$MODDIR" early
  run_tool doze "$MODDIR" early
}

apply_late() {
  load_conf
  log 1 "[START] apply_late"
  run_tool miui late
  run_tool gms "$GMSLIST" late
  run_tool doze "$MODDIR" late
  tweak_cpu_pin
  log 1 "[END] apply_late"
}

restore_all() {
  load_conf
  run_tool gms "$GMSLIST" set GMS_MASTER 0
  run_tool gms "$GMSLIST" set DISABLE_DROIDGUARD 0
  run_tool gms "$GMSLIST" set GMS_LOG_DISABLE 0
  run_tool miui set MIUI_SERVICES 0
  run_tool doze "$MODDIR" set GMS_DOZE 0
  if [ -f "$PROP_TRACK" ]; then
    local rp="$(resetprop_bin)"
    sort -u "$PROP_TRACK" | while IFS=' ' read -r _ name; do
      [ -n "$name" ] || continue
      "$rp" --delete "$name" 2>/dev/null || "$rp" -d "$name" 2>/dev/null
    done
    rm -f "$PROP_TRACK"
  fi
  log 1 "restore_all: services re-enabled, tracked properties deleted (persist.* still need a reboot to fully clear since init reapplies them)"
}
