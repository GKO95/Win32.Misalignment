#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include ".\..\SplitLockDrv\SplitLock.h"
#include <iostream>
#include <SetupAPI.h>
#include <intrin.h>

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
