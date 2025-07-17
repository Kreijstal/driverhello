#include <ntddk.h>
#include <initguid.h>
#include <wdmsec.h>

// Define a unique pool tag
#define POOL_TAG ((ULONG)'lleH')

// Define the IOCTL code, must match the one in loader.c
#define IOCTL_HELLO CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

// Forward declarations for our new functions
VOID HelloWorldUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS HelloWorldCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HelloWorldDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

//
// DriverEntry routine
//
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING deviceName, symLinkName;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: DriverEntry - Hello World!\n");

    // Register IRP dispatch routines
    DriverObject->DriverUnload = HelloWorldUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = HelloWorldCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = HelloWorldCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HelloWorldDeviceControl;

    // Create device and symbolic link
    RtlInitUnicodeString(&deviceName, L"\\Device\\HelloWorld");
    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        0, // No specific device characteristics needed
        FALSE,
        &deviceObject);

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Failed to create device (0x%08X)\n", status);
        return status;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Device created at \\Device\\HelloWorld\n");
    
    // Create the symbolic link for user-mode access
    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\HelloWorld");
    status = IoCreateSymbolicLink(&symLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "Failed to create symlink (0x%08X)\n", status);
        IoDeleteDevice(deviceObject); // Clean up on failure
        return status;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "Symbolic link created successfully\n");

    // Clear the initializing flag
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Driver loaded successfully\n");

    return status;
}

//
// Create/Close Dispatch Routine
// Handles IRP_MJ_CREATE and IRP_MJ_CLOSE.
//
NTSTATUS HelloWorldCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    
    // We don't need to do any special processing for open/close,
    // so we just complete the request successfully.
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    
    return STATUS_SUCCESS;
}

//
// DeviceControl Dispatch Routine
// Handles IRP_MJ_DEVICE_CONTROL.
//
NTSTATUS HelloWorldDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    
    PIO_STACK_LOCATION irpStack;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG bytesReturned = 0;

    // Get a pointer to the current I/O stack location.
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    if (irpStack)
    {
        // Get the IOCTL code from the IRP
        ULONG ioctlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

        if (ioctlCode == IOCTL_HELLO) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: IOCTL_HELLO received\n");
            
            // For METHOD_BUFFERED, the I/O manager allocates a single buffer
            // at Irp->AssociatedIrp.SystemBuffer. It copies the input data
            // into this buffer and will copy the output data from it on completion.
            PCHAR buffer = (PCHAR)Irp->AssociatedIrp.SystemBuffer;
            ULONG inputLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
            ULONG outputLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
            
            // Print the message from user-mode
            if (inputLength > 0 && buffer != NULL) {
                 DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "User mode says: %s\n", buffer);
            }

            // Write our response back to the buffer
            const char* response = "Hello back from the kernel!";
            size_t responseLength = strlen(response) + 1; // +1 for null terminator
            
            if (outputLength >= responseLength) {
                RtlCopyMemory(buffer, response, responseLength);
                bytesReturned = (ULONG)responseLength;
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
        } else {
            status = STATUS_INVALID_DEVICE_REQUEST;
        }
    }

    // Complete the request
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = bytesReturned;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}


//
// HelloWorldUnload routine
//
VOID HelloWorldUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    UNICODE_STRING symLinkName;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: DriverUnload - Goodbye World!\n");

    // Delete the symbolic link first
    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\HelloWorld");
    IoDeleteSymbolicLink(&symLinkName);
    
    // Then delete the device object
    if (DriverObject->DeviceObject) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Driver unloaded.\n");
}
