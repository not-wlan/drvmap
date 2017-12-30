#include "util.hpp"
#include <iterator>
#include <fstream>

namespace drvmap::util
{
	void open_binary_file(const std::string & file, std::vector<uint8_t>& data)
	{
		std::ifstream file_stream(file, std::ios::binary);
		file_stream.unsetf(std::ios::skipws);
		file_stream.seekg(0, std::ios::end);

		const auto file_size = file_stream.tellg();

		file_stream.seekg(0, std::ios::beg);
		data.reserve(static_cast<uint32_t>(file_size));
		data.insert(data.begin(), std::istream_iterator<uint8_t>(file_stream), std::istream_iterator<uint8_t>());
	}
}
