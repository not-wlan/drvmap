#define WIN32_NO_STATUS
#include "capcom.hpp"
#include <intrin.h>
#include <cassert>
#include <Windows.h>
#include <Winternl.h>
#pragma intrinsic(_disable)  
#pragma intrinsic(_enable)  
#undef WIN32_NO_STATUS
#include <ntstatus.h>

namespace capcom
{
	std::unique_ptr<user_function> g_user_function = nullptr;

	void capcom_dispatcher(const kernel::MmGetSystemRoutineAddressFn mm_get_system_routine_address)
	{
		(*g_user_function)(mm_get_system_routine_address);
	}

#pragma pack(push, 1)
	struct capcom_payload
	{
		void* operator new(const std::size_t sz) {
			return VirtualAlloc(nullptr, sz, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		}
		void operator delete(void* ptr, const std::size_t sz) {
			VirtualFree(ptr, 0, MEM_RELEASE);
		}
		auto get() const noexcept -> void* { return const_cast<void**>(&payload_ptr); }
	private:
		void* payload_ptr = movabs_rax;
		uint8_t movabs_rax[2] = { 0x48, 0xB8 };
		void* function_ptr = &capcom_dispatcher;
		uint8_t jmp_rax[2] = { 0xFF, 0xE0 };
	};
#pragma pack(pop)

	unsigned long capcom_run(const driver_handle device, user_function payload_function)
	{
		g_user_function = std::make_unique<user_function>(payload_function);
		const auto payload = std::make_unique<capcom_payload>();
		DWORD output_buffer;
		DWORD bytes_returned;
		if (DeviceIoControl(device.get(), ioctl_x64, payload->get(), 8, &output_buffer, 4, &bytes_returned, nullptr))
			return 0;
		return GetLastError();
	}

	uintptr_t capcom_driver::get_system_routine_internal(const std::wstring& name)
	{
		uintptr_t address = { 0 };
		UNICODE_STRING unicode_name = { 0 };

		RtlInitUnicodeString(&unicode_name, name.c_str());

		const auto fetch = [&unicode_name, &address](kernel::MmGetSystemRoutineAddressFn mm_get_system_routine)
		{
			address = (uintptr_t)mm_get_system_routine(&unicode_name);
		};

		run(fetch);

		return address;
	}

	capcom_driver::capcom_driver()
	{
		m_capcom_driver = driver_handle(CreateFile(device_name, FILE_ALL_ACCESS, FILE_SHARE_READ, nullptr, FILE_OPEN, FILE_ATTRIBUTE_NORMAL, nullptr), CloseHandle);
		assert(m_capcom_driver.get() != INVALID_HANDLE_VALUE);
	}

	void capcom_driver::run(user_function payload, const bool enable_interrupts)
	{
		const auto wrapper = [&payload](kernel::MmGetSystemRoutineAddressFn address)
		{
			_enable();
			payload(address);
			_disable();
		};

		if (enable_interrupts)
			capcom_run(m_capcom_driver, wrapper);
		else
			capcom_run(m_capcom_driver, payload);
	}

	uintptr_t capcom_driver::get_system_routine(const std::wstring& name)
	{
		const auto iter = m_functions.find(name);
		
		if(iter == m_functions.end())
		{
			const auto address = get_system_routine_internal(name);
			if (address != 0)
				m_functions.insert_or_assign(name, address);

			return address;
		}

		return iter->second;
	}

	uintptr_t capcom_driver::get_kernel_module(const std::string_view kmodule)
	{
		NTSTATUS status = 0x0;
		ULONG bytes = 0;
		std::vector<uint8_t> data;
		unsigned long required = 0;


		while ((status = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)11, data.data(), (ULONG)data.size(), &required)) == STATUS_INFO_LENGTH_MISMATCH) {
			data.resize(required);
		}

		if (!NT_SUCCESS(status))
		{
			return 0;
		}
		const auto modules = reinterpret_cast<kernel::PRTL_PROCESS_MODULES>(data.data());
		for (unsigned i = 0; i < modules->NumberOfModules; ++i)
		{
			const auto& driver = modules->Modules[i];
			const auto image_base = reinterpret_cast<uintptr_t>(driver.ImageBase);
			std::string base_name = reinterpret_cast<char*>((uintptr_t)driver.FullPathName + driver.OffsetToFileName);
			const auto offset = base_name.find_last_of(".");

			if (kmodule == base_name)
				return reinterpret_cast<uintptr_t>(driver.ImageBase);

			if (offset != base_name.npos)
				base_name = base_name.erase(offset, base_name.size() - offset);

			if (kmodule == base_name)
				return reinterpret_cast<uintptr_t>(driver.ImageBase);
		}

		return 0;
	}

	uintptr_t capcom_driver::get_export(uintptr_t base, const char* name)
	{
		auto RtlFindExportedRoutineByName = reinterpret_cast<kernel::RtlFindExportedRoutineByNameFn>(get_system_routine(kernel::names::RtlFindExportedRoutineByName));
		assert(RtlFindExportedRoutineByName != nullptr);

		uintptr_t address = { 0 };
		
		const auto _get_export = [&name, &base, &RtlFindExportedRoutineByName, &address](auto mm_get)
		{
			address = (uintptr_t)RtlFindExportedRoutineByName((void*)base, name);
		};

		run(_get_export);

		assert(address != 0);
		return address;
	}

	uintptr_t capcom_driver::allocate_pool(size_t size,  kernel::POOL_TYPE pool_type, const bool page_align, size_t* out_size)
	{
		constexpr auto page_size = 0x1000u;

		uintptr_t address = { 0 };

		if (page_align && size % page_size != 0)
		{
			auto pages = size / page_size;
			size = page_size * ++pages;
		}

		auto ex_allocate_pool = reinterpret_cast<kernel::ExAllocatePoolFn>(get_system_routine(kernel::names::ExAllocatePool));
		assert(ex_allocate_pool != nullptr);

		const auto allocate_fn = [&size, &pool_type, &ex_allocate_pool, &address](auto mm_get)
		{
			address = reinterpret_cast<uintptr_t>(ex_allocate_pool(pool_type, size));
		};

		run(allocate_fn);

		if (out_size != nullptr)
			*out_size = size;

		return address;
	}

	uintptr_t capcom_driver::allocate_pool(size_t size, uint16_t pooltag, kernel::POOL_TYPE pool_type, const bool page_align, size_t* out_size)
	{
		constexpr auto page_size = 0x1000u;

		uintptr_t address = { 0 };

		if(page_align && size % page_size != 0)
		{
			auto pages = size / page_size;
			size = page_size * ++pages;
		}

		auto ex_allocate_pool_with_tag = reinterpret_cast<kernel::ExAllocatePoolWithTagFn>(get_system_routine(kernel::names::ExAllocatePoolWithTag));
		assert(ex_allocate_pool_with_tag != nullptr);

		const auto allocate_fn = [&size, &pooltag, &pool_type, &ex_allocate_pool_with_tag, &address](auto mm_get)
		{
			address = reinterpret_cast<uintptr_t>(ex_allocate_pool_with_tag(pool_type, size, pooltag));
		};

		run(allocate_fn);

		if (out_size != nullptr)
			*out_size = size;

		return address;
	}

}
