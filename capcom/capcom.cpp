#include "capcom.hpp"
#include <intrin.h>
#pragma intrinsic(_disable)  
#pragma intrinsic(_enable)  

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

	uintptr_t get_system_routine(const driver_handle device, const std::wstring& name)
	{
		UNICODE_STRING routine_name;
		RtlInitUnicodeString(&routine_name, name.c_str());
		uintptr_t result = 0;

		const auto kernel_result = capcom_run(device, [&routine_name, &result](auto mm_get_routine) {
			_enable();
			result = (uintptr_t)(mm_get_routine(&routine_name));
			_disable();
		});

		//RtlFreeUnicodeString(&routine_name);

		if (kernel_result != 0)
			result = 0;
		return result;
	}
}
