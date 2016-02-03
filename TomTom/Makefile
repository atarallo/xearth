# simple Makefile for TTxearth (c) Markus Hoffmann 2009-02-21
#

ARMCC	= arm-linux-gcc
CC      = gcc
CFLAGS	= -O2 $(DEFINES)
LDFLAGS = 

PROG	= xearth
SRCS	= ../xearth.c ../dither.c ../extarr.c ../gif.c ../gifout.c ../mapdata.c \
	  ../markers.c ../ppm.c ../render.c ../resources.c ../scan.c ../sunpos.c ../x11.c \
	  ../framebuffer.c
OBJS	= xearth.o dither.o extarr.o gif.o gifout.o mapdata.o \
	  markers.o ppm.o render.o resources.o scan.o sunpos.o x11.o \
	  framebuffer.o
LIBS	= -lm

HSRC    = ../framebuffer.h ../xearth.h ../port.h  ../gifint.h ../giflib.h ../extarr.h

DIST	= README.TomTom RELEASE_NOTES INSTALL HISTORY ../BUILT-IN Makefile \
	  ../xearth.man xearth.bmp xearth.cap xearth-wrapper $(SRCS) $(HSRC)
DIR=TTxearth-1.01
DIR2=$(DIR)-bin
BINDIST= README.TomTom RELEASE_NOTES xearth xearth.bmp xearth.cap xearth-wrapper


all:	$(PROG) $(PROG)-l

xearth:	$(SRCS)
	$(ARMCC) -DFRAMEBUFFER -I.. -o $@ $(LDFLAGS) $(SRCS) $(LIBS)
xearth-l: $(SRCS)
	$(CC) -DFRAMEBUFFER -I.. -o $@ $(LDFLAGS) $(SRCS) $(LIBS)

clean:
	/bin/rm -f $(PROG) xearth-l $(OBJS)

bindist : $(DIR2).zip

$(DIR2).zip : $(BINDIST)
	rm -rf /tmp/$(DIR2)
	mkdir /tmp/$(DIR2)
	mkdir /tmp/$(DIR2)/bin
	mkdir /tmp/$(DIR2)/SDKRegistry
	rsync -a README /tmp/$(DIR2)/
	cp RELEASE_NOTES /tmp/$(DIR2)/
	rsync -a xearth /tmp/$(DIR2)/bin/
	rsync -a xearth-wrapper /tmp/$(DIR2)/bin/
	rsync -a xearth.bmp /tmp/$(DIR2)/SDKRegistry/
	rsync -a xearth.cap /tmp/$(DIR2)/SDKRegistry/
	(cd /tmp ; zip -r $(DIR2).zip $(DIR2))
	mv /tmp/$(DIR2).zip .	

dist :	$(DIR).tar.gz
$(DIR).tar.gz : $(DIST)
	rm -rf /tmp/$(DIR)
	mkdir /tmp/$(DIR)
	(tar cf - $(DIST))|(cd /tmp/$(DIR); tar xpf -)
	(cd /tmp; tar cf - $(DIR)|gzip -9 > $(DIR).tar.gz)
	mv /tmp/$(DIR).tar.gz .
