#include <cstdio>
#include <string>
#include "drv_image.hpp"
#include "util.hpp"
#include "capcom.hpp"
#include "structs.hpp"
#include <intrin.h>
#include <cassert>
#include <locale>
#include <variant>

#pragma intrinsic(_disable)  
#pragma intrinsic(_enable)  

#pragma comment(lib, "capcom.lib")

constexpr auto page_size = 0x1000u;
constexpr auto pool_tag = 'naJ?';

#pragma pack(push, 1)
typedef struct dispatch_object
{
	WCHAR name[20] = { 0 };
	drvmap::structs::PDRIVER_INITIALIZE init = { nullptr };
} *p_dispatch_object;
#pragma pack(pop)

uintptr_t get_kernel_module(const capcom::driver_handle capcom, const std::string_view kmodule)
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
		printf("NtQuerySystemInformation failed! Error: 0x%04X\n", status);
		return 0;
	}
	const auto modules = reinterpret_cast<drvmap::structs::PRTL_PROCESS_MODULES>(data.data());
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

#ifdef DEBUG
		printf("driver: %s\n", base_name.c_str());
#endif

		if (kmodule == base_name)
			return reinterpret_cast<uintptr_t>(driver.ImageBase);
	}

	return 0;
}

std::pair<size_t, uintptr_t> allocate_kernel_memory(const capcom::driver_handle capcom, size_t size)
{
	using namespace drvmap::structs;
	uintptr_t image_section;

	static auto ExAllocatePoolWithTag = reinterpret_cast<ExAllocatePoolWithTagFn>(capcom::get_system_routine(capcom, L"ExAllocatePoolWithTag"));
	assert(ExAllocatePoolWithTag != nullptr);

	if (size % page_size == 0)
	{
		printf("buffer size is page aligned. won't resize\n");
	}
	else
	{
		printf("buffer is not page aligned, resizing [0x%I64X] -> ", size);
		size = ((size / page_size) + 1) * page_size;
		printf("[0x%I64X]\n", size);
	}


	capcom::capcom_run(capcom, [&size, &image_section](auto mm_get_routine) {
		_enable();
		image_section = (uintptr_t)ExAllocatePoolWithTag(NonPagedPool, size, pool_tag);
		_disable();
	});

	assert(image_section != 0);

	return std::make_pair(size, image_section);
}

uintptr_t get_export(const capcom::driver_handle capcom, uintptr_t base, const char* name)
{
	using namespace drvmap::structs;
	static auto RtlFindExportedRoutineByName = reinterpret_cast<RtlFindExportedRoutineByNameFn>(capcom::get_system_routine(capcom, L"RtlFindExportedRoutineByName"));
	assert(RtlFindExportedRoutineByName != nullptr);
	uintptr_t address = 0;
	capcom::capcom_run(capcom, [&address, &name, &base](auto mm_get_routine)
	{
		_enable();
		address = reinterpret_cast<uintptr_t>(RtlFindExportedRoutineByName((void*)base, name));
		_disable();
	});
	assert(address != 0);
	return address;
}

uintptr_t get_export(const capcom::driver_handle capcom, uintptr_t base, uint16_t ordinal)
{
	using namespace drvmap::structs;
	static auto RtlFindExportedRoutineByName = reinterpret_cast<RtlFindExportedRoutineByNameFn>(capcom::get_system_routine(capcom, L"RtlFindExportedRoutineByName"));
	assert(RtlFindExportedRoutineByName != nullptr);
	const auto id = MAKEINTRESOURCEA(ordinal);
	uintptr_t address = 0;
	capcom::capcom_run(capcom, [&id, &base, &address](auto mm_get_routine)
	{
		_enable();
		address = reinterpret_cast<uintptr_t>(RtlFindExportedRoutineByName((void*)base, id));
		_disable();
	});
	assert(address != 0);
	return address;
}

int __stdcall main(const int argc, char** argv)
{
	if (argc != 2)
	{
		printf("usage: drvmap.exe <driver>\n");
		return 0;
	}

	const capcom::driver_handle capcom(CreateFile(capcom::device_name, FILE_ALL_ACCESS, FILE_SHARE_READ, nullptr, FILE_OPEN, FILE_ATTRIBUTE_NORMAL, nullptr), CloseHandle);

	if (capcom.get() == INVALID_HANDLE_VALUE)
	{
		printf("CreateFileA failed! error: %u\n", GetLastError());
		return 0;
	}

	std::vector<uint8_t> driver_image;
	drvmap::util::open_binary_file(argv[1], driver_image);
	drvmap::drv_image driver(driver_image);

	const auto kernel_memory = allocate_kernel_memory(capcom, driver.size());

	printf("allocated 0x%llX bytes at 0x%I64X\n", driver.size(), kernel_memory.second);
	
	driver.fix_imports([&capcom](std::string_view name)
	{
		return get_kernel_module(capcom, name);
	}, [&capcom](uintptr_t base, const char* name)
	{
		return get_export(capcom, base, name);
	}, [&capcom](uintptr_t base, uint16_t name)
	{
		return get_export(capcom, base, name);
	});

	driver.map();
	driver.relocate(kernel_memory.second);

	auto RtlCopyMemoryPtr = capcom::get_system_routine(capcom, L"RtlCopyMemory");
	
	assert(RtlCopyMemoryPtr != 0);
	
	using RtlCopyMemoryFn = void(*)(VOID UNALIGNED*, const VOID UNALIGNED*, SIZE_T);
	
	const auto size = driver.size();
	const auto source = driver.data();
	const auto target = reinterpret_cast<void*>(kernel_memory.second);
	const auto entry_point = kernel_memory.second + driver.entry_point();
	auto status = STATUS_SUCCESS;

	capcom::capcom_run(capcom, [&RtlCopyMemoryPtr, &target, &size, &source](auto routine)
	{
		_enable();
		auto _RtlCopyMemory = reinterpret_cast<RtlCopyMemoryFn>(RtlCopyMemoryPtr);
		_RtlCopyMemory(target, source, size);
		_disable();
	});

	printf("calling entry point at 0x%I64X\n", entry_point);

	static auto PsCreateSystemThread = reinterpret_cast<drvmap::structs::PsCreateSystemThreadFn>(capcom::get_system_routine(capcom, L"PsCreateSystemThread"));
	assert(PsCreateSystemThread != nullptr);
	static auto ZwClose = (drvmap::structs::ZwCloseFn)(capcom::get_system_routine(capcom, L"ZwClose"));
	assert(ZwClose != nullptr);

	HANDLE handle;
	OBJECT_ATTRIBUTES obAttr = { 0 };
	InitializeObjectAttributes(&obAttr, nullptr, OBJ_KERNEL_HANDLE, nullptr, nullptr);

	capcom::capcom_run(capcom, [&status, &handle, &obAttr, &entry_point](auto routine)
	{
		_enable();
		using namespace drvmap::structs;
		/*status = PsCreateSystemThread(&handle, GENERIC_READ, &obAttr, nullptr, nullptr, (void(*)(void*))entry_point, nullptr);

		if(NT_SUCCESS(status))
		{
			ZwClose(handle);
		}
		*/
		((PDRIVER_INITIALIZE)entry_point)(nullptr, nullptr);

		_disable();
	});

	if(NT_SUCCESS(status))
	{
		printf("successfully created driver object!\n");
	} else
	{
		printf("creating of driver object failed! 0x%I32X\n", status);
	}

	return 0;
}