#include <ntddk.h>
#include <initguid.h>
#include <wdmsec.h>
// Define a unique pool tag (Standard C, independent of header)
// Cast to ULONG might be needed depending on how POOL_TAG type is defined/used elsewhere
#define POOL_TAG ((ULONG)'lleH')

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

// Forward declaration for the Unload routine (using the type from mingw-wdk.h)
// Note: The function signature must match the PDRIVER_UNLOAD definition
VOID HelloWorldUnload(
    PDRIVER_OBJECT DriverObject // Parameter type matches PDRIVER_UNLOAD
);

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

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: DriverEntry - Hello World! (mingw-wdk extended simulation)\n");

    // Create device and symbolic link
    RtlInitUnicodeString(&deviceName, L"\\Device\\HelloWorld");
    RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\HelloWorld");

    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject);

    if (NT_SUCCESS(status)) {
        deviceObject->Flags |= DO_BUFFERED_IO;
        deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "Device created at \\Device\\HelloWorld\n");
    }

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Failed to create device (0x%08X)\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&symLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Failed to create symlink (0x%08X)\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    // Store device in driver object for cleanup
    DriverObject->DeviceObject = deviceObject;
    DriverObject->DriverUnload = HelloWorldUnload;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Mailbox device created successfully\n");
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Driver loaded successfully\n");

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

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: DriverUnload - Goodbye World! (mingw-wdk extended simulation)\n");

    if (DriverObject->DeviceObject) {
        RtlInitUnicodeString(&symLinkName, L"\\DosDevices\\HelloWorld");
        IoDeleteSymbolicLink(&symLinkName);
        IoDeleteDevice(DriverObject->DeviceObject);
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Mailbox device removed\n");
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Driver unloaded.\n");
}
