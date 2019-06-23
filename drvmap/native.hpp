#pragma once

#include <string>
#include <Windows.h>
#include <Winternl.h>

namespace native
{
	extern "C" NTSTATUS NTAPI ZwLoadDriver(PUNICODE_STRING str);

	extern "C" NTSTATUS NTAPI ZwUnloadDriver(PUNICODE_STRING str);

	namespace internal
	{
		inline LSTATUS prepare_driver_registry(const wchar_t *svcName, const wchar_t *svcDrv, const wchar_t *group, int startupType)
		{
			HKEY key;
			HKEY subkey;

			DWORD type = 1;
			DWORD err = 0;
			LSTATUS status = 0;

			wchar_t path[MAX_PATH];
			swprintf_s(path, ARRAYSIZE(path), L"\\??\\%s", svcDrv);

			status = RegOpenKeyW(HKEY_LOCAL_MACHINE, L"system\\CurrentControlSet\\Services", &key);
			if (status)
				return status;

			status = RegCreateKeyW(key, svcName, &subkey);
			if (status)
			{
				RegCloseKey(key);
				return status;
			}

			status |= RegSetValueExW(subkey, L"DisplayName", 0, REG_SZ, (const BYTE *)svcName, (DWORD)(sizeof(WCHAR) * wcslen(svcName) + 1));
			status |= RegSetValueExW(subkey, L"ErrorControl", 0, REG_DWORD, (const BYTE *)&err, sizeof(err));
			status |= RegSetValueExW(subkey, L"Group", 0, REG_SZ, (const BYTE *)group, sizeof(WCHAR) * ((DWORD)wcslen(group) + 1));
			status |= RegSetValueExW(subkey, L"ImagePath", 0, REG_SZ, (const BYTE *)path, (sizeof(WCHAR) * ((DWORD)wcslen(path) + 1)));
			status |= RegSetValueExW(subkey, L"Start", 0, REG_DWORD, (const BYTE *)&startupType, sizeof(startupType));
			status |= RegSetValueExW(subkey, L"Type", 0, REG_DWORD, (const BYTE*)&type, sizeof(type));
			
			RegCloseKey(subkey);

			return status;
		}

		inline std::wstring make_path(const std::wstring& svcName)
		{
			std::wstring path = L"\\registry\\machine\\SYSTEM\\CurrentControlSet\\Services\\";
			path += svcName;
			return path;
		}

		inline bool set_privilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
		{
			TOKEN_PRIVILEGES tp;
			LUID luid;

			if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))
				return false;

			tp.PrivilegeCount = 1;
			tp.Privileges[0].Luid = luid;
			if (bEnablePrivilege)
				tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			else
				tp.Privileges[0].Attributes = 0;

			// Enable the privilege or disable all privileges.
			if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES),
				(PTOKEN_PRIVILEGES)NULL, (PDWORD)NULL))
				return false;

			return GetLastError() != ERROR_NOT_ALL_ASSIGNED;
		}
	}

	inline bool load_driver(const std::wstring& path, const std::wstring& service)
	{
		LSTATUS stat = internal::prepare_driver_registry(service.c_str(), path.c_str(), L"Base", 1);
		if (stat != ERROR_SUCCESS)
			return stat;

		UNICODE_STRING str;
		auto wpath = internal::make_path(service);
		RtlInitUnicodeString(&str, wpath.c_str());

		HANDLE token;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
			return false;

		bool done = internal::set_privilege(token, TEXT("SeLoadDriverPrivilege"), TRUE);
		CloseHandle(token);
		if (!done)
			return false;

		return ZwLoadDriver(&str) >= 0;
	}

	inline bool unload_driver(const std::wstring& service)
	{
		UNICODE_STRING str;
		auto wservice = internal::make_path(service);
		RtlInitUnicodeString(&str, wservice.c_str());

		bool isUnloaded = ZwUnloadDriver(&str) >= 0;

		if (isUnloaded)
		{
			HKEY key;

			if (!RegOpenKeyW(HKEY_LOCAL_MACHINE, L"system\\CurrentControlSet\\Services", &key))
			{
			    RegDeleteKeyW(key, L"Capcom");
			    RegCloseKey(key);
			}
		}
		
		return isUnloaded;
	}
}
