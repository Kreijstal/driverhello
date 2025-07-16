# MinGW-w64 Makefile for driver project with loader
CC = gcc
LD = ld
WINDRES = windres
CFLAGS = -nostdlib -fno-builtin -fno-stack-protector
LDFLAGS = --subsystem native -lntoskrnl
LOADER_LIBS = -ladvapi32

all: driver.sys loader.exe

driver.sys: driver.c
	$(CC) -c $(CFLAGS) -o driver.o $<
	$(LD) -o $@ driver.o $(LDFLAGS)

loader.exe: loader.o resource.o
	$(CC) -o $@ $^ $(LOADER_LIBS)

loader.o: loader.c
	$(CC) -c -o $@ $<

resource.o: resource.rc driver.sys
	$(WINDRES) -i $< -o $@

clean:
	rm -f *.o *.sys *.exe

.PHONY: all clean