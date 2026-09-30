#!/system/bin/sh
##############################################################################
# MIUI Tweaks - late boot stage (services, CPU, dexopt)
# Waits for boot_completed + first unlock before writing to /storage.
##############################################################################
MODDIR="${0%/*}"
. "$MODDIR/common/load.sh"

wait_until_login() {
  until [ "$(getprop sys.boot_completed)" -eq 1 ]; do sleep 1; done
  test_file="/storage/emulated/0/Android/.PERMISSION_TEST"
  until touch "$test_file" 2>/dev/null; do sleep 1; done
  rm -f "$test_file"
}

wait_until_login
sleep 30
apply_late
