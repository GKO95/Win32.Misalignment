#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <iostream>
#include <SetupAPI.h>
#include <initguid.h>
#include <intrin.h>

// {076B079F-986D-4CCD-83BF-4A0C6673840C}
DEFINE_GUID(GUID_DEVINTERFACE_SPLITLOCK,
	0x76b079f, 0x986d, 0x4ccd, 0x83, 0xbf, 0x4a, 0xc, 0x66, 0x73, 0x84, 0xc);

#define IOCTL_SPLITLOCK_CR0_AM_SET		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPLITLOCK_CR0_AM_CLEAR	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPLITLOCK_CR0_AM_READ		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPLITLOCK_EFLAG_AC_SET	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPLITLOCK_EFLAG_AC_CLEAR	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SPLITLOCK_EFLAG_AC_READ	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

#ifndef __INTRIN_H_
extern "C" 
{
	void EFLAGACEnable();
	void EFLAGACDisable();
}
#endif

static HANDLE hDevice = NULL;

HRESULT SplitLockAppIntialization(const GUID* ptrGuid);
HRESULT SplitLockAppCleanup();

HRESULT SplitLockAppDeviceIoControl(DWORD dwIoControlCode);