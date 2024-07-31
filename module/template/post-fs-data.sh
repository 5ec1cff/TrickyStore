MODDIR=${0%/*}

CONFIG_DIR=/data/adb/tricky_store

if [ -f "$CONFIG_DIR/spoof_build_vars" ]; then
  mv "$MODDIR/zygisk-disabled" "$MODDIR/zygisk" 2>/dev/null
else
  mv "$MODDIR/zygisk" "$MODDIR/zygisk-disabled" 2>/dev/null
fi
