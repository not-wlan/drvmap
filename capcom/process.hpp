#pragma once
#include "kernel.hpp"

namespace kernel::process
{

	using PEPROCESS = struct  _EPROCESS*;
	using PACCESS_STATE = struct  _ACCESS_STATE*;
	using POBJECT_TYPE = struct  _OBJECT_TYPE*;
	using KPROCESSOR_MODE = CCHAR;

	extern POBJECT_TYPE* PsProcessType;
	extern NTSTATUS(NTAPI* PsLookupProcessByProcessId)(HANDLE, PEPROCESS*);
	extern VOID(NTAPI* ObDereferenceObject)(PVOID);
	extern NTSTATUS(NTAPI* ObOpenObjectByPointer)(PVOID, ULONG, PACCESS_STATE, ACCESS_MASK, POBJECT_TYPE, KPROCESSOR_MODE, PHANDLE);
	extern NTSTATUS(NTAPI* ZwTerminateProcess)(HANDLE, NTSTATUS);

	void open_process(MmGetSystemRoutineAddressFn MmGetSystemRoutineAddress, HANDLE ProcessId, ACCESS_MASK Access, PHANDLE ReturnedHandle);
	void kill_process(MmGetSystemRoutineAddressFn MmGetSystemRoutineAddress, HANDLE ProcessId, PNTSTATUS Result);
}

