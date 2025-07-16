gcc driver.c -shared -nostdlib -e DriverEntry -lntoskrnl -o driver.sys -Wl,--subsystem,native
MSYS2_ARG_CONV_EXCL="/set" bcdedit /set testsigning off
MSYS2_ARG_CONV_EXCL="/set" bcdedit /set nointegritychecks on
MSYS2_ARG_CONV_EXCL="/set" bcdedit /set onetimeadvancedoptions on
sc create HelloWorldDriver binPath="$(cygpath -wa ./driver.sys)" type= kernel start= demand error= normal DisplayName= "Hello World Kernel Driver"
sc start HelloWorldDriver
