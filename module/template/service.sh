DEBUG=@DEBUG@

MODDIR=${0%/*}

cd $MODDIR

(
while true; do
  ./daemon || exit 1
done
) &
