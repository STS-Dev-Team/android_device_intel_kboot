#!/bin/sh

ABOOT=/recovery/bin/aboot
MESSAGE="Override aboot version by putting it on the root of the media partition (mmcblk0p3)" 
TTYS0=/dev/ttyS0

mkdir -p /mnt/media                > /dev/null 2>&1
mount /dev/mmcblk0p3 /mnt/media    > /dev/null 2>&1

if [ -x /mnt/media/aboot ]; then
    MESSAGE="Copying and executing SD-CARD version aboot from /dev/mmcblk0p3."
    ABOOT=/aboot
    cp /mnt/media/aboot /aboot
fi

umount /mnt/media                  > /dev/null 2>&1

TEE=/bin/cat
if [ -c "$TTYS0" ]; then
    TEE="/usr/bin/tee /dev/ttyS0"
fi

(   echo
    echo $MESSAGE
    echo
) | $TEE > /dev/tty0

if [ ! -e /recovery/bin/aboot ]; then
    tar -zxf /recovery/recovery.tar.gz -C /recovery
fi

$ABOOT &
