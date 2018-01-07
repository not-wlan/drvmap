#pragma once
#include <functional>
#include <memory>
#include <map>

#include "kernel.hpp"

//Links against ntdll for RtlInitUnicodeString implementation
#pragma comment(lib, "ntdll.lib")

namespace capcom
{
	constexpr auto device_name = TEXT("\\\\.\\Htsysm72FB");
	constexpr auto ioctl_x86 = 0xAA012044u;
	constexpr auto ioctl_x64 = 0xAA013044u;

	using user_function = std::function<void(kernel::MmGetSystemRoutineAddressFn)>;
	using driver_handle = std::shared_ptr<std::remove_pointer_t<HANDLE>>;

	class capcom_driver
	{
		driver_handle m_capcom_driver;
		std::unordered_map<std::wstring, uintptr_t> m_functions;

		uintptr_t get_system_routine_internal(const std::wstring& name);
	public:
		capcom_driver();
		void close_driver_handle();
		void run(user_function, bool enable_interrupts = true);
		uintptr_t get_system_routine(const std::wstring& name);
		static uintptr_t get_kernel_module(const std::string_view kmodule);
		size_t get_header_size(uintptr_t base);
		uintptr_t get_export(uintptr_t base, uint16_t ordinal);
		uintptr_t get_export(uintptr_t base, const char* name);
		uintptr_t allocate_pool(size_t size, kernel::POOL_TYPE pool_type, const bool page_align, size_t* out_size = nullptr);
		uintptr_t allocate_pool(std::size_t size, uint16_t pooltag, kernel::POOL_TYPE = kernel::NonPagedPool, bool page_align = false, size_t* out_size = nullptr);
		template <typename T>
		T get_system_routine(const std::wstring& name)
		{
			return (T)get_system_routine(name);
		}
				
	};

	

}
