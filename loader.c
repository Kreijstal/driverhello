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
    if (!scm) {
        printf("Failed to open SCM (Error: %d)\n", GetLastError());
        return FALSE;
    }

    BOOL success = FALSE;
    SC_HANDLE service = NULL;

    if (install) {
        // --- NEW RESILIENCY LOGIC ---
        // Before creating, check if a stale service exists and try to remove it.
        service = OpenService(scm, serviceName, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
        if (service) {
            printf("Found a pre-existing service '%s'. Attempting to clean up...\n", serviceName);
            
            // Try to stop it first.
            SERVICE_STATUS status;
            ControlService(service, SERVICE_CONTROL_STOP, &status);
            
            // Wait for the service to actually stop.
            // Poll for up to 5 seconds.
            for (int i = 0; i < 10; i++) {
                if (!QueryServiceStatus(service, &status)) break;
                if (status.dwCurrentState == SERVICE_STOPPED) break;
                Sleep(500);
            }

            if (!DeleteService(service)) {
                // If deletion fails, it might be marked for deletion. A reboot is the only fix.
                if (GetLastError() == ERROR_SERVICE_MARKED_FOR_DELETE) {
                    printf("Service is marked for deletion. A reboot is required to fully remove it.\n");
                } else {
                    printf("Failed to delete the pre-existing service (Error: %d). Manual cleanup with 'sc delete' might be needed.\n", GetLastError());
                }
                CloseServiceHandle(service);
                CloseServiceHandle(scm);
                return FALSE; // Can't proceed.
            }

            printf("Pre-existing service deleted. Waiting a moment for SCM to update...\n");
            CloseServiceHandle(service);
            Sleep(2000); // Give the SCM a second or two to process the deletion.
        }
        // --- END OF NEW LOGIC ---

        // Now, proceed with creating the service. This should be a clean attempt.
        service = CreateService(scm, serviceName, serviceName,
            SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
            SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
            driverPath, NULL, NULL, NULL, NULL, NULL);

        if (!service) {
            printf("CreateService failed after cleanup attempt (Error: %d)\n", GetLastError());
            CloseServiceHandle(scm);
            return FALSE;
        }

        if (StartService(service, 0, NULL)) {
            success = TRUE;
        } else {
            // Check if it's already running, which shouldn't happen on a clean install but is good practice.
            if (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) {
                success = TRUE;
            } else {
                 printf("StartService failed (Error: %d)\n", GetLastError());
            }
        }
        CloseServiceHandle(service);

    } else { // UNLOAD LOGIC (remains the same)
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
                 // If it's not running, we can still try to delete it.
                if (GetLastError() == ERROR_SERVICE_NOT_ACTIVE) {
                     printf("Service was not running. Deleting...\n");
                     if (DeleteService(service)) {
                        printf("Service deleted successfully.\n");
                        success = TRUE;
                     } else {
                        printf("Failed to delete service (Error: %d)\n", GetLastError());
                     }
                }
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
    
    printf("Testing device paths...\n");
    
    // Test paths including non-existent one for reference
    const char* paths[] = {
        "\\\\.\\HelloWorld",  // Our device
        "\\\\.\\NonexistentDevice",  // Should give FILE_NOT_FOUND
        "\\Device\\HelloWorld",  // Direct path
        "\\??\\HelloWorld"  // Alternative NT path
    };
    
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    
    for (int i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
        printf("Trying path: %s\n", paths[i]);
        hDevice = CreateFileA(
            paths[i],
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
            
        if (hDevice != INVALID_HANDLE_VALUE) {
            break;
        }
        printf("Failed (Error: %d)\n", GetLastError());
    }

    if (hDevice == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        printf("Failed to open \\\\.\\Global\\HelloWorld (Error: %d)\n", err);
        
        // Try standard path
        hDevice = CreateFileA("\\\\.\\HelloWorld",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
            
        if (hDevice == INVALID_HANDLE_VALUE) {
            printf("Failed to open \\\\.\\HelloWorld (Error: %d)\n", GetLastError());
            return;
        }
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