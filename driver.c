#include "windows.h"
#include "ntstatus.h"
#include "ntos.h"

// Define MDL (Memory Descriptor List) structure
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

// Define IRP structure (I/O Request Packet)
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
} IRP, *PIRP;

// Define unique pool tag
// Cast to ULONG might be needed depending on how POOL_TAG type is defined/used elsewhere
#define POOL_TAG ((ULONG)'lleH')

// Declare Missing WDM Function Prototypes
NTSTATUS IoCreateDevice(
    PDRIVER_OBJECT  DriverObject,
    ULONG           DeviceExtensionSize,
    PUNICODE_STRING DeviceName,
    DEVICE_TYPE     DeviceType,
    ULONG           DeviceCharacteristics,
    BOOLEAN         Exclusive,
    PDEVICE_OBJECT  *DeviceObject
);

NTSTATUS IoCreateSymbolicLink(
    PUNICODE_STRING SymbolicLinkName,
    PUNICODE_STRING DeviceName
);

VOID IoDeleteDevice(
    PDEVICE_OBJECT DeviceObject
);

NTSTATUS IoDeleteSymbolicLink(
    PUNICODE_STRING SymbolicLinkName
);

VOID IoCompleteRequest(
    PIRP Irp,
    CCHAR PriorityBoost
);

#define IofCompleteRequest( Irp, PriorityBoost ) IoCompleteRequest( Irp, PriorityBoost )

// Forward declaration for the Unload routine
VOID HelloWorldUnload(
    PDRIVER_OBJECT DriverObject
);


// NOTE: alloc_text still requires compiler/linker specific handling for GCC.

//
// DriverEntry routine
//
NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(RegistryPath); // Parameter still unused in this simple example

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: DriverEntry - Hello World! (mingw-wdk extended simulation)\n");

    // !!! THIS NOW COMPILES !!!
    // Because mingw-wdk.h defines struct _DRIVER_OBJECT, the compiler knows
    // the 'DriverUnload' member exists and its type (PDRIVER_UNLOAD).
    DriverObject->DriverUnload = HelloWorldUnload;

    // We can potentially initialize MajorFunction entries if needed, e.g.:
    // for (int i = 0; i < 28; i++) { // 28 is IRP_MJ_MAXIMUM_FUNCTION + 1
    //     DriverObject->MajorFunction[i] = DefaultDispatchRoutine; // Assuming such a function exists
    // }
    // DriverObject->MajorFunction[IRP_MJ_CREATE] = MyCreateDispatch; // etc.
    // But for Hello World, just printing is enough.

    // Create device object
    UNICODE_STRING deviceName;
    RtlInitUnicodeString(&deviceName, L"\\Device\\HelloWorld");

    PDEVICE_OBJECT deviceObject = NULL;
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

    // Create symbolic link
    UNICODE_STRING symLink;
    RtlInitUnicodeString(&symLink, L"\\DosDevices\\HelloWorld");

    status = IoCreateSymbolicLink(&symLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Failed to create symlink (0x%08X)\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Device and symlink created successfully\n");
    return status;
}

//
// HelloWorldUnload routine
//
VOID HelloWorldUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: DriverUnload - Goodbye World! (mingw-wdk extended simulation)\n");
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Driver unloaded.\n");
}
