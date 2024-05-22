#!/bin/sh
mkdir zram
sudo modprobe zram
sudo zramctl /dev/zram0 --algorithm zstd --size 32G
sudo mke2fs -q -m 0 -b 4096 -O sparse_super -L zram /dev/zram0
sudo mount -o relatime,noexec,nosuid /dev/zram0 ./zram
OWNER=`whoami`
sudo chown $OWNER:$OWNER zram
