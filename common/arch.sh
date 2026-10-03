#!/system/bin/sh
# customize.sh already extracted only this device's ABI at install time
# and dropped the _32/_64/_x86/_x64 suffix, so this just points at the
# plain tool name under system/bin/.

bin_path() {
  local p="$MODDIR/system/bin/$1"
  [ -f "$p" ] && { echo "$p"; return 0; }
  echo ""
  return 1
}
