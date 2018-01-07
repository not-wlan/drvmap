#pragma once
#include <string>
#include <vector>

namespace drvmap::util
{
	void open_binary_file(const std::string& file, std::vector<uint8_t>& data);
}