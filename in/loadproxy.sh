#!/bin/sh
echo "========================================"
echo "   Load AT proxy "
echo "========================================"

# Start Proxy with no monitoring IFX port with tunneling on, by default.
tty_monitor="none"
tty_pc="/dev/ttyGS0"
tty_modem="/dev/ttyIFX0"
tunnel_switch="on"

while getopts d:t:m:p: opt
do
	case "${opt}" in
	t)
		tunnel_switch="${OPTARG}"
		;;
	p)
		tty_pc="${OPTARG}"
		;;
	m)
		tty_modem=${OPTARG}
		;;
	d)
		tty_monitor=${OPTARG}
		;;
	esac
done

#Load HSI drivers
insmod /lib/modules/intel_mid_hsi.ko
insmod /lib/modules/hsi_ffl_tty.ko

#Check to see if the HSI node was created indicating that the HSI bus
#is enabled.
if [ -e /sys/bus/hsi/devices/port0 ]; then
    hsi_opt='-p /dev/ttyIFX0'
else
#if no HSI bus load the SPI Modem driver
    echo "no HSI node"
	exit 0
fi

# Reset the modem
echo 0 > /sys/class/gpio/gpio174/value
sleep 1
echo 1 > /sys/class/gpio/gpio174/value
sleep 4

echo proxy -d $tty_monitor -m $tty_modem -p $tty_pc -t $tunnel_switch
proxy -d $tty_monitor -m $tty_modem -p $tty_pc -t $tunnel_switch &

exit 0
