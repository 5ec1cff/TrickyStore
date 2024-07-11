DEBUG=@DEBUG@

MODDIR=${0%/*}

cd $MODDIR

(
while [ true ]; do
  /system/bin/app_process -Djava.class.path=./service.apk / --nice-name=TrickyStore io.github.a13e300.tricky_store.MainKt
  if [ $? -ne 0 ]; then
    exit 1
  fi
done
) &
