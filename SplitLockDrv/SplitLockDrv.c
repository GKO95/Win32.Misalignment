#include "SplitLockDrv.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	DriverObject->DriverUnload = SplitLockDrvUnload;
	DriverObject->DriverExtension->AddDevice = SplitLockDrvAddDevice;
	DriverObject->MajorFunction[IRP_MJ_PNP] = SplitLockDrvDispatchPnP;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = SplitLockDrvDispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = SplitLockDrvDispatchCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SplitLockDrvDispatchDeviceControl;
	return STATUS_SUCCESS;
}

VOID SplitLockDrvUnload(PDRIVER_OBJECT DriverObject)
{
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvUnload\n");
	UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS SplitLockDrvAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject)
{
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvAddDevice\n");
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
	DeviceObject->Flags |= DO_BUFFERED_IO | DO_POWER_PAGABLE;

	// Attach the device object
	DeviceExtension->LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);
	if (DeviceExtension->LowerDeviceObject == NULL)
	{
		IoDeleteDevice(DeviceObject);
		return STATUS_NO_SUCH_DEVICE;
	}

	// Set the initializing flag to false
	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}

NTSTATUS SplitLockDrvDispatchPnP(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchPnP\n");

	PSPLITLOCK_EXTENSION DeviceExtension = (PSPLITLOCK_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG MinorFunction = IrpStack->MinorFunction;
	NTSTATUS Status = STATUS_SUCCESS;

	KEVENT Event = { 0 };

	switch (MinorFunction)
	{
	case IRP_MN_START_DEVICE:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchPnP: IRP_MN_START_DEVICE\n");

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

	case IRP_MN_REMOVE_DEVICE:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchPnP: IRP_MN_REMOVE_DEVICE\n");

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
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchPnP: IRP_MN_XXX\n");
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		break;
	}

	return Status;
}

NTSTATUS SplitLockStartDeviceCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS SplitLockDrvDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchCreateClose\n");

	PSPLITLOCK_EXTENSION DeviceExtension = (PSPLITLOCK_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);

	UNREFERENCED_PARAMETER(DeviceExtension);

	switch (IrpStack->MajorFunction)
	{
	case IRP_MJ_CREATE:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchCreateClose: IRP_MJ_CREATE\n");
		Irp->IoStatus.Information = 0;
		break;

	case IRP_MJ_CLOSE:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchCreateClose: IRP_MJ_CLOSE\n");
		Irp->IoStatus.Information = 0;
		break;

	default:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchCreateClose: IRP_MJ_XXX\n");
		break;
	}

	return STATUS_SUCCESS;
}

NTSTATUS SplitLockDrvDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDrvDispatchDeviceControl\n");
	UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION IrpStack = IoGetCurrentIrpStackLocation(Irp);
	ULONG ControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;
	NTSTATUS Status = STATUS_SUCCESS;

	switch (ControlCode)
	{
	case IOCTL_SPLITLOCK_CR0_AM_SET:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_CR0_SET\n");

#ifdef __INTRIN_H_
		__writecr0(__readcr0() | 0x40000);
#else
		CR0AMEnable();
#endif
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "CR0 AM flag is set.\n");

		Status = STATUS_SUCCESS;
		break;

	case IOCTL_SPLITLOCK_CR0_AM_CLEAR:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_CR0_CLEAR\n");

#ifdef __INTRIN_H_
		__writecr0(__readcr0() & ~0x40000);
#else
		CR0AMDisable();
#endif
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "CR0 AM flag is clear.\n");

		Status = STATUS_SUCCESS;
		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_SET:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_EFLAG_SET\n");

#ifdef DBG
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "EFLAGS register is accessible in both user and kernel mode.\n");
#endif

		Status = STATUS_SUCCESS;
		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_CLEAR:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_EFLAG_CLEAR\n");
		
#ifdef DBG
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "EFLAGS register is accessible in both user and kernel mode.\n");
#endif

		Status = STATUS_SUCCESS;
		break;

	case IOCTL_SPLITLOCK_CR0_AM_READ:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_CR0_READ\n");
		
#ifdef __INTRIN_H_
		if (__readcr0() & 0x40000)
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "CR0 AM flag is set.\n");
		}
		else
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "CR0 AM flag is clear.\n");
		}
#else
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "IOCTL_SPLITLOCK_CR0_AM_READ is only available wtih an intrinsic.\n");
#endif

		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_READ:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDispatchDeviceControl: IOCTL_SPLITLOCK_EFLAG_READ\n");

#ifdef DBG
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "EFLAGS register is accessible in both user and kernel mode.\n");
#endif

		break;

	default:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "SplitLockDispatchDeviceControl: IOCTL_XXX\n");

		Status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
