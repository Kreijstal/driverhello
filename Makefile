# MinGW-w64 Makefile for driver project with DDK headers
CC = gcc
LD = ld
WINDRES = windres
CFLAGS = -nostdlib -fno-builtin -fno-stack-protector -I$${MINGW_PREFIX:-/ucrt64}/include/ddk -Wno-macro-redefined -Wno-redefined
LDFLAGS = -Wl,--subsystem,native -Wl,--entry,DriverEntry -shared -nodefaultlib
LDFLAGS = --subsystem native -lntoskrnl
LOADER_LIBS = -ladvapi32

all: signed-driver.sys loader.exe

cert.pem key.pem:
	MSYS2_ARG_CONV_EXCL="*" openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes -subj "/CN=Test Driver Signing"

signed-driver.sys: driver.sys cert.pem key.pem
	osslsigncode sign -certs cert.pem -key key.pem -n "Test Driver" -i http://example.com -in $< -out $@

driver.sys: driver.c
	$(CC) -c $(CFLAGS) -o driver.o $<
	$(LD) -o $@ driver.o $(LDFLAGS)

loader.exe: loader.o resource.o
	$(CC) -o $@ $^ $(LOADER_LIBS)

loader.o: loader.c
	$(CC) -c -o $@ $<

resource.o: resource.rc signed-driver.sys
	$(WINDRES) -i $< -o $@

clean:
	rm -f *.o *.sys *.exe

.PHONY: all clean