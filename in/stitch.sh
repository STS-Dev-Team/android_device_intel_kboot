#!/bin/bash
# $1 is cmdline path
# $2 is bootstub path
# $3 is bzImage path
# $4 is initrd path
# $5 is spi uart disable flag (0: default, 1: disable)
# $6 is spi controller (0: SPI0(Moorestown) 1: SPI1(Medfield))
# $7 is output image

if [ $# -lt 7 ]; then
	echo "usage: nand.sh cmdline_path bootstub_path bzImage_path initrd_path spi_suppress_flag(0: output, 1: suppress) spi_type(0: Moorestown 1:Medfield) output_image"
	exit 1
fi

if [ ! -e "$1" ]; then
	echo "cmdline file not exist!"
	exit 1
fi

if [ ! -e "$2" ]; then
	echo "bootstub file not exist!"
	exit 1
fi

if [ ! -e "$3" ]; then
	echo "no kernel bzImage file!"
	exit 1
fi

if [ ! -e "$4" ]; then
	echo "no initrd file!"
	exit 1
fi

# convert a decimal number to the sequence that printf could recognize to output binary integer (not ASCII)
binstring ()
{
	h1=$(($1%256))
	h2=$((($1/256)%256))
	h3=$((($1/256/256)%256))
	h4=$((($1/256/256/256)%256))
	binstr=`printf "\x5cx%02x\x5cx%02x\x5cx%02x\x5cx%02x" $h1 $h2 $h3 $h4`
}

# add cmdline to the first part of boot image
cat $1 /dev/zero | dd of=$7 bs=4096 count=1

# append bootstub
cat $2 /dev/zero | dd of=$7 bs=4096 count=1 seek=1

# append bzImage and initrd 
cat $3 $4 | dd of=$7 bs=4096 seek=2

# fill bzImage_size and initrd_size
binstring `stat -L -c %s $3`
printf $binstr | dd of=$7 bs=1 seek=1024 conv=notrunc
binstring `stat -L -c %s $4`
printf $binstr | dd of=$7 bs=1 seek=1028 conv=notrunc
binstring "$5"
printf $binstr | dd of=$7 bs=1 seek=1032 conv=notrunc
binstring "$6"
printf $binstr | dd of=$7 bs=1 seek=1036 conv=notrunc

# done
echo 'Image stitch done'
exit 0
