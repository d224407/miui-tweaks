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
