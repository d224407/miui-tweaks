#!/system/bin/sh
MODDIR="/data/adb/modules/miui_tweaks"
. "$MODDIR/common/load.sh" 2>/dev/null
restore_all 2>/dev/null

echo "[$(date '+%Y-%m-%d %H:%M:%S')] MIUI Tweaks uninstalled: services re-enabled, tracked properties deleted." \
  >> /data/local/tmp/miui_tweaks_uninstall.log 2>/dev/null
