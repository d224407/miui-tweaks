#!/system/bin/sh
# Wi-Fi (Qualcomm WCNSS config patch)

# Wi-Fi: Qualcomm WCNSS config patch

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
  # $1=file $2=key $3=value (replace if present, insert if not).
  # The WCNSS parser stops reading at a line that is just "END" - anything
  # appended after it is silently ignored, so a new key must be inserted
  # *before* that line, not tacked onto the end of the file.
  if grep -q "^[[:space:]]*$2=" "$1"; then
    sed -i "s|^[[:space:]]*$2=.*|$2=$3|" "$1"
  elif grep -q "^[[:space:]]*END[[:space:]]*$" "$1"; then
    sed -i "0,/^[[:space:]]*END[[:space:]]*$/s||$2=$3\n&|" "$1"
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
    mkdir -p "$(dirname "$dst")" || { log 3 "wifi_qcom_fix: mkdir failed for $dst"; continue; }
    cp -f "$src" "$dst" || { log 3 "wifi_qcom_fix: cp failed, $src -> $dst"; continue; }
    is_on "$WIFI_KEY_ARP" && wifi_set_key "$dst" hostArpOffload 0
    is_on "$WIFI_KEY_NS" && wifi_set_key "$dst" hostNsOffload 0
    is_on "$WIFI_KEY_MCADDR" && wifi_set_key "$dst" gMCAddrListEnable 1
    is_on "$WIFI_KEY_POWERSAVE" && wifi_set_key "$dst" gEnablePowerSaveOffload 5
    is_on "$WIFI_KEY_RUNTIMEPM" && wifi_set_key "$dst" gRuntimePM 1
    is_on "$WIFI_KEY_ROAM" && wifi_set_key "$dst" RoamRssiDiff 3
    is_on "$WIFI_KEY_11D" && wifi_set_key "$dst" g11dSupportEnabled 0
    is_on "$WIFI_KEY_RTS" && wifi_set_key "$dst" RTSThreshold 1048576
    if is_on "$WIFI_KEY_SCANTIME"; then
      wifi_set_key "$dst" gActiveMaxChannelTime 40
      wifi_set_key "$dst" gActiveMinChannelTime 20
    fi
    is_on "$WIFI_KEY_SESSIONS" && wifi_set_key "$dst" gMaxConcurrentActiveSessions 2
    is_on "$WIFI_KEY_WAKELOCK" && wifi_set_key "$dst" rx_wakelock_timeout 0
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
