# shellcheck disable=SC2034
SKIPUNZIP=1

DEBUG=@DEBUG@
SONAME=@SONAME@
SUPPORTED_ABIS="@SUPPORTED_ABIS@"
MIN_SDK=@MIN_SDK@

if [ "$BOOTMODE" ] && [ "$KSU" ]; then
  ui_print "- Installing from KernelSU app"
  ui_print "- KernelSU version: $KSU_KERNEL_VER_CODE (kernel) + $KSU_VER_CODE (ksud)"
  if [ "$(which magisk)" ]; then
    ui_print "*********************************************************"
    ui_print "! Multiple root implementation is NOT supported!"
    ui_print "! Please uninstall Magisk before installing Zygisk Next"
    abort    "*********************************************************"
  fi
elif [ "$BOOTMODE" ] && [ "$MAGISK_VER_CODE" ]; then
  ui_print "- Installing from Magisk app"
else
  ui_print "*********************************************************"
  ui_print "! Install from recovery is not supported"
  ui_print "! Please install from KernelSU or Magisk app"
  abort    "*********************************************************"
fi

VERSION=$(grep_prop version "${TMPDIR}/module.prop")
ui_print "- Installing $SONAME $VERSION"

# check architecture
support=false
for abi in $SUPPORTED_ABIS
do
  if [ "$ARCH" == "$abi" ]; then
    support=true
  fi
done
if [ "$support" == "false" ]; then
  abort "! Unsupported platform: $ARCH"
else
  ui_print "- Device platform: $ARCH"
fi

# check android
if [ "$API" -lt $MIN_SDK ]; then
  ui_print "! Unsupported sdk: $API"
  abort "! Minimal supported sdk is $MIN_SDK"
else
  ui_print "- Device sdk: $API"
fi

ui_print "- Extracting verify.sh"
unzip -o "$ZIPFILE" 'verify.sh' -d "$TMPDIR" >&2
if [ ! -f "$TMPDIR/verify.sh" ]; then
  ui_print "*********************************************************"
  ui_print "! Unable to extract verify.sh!"
  ui_print "! This zip may be corrupted, please try downloading again"
  abort    "*********************************************************"
fi
. "$TMPDIR/verify.sh"
extract "$ZIPFILE" 'customize.sh'  "$TMPDIR/.vunzip"
extract "$ZIPFILE" 'verify.sh'     "$TMPDIR/.vunzip"
extract "$ZIPFILE" 'sepolicy.rule' "$TMPDIR"

ui_print "- Extracting module files"
extract "$ZIPFILE" 'module.prop'     "$MODPATH"
extract "$ZIPFILE" 'post-fs-data.sh' "$MODPATH"
extract "$ZIPFILE" 'service.sh'      "$MODPATH"
extract "$ZIPFILE" 'service.apk'     "$MODPATH"
mv "$TMPDIR/sepolicy.rule" "$MODPATH"

mkdir "$MODPATH/zygisk"

if [ "$ARCH" = "x64" ]; then
  ui_print "- Extracting x64 libraries"
  extract "$ZIPFILE" "lib/x86_64/lib$SONAME.so" "$MODPATH" true
  extract "$ZIPFILE" "lib/x86_64/libinject.so" "$MODPATH" true
  extract "$ZIPFILE" "lib/x86_64/libtszygisk.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/libtszygisk.so" "$MODPATH/zygisk/x86_64.so"
else
  ui_print "- Extracting arm64 libraries"
  extract "$ZIPFILE" "lib/arm64-v8a/lib$SONAME.so" "$MODPATH" true
  extract "$ZIPFILE" "lib/arm64-v8a/libinject.so" "$MODPATH" true
  extract "$ZIPFILE" "lib/arm64-v8a/libtszygisk.so" "$MODPATH/zygisk" true
  mv "$MODPATH/zygisk/libtszygisk.so" "$MODPATH/zygisk/arm64-v8a.so"
fi

mv "$MODPATH/libinject.so" "$MODPATH/inject"
chmod 755 "$MODPATH/inject"

LIST="/data/adb/tricky_store/target.txt"
mkdir -p /data/adb/tricky_store
if [ ! -e "$LIST" ]; then
    echo "" > "$LIST"
fi

ui_print "***********************************************************************"
ui_print "- Put keybox.xml to /data/adb/tricky_store/keybox.xml"
ui_print "- Put target packages to /data/adb/tricky_store/target.txt (Optional)"
ui_print "- Format: one package per line"
ui_print "***********************************************************************"
