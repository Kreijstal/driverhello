#include <windows.h>
#include <stdio.h>

#define DRIVER_NAME "HelloWorld"
#define DRIVER_RESOURCE_ID 101
#define IOCTL_HELLO CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

BOOL ExtractDriver(const char* path) {
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(DRIVER_RESOURCE_ID), RT_RCDATA);
    if (!hRes) return FALSE;
    
    DWORD size = SizeofResource(NULL, hRes);
    void* data = LockResource(LoadResource(NULL, hRes));
    
    HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    DWORD written;
    BOOL result = WriteFile(hFile, data, size, &written, NULL);
    CloseHandle(hFile);
    return result && (written == size);
}

BOOL ManageDriverService(const char* serviceName, const char* driverPath, BOOL install) {
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return FALSE;

    BOOL success = FALSE;
    SC_HANDLE service = NULL;

    if (install) {
        service = CreateService(scm, serviceName, serviceName,
            SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
            driverPath, NULL, NULL, NULL, NULL, NULL);
        
        if (!service && GetLastError() == ERROR_SERVICE_EXISTS) {
            service = OpenService(scm, serviceName, SERVICE_ALL_ACCESS);
        }

        if (service) {
            success = StartService(service, 0, NULL) ||
                     GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
            CloseServiceHandle(service);
        }
    } else {
        // UNLOAD LOGIC
        service = OpenService(scm, serviceName, SERVICE_STOP | DELETE);
        if (service) {
            SERVICE_STATUS status;
            printf("Stopping service...\n");
            if (ControlService(service, SERVICE_CONTROL_STOP, &status)) {
                printf("Service stopped successfully.\n");
                printf("Deleting service...\n");
                if (DeleteService(service)) {
                    printf("Service deleted successfully.\n");
                    success = TRUE;
                } else {
                    printf("Failed to delete service (Error: %d)\n", GetLastError());
                }
            } else {
                printf("Failed to stop service (Error: %d)\n", GetLastError());
            }
            CloseServiceHandle(service);
        } else {
             printf("Failed to open service for unload (Error: %d)\n", GetLastError());
        }
    }

    CloseServiceHandle(scm);
    return success;
}

void CommunicateWithDriver() {
    // Wait briefly for driver to initialize
    Sleep(1000);
    
    HANDLE hDevice = CreateFileA("\\\\.\\HelloWorld",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hDevice == INVALID_HANDLE_VALUE) {
        printf("Failed to open device (Error: %d)\n", GetLastError());
        return;
    }

    char input[] = "Hello from user mode!";
    char output[256] = {0};
    DWORD bytesReturned = 0;

    if (DeviceIoControl(hDevice, IOCTL_HELLO,
                       input, sizeof(input),
                       output, sizeof(output),
                       &bytesReturned, NULL)) {
        printf("Driver responded: %s\n", output);
    } else {
        printf("IOCTL failed (Error: %d)\n", GetLastError());
    }

    CloseHandle(hDevice);
}

int main() {
    char path[MAX_PATH] = "C:\\Windows\\Temp\\driver.sys";
    
    if (!ExtractDriver(path)) {
        printf("Failed to extract driver\n");
        return 1;
    }

    if (!ManageDriverService(DRIVER_NAME, path, TRUE)) {
        printf("Failed to install/start service (Error: %d)\n", GetLastError());
        DeleteFile(path);
        return 1;
    }
    printf("Driver service started successfully\n");

    printf("Driver loaded. Press enter to communicate...\n");
    getchar();

    CommunicateWithDriver();

    printf("Press enter to unload driver...\n");
    getchar();

    ManageDriverService(DRIVER_NAME, path, FALSE);
    DeleteFile(path);

    return 0;
}