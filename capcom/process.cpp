#include "process.hpp"
#include <intrin.h>
#pragma intrinsic(_disable)  
#pragma intrinsic(_enable)  
#include <ntstatus.h>

#pragma comment(lib, "ntdll.lib")

namespace kernel::process
{
	static bool g_initialized;

	decltype(PsLookupProcessByProcessId) PsLookupProcessByProcessId = nullptr;
	decltype(PsProcessType) PsProcessType = nullptr;
	decltype(ObDereferenceObject) ObDereferenceObject = nullptr;
	decltype(ObOpenObjectByPointer)  ObOpenObjectByPointer = nullptr;
	decltype(ZwTerminateProcess) ZwTerminateProcess = nullptr;

	void get_system_routines(MmGetSystemRoutineAddressFn MmGetSystemRoutineAddress)
	{
		UNICODE_STRING
			usPsLookupProcessByProcessId,
			usObDereferenceObject,
			usPsProcessType,
			usObOpenObjectByPointer,
			usZwTerminateProcess;

		RtlInitUnicodeString(&usPsLookupProcessByProcessId, L"PsLookupProcessByProcessId");
		RtlInitUnicodeString(&usObDereferenceObject, L"ObDereferenceObject");
		RtlInitUnicodeString(&usPsProcessType, L"PsProcessType");
		RtlInitUnicodeString(&usObOpenObjectByPointer, L"ObOpenObjectByPointer");
		RtlInitUnicodeString(&usZwTerminateProcess, L"ZwTerminateProcess");

		// MmGetSystemRoutineAddress can only be called at IRQL PASSIVE_LEVEL, and the Capcom driver uses _disable()
		_enable();

		PsLookupProcessByProcessId = static_cast<decltype(PsLookupProcessByProcessId)>(MmGetSystemRoutineAddress(&usPsLookupProcessByProcessId));
		ObDereferenceObject = static_cast<decltype(ObDereferenceObject)>(MmGetSystemRoutineAddress(&usObDereferenceObject));
		PsProcessType = static_cast<decltype(PsProcessType)>(MmGetSystemRoutineAddress(&usPsProcessType));
		ObOpenObjectByPointer = static_cast<decltype(ObOpenObjectByPointer)>(MmGetSystemRoutineAddress(&usObOpenObjectByPointer));
		ZwTerminateProcess = static_cast<decltype(ZwTerminateProcess)>(MmGetSystemRoutineAddress(&usZwTerminateProcess));

		// Disable Interrupts again before returning to execution
		_disable();

		g_initialized = true;
	}

	void kill_process(MmGetSystemRoutineAddressFn MmGetSystemRoutineAddress, HANDLE ProcessId, PNTSTATUS Result)
	{
		if (Result == nullptr) // || ProcessId == nullptr || ProcessId == INVALID_HANDLE_VALUE)
			return;

		*Result = STATUS_SUCCESS;
		PEPROCESS eprocess = { nullptr };
		
		if (!g_initialized) {
			get_system_routines(MmGetSystemRoutineAddress);
		}

		if(ZwTerminateProcess == nullptr)
		{
			*Result = -1;
			return;
		}

		*Result = PsLookupProcessByProcessId(ProcessId, &eprocess);
		
		if(!NT_SUCCESS(*Result))
			return;
	
		HANDLE process_handle;
		*Result = ObOpenObjectByPointer(eprocess, NULL, nullptr, MAXIMUM_ALLOWED, *PsProcessType, 0/*KernelMode*/, &process_handle);

		if(NT_SUCCESS(*Result))
		{
			_enable();
			*Result = ZwTerminateProcess(process_handle, 0);
			_disable();
		}
			
	}

	void open_process(MmGetSystemRoutineAddressFn MmGetSystemRoutineAddress, HANDLE ProcessId, ACCESS_MASK Access, PHANDLE ReturnedHandle)
	{
		NTSTATUS            status = 0;
		PEPROCESS           process = nullptr;
		HANDLE              handle = nullptr;

		if (!g_initialized) {
			get_system_routines(MmGetSystemRoutineAddress);
		}

		__try {
			if (ProcessId != nullptr) {
				status = PsLookupProcessByProcessId(ProcessId, &process);
			}
			if (status >= 0) {
				status = ObOpenObjectByPointer(
					process,
					0,
					nullptr,
					Access,
					*PsProcessType,
					0/*KernelMode*/,
					&handle);
				if (status >= 0) {
					*ReturnedHandle = handle;
				}
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{

		}
		if (process != nullptr)
			ObDereferenceObject(process);
	}
}
