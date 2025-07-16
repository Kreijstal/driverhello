#include "windows.h"
#include "ntstatus.h"
#include "ntos.h"

// =======================================================================
// Minimal WDK definitions for IRP handling
// =======================================================================

// MDL (Memory Descriptor List) structure
typedef struct _MDL {
    struct _MDL *Next;
    SHORT Size;
    SHORT MdlFlags;
    struct _EPROCESS *Process;
    PVOID MappedSystemVa;
    PVOID StartVa;
    ULONG ByteCount;
    ULONG ByteOffset;
} MDL, *PMDL;

// IO_STACK_LOCATION structure for IRP handling
typedef struct _IO_STACK_LOCATION {
    UCHAR MajorFunction;
    UCHAR MinorFunction;
    UCHAR Flags;
    UCHAR Control;
    union {
        struct { 
            ULONG OutputBufferLength; 
            ULONG InputBufferLength; 
            ULONG IoControlCode; 
            PVOID Type3InputBuffer; 
        } DeviceIoControl;
    } Parameters;
    PDEVICE_OBJECT DeviceObject;
    PFILE_OBJECT FileObject;
} IO_STACK_LOCATION, *PIO_STACK_LOCATION;

// IRP structure with CurrentStackLocation
typedef struct _IRP {
    PMDL MdlAddress;
    ULONG Flags;
    union {
        struct _IRP *MasterIrp;
        __volatile LONG IrpCount;
        PVOID SystemBuffer;
    } AssociatedIrp;
    IO_STATUS_BLOCK IoStatus;
    KPROCESSOR_MODE RequestorMode;
    BOOLEAN PendingReturned;
    CHAR StackCount;
    CHAR CurrentLocation;
    BOOLEAN Cancel;
    KIRQL CancelIrql;
    CCHAR ApcEnvironment;
    ULONG AllocationFlags;
    union {
        PIO_STACK_LOCATION CurrentStackLocation;
    } Tail;
} IRP, *PIRP;

// Constants for IOCTL handling
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0

// Our specific IOCTL definition (must match loader.c)
#define IOCTL_HELLO CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Priority boost constant
#define IO_NO_INCREMENT 0

// Macro to get current stack location
#define IoGetCurrentIrpStackLocation(Irp) ((Irp)->Tail.CurrentStackLocation)

// =======================================================================
// WDM Function Prototypes
// =======================================================================
NTSTATUS IoCreateDevice(PDRIVER_OBJECT, ULONG, PUNICODE_STRING, DEVICE_TYPE, ULONG, BOOLEAN, PDEVICE_OBJECT*);
NTSTATUS IoCreateSymbolicLink(PUNICODE_STRING, PUNICODE_STRING);
VOID IoDeleteDevice(PDEVICE_OBJECT);
NTSTATUS IoDeleteSymbolicLink(PUNICODE_STRING);
VOID IoCompleteRequest(PIRP, CCHAR);
#define IofCompleteRequest(Irp, PriorityBoost) IoCompleteRequest(Irp, PriorityBoost)

// =======================================================================
// Driver Functions
// =======================================================================
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath);
VOID HelloWorldUnload(_In_ PDRIVER_OBJECT DriverObject);
NTSTATUS HelloWorldCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
NTSTATUS HelloWorldDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING deviceName, symLink;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: DriverEntry\n");

    RtlInitUnicodeString(&deviceName, L"\\Device\\HelloWorld");
    RtlInitUnicodeString(&symLink, L"\\DosDevices\\HelloWorld");

    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &deviceObject);

    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Failed to create device (0x%08X)\n", status);
        return status;
    }

    status = IoCreateSymbolicLink(&symLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Failed to create symlink (0x%08X)\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    DriverObject->DriverUnload = HelloWorldUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = HelloWorldCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = HelloWorldCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HelloWorldDeviceControl;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Device and symlink created successfully\n");
    return STATUS_SUCCESS;
}

VOID HelloWorldUnload(_In_ PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symLink;
    PAGED_CODE();

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Unloading...\n");

    RtlInitUnicodeString(&symLink, L"\\DosDevices\\HelloWorld");
    IoDeleteSymbolicLink(&symLink);

    if (DriverObject->DeviceObject) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Driver unloaded.\n");
}

NTSTATUS HelloWorldCreateClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS HelloWorldDeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR info = 0;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
        case IOCTL_HELLO:
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "IOCTL_HELLO received!\n");
            
            char* buffer = (char*)Irp->AssociatedIrp.SystemBuffer;
            ULONG inSize = stack->Parameters.DeviceIoControl.InputBufferLength;
            ULONG outSize = stack->Parameters.DeviceIoControl.OutputBufferLength;
            const char* response = "Hello back from the kernel!";
            size_t responseLen = strlen(response) + 1;

            if (buffer && inSize > 0) {
                 DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "User mode says: %s\n", buffer);
            }

            if (outSize >= responseLen) {
                RtlCopyMemory(buffer, response, responseLen);
                info = responseLen;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}
