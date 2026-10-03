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
apply_late

PKG_HASH_FILE="$MODDIR/config/.pkg_hash"
PKG_POLL_INTERVAL=20

pkg_state_hash() {
  { pm list packages -e; pm list packages -d; } 2>/dev/null | md5sum | awk '{print $1}'
}

pkg_watch_loop() {
  local prev="" cur=""
  [ -f "$PKG_HASH_FILE" ] && prev="$(cat "$PKG_HASH_FILE" 2>/dev/null)"
  while true; do
    sleep "$PKG_POLL_INTERVAL"
    cur="$(pkg_state_hash)"
    if [ -n "$cur" ] && [ "$cur" != "$prev" ]; then
      prev="$cur"
      echo "$cur" > "$PKG_HASH_FILE"
      log 1 "pkg_watch: package state changed, re-applying"
      apply_late
    fi
  done
}

pkg_watch_loop &
