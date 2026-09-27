##############################################################################
# MIUI Tweaks - installer
##############################################################################
SKIPUNZIP=0
SKIPMOUNT=false

set_permissions() {
  set_perm_recursive "$MODPATH" 0 0 0755 0755
}

ui_print "- MIUI Tweaks installed"
ui_print "- A safe default set of tweaks is enabled (see README.md)"
ui_print "- Open the module's WebUI to review or change each tweak"
