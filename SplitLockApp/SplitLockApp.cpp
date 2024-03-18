#include "SplitLockApp.hpp"

void DbgPrintApp(const char* str) {
#ifdef _DEBUG
	std::cout << "[DEBUG] " << str << std::endl;
#endif
}

// Windows Data Alignment on IPF, x86, and x64: https://archive.md/VZWAf
int mswindows_handle_hardware_exceptions(DWORD code)
{
	printf("Handling exception\n");
	if (code == STATUS_DATATYPE_MISALIGNMENT)
	{
		printf("misalignment fault!\n");
		return EXCEPTION_EXECUTE_HANDLER;
	}
	else
		return EXCEPTION_CONTINUE_SEARCH;
}

int main() {
	HRESULT hResult = S_OK;
	BOOL bResult = FALSE;

	hResult = SplitLockAppIntialization(&GUID_DEVINTERFACE_SPLITLOCK);
	if (hResult != S_OK)
	{
		DbgPrintApp("SplitLockAppIntialization failed.");
		return hResult;
	}

	{
		SplitLockAppDeviceIoControl(IOCTL_SPLITLOCK_CR0_AM_SET);
		SplitLockAppDeviceIoControl(IOCTL_SPLITLOCK_EFLAGS_AC_SET);

		// Windows Data Alignment on IPF, x86, and x64: https://archive.md/VZWAf
		__try
		{
			char temp[10];
			memset(temp, 0, 10);
			double* val;
			val = (double*)(&temp[3]);
			printf("%lf\n", *val);
		}
		__except (mswindows_handle_hardware_exceptions(GetExceptionCode()))
		{
			std::cout << "Executing Handler" << std::endl;
		}

		SplitLockAppDeviceIoControl(IOCTL_SPLITLOCK_EFLAGS_AC_CLEAR);
		SplitLockAppDeviceIoControl(IOCTL_SPLITLOCK_CR0_AM_CLEAR);
	}

	if (hDevice != NULL)
	{
		CloseHandle(hDevice);
		hDevice = NULL;
	}

	return hResult;
}

HRESULT SplitLockAppIntialization(const GUID* ptrGuid) {

	HRESULT hResult = S_OK;
	HDEVINFO hDevInfo = SetupDiGetClassDevs(ptrGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	SP_DEVICE_INTERFACE_DATA spDeviceInterfaceData = { 0 };
	spDeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	DWORD dwIndex = 0, dwSize = 0;
	while (1)
	{
		BOOL bResult = SetupDiEnumDeviceInterfaces(hDevInfo, NULL, ptrGuid, dwIndex, &spDeviceInterfaceData);
		if (bResult == FALSE)
		{
			break;
		}
		dwIndex++;
	}

	std::cout << "EnumDeviceInterface count: " << dwIndex << std::endl;
	if (dwIndex == 0)
	{
		std::cout << "No device found." << std::endl;
		return E_FAIL;
	}
	else if (dwIndex > 1)
	{
		std::cout << "More than one device found." << std::endl;
	}

	SetupDiEnumDeviceInterfaces(hDevInfo, NULL, ptrGuid, dwIndex, &spDeviceInterfaceData);
	SetupDiGetDeviceInterfaceDetail(hDevInfo, &spDeviceInterfaceData, NULL, 0, &dwSize, NULL);

	PSP_DEVICE_INTERFACE_DETAIL_DATA pspDeviceInterfaceDetailData = NULL;
	pspDeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(dwSize);
	if (pspDeviceInterfaceDetailData == NULL)
	{
		DbgPrintApp("malloc failed to allocate memory for SP_DEVICE_INTERFACE_DETAIL_DATA structure.");
		return E_FAIL;
	}
	else
	{
		pspDeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		SetupDiGetDeviceInterfaceDetail(hDevInfo, &spDeviceInterfaceData, pspDeviceInterfaceDetailData, dwSize, NULL, NULL);
	}

	PWCHAR pwszDevicePath = NULL;
	pwszDevicePath = (PWCHAR)malloc((wcslen(pspDeviceInterfaceDetailData->DevicePath) + 1) * sizeof(WCHAR));
	if (pwszDevicePath == NULL)
	{
		DbgPrintApp("malloc failed to allocate memory for DevicePath.");
	}
	else
	{
		memset(pwszDevicePath, 0, (wcslen(pspDeviceInterfaceDetailData->DevicePath) + 1) * sizeof(WCHAR));
		wcscpy(pwszDevicePath, pspDeviceInterfaceDetailData->DevicePath);
		free(pspDeviceInterfaceDetailData);
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	wprintf(L"DevicePath: %ws\n", pwszDevicePath);

	hDevice = CreateFile(pwszDevicePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		std::cout << "CreateFile failed to create a handle." << std::endl;
		return E_FAIL;
	}
	else
	{
		DbgPrintApp("CreateFile succeeded.");
	}

	return hResult;
}

HRESULT SplitLockAppDeviceIoControl(DWORD dwIoControlCode) {
	
	DWORD dwBytesReturned = 0;

	switch (dwIoControlCode)
	{
	case IOCTL_SPLITLOCK_CR0_AM_SET:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			DbgPrintApp("IOCTL_SPLITLOCK_CR0_SET failed.");
		}
		else
		{
			goto CR0_AM_READ;
		}
		break;

	case IOCTL_SPLITLOCK_CR0_AM_CLEAR:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			DbgPrintApp("IOCTL_SPLITLOCK_CR0_CLEAR failed.");
		}
		else
		{
			goto CR0_AM_READ;
		}
		break;

	case IOCTL_SPLITLOCK_EFLAGS_AC_SET:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			DbgPrintApp("IOCTL_SPLITLOCK_EFLAG_SET failed.");
		}
		else
		{
#ifdef __INTRIN_H_
			__writeeflags(__readeflags() | 0x40000);
#else
			EFLAGACEnable();
#endif
			goto EFLAGS_AC_READ;
		}
		break;

	case IOCTL_SPLITLOCK_EFLAGS_AC_CLEAR:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			DbgPrintApp("IOCTL_SPLITLOCK_EFLAG_CLEAR failed.");
		}
		else
		{
#ifdef __INTRIN_H_
			__writeeflags(__readeflags() & ~0x40000);
#else
			EFLAGACDisable();
#endif
			goto EFLAGS_AC_READ;
		}
		break;

	case IOCTL_SPLITLOCK_CR0_AM_READ:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			DbgPrintApp("IOCTL_SPLITLOCK_CR0_READ failed.");
		}
		else
		{
			CR0_AM_READ:
			DbgPrintApp("CR0 register is only accessible in kernel mode.");
		}
		break;

	case IOCTL_SPLITLOCK_EFLAGS_AC_READ:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			DbgPrintApp("IOCTL_SPLITLOCK_EFLAG_READ failed.");
		}
		else
		{
			EFLAGS_AC_READ:
#ifdef __INTRIN_H_
			std::cout << "EFLAGS AC flag is " << (__readeflags() & 0x40000 ? "set." : "clear.") << std::endl;
#else
			std::cout << "IOCTL_SPLITLOCK_EFLAG_READ is only available wtih an intrinsic." << std::endl;
#endif
		}
		break;

	default:
		DbgPrintApp("Invalid IoControlCode.");
		break;
	}

	return S_OK;
}
