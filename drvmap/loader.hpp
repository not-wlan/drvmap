#pragma once

#include <fstream>
#include "native.hpp"

namespace loader
{
	inline bool load_vuln_driver(uint8_t* driver, int size, const std::wstring& path, const std::wstring& service)
	{
		std::ofstream file(path.c_str(), std::ios_base::out | std::ios_base::binary);
		file.write((char*)driver, size);
		file.close();
		return native::load_driver(path, service);
	}

	inline bool unload_vuln_driver(const std::string& path, const std::wstring& service)
	{
		if (!native::unload_driver(service))
			return false;
		return !std::remove(std::string(path.begin(), path.end()).c_str());
	}
}