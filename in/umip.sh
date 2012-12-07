#!/bin/sh

a_timeouts() {
cat /sys/devices/platform/intel_mid_umip/available_timeouts
return 0;
}

r_timeout() {
cat /sys/devices/platform/intel_mid_umip/current_timeout
return 0;
}

if [[ $1 == 'w' && $# == 2 ]]
        then
        echo $2 | tr -d "\n" > $2
        echo $2 > /sys/devices/platform/intel_mid_umip/current_timeout
        else
                case $1 in
                'a') a_timeouts;;
                'r') r_timeout;;
                *)  echo "Invalid Argument" ;;
                esac
fi

