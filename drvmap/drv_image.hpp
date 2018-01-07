#pragma once
#include <vector>
#define WIN32_NO_STATUS
#include <Windows.h>
#include <Winternl.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>

#include <functional>
#include <DbgHelp.h>
#include <variant>

#pragma comment(lib, "Dbghelp.lib")
namespace drvmap
{
	class drv_image
	{
		std::vector<uint8_t> m_image;
		std::vector<uint8_t> m_image_mapped;
		PIMAGE_DOS_HEADER m_dos_header = nullptr;
		PIMAGE_NT_HEADERS64 m_nt_headers = nullptr;
		PIMAGE_SECTION_HEADER m_section_header = nullptr;
		
	public:
		explicit drv_image(std::vector<uint8_t> image);
		size_t size() const;
		uintptr_t entry_point() const;
		void map();
		static bool process_relocation(size_t image_base_delta, uint16_t data, uint8_t* relocation_base);
		void relocate(uintptr_t base) const;

		template<typename T>
		__forceinline T* get_rva(const unsigned long offset)
		{
			return (T*)::ImageRvaToVa(m_nt_headers, m_image.data(), offset, nullptr);
		}
	
		void fix_imports(const std::function<uintptr_t(std::string_view)> get_module, const std::function<uintptr_t(uintptr_t, const char*)> get_function, const std::function<uintptr_t(uintptr_t, uint16_t)> get_function_ord );
		void add_cookie(uintptr_t base);
		void* data();
		size_t header_size();
	};
}
