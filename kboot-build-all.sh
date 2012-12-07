#!/bin/bash

function gettop()
{
    local TOPFILE=./.repo/manifest.xml
    if [ -n "$TOP" -a -f "$TOP/$TOPFILE" ] ; then
        echo $TOP
    else
        if [ -f $TOPFILE ] ; then
            echo $PWD
        else
            echo $ANDROID_BUILD_TOP
        fi
    fi
}

init_variables() {
    TOP=$(gettop)
    if [[ -z $TOP ]];
        then TOP=$PWD
    fi
    echo "TOP: $TOP"
    JOBS=`grep -c processor /proc/cpuinfo`
    VERSION=`date "+%Y.%m%d.%H%M"`
    DEVICES="ivydale mrst_ref mrst_edv crossroads mfld_cdk mfld_pr1 mfld_pr2 sc1"

    KERNEL_DIR=$TOP/hardware/intel/linux-2.6
    CONFIG_DIR=$KERNEL_DIR/arch/x86/configs

    ABOOT_DIR=$TOP/device/intel/aboot
    KBOOT_DIR=$TOP/device/intel/kboot
    ATPROXY_DIR=$TOP/device/intel/telephony_tools/at_proxy/native_app
    LOADFW_DIR=$TOP/device/intel/kboot/firmware_loader
    FLASH_DIR=$KBOOT_DIR/flash_stitched

    # Root of the watchdog daemon
    WATCHDOG_DIR="$TOP/hardware/intel/ia_watchdog/watchdog_daemon"

    # Root directory for the Chaabi files.
    CHAABI_DIR="$TOP/hardware/intel/PRIVATE/chaabi"
    # Root directory for Modem Flashing Tool app and (static) library
    CMFWDL_APP_DIR=$TOP/hardware/intel/cmfwdl/reference_application
    CMFWDL_LIB_DIR=$TOP/hardware/intel/PRIVATE/cmfwdl/lib

    if [ ! -z $ANDROID_PRODUCT_OUT ]; then
        KBOOT_IMAGE_NAME="${ANDROID_PRODUCT_OUT}/kboot.bin"
        KBOOT_IMAGE="${ANDROID_PRODUCT_OUT}/kboot.unsigned"
    fi
}

exit_on_error () {
    if [ "$1" -ne 0 ]; then
    echo "$_pgmname: Error: $2"
    exit $1
    fi
}

build_modem_flash_tool() {
    device=$1
    cd $CMFWDL_LIB_DIR
    echo; echo "kboot ${device}: Building Modem Flashing tool (cmfwdl-app)"
    echo "kboot ${device}: Building Modem Flashing tool (cmfwdl-app)" >> $LOGFILE 2>&1
    make clean >> $LOGFILE 2>&1
    make >> $LOGFILE 2>&1
    cd $CMFWDL_APP_DIR
    make clean >> $LOGFILE 2>&1
    make >> $LOGFILE 2>&1
}

build_flash_stitched() {
    device=$1
    pushd $FLASH_DIR
    echo; echo "kboot ${device}: Building flash_stitched"
    echo "kboot ${device}: Building flash_stitched" >> $LOGFILE 2>&1
    make clobber >> $LOGFILE 2>&1
    make >> $LOGFILE 2>&1
    popd
}

clean_flash_stitched() {
    pushd $FLASH_DIR
    make clobber >> $LOGFILE 2>&1
    popd
}

build_at_proxy() {
    device=$1
    cd $ATPROXY_DIR
    echo; echo "kboot ${device}: Building Atproxy"
    make clean >> $LOGFILE 2>&1
    make >> $LOGFILE 2>&1
    cp --force proxy $KBOOT_DIR/in/
    make clean >> $LOGFILE 2>&1
}

build_loadfw() {
    device=$1
    pushd $LOADFW_DIR
    echo; echo "kboot ${device}: Building loadfw"
    make clean >> $LOGFILE 2>&1
    make >> $LOGFILE 2>&1
    popd
}

clean_loadfw() {
    pushd $LOADFW_DIR
    make clean >> $LOGFILE 2>&1
    popd
}

build_aboot() {
    device=$1
    echo "kboot ${device}: Building aboot"
    cd $ABOOT_DIR
    make clean >> $LOGFILE 2>&1
    exit_on_error $? "Unable to clean aboot directory for $device"
    ABOOT_OUT=$TOP/out/target/product/$device
    if [ ! -e $ABOOT_OUT ]; then
        mkdir -p $ABOOT_OUT
    fi
    ./build.sh -p $device -o $ABOOT_OUT >> $LOGFILE 2>&1
    exit_on_error $? "Unable to build aboot for $device"
}

build_kboot() {
    device=$1
    if [ "$device" == "mfld_cdk" ] || [ "$device" == "mfld_pr1" ] || [ "$device" == "mfld_pr2" ] || [ "$device" == "sc1" ] ;then
	    SILICON=1;
    else
	    SILICON=0;
    fi

    if [ -z $KBOOT_IMAGE_NAME ]; then
        KBOOT_IMAGE_NAME="${TOP}/kboot.$device.$VERSION.bin"
    fi
    if [ -z $KBOOT_IMAGE ]; then
        KBOOT_IMAGE="${TOP}/kboot.$device.$VERSION.unsigned"
    fi
    DIFFCONFIG_KBOOT=$TOP/vendor/intel/${device}/kboot_diffconfig
    DIFFCONFIG_BASE=$KBOOT_DIR/kernel/kboot_diffconfig
    DIFFCONFIG_NO_VIDEO=$KBOOT_DIR/kernel/novideo_diffconfig
    DIFFCONFIG_DEVICE=$KBOOT_DIR/kernel/kboot_${device}_diffconfig
    BZIMAGE=$TOP/out/target/product/$device/kboot/kernel_build/arch/i386/boot/bzImage
    MODULE_DIR=$TOP/out/target/product/$device/kboot/root/lib/modules

    echo "kboot ${device}: Generating diffconfig file"
    {
        cat $DIFFCONFIG_BASE

        if [ -f "$DIFFCONFIG_DEVICE" ]; then
            cat $DIFFCONFIG_DEVICE
	fi
        if [ "$NO_VIDEO" ]; then
            cat $DIFFCONFIG_NO_VIDEO
        fi

        echo "CONFIG_LOCALVERSION=\"-kboot.$VERSION.$device"\"
    } > $DIFFCONFIG_KBOOT

    # We would prefer to take TARGET_TOOLS_PREFIX from Android (build/core/combo/TARGET_linux-x86.mk)
    # For now, we'll just set it here since this isn't called from an android make (yet).
    export TARGET_TOOLS_PREFIX=$TOP/prebuilt/linux-x86/toolchain/i686-android-linux-4.4.3/bin/i686-android-linux-

    echo "kboot ${device}: Building kernel"
    pushd $TOP
    DIFFCONFIGS=kboot $TOP/vendor/intel/support/kernel-build.sh -C -c ${device} -K >> $LOGFILE 2>&1
    exit_on_error $? "Unable to build kboot kernel for $device"
    popd

    # Build the unsigned kboot image
    echo "kboot ${device}: Creating unsigned image"
    pushd $KBOOT_DIR
    rm -f $KBOOT_IMAGE
    ./kboot-build.sh $BZIMAGE ${ABOOT_OUT}/recovery.tar.gz "kboot.$device.$VERSION.bin" $KBOOT_IMAGE $SILICON ${CHAABI_DIR} $CMFWDL_APP_DIR $MODULE_DIR $ATPROXY_DIR $WATCHDOG_DIR >> $LOGFILE 2>&1
    exit_on_error $? "Unable to build unsigned image for $device"
    popd

    # Stitch unsigned kboot with FSTK.
    readlink -e $TOP/device/intel/PRIVATE/xfstk-stitcher
    if [ $? -eq 0 ]; then
        $TOP/device/intel/PRIVATE/xfstk-stitcher/gen-os.sh --input $KBOOT_IMAGE --output $KBOOT_IMAGE_NAME --xml POS.XML
    else
        $TOP/device/intel/PRIVATE/lfstk/gen-os.sh $KBOOT_IMAGE $KBOOT_IMAGE_NAME POS.XML
    fi
}

clean_kboot() {
    device=$1
    rm $TOP/device/intel/kboot/in/cmdline
    rm $TOP/vendor/intel/${device}/kboot_diffconfig
}

#---------------------------------------------------------------------
# Build Watchdog Daemon.
#---------------------------------------------------------------------
build_watchdog()
{
  device="${1}"
  WATCHDOG_BUILD_NAME="Watchdog Daemon"
  WATCHDOG_MAKE="make"

  echo "kboot ${device}: Building ${WATCHDOG_BUILD_NAME}"

  echo "" >> $LOGFILE 2>&1
  echo "Building ${WATCHDOG_BUILD_NAME} for ${device} kboot" >> $LOGFILE 2>&1
  echo "" >> $LOGFILE 2>&1

  # Build the Watchdog Daemon.
  cd ${WATCHDOG_DIR}

  ${WATCHDOG_MAKE} clean >> $LOGFILE 2>&1

  ${WATCHDOG_MAKE} >> $LOGFILE 2>&1

  echo "" >> $LOGFILE 2>&1
  echo "Done building ${WATCHDOG_BUILD_NAME} for ${device} kboot" >> $LOGFILE 2>&1
  echo "" >> $LOGFILE 2>&1
}

#---------------------------------------------------------------------
# Build Chaabi middleware.
#---------------------------------------------------------------------
build_chaabi_mw()
{
  device="${1}"

  if [ "mfld_cdk" != "${device}" ]  && [ "mfld_pr2" != "${device}" ] && [ "sc1" != "${device}" ]

  then
    # Don't build Chaabi MW for non-Medfield targets.
    return
  fi

  CHAABI_BUILD_NAME="Chaabi MW"

  # Chaabi paths.
  CHAABI_LOCAL_LIB_DIR="${CHAABI_DIR}/Lib"
  CHAABI_APP_DIR="${CHAABI_DIR}/App"
  CHAABI_LIB_DIR="${CHAABI_DIR}/workspace/linux/LIB"

  # Use all of the available processors when using make.
  if [[ -f /proc/cpuinfo ]]
  then
    CHAABI_MAKE="make -j $(command grep --count processor /proc/cpuinfo)"
  else
    CHAABI_MAKE="make"
  fi


  echo "kboot ${device}: Building ${CHAABI_BUILD_NAME}"

  echo "" >> $LOGFILE 2>&1
  echo "Building ${CHAABI_BUILD_NAME} for ${device} kboot" >> $LOGFILE 2>&1
  echo "" >> $LOGFILE 2>&1

  # Build the Discretix libraries.
  cd ${CHAABI_LIB_DIR}

  ${CHAABI_MAKE} clean >> $LOGFILE 2>&1

  ${CHAABI_MAKE} >> $LOGFILE 2>&1

  ${CHAABI_MAKE} -f EXTAPPMakefile >> $LOGFILE 2>&1


  # Build the local chaabi library directory.
  cd ${CHAABI_LOCAL_LIB_DIR}

  ${CHAABI_MAKE} clean >> $LOGFILE 2>&1

  ${CHAABI_MAKE} >> $LOGFILE 2>&1

  # Build the secureIPC_ATP.out program.
  cd ${CHAABI_APP_DIR}

  ${CHAABI_MAKE} clean >> $LOGFILE 2>&1

  ${CHAABI_MAKE} >> $LOGFILE 2>&1


  echo "" >> $LOGFILE 2>&1
  echo "Done building ${CHAABI_BUILD_NAME} for ${device} kboot" >> $LOGFILE 2>&1
  echo "" >> $LOGFILE 2>&1
} # end build_chaabi_mw()


usage() {
    echo >&2 "`basename $0` [-h] [-V] [-v <version>] [-l <logfile>] [-j <jobs>] [ <device> ...]"
    echo >&2 "   -h                    help message"
    echo >&2 "   -v <version name>     version/name for the kboot file and message [$VERSION]"
    echo >&2 "   -l <logfile>          absolute path for the logfile. [$_pgmname.$VERSION.log]"
    echo >&2 "   -j <jobs>             makefile -j factor [$JOBS]"
    echo >&2 "   <device>...           Devices to build for [$DEVICES]"
}

main() {
    _pgmname=`basename $0`
    device="${1}"
    init_variables

    while getopts Vv:hj:l: opt
    do
        case "${opt}" in
        V)
            NO_VIDEO=1
            ;;
        v)
            VERSION="${OPTARG}"
            ;;
        l)
            LOGFILE="${OPTARG}"
            ;;
        h)
            usage
            exit 0
            ;;
        j)
            JOBS=${OPTARG}
            ;;
        ?)
            echo "$_pgmname: Unknown option"
            usage
            exit 0
           ;;
        esac
    done

    if [ -z "$LOGFILE" ]; then
        # Set the log to stout if nothing is specified
        LOGFILE=/dev/stdout
    fi

    shift $(($OPTIND - 1))
    if [ $# -ne 0 ]; then
        DEVICES="$*"
    fi

    if [ "ctp_pr0" == "${device}" ] ; then
	# Jerome Durand: for ctp there a 2 kboots: 1 for pr0, 1 for vv
	# the defauld one (kboot.bin) is for vv
	cp $KBOOT_DIR/in/kboot_ctp_vv.bin $KBOOT_IMAGE_NAME
	cp $KBOOT_DIR/in/kboot_ctp_pr0.bin ${ANDROID_PRODUCT_OUT}/kboot_pr0.bin
    elif [ "ctp_pr1" == "${device}" ] ; then
	cp $KBOOT_DIR/in/kboot_ctp_pr1.bin $KBOOT_IMAGE_NAME
    elif [ "mfld_gi" == "${device}" ] ; then
        cp $KBOOT_DIR/in/kboot_gi.bin $KBOOT_IMAGE_NAME
    elif [ "mfld_dv10" == "${device}" ] ; then
        cp $KBOOT_DIR/in/kboot_dv10.bin $KBOOT_IMAGE_NAME
    else
	cp $KBOOT_DIR/in/kboot.bin $KBOOT_IMAGE_NAME
    fi
#    do
#        cp $KBOOT_DIR/in/cmdline_$dev $KBOOT_DIR/in/cmdline
#	build_modem_flash_tool $dev
#        build_at_proxy $dev
#	build_watchdog $dev
#        build_loadfw $dev
#        build_chaabi_mw ${dev}
#        build_flash_stitched $dev
#        build_aboot ${dev}
#        build_kboot ${dev}
#
#        clean_loadfw ${dev}
#        clean_flash_stitched ${dev}
#        clean_kboot ${dev}
#    done
}

main $*
