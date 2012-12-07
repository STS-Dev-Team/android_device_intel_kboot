#!/bin/sh

# Format a device for Android.
# Shell code (below) uses only the features available in the kboot (busybox)
# binaries and shell. Very primitive.
#
# All calculations and size numbers are performed in 1k blocks.

device=$1

# make partitions on NFS server if device is on NFS
echo $device|grep ':'
if [ $? -eq 0 ];then
	mount -t nfs $device -o nolock,rsize=1024,wsize=1024 /mnt
	if [ $? -ne 0 ];then
		echo "mount $device failed!"
		return 1
	fi
	for i in 1 2 3 4 5 6 7 8 10;do
		if [ -e /mnt/nfs$i ];then
			rm -fr /mnt/nfs$i
		fi
			mkdir /mnt/nfs$i
	done
	umount /mnt
	return 0
fi
 
device_base=`basename $device`
device_partitions=`grep "$device_base" /proc/partitions`

if [ ! -b "$device" -o -z "$device_partitions" ]; then
    echo "Error <$device> is not a mass storage device"
    exit 1
fi

MILLION=1000000
CYL_SIZE_BYTES=`fdisk -l $device|grep "cylinders of"|awk '{print $9}'`
NDA_OFFSET_SIZE=100
NDA_OFFSET_CYLS=`expr $NDA_OFFSET_SIZE \* $MILLION / $CYL_SIZE_BYTES `
case $device in
/dev/nda)
    FACTORY_START_CYL=`expr $NDA_OFFSET_CYLS + 1`
    ;;
/dev/mmcblk0)
    FACTORY_START_CYL=`expr $NDA_OFFSET_CYLS + 1`
    ;;
*)
    FACTORY_START_CYL=1
    ;;
esac
# Partitions, their labels and size. SDCARD uses everything that is left over.
FACTORY_NAME=factory
FACTORY_SIZE=256
FACTORY_PART=1

DATA_NAME=data
DATA_SIZE=512
DATA_PART=2
DATA_START_CYL=`expr $FACTORY_SIZE \* $MILLION / $CYL_SIZE_BYTES + $FACTORY_START_CYL + 2`

SDCARD_NAME=media
SDCARD_SIZE=0
SDCARD_PART=3
SDCARD_START_CYL=`expr $DATA_SIZE \* $MILLION / $CYL_SIZE_BYTES + $DATA_START_CYL + 2`

# Extended is 4

RECOVERY_NAME=recovery
RECOVERY_SIZE=128
RECOVERY_PART=5

SYSTEM_NAME=system
SYSTEM_SIZE=512
SYSTEM_PART=6

# The fdisk script doesn't specify a size for the cache partition... it'll use up
# the remainder of the device. Because of this, it'll be a couple of hundred blocks short
# of the calculated request. We specify CACHE_SIZE here so that the calculation for the
# size of the media partition leaves the right amount for the cache partition.
CACHE_NAME=cache
CACHE_SIZE=256
CACHE_PART=7

CONFIG_NAME=config
CONFIG_SIZE=16
CONFIG_PART=8

PANIC_NAME=panic
PANIC_SIZE=2
PANIC_PART=9

ILOG_NAME=ilog
ILOG_SIZE=1024
ILOG_PART=10

device_size=`fdisk -l $device|grep "Disk $device:"|awk '{print int($3)}'`
size_in_GB=`fdisk -l $device|grep "Disk $device:"|awk '{print $4}'`
if [ "$size_in_GB" = "GB," ]; then
    device_size=`expr $device_size \* 1000`
fi
fixed_part_total=`expr $FACTORY_SIZE + $DATA_SIZE + $SDCARD_SIZE + $RECOVERY_SIZE + $SYSTEM_SIZE + $CACHE_SIZE + $PANIC_SIZE + $CONFIG_SIZE + $ILOG_SIZE + $NDA_OFFSET_SIZE `
# Since Media has all of the left-over...
SDCARD_SIZE=`expr $device_size - $fixed_part_total`
EXTEND_START_CYL=`expr $SDCARD_SIZE \* $MILLION / $CYL_SIZE_BYTES + $SDCARD_START_CYL + 2`

# Format the device
fdisk $device <<! > /dev/null
o
n
p
1
${FACTORY_START_CYL}
+${FACTORY_SIZE}M
n
p
2
${DATA_START_CYL}
+${DATA_SIZE}M
n
p
3
${SDCARD_START_CYL}
+${SDCARD_SIZE}M
n
e
${EXTEND_START_CYL}

n

+${RECOVERY_SIZE}M
n

+${SYSTEM_SIZE}M
n

+${CACHE_SIZE}M
n

+${CONFIG_SIZE}M
n

+${PANIC_SIZE}M
n


w
!
rc=$?
if [ $rc -ne 0 ]; then
    exit $rc
fi

echo; echo
echo New partition layout:
echo p | fdisk $device

echo; echo
echo "Erasing the superblock on all partitions of ${device}"
for i in ${device}?*
do
    case "$i" in
    *4 )
        # Don't touch the extended partition
        ;;
    *8 )
        # Don't touch the configuration partition
        ;;
    *)
        #dd if=/dev/zero of=$i bs=1024k count=1 >/dev/null  2>&1
        ;;
    esac
done

exit 0
