
CFLAGS = -m32

all: loadfw

loadfw:	loadfw.c
	gcc $(CFLAGS) -o loadfw loadfw.c

clean:
	rm -rf *.o ~*

clobber: clean
	rm -f loadfw
