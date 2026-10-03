#!/system/bin/sh

bin_path() {
  local p="$MODDIR/system/bin/$1"
  [ -f "$p" ] && { echo "$p"; return 0; }
  echo ""
  return 1
}
