CC =  ../../../../prebuilt/linux-x86/toolchain/i686-linux-glibc2.7-4.4.3/bin/i686-linux-gcc
CFLAGS = -m32
LDFLAGS = -m32

all:		ifwi-update

ifwi-update:	ifwi-update.o
	$(CC) $(CFLAGS) -o ifwi-update ifwi-update.o

ifwi-update.o:	ifwi-update.c
	$(CC) $(LDFLAGS) -c ifwi-update.c

clean:
	rm -f ifwi-update ifwi-update.o
