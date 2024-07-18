DEBUG=@DEBUG@

MODDIR=${0%/*}

cd $MODDIR

(
while [ true ]; do
  ./daemon
  if [ $? -ne 0 ]; then
    exit 1
  fi
done
) &
