#!/system/bin/sh
# System log/debug binary stubs (mount overlay, like the Wi-Fi tweak).
# Replaces each binary with a no-op script under $MODDIR/system/bin so the
# real daemon never starts. Does not touch the matching .rc files - init
# will back off retrying after a few quick exits.

SYSBIN_GROUPS="STUB_LOG:log logcat logcatd logger logname logwrapper logpersist.cat logpersist.start logpersist.stop
STUB_TRACED:traced traced_probes traced_perf atrace
STUB_DEBUG:debuggerd tombstoned crash_dump32 crash_dump64
STUB_BUGREPORT:bugreport bugreportz bugreport_procdump dumpstate dmesgd dmesg lpdump lpdumpd
STUB_NETDIAG:tcpdump tracepath tracepath6 traceroute6 diag_socket_log i2cdump dmabuf_dump notify_traceur.sh"

sysbin_stub_write() {
  local dst="$MODDIR/system/bin/$1"
  mkdir -p "$MODDIR/system/bin"
  printf '#!/system/bin/sh\nexit 0\n' > "$dst" || { log 3 "sysbin: failed writing stub $1"; return 1; }
  chmod 755 "$dst"
}

tweak_sysbin_stubs() {
  local flag bins b val any=0
  echo "$SYSBIN_GROUPS" | while IFS=':' read -r flag bins; do
    [ -n "$flag" ] || continue
    eval "val=\$$flag"
    for b in $bins; do
      if is_on "$SYSBIN_MASTER" && is_on "$val"; then
        sysbin_stub_write "$b"
      else
        rm -f "$MODDIR/system/bin/$b"
      fi
    done
  done
  find "$MODDIR/system/bin" -type f 2>/dev/null | grep -q . && any=1
  [ "$any" = 1 ] && log 1 "sysbin_stubs: applied (reboot to take effect)"
  rmdir "$MODDIR/system/bin" 2>/dev/null
  return 0
}
