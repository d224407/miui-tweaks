#!/system/bin/sh
##############################################################################
# GMS: service categories + logging
# Matches the "GMS" section of config/tweaks.conf.
##############################################################################

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

##############################################################################
# GMS: logging/telemetry Settings.Global keys
# Distinct from SYS_LOG_PROPS (device-wide sysprops) - these are GMS's own
# Settings.Global flags, applied via `settings put`, matching what the
# source GhostGMS module's "Disable GMS Logging" option actually did.
##############################################################################

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

