#!/bin/bash
# kboot build for android


if [ $# -lt 6 ]; then
    echo "Usage: $0 bzImage recovery version out silicon chaabi_dir"
    echo "bzImage = binary kernel to include"
    echo "recovery = binary recovery program to include"
    echo "version = version number to display at boot time"
    echo "out = name of image to create"
    echo "silicon type = 0: Moorestown 1: Medfield"
    echo "chaabi_dir = Chaabi directory"
    echo "module_dir = kernel module directory"
    echo "The rest of the files are in /in"
    exit 1
fi

BZIMAGE=$1
RECOVERY=$2
VERSION=$3
OUT=$4
SILICON=$5
CHAABI_DIR="${6}"
CMFWLD_APP_DIR=$7
MODULE_DIR=$8
ATPROXY_DIR=$9
WATCHDOG_DIR=${10}

echo "watchdog is ${WATCHDOG_DIR}"

if [ ! -e ${BZIMAGE} ]; then
    echo "error: bzImage file ${BZIMAGE} does not exist"
    exit 1
fi

if [ ! -e ${RECOVERY} ]; then
    echo "error: recovery file ${RECOVERY} does not exist"
    exit 1
fi

if [ -e "${OUT}" ]; then
    echo "error: ${OUT} exists"
    exit 1
fi

if [[   ! -e in/AutoBoot.sh ||
    ! -e in/cmdline ||
    ! -e in/kboot-init ||
    ! -e in/initrd-base.gz ||
    ! -e in/bootstub ]]; then
    echo "/in directory needs to contain the following files:"
    echo "AutoBoot.sh, cmdline, kboot-init, initrd-base.gz, bootstub"
    exit 1
fi

echo "Creating image \`${OUT}'for FSTK stitching..."

# expand base initrd
rm -rf tmp
mkdir tmp
cd tmp
gzip -dc ../in/initrd-base.gz | cpio -id --quiet
mkdir lib/modules
cd ..

# update initrd files
mkdir tmp/recovery
cp -f ${RECOVERY} tmp/recovery/
cp -f in/AutoBoot.sh tmp/sbin/
cp -f in/PartitionDisk.sh tmp/sbin/
cp -f in/kboot-init tmp/sbin/
# add the Modem FW Flashing tool
cp -f $CMFWLD_APP_DIR/build/cmfwdl-app tmp/bin/
cp -f in/proxy tmp/sbin/
cp -f in/loadfw_modem.sh tmp/sbin/
cp -f in/loadproxy.sh tmp/sbin/
cp -f $MODULE_DIR/ifx6x60.ko tmp/lib/modules/
cp -f $MODULE_DIR/intel_mid_hsi.ko tmp/lib/modules/
cp -f $MODULE_DIR/hsi_ffl_tty.ko tmp/lib/modules/

# Add in firmware updater
if [ -f ../../../device/intel/kboot/firmware_loader/ifwi-update ]; then
    cp ../../../device/intel/kboot/firmware_loader/ifwi-update tmp/sbin/
fi
# Add watchdog daemon
cp -f ${WATCHDOG_DIR}/watchdogd tmp/sbin/watchdogd

# Add in osip updater
cp -f flash_stitched/update_osip tmp/sbin/
cp -f flash_stitched/ifwi_version_check tmp/sbin/
cp -f in/invalidate_osip.sh tmp/sbin/invalidate_osip
cp -f in/restore_osip.sh tmp/sbin/restore_osip
cp -f in/flash_stitched.sh tmp/sbin/flash_stitched
cp -f in/umip.sh tmp/sbin/umip

# Add Chaabi MW files.
if [[ "1" == "${SILICON}" ]]
then
  # Building for Medfield so copy the Chaabi MW files.

  KBOOT_IMAGE_ROOT_DIR="tmp"

  if [[ ! -d ${CHAABI_DIR} ]]
  then
    # The Chaabi MW directory does not exist!
    echo "ERROR - The Chaabi MW root directory ${CHAABI_DIR} does not exist." >&2
    echo "Cannot copy Chaabi MW files to kboot image directory." >&2
  else
    # Copy Chaabi MW files to kboot image.
    CHAABI_APP_DIR="${CHAABI_DIR}/App"
    CHAABI_DXSEPREPORT_DIR="${CHAABI_APP_DIR}/DxHostPrintfDaemon"

    CHAABI_PROGRAM_DEST_DIR="${KBOOT_IMAGE_ROOT_DIR}/${CHAABI_DIR##*/}"
    CHAABI_DXSEPREQUEST_CONF_DEST_DIR="${KBOOT_IMAGE_ROOT_DIR}/etc"
    CHAABI_DXSEPREQUEST_DATA_DIR="${KBOOT_IMAGE_ROOT_DIR}/data"

    if [[ ! -d ${CHAABI_PROGRAM_DEST_DIR} ]]
    then
      # Create the directory for the Chaabi MW files.
      mkdir --parents ${CHAABI_PROGRAM_DEST_DIR}
    fi

    if [[ ! -d ${CHAABI_DXSEPREQUEST_DATA_DIR} ]]
    then
      # Create the data directory for the DxSepRequest daemon.
      mkdir --parents ${CHAABI_DXSEPREQUEST_DATA_DIR}
    fi

    # Copy the Chaabi MW .out programs.
    cp --force ${CHAABI_APP_DIR}/*.out ${CHAABI_PROGRAM_DEST_DIR}
    cp --force ${CHAABI_DXSEPREPORT_DIR}/dxseprequest.out ${CHAABI_PROGRAM_DEST_DIR}

    # Copy the Chaabi MW DxSepRequest configuration file.
    cp --force ${CHAABI_DXSEPREPORT_DIR}/dxseprequest.conf ${CHAABI_DXSEPREQUEST_CONF_DEST_DIR}
  fi
fi

# update  version number
sed -i "s/KBOOT_VERSION_PLACEHOLDER/${VERSION}/g" tmp/sbin/kboot-init

# recompress initrd
cd tmp
find ./ | cpio -H newc -o --quiet > ../initrd-new
cd ..
gzip -f initrd-new

./in/stitch.sh in/cmdline in/bootstub ${BZIMAGE} initrd-new.gz 0 ${SILICON} ${OUT}

if [ "0" -ne "$?" ]; then
    echo "error running stitch.sh"
    exit 1
fi

# clean up
rm -rf tmp
rm -f initrd-new.gz

echo "Done."
