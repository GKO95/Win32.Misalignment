#include "SplitLockDrv.h"

VOID DbgPrintDrv(const char* str)
{
#ifdef DBG
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[SplitLockDrv] %s\n", str);
#else
	UNREFERENCED_PARAMETER(str);
#endif
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DbgPrintDrv("DriverEntry");
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload							= SplitLockDrvUnload;
	DriverObject->DriverExtension->AddDevice			= SplitLockDrvAddDevice;
	DriverObject->MajorFunction[IRP_MJ_PNP]				= SplitLockDrvDispatchPnP;
	DriverObject->MajorFunction[IRP_MJ_CREATE]			= SplitLockDrvDispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]			= SplitLockDrvDispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]	= SplitLockDrvDispatchDeviceControl;

	return STATUS_SUCCESS;
}

VOID SplitLockDrvUnload(PDRIVER_OBJECT DriverObject)
{
	DbgPrintDrv("SplitLockDrvUnload");
	UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS SplitLockDrvAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{	// AddDevice Routines in Function or Filter Drivers: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/adddevice-routines-in-function-or-filter-drivers

	DbgPrintDrv("SplitLockDrvAddDevice");
	UNREFERENCED_PARAMETER(PhysicalDeviceObject);

	// Create a device object
	PDEVICE_OBJECT DeviceObject;
	NTSTATUS Status = IoCreateDevice(DriverObject, sizeof(SPLITLOCK_EXTENSION), NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	// Initialize the device object
	PSPLITLOCK_EXTENSION DeviceExtension = (PSPLITLOCK_EXTENSION)DeviceObject->DeviceExtension;
	RtlZeroMemory(DeviceExtension, sizeof(SPLITLOCK_EXTENSION));

	Status = IoRegisterDeviceInterface(DeviceExtension->PhysicalDeviceObject = PhysicalDeviceObject, &GUID_DEVINTERFACE_SPLITLOCK, NULL, &DeviceExtension->SymbolicLinkName);
	if (!NT_SUCCESS(Status))
	{
		IoDeleteDevice(DeviceObject);
		return Status;
	}

	// Set the flags and characteristics
	// * Buffered I/O: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-buffered-i-o
	// * Set the DO_POWER_PAGABLE flag: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/setting-device-object-flags-for-power-management
	DeviceObject->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;

	// Attach the device object
	DeviceExtension->LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	if (DeviceExtension->LowerDeviceObject == NULL)
	{
		IoDeleteDevice(DeviceObject);
		return STATUS_NO_SUCH_DEVICE;
	}

	// Clear the initializing flag: https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/clearing-the-do-device-initializing-flag
	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}

NTSTATUS SplitLockStartDeviceCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	DbgPrintDrv("SplitLockStartDeviceCompletion");
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS SplitLockDrvDispatchPnP(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DbgPrintDrv("SplitLockDrvDispatchPnP");

	PSPLITLOCK_EXTENSION DeviceExtension = (PSPLITLOCK_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG MinorFunction = IrpStack->MinorFunction;
	NTSTATUS Status = STATUS_SUCCESS;

	KEVENT Event = { 0 };

	switch (MinorFunction)
	{
	case IRP_MN_START_DEVICE:	// IRP_MN_START_DEVICE: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mn-start-device
								// This IRP must be handled first by the parent bus driver for a device and then by each higher driver in the device stack.

		DbgPrintDrv("SplitLockDrvDispatchPnP: IRP_MN_START_DEVICE");

		// Set the event and copy the IRP stack location
		KeInitializeEvent(&Event, NotificationEvent, FALSE);
		IoCopyCurrentIrpStackLocationToNext(Irp);

		// Set the completion routine and call the lower driver
		IoSetCompletionRoutine(Irp, SplitLockStartDeviceCompletion, &Event, TRUE, TRUE, TRUE);
		Status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
		if (Status == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
			Status = Irp->IoStatus.Status;
		}

		// Complete the IRP and set the device interface state
		if (NT_SUCCESS(Status))
		{
			Irp->IoStatus.Status = Status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			IoSetDeviceInterfaceState(&DeviceExtension->SymbolicLinkName, TRUE);
		}
		else
		{
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			IoDeleteDevice(DeviceObject);
		}
		break;

	case IRP_MN_REMOVE_DEVICE:	// IRP_MN_REMOVE_DEVICE: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mn-remove-device
								// This IRP is sent to the drivers for a device when the device is being removed from the system.

		DbgPrintDrv("SplitLockDrvDispatchPnP: IRP_MN_REMOVE_DEVICE");

		// Set the device interface state, free the symbolic link name, detach the device, and delete the device
		IoSetDeviceInterfaceState(&DeviceExtension->SymbolicLinkName, FALSE);
		RtlFreeUnicodeString(&DeviceExtension->SymbolicLinkName);
		IoDetachDevice(DeviceExtension->LowerDeviceObject);
		IoDeleteDevice(DeviceObject);

		// Complete the IRP and call the lower driver
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation(Irp);
		Status = IoCallDriver(DeviceExtension->LowerDeviceObject, Irp);
		break;

	default:
		DbgPrintDrv("SplitLockDrvDispatchPnP: IRP_MN_XXX");
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		break;
	}

	return Status;
}

NTSTATUS SplitLockDrvDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DbgPrintDrv("SplitLockDrvDispatchCreateClose");

	PSPLITLOCK_EXTENSION DeviceExtension = (PSPLITLOCK_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);

	UNREFERENCED_PARAMETER(DeviceExtension);

	switch (IrpStack->MajorFunction)
	{
	case IRP_MJ_CREATE:
		DbgPrintDrv("SplitLockDrvDispatchCreateClose: IRP_MJ_CREATE");
		Irp->IoStatus.Information = 0;
		break;

	case IRP_MJ_CLOSE:
		DbgPrintDrv("SplitLockDrvDispatchCreateClose: IRP_MJ_CLOSE");
		Irp->IoStatus.Information = 0;
		break;

	default:
		DbgPrintDrv("SplitLockDrvDispatchCreateClose: IRP_MJ_XXX");
		break;
	}

	return STATUS_SUCCESS;
}

NTSTATUS SplitLockDrvDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DbgPrintDrv("SplitLockDrvDispatchDeviceControl");
	UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG ControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;
	NTSTATUS Status = STATUS_SUCCESS;

	switch (ControlCode)
	{
	case IOCTL_SPLITLOCK_CR0_AM_SET:
		DbgPrintDrv("SplitLockDrvDispatchDeviceControl: IOCTL_SPLITLOCK_CR0_SET");

#ifdef __INTRIN_H_
		__writecr0(__readcr0() | 0x40000);
#else
		CR0AMEnable();
#endif
		DbgPrintDrv("CR0 AM flag is set.");
		Status = STATUS_SUCCESS;
		break;

	case IOCTL_SPLITLOCK_CR0_AM_CLEAR:
		DbgPrintDrv("SplitLockDrvDispatchDeviceControl: IOCTL_SPLITLOCK_CR0_CLEAR");

#ifdef __INTRIN_H_
		__writecr0(__readcr0() & ~0x40000);
#else
		CR0AMDisable();
#endif
		DbgPrintDrv("CR0 AM flag is clear.");
		Status = STATUS_SUCCESS;
		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_SET:
		DbgPrintDrv("SplitLockDrvDispatchDeviceControl: IOCTL_SPLITLOCK_EFLAG_SET");

		DbgPrintDrv("EFLAGS register is accessible in both user and kernel mode.");
		Status = STATUS_SUCCESS;
		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_CLEAR:
		DbgPrintDrv("SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_EFLAG_CLEAR");
		
		DbgPrintDrv("EFLAGS register is accessible in both user and kernel mode.");
		Status = STATUS_SUCCESS;
		break;

	case IOCTL_SPLITLOCK_CR0_AM_READ:
		DbgPrintDrv("SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_CR0_READ");
		
#ifdef __INTRIN_H_
		if (__readcr0() & 0x40000)
		{
			DbgPrintDrv("CR0 AM flag is set.");
		}
		else
		{
			DbgPrintDrv("CR0 AM flag is clear.");
		}
#else
		DbgPrintDrv("IOCTL_SPLITLOCK_CR0_AM_READ is only available wtih an intrinsic.");
#endif
		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_READ:
		DbgPrintDrv("SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_EFLAG_READ\n");

		DbgPrintDrv("EFLAGS register is accessible in both user and kernel mode.");
		break;

	default:
		DbgPrintDrv("SplitLockDispatchDeviceControl: IOCTL_XXX\n");
		Status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
