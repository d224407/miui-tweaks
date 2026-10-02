##############################################################################
# MIUI Tweaks - installer
##############################################################################
SKIPUNZIP=0
SKIPMOUNT=false

set_permissions() {
  set_perm_recursive "$MODPATH" 0 0 0755 0755
}

##############################################################################
# Preserve an existing config on reinstall/update.
# Only carries over values for keys that already exist; new keys shipped
# in this version keep their default, nothing in the old file is dropped
# from the check even if this version no longer ships that key.
##############################################################################
merge_old_config() {
  local new="$MODPATH/config/tweaks.conf"
  local old=""
  for cand in \
    "/data/adb/modules/$MODID/config/tweaks.conf" \
    "/data/adb/modules_update/$MODID/config/tweaks.conf"
  do
    [ -f "$cand" ] && { old="$cand"; break; }
  done
  [ -n "$old" ] || return 0

  ui_print "- Existing config found, keeping your tweak settings"
  while IFS='=' read -r key val; do
    case "$key" in ''|'#'*) continue ;; esac
    [ -n "$key" ] || continue
    grep -q "^$key=" "$new" && sed -i "s|^$key=.*|$key=$val|" "$new"
  done < "$old"
}

MODID="$(grep '^id=' "$MODPATH/module.prop" | cut -d= -f2)"
merge_old_config

ui_print "- MIUI Tweaks installed"
ui_print "- A safe default set of tweaks is enabled on first install (see README.md)"
ui_print "- Open the module's WebUI to review or change each tweak"

ui_print "- Checking tweak-tool binaries..."
ARCH_SUFFIX=""
case "$(getprop ro.product.cpu.abi)" in
  arm64-v8a|arm64) ARCH_SUFFIX="64" ;;
  armeabi-v7a|armeabi) ARCH_SUFFIX="32" ;;
  x86_64*) ARCH_SUFFIX="x64" ;;
  x86|i686|i586|i486|i386) ARCH_SUFFIX="x86" ;;
esac
if [ -n "$ARCH_SUFFIX" ]; then
  MISSING=0
  for t in miui shared gms wifi sysbin legacy; do
    [ -f "$MODPATH/system/bin/${t}_${ARCH_SUFFIX}" ] || MISSING=1
  done
  if [ "$MISSING" = 1 ]; then
    ui_print "  WARNING: some binaries for this ABI ($ARCH_SUFFIX) are missing from this zip"
  else
    ui_print "  OK ($ARCH_SUFFIX)"
  fi
else
  ui_print "  WARNING: unrecognized ABI $(getprop ro.product.cpu.abi), tweaks will not run"
fi
