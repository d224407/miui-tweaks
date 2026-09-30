#!/system/bin/sh
##############################################################################
# Shared (MIUI + GMS): low-level tweaks that don't belong to either side
# specifically. Matches the "Shared" section of config/tweaks.conf.
##############################################################################

tweak_lmk_props() {
  is_on "$LMK_PROPS" || return 0
  set_prop LMK_PROPS ro.lmk.debug false
  set_prop LMK_PROPS ro.lmk.log_stats false
  log 1 "lmk_props: applied"
}

tweak_tombstone_disable() {
  is_on "$TOMBSTONE_DISABLE" || return 0
  set_prop TOMBSTONE_DISABLE tombstoned.max_tombstone_count 0
  log 1 "tombstone_disable: applied"
}

tweak_blur_disable() {
  is_on "$BLUR_DISABLE" || return 0
  set_prop BLUR_DISABLE disableBlurs true
  set_prop BLUR_DISABLE enable_blurs_on_windows 0
  set_prop BLUR_DISABLE ro.launcher.blur.appLaunch 0
  set_prop BLUR_DISABLE ro.sf.blurs_are_expensive 0
  set_prop BLUR_DISABLE ro.surface_flinger.supports_background_blur 0
  log 1 "blur_disable: applied"
}
