gcc driver.c -shared -nostdlib -e DriverEntry -lntoskrnl -o driver.sys -Wl,--subsystem,native
sc create HelloWorldDriver binPath= ./driver.sys type= kernel start= demand error= normal DisplayName= "Hello World Kernel Driver"
sc start HelloWorldDriver
