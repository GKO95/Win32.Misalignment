#pragma once
#include <wdm.h>
#include <intrin.h>
#include "SplitLock.h"

typedef struct {
	PDEVICE_OBJECT LowerDeviceObject;
	PDEVICE_OBJECT PhysicalDeviceObject;
	UNICODE_STRING SymbolicLinkName;
} SPLITLOCK_EXTENSION, * PSPLITLOCK_EXTENSION;

#ifndef __INTRIN_H_
extern void CR0AMEnable(void);
extern void CR0AMDisable(void);
#endif

VOID SplitLockDrvUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS SplitLockDrvAddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT PhysicalDeviceObject);
NTSTATUS SplitLockDrvDispatchPnP(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SplitLockDrvDispatchCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS SplitLockDrvDispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
