CFLAGS = -m32

all: update_osip ifwi_version_check

ifwi_version_check :
	gcc $(CFLAGS) -o ifwi_version_check ifwi_version_check.c

update_osip : manage_device.o osip_utils.o update_osip.o
	gcc $(CFLAGS) -o update_osip manage_device.o osip_utils.o update_osip.o

manage_device.o: manage_device.c manage_device.h Makefile
	gcc $(CFLAGS) -c manage_device.c

update_osip.o: update_osip.c osip.h manage_device.h Makefile
	gcc $(CFLAGS) -c update_osip.c

osip_utils.o: osip_utils.c osip.h manage_device.h Makefile
	gcc $(CFLAGS) -c osip_utils.c

clean:
	rm -rf *.o *~

clobber: clean
	rm -f update_osip ifwi_version_check
