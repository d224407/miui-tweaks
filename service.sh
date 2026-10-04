#!/system/bin/sh
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
rm -f "$MODDIR/config/.pkg_hash"
apply_late

# The only background process: re-apply once every 24h, no polling.
# sleepboot counts suspended time too (plain `sleep` stops while the phone
# sleeps, which can stretch 24h into days); if it is missing or fails, fall
# back to a plain sleep so this can never turn into a busy loop.
daily_apply() {
  local sb="$(bin_path sleepboot)"
  while true; do
    if [ -n "$sb" ]; then "$sb" 86400 || sleep 86400; else sleep 86400; fi
    log 1 "daily_apply: 24h passed, re-applying"
    apply_late
  done
}

daily_apply &
