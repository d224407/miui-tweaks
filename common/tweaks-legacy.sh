#!/system/bin/sh
# Legacy (GhostGMS Legacy 1.3/2.0 deep tweak set)

tweak_legacy_mode() {
  is_on "$LEGACY_MODE" || return 0
  # Deep sysprop set carried over from GhostGMS Legacy 1.3/2.0, deduped
  # against SYS_LOG_PROPS/SYS_DALVIK_PROPS above (only new keys here).
  # persist.ims.disabled and wifi.interface=wlan0 from the source module
  # are intentionally left out - the first disables VoLTE/VoWiFi calling,
  # the second is known to bootloop some ROMs (the source module itself
  # ships that line commented out).
  local props="av.debug.disable.pers.cache true
debug.composition.type gpu
debug.kill_allocating_task 0
debug.qualcomm.sns.daemon 0
debug.qualcomm.sns.hal 0
debug.qualcomm.sns.libsensor1 0
debug.sf.disable_backpressure 1
debug.sf.gpu_comp_tiling 0
debug_test 0
hwui.use_gpu_pixel_buffers false
live.logcat disable
log.cffdump 0
log.cffdump_no_memzero 0
log.cffdump_with_ifh 0
log.dumpx 0
log.pm4 0
log.pm4mem 0
log.primitives 0
log.resolves 0
log.sc_dev 0
log.shaders 0
log_ao 0
log_audiodecnode 0
log_audiooutput 0
log_basedecnode 0
log_datapath 0
log_fps_interval 0
log_frame_info 0
log_metadatadriver 0
log_mp4dectime 0
log_mp4parsernode 0
log_omxmp4 0
log_outputnode 0
log_outputnodeinputport 0
log_playerdriver 0
log_playerengine 0
log_posttime 0
log_profile 0
log_surfaceoutput 0
log_videodecnode 0
logcast.live disable
persist.brcm.ap_crash none
persist.brcm.cp_crash none
persist.brcm.log none
persist.bt.a2dp.aac_disable true
persist.ims.enableADBLogs 0
persist.ims.enableDebugLogs 0
persist.radio.oem_socket false
persist.service.lgospd.enable 0
persist.service.pcsync.enable 0
persist.sys.composition.type gpu
persist.sys.dun.override 0
persist.sys.offlinelog.kernel 1
persist.sys.offlinelog.logcat 1
persist.sys.wfd.virtual 0
pm.sleep_mode 1
profiler.debugmonitor false
profiler.forse_disable_err_rpt 1
profiler.forse_disable_ulog 1
profiler.hung.dumpdobugreport false
profiler.launch false
ro.compcache.default 0
ro.config.ksm.support false
ro.debuggable 0
ro.egl.destroy_after_detach false
ro.kernel.android.checkjni 0
ro.kernel.checkjni 0
ro.kernel.qemu.gles 0
ro.sf.battery.log.enabled 0
ro.sf.battery_log 0
ro.telephony.call_ring.multiple false
sdm.debug.disable_inline_rotator 1
sdm.debug.disable_skip_validate 1
sys.games.gt.prof 1
sys.hwc.gpu_perf_mode 0
vendor.fm.a2dp.conc.disabled true
vendor.vidc.enc.disable_bframes 1
video.disable.ubwc 1"
  apply_prop_block "LEGACY_MODE" "$props"
  log 1 "legacy_mode: applied (78 properties)"
}
