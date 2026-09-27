#!/system/bin/sh
##############################################################################
# MIUI Tweaks - uninstall (re-enable every disabled service)
##############################################################################
MODDIR="/data/adb/modules/miui_tweaks"
. "$MODDIR/common/tweaks.sh" 2>/dev/null
restore_all 2>/dev/null

echo "[$(date '+%Y-%m-%d %H:%M:%S')] MIUI Tweaks uninstalled, services restored. persist.* properties revert on next reboot." \
  >> /data/local/tmp/miui_tweaks_uninstall.log 2>/dev/null
