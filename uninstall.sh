#!/system/bin/sh
##############################################################################
# MIUI Tweaks - uninstall
# Re-enables every disabled service and deletes every non-persist property
# this module set, so the device is back to normal without a reboot.
# persist.* properties still need a reboot (init reapplies them from
# build.prop otherwise). The Wi-Fi overlay and its backup live inside this
# module's folder, so they are removed automatically when the module folder
# itself is deleted - no separate cleanup needed for that part.
##############################################################################
MODDIR="/data/adb/modules/miui_tweaks"
. "$MODDIR/common/tweaks.sh" 2>/dev/null
restore_all 2>/dev/null

echo "[$(date '+%Y-%m-%d %H:%M:%S')] MIUI Tweaks uninstalled: services re-enabled, tracked properties deleted." \
  >> /data/local/tmp/miui_tweaks_uninstall.log 2>/dev/null
