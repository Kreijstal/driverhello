// Include our hypothetical *extended* header ONLY
#include "windows.h"
#include "ntstatus.h"
#include "ntos.h"
// Assumes mingw-wdk.h now defines:
// - NTSTATUS, STATUS_SUCCESS
// - PDRIVER_OBJECT (pointing to a defined struct _DRIVER_OBJECT)
// - DRIVER_UNLOAD (function ptr type)
// - PUNICODE_STRING
// - DbgPrintEx declaration
// - UNREFERENCED_PARAMETER macro
// - PAGED_CODE macro
// - DPFLTR_... constants

// Define a unique pool tag (Standard C, independent of header)
// Cast to ULONG might be needed depending on how POOL_TAG type is defined/used elsewhere
#define POOL_TAG ((ULONG)'lleH')

// Forward declaration for the Unload routine (using the type from mingw-wdk.h)
// Note: The function signature must match the PDRIVER_UNLOAD definition
VOID WDK_API HelloWorldUnload(
    PDRIVER_OBJECT DriverObject // Parameter type matches PDRIVER_UNLOAD
);


// NOTE: alloc_text still requires compiler/linker specific handling for GCC.

//
// DriverEntry routine
//
NTSTATUS WDK_API DriverEntry(
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

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Driver loaded successfully (unload routine set).\n");

    return status;
}

//
// HelloWorldUnload routine
//
VOID WDK_API HelloWorldUnload(
    _In_ PDRIVER_OBJECT DriverObject
    )
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: DriverUnload - Goodbye World! (mingw-wdk extended simulation)\n");
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "HelloWorldDriver: Driver unloaded.\n");
}
