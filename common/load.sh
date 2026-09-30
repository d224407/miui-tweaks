#!/system/bin/sh
##############################################################################
# MIUI Tweaks - loader
# Single entry point: sources the engine (apply-side logic) then every
# tweaks-*.sh (tweak-side definitions). post-fs-data.sh, service.sh,
# uninstall.sh and the WebUI all source this one file instead of each
# common/*.sh individually.
#
# Requires MODDIR to already be set by the caller (see the note at the top
# of engine.sh for why this file cannot safely derive it from $0 itself).
##############################################################################

. "$MODDIR/common/engine.sh"
. "$MODDIR/common/tweaks-miui.sh"
. "$MODDIR/common/tweaks-shared.sh"
. "$MODDIR/common/tweaks-gms.sh"
. "$MODDIR/common/tweaks-wifi.sh"
. "$MODDIR/common/tweaks-legacy.sh"
