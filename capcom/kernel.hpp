#pragma once
#define WIN32_NO_STATUS
#include <Windows.h>
#include <Winternl.h>
#undef WIN32_NO_STATUS
#include <string>

namespace kernel
{
	typedef enum _POOL_TYPE {
		NonPagedPool,
		NonPagedPoolExecute = NonPagedPool,
		PagedPool,
		NonPagedPoolMustSucceed = NonPagedPool + 2,
		DontUseThisType,
		NonPagedPoolCacheAligned = NonPagedPool + 4,
		PagedPoolCacheAligned,
		NonPagedPoolCacheAlignedMustS = NonPagedPool + 6,
		MaxPoolType,
		NonPagedPoolBase = 0,
		NonPagedPoolBaseMustSucceed = NonPagedPoolBase + 2,
		NonPagedPoolBaseCacheAligned = NonPagedPoolBase + 4,
		NonPagedPoolBaseCacheAlignedMustS = NonPagedPoolBase + 6,
		NonPagedPoolSession = 32,
		PagedPoolSession = NonPagedPoolSession + 1,
		NonPagedPoolMustSucceedSession = PagedPoolSession + 1,
		DontUseThisTypeSession = NonPagedPoolMustSucceedSession + 1,
		NonPagedPoolCacheAlignedSession = DontUseThisTypeSession + 1,
		PagedPoolCacheAlignedSession = NonPagedPoolCacheAlignedSession + 1,
		NonPagedPoolCacheAlignedMustSSession = PagedPoolCacheAlignedSession + 1,
		NonPagedPoolNx = 512,
		NonPagedPoolNxCacheAligned = NonPagedPoolNx + 4,
		NonPagedPoolSessionNx = NonPagedPoolNx + 32
	} POOL_TYPE;

	typedef struct _RTL_PROCESS_MODULE_INFORMATION
	{
		HANDLE Section;
		PVOID MappedBase;
		PVOID ImageBase;
		ULONG ImageSize;
		ULONG Flags;
		USHORT LoadOrderIndex;
		USHORT InitOrderIndex;
		USHORT LoadCount;
		USHORT OffsetToFileName;
		UCHAR FullPathName[256];
	} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

	typedef struct _RTL_PROCESS_MODULES
	{
		ULONG NumberOfModules;
		RTL_PROCESS_MODULE_INFORMATION Modules[1];
	} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

	using MmGetSystemRoutineAddressFn = PVOID(NTAPI*)(PUNICODE_STRING);
	using ExAllocatePoolWithTagFn = PVOID(*)(POOL_TYPE, SIZE_T, ULONG);
	using RtlFindExportedRoutineByNameFn = void*(__fastcall*)(void *, const char *);
	using ExAllocatePoolFn = PVOID(*)(POOL_TYPE, SIZE_T);
	using DbgPrintFn = ULONG(*)(const char*, ...);

	namespace names
	{
		constexpr auto RtlFindExportedRoutineByName = L"RtlFindExportedRoutineByName";
		constexpr auto ExAllocatePoolWithTag = L"ExAllocatePoolWithTag";
		constexpr auto ExAllocatePool = L"ExAllocatePool";
		constexpr auto DbgPrint = L"DbgPrint";
	}
}