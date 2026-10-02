#!/system/bin/sh
# GMS: service categories + logging

# GMS: service categories

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
  if ! is_on "$GMS_MASTER"; then
    restore_gms_services
    return 0
  fi
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
  # $1 = category token, $2 = "1" disable / "0" enable
  [ -f "$GMSLIST" ] || return 0
  is_on "$GMS_MASTER" || return 0
  local action="pm enable" fail=0
  [ "$2" = "1" ] && action="pm disable"
  while IFS='|' read -r svc cat || [ -n "$svc" ]; do
    case "$svc" in ''|'#'*) continue ;; esac
    if [ "$cat" = "$1" ]; then
      $action "$svc" >/dev/null 2>&1 || fail=$((fail+1))
    fi
  done < "$GMSLIST"
  [ "$fail" -gt 0 ] && log 3 "gms category $1: $fail service(s) failed to apply"
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

# GMS: logging/telemetry Settings.Global keys

tweak_droidguard() {
  is_on "$DISABLE_DROIDGUARD" || return 0
  pm disable com.google.android.gms/com.google.android.gms.droidguard.DroidGuardService 2>/dev/null \
    || log 3 "droidguard: pm disable DroidGuardService failed"
  pm disable com.google.android.gms/com.google.android.gms.droidguard.DroidGuardGcmTaskService 2>/dev/null \
    || log 3 "droidguard: pm disable DroidGuardGcmTaskService failed"
  log 1 "droidguard: disabled (breaks SafetyNet/Play Integrity - banking, Wallet, Play Store checks)"
}

revert_droidguard() {
  pm enable com.google.android.gms/com.google.android.gms.droidguard.DroidGuardService 2>/dev/null
  pm enable com.google.android.gms/com.google.android.gms.droidguard.DroidGuardGcmTaskService 2>/dev/null
  log 1 "droidguard: re-enabled"
}

GMS_LOG_KEYS="gmscorestat_enabled play_store_panel_logging_enabled clearcut_events clearcut_gcm ga_collection_enabled clearcut_enabled analytics_enabled uploading_enabled bug_report_in_power_menu usage_stats_enabled usagestats_collection_enabled"

tweak_gms_log_disable() {
  is_on "$GMS_LOG_DISABLE" || return 0
  for k in $GMS_LOG_KEYS; do settings put global "$k" 0; done
  settings put global phenotype__debug_bypass_phenotype 1
  settings put global phenotype_boot_count 99
  settings put global phenotype_flags "disable_log_upload=1,disable_log_for_missing_debug_id=1"
  log 1 "gms_log_disable: applied"
}

revert_gms_log_disable() {
  # No single "default" value exists for these (GMS-internal flags, not
  # AOSP settings) - deleting the row is the correct revert, same as a
  # fresh install where GMS has never written them.
  for k in $GMS_LOG_KEYS; do settings delete global "$k" >/dev/null 2>&1; done
  settings delete global phenotype__debug_bypass_phenotype >/dev/null 2>&1
  settings delete global phenotype_boot_count >/dev/null 2>&1
  settings delete global phenotype_flags >/dev/null 2>&1
  log 1 "gms_log_disable: reverted"
}

