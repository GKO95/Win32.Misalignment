#include "SplitLockApp.hpp"

// Source code: https://archive.md/VZWAf
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
		std::cout << "SplitLockAppIntialization failed." << std::endl;
		return hResult;
	}

	{
		SplitLockAppDeviceIoControl(IOCTL_SPLITLOCK_CR0_AM_SET);
		SplitLockAppDeviceIoControl(IOCTL_SPLITLOCK_EFLAG_AC_SET);

		// Source code: https://archive.md/VZWAf
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

		SplitLockAppDeviceIoControl(IOCTL_SPLITLOCK_EFLAG_AC_CLEAR);
		SplitLockAppDeviceIoControl(IOCTL_SPLITLOCK_CR0_AM_CLEAR);
	}

	SplitLockAppCleanup();

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

	std::cout << "[DEBUG] EnumDeviceInterface count: " << dwIndex << std::endl;
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
		std::cout << "[DEBUG] malloc failed to allocate memory for SP_DEVICE_INTERFACE_DETAIL_DATA structure." << std::endl;
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
		std::cout << "[DEBUG] malloc failed to allocate memory for DevicePath." << std::endl;
	}
	else
	{
		memset(pwszDevicePath, 0, (wcslen(pspDeviceInterfaceDetailData->DevicePath) + 1) * sizeof(WCHAR));
		wcscpy(pwszDevicePath, pspDeviceInterfaceDetailData->DevicePath);
		free(pspDeviceInterfaceDetailData);
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	wprintf(L"[DEBUG] DevicePath: %ws\n", pwszDevicePath);

	hDevice = CreateFile(pwszDevicePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		std::cout << "CreateFile failed to create a handle." << std::endl;
		return E_FAIL;
	}
	else
	{
		std::cout << "[DEBUG] CreateFile succeeded." << std::endl;
	}

	return hResult;
}

HRESULT SplitLockAppCleanup() {

	if (hDevice != NULL)
	{
		CloseHandle(hDevice);
		hDevice = NULL;
	}

	return S_OK;
}

HRESULT SplitLockAppDeviceIoControl(DWORD dwIoControlCode) {
	
	DWORD dwBytesReturned = 0;

	switch (dwIoControlCode)
	{
	case IOCTL_SPLITLOCK_CR0_AM_SET:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			std::cout << "IOCTL_SPLITLOCK_CR0_SET failed." << std::endl;
		}
		else
		{
#ifdef _DEBUG
			std::cout << "[DEBUG] CR0 register is only accessible in kernel mode." << std::endl;
#endif
		}
		break;

	case IOCTL_SPLITLOCK_CR0_AM_CLEAR:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			std::cout << "IOCTL_SPLITLOCK_CR0_CLEAR failed." << std::endl;
		}
		else
		{
#ifdef _DEBUG
			std::cout << "[DEBUG] CR0 register is only accessible in kernel mode." << std::endl;
#endif
		}
		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_SET:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			std::cout << "IOCTL_SPLITLOCK_EFLAG_SET failed." << std::endl;
		}
		else
		{
#ifdef __INTRIN_H_
			__writeeflags(__readeflags() | 0x40000);
#else
			EFLAGACEnable();
#endif
			std::cout << "EFLAGS AC flag is set." << std::endl;
		}
		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_CLEAR:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			std::cout << "IOCTL_SPLITLOCK_EFLAG_CLEAR failed." << std::endl;
		}
		else
		{
#ifdef __INTRIN_H_
			__writeeflags(__readeflags() & ~0x40000);
#else
			EFLAGACDisable();
#endif
			std::cout << "EFLAGS AC flag is clear." << std::endl;
		}
		break;

	case IOCTL_SPLITLOCK_CR0_AM_READ:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			std::cout << "IOCTL_SPLITLOCK_CR0_READ failed." << std::endl;
		}
		else
		{
#ifdef _DEBUG
			std::cout << "[DEBUG] CR0 register is only accessible in kernel mode." << std::endl;
#endif
		}
		break;

	case IOCTL_SPLITLOCK_EFLAG_AC_READ:
		if (!DeviceIoControl(hDevice, dwIoControlCode, NULL, 0, NULL, 0, &dwBytesReturned, NULL))
		{
			std::cout << "IOCTL_SPLITLOCK_EFLAG_READ failed." << std::endl;
		}
		else
		{
#ifdef __INTRIN_H_
			if (__readeflags() & 0x40000)
			{
				std::cout << "EFLAGS AC flag is set." << std::endl;
			}
			else
			{
				std::cout << "EFLAGS AC flag is clear." << std::endl;
			}
#else
			std::cout << "IOCTL_SPLITLOCK_EFLAG_READ is only available wtih an intrinsic." << std::endl;
#endif
		}
		break;

	default:
		std::cout << "Invalid IoControlCode." << std::endl;
		break;
	}

	return S_OK;
}
