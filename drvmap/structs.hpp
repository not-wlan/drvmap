#pragma once
#include <Windows.h>
#define MDL_MAPPED_TO_SYSTEM_VA     0x0001
#define MDL_PAGES_LOCKED            0x0002
#define MDL_SOURCE_IS_NONPAGED_POOL 0x0004
#define MDL_ALLOCATED_FIXED_SIZE    0x0008
#define MDL_PARTIAL                 0x0010
#define MDL_PARTIAL_HAS_BEEN_MAPPED 0x0020
#define MDL_IO_PAGE_READ            0x0040
#define MDL_WRITE_OPERATION         0x0080
#define MDL_LOCKED_PAGE_TABLES      0x0100
#define MDL_PARENT_MAPPED_SYSTEM_VA MDL_LOCKED_PAGE_TABLES
#define MDL_FREE_EXTRA_PTES         0x0200
#define MDL_DESCRIBES_AWE           0x0400
#define MDL_IO_SPACE                0x0800
#define MDL_NETWORK_HEADER          0x1000
#define MDL_MAPPING_CAN_FAIL        0x2000
#define MDL_PAGE_CONTENTS_INVARIANT 0x4000
#define MDL_ALLOCATED_MUST_SUCCEED  MDL_PAGE_CONTENTS_INVARIANT
#define MDL_INTERNAL                0x8000

#define MDL_MAPPING_FLAGS (MDL_MAPPED_TO_SYSTEM_VA     | \
                           MDL_PAGES_LOCKED            | \
                           MDL_SOURCE_IS_NONPAGED_POOL | \
                           MDL_PARTIAL_HAS_BEEN_MAPPED | \
                           MDL_PARENT_MAPPED_SYSTEM_VA | \
                           MDL_SYSTEM_VA               | \
                           MDL_IO_SPACE )

namespace drvmap::structs
{
	using PHYSICAL_ADDRESS = LARGE_INTEGER;
	using KPROCESSOR_MODE = CCHAR;
	typedef enum _MEMORY_CACHING_TYPE {
		MmNonCached = 0,
		MmCached = 1,
		MmWriteCombined = 2,
		MmHardwareCoherentCached = 3,
		MmNonCachedUnordered = 4,
		MmUSWCCached = 5,
		MmMaximumCacheType = 6
	} MEMORY_CACHING_TYPE;

	typedef enum _MM_PAGE_PRIORITY {
		LowPagePriority,
		NormalPagePriority = 16,
		HighPagePriority = 32
	} MM_PAGE_PRIORITY;

	typedef enum _MODE {
		KernelMode,
		UserMode,
		MaximumMode
	} MODE;

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

	typedef struct _MDL {
		_MDL* Next;
		SHORT Size;
		SHORT MdlFlags;
		SHORT AllocationProcessorNumber;
		SHORT Reserved;
		PVOID Process; // EPROCESS
		PVOID MappedSystemVa;
		PVOID StartVa;
		UINT32 ByteCount;
		UINT32 ByteOffset;
	} MDL, *PMDL;



	using POBJECT_TYPE = struct  _OBJECT_TYPE*;
	
	using MmAllocatePagesForMdlFn = PMDL(*)(PHYSICAL_ADDRESS, PHYSICAL_ADDRESS, PHYSICAL_ADDRESS, SIZE_T);
	using MmMapLockedPagesSpecifyCacheFn = PVOID(*)(PVOID, KPROCESSOR_MODE, MEMORY_CACHING_TYPE, PVOID, ULONG, MM_PAGE_PRIORITY);
	using DRIVER_INITIALIZE = NTSTATUS(__stdcall)(
		struct _DRIVER_OBJECT *,
			    PUNICODE_STRING
	);

	typedef DRIVER_INITIALIZE *PDRIVER_INITIALIZE;
	using PCLIENT_ID = CLIENT_ID * ;
	using KSTART_ROUTINE = VOID(PVOID);
	typedef KSTART_ROUTINE *PKSTART_ROUTINE;
	using PsCreateSystemThreadFn =  NTSTATUS(*)(PHANDLE, ULONG, POBJECT_ATTRIBUTES, HANDLE, PCLIENT_ID, PKSTART_ROUTINE, PVOID);
	using ExAllocatePoolWithTagFn = PVOID(*)(POOL_TYPE, SIZE_T, ULONG);
	using RtlFindExportedRoutineByNameFn = void*(__fastcall*)(void *, const char *);
	using IoCreateDriverFn = NTSTATUS(NTAPI*)(PUNICODE_STRING, PDRIVER_INITIALIZE);
	using ZwCloseFn = NTSTATUS(NTAPI*)(HANDLE);
	using ObReferenceObjectByHandleFn = NTSTATUS (NTAPI*)(HANDLE, ACCESS_MASK, POBJECT_TYPE, KPROCESSOR_MODE, PVOID*,PVOID);
	using RtlCopyMemoryFn = void(*)(VOID UNALIGNED*, const VOID UNALIGNED*, SIZE_T);
	using RtlZeroMemoryFn = VOID(*)(_Out_ VOID UNALIGNED *,	_In_  SIZE_T);
}
