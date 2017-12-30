#pragma once
#include <functional>
#include <memory>

#include "kernel.hpp"

//Links against ntdll for RtlInitUnicodeString implementation
#pragma comment(lib, "ntdll.lib")

namespace capcom
{
	constexpr auto device_name = "\\\\.\\Htsysm72FB";
	constexpr auto ioctl_x86 = 0xAA012044u;
	constexpr auto ioctl_x64 = 0xAA013044u;

	using user_function = std::function<void(kernel::MmGetSystemRoutineAddressFn)>;
	using driver_handle = std::shared_ptr<std::remove_pointer_t<HANDLE>>;

	unsigned long capcom_run(const driver_handle device, user_function payload);

	uintptr_t get_system_routine(const driver_handle device, const std::wstring& name);
}
