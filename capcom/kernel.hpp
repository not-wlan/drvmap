#pragma once
#define WIN32_NO_STATUS
#include <Windows.h>
#include <Winternl.h>
#undef WIN32_NO_STATUS
#include <string>

namespace kernel
{
	using MmGetSystemRoutineAddressFn = PVOID(NTAPI*)(PUNICODE_STRING);
}