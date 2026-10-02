#!/system/bin/sh
# Picks the right prebuilt binary for this device's ABI, same convention
# KernelEnhancer uses (module.prop author's other project): system/bin/
# <name>_32 / _64 / _x86 / _x64.

bin_path() {
  # $1 = tool name (miui, shared, gms, wifi, sysbin, legacy)
  local arch="$(getprop ro.product.cpu.abi)"
  local suffix=""
  case "$arch" in
    arm64-v8a|arm64) suffix="64" ;;
    armeabi-v7a|armeabi) suffix="32" ;;
    x86_64|x86_64-v2|x86_64-v3) suffix="x64" ;;
    x86|i686|i586|i486|i386) suffix="x86" ;;
  esac
  local p="$MODDIR/system/bin/${1}_${suffix}"
  if [ -n "$suffix" ] && [ -f "$p" ]; then echo "$p"; return 0; fi
  # Fallback: try every variant in case ro.product.cpu.abi was unexpected
  for s in 64 32 x64 x86; do
    p="$MODDIR/system/bin/${1}_${s}"
    [ -f "$p" ] && { echo "$p"; return 0; }
  done
  echo ""
  return 1
}
