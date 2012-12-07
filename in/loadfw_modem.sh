#!/bin/sh
echo "========================================"
echo "   Update Modem Firmware "
echo "========================================"
usage="Usage: $0 [-r <XXXXcert.bin>] [-l] [-e] [-h | -i <filename>] [-f] <firmware>"
#
# The <firmware> file is required.  It is used for the boot option
# when any of the following optional arguments are used.
#
# Optional arguments:
# -f <firmware> 	Write the firmware in the file into them modem flash.
# -r <XXXCert.bin> 	write and read back and print out R&D Certificate from XXXCert.bin file
# -h  			print modem HW ID
# -i <filename>		store the modem HW ID to <filename>
# -e 			erase R&D Certificate.
# -l Enable trace

while getopts ":r:lehi:f:" opt; do
    case $opt in
    r ) cert_file=$OPTARG
        rd_cert="-r";;
    e ) erase_rd="--erase-rd" ;;
    h )
        hardware_id="-h"    ;;
    i )
        # Save the hardware ID to a file
        echo "optarg=$OPTARG"
        if [ ! -z $OPTARG ]; then
            hardware_id="--hwid"
            hwid_file=$OPTARG
        else
            hardware_id="-h"
        fi
    ;;

    f ) fw_file=$OPTARG
        flash_file="-f $OPTARG"
    ;;

    l ) en_trace="-l" ;;

    \? ) echo $usage
        exit 1;;
    esac
done

shift $(($OPTIND - 1))

# The modem firmware file
if [  ! -z $1 ]; then
    fw_file="$1"
fi

if [ ! -e $fw_file ]; then
    echo "Error: $fw_file not found"
    echo $usage
    exit 1
fi

#load the HSI drivers for accessing the modem
if [ -e /lib/modules/intel_mid_hsi.ko ] && [ ! -e /sys/module/intel_mid_hsi ]; then
    insmod /lib/modules/intel_mid_hsi.ko
fi
if [ -e /lib/modules/hsi_ffl_tty.ko ] && [ ! -e /sys/module/hsi_ffl_tty ]; then
    insmod /lib/modules/hsi_ffl_tty.ko
fi

#
# Check to see if the HSI node was created indicating that the HSI bus
# is enabled.
#
if [ -e /sys/bus/hsi/devices/port0 ]; then
    hsi_opt='-p /dev/ttyIFX0'
else
    echo "Error: Modem flashing cannot proceed without the HSI driver"
    exit 1
fi

if [ ! -z $cert_file ] || [ ! -z $erase_rd ] || [ ! -z $hardware_id ] || [ ! -z $flash_file ]; then
    cmfwdl-app -t /dev/ttyMFD1 $hsi_opt $en_trace -b $fw_file $erase_rd $hardware_id $hwid_file $rd_cert $cert_file $flash_file
else
    cmfwdl-app -t /dev/ttyMFD1 $hsi_opt $en_trace -b $fw_file -f $fw_file
fi

exit $?
