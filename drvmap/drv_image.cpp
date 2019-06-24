#include "drv_image.hpp"

#include <cassert>

#include <fstream>


namespace drvmap
{
	drv_image::drv_image(std::vector<uint8_t> image) : m_image(std::move(image))
	{
		m_dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(m_image.data());
		assert(m_dos_header->e_magic == IMAGE_DOS_SIGNATURE);
		m_nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS64>((uintptr_t)m_dos_header + m_dos_header->e_lfanew);
		assert(m_nt_headers->Signature == IMAGE_NT_SIGNATURE);
		assert(m_nt_headers->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
		m_section_header = reinterpret_cast<IMAGE_SECTION_HEADER*>((uintptr_t)(&m_nt_headers->OptionalHeader) + m_nt_headers->FileHeader.SizeOfOptionalHeader);
	}

	size_t drv_image::size() const
	{
		return m_nt_headers->OptionalHeader.SizeOfImage;
	}

	uintptr_t drv_image::entry_point() const
	{
		return m_nt_headers->OptionalHeader.AddressOfEntryPoint;
	}

	uint32_t drv_image::rva_to_offset(const uint32_t rva) const
	{
		PIMAGE_SECTION_HEADER section_headers = IMAGE_FIRST_SECTION(m_nt_headers);
		const uint16_t num_sections = m_nt_headers->FileHeader.NumberOfSections;
		uint32_t offset = 0;
		for (uint16_t i = 0; i < num_sections; ++i)
		{
			if (section_headers->VirtualAddress <= rva &&
				section_headers->VirtualAddress + section_headers->Misc.VirtualSize > rva)
			{
				offset = rva - section_headers->VirtualAddress +
								section_headers->PointerToRawData;
				break;
			}
			section_headers++;
		}
		return offset;
	}

	void drv_image::map()
	{

		m_image_mapped.clear();
		m_image_mapped.resize(m_nt_headers->OptionalHeader.SizeOfImage);
		std::copy_n(m_image.begin(), m_nt_headers->OptionalHeader.SizeOfHeaders, m_image_mapped.begin());

		for (size_t i = 0; i < m_nt_headers->FileHeader.NumberOfSections; ++i)
		{
			const auto& section = m_section_header[i];
			const auto target = (uintptr_t)m_image_mapped.data() + section.VirtualAddress;
			const auto source = (uintptr_t)m_dos_header + section.PointerToRawData;
			std::copy_n(m_image.begin() + section.PointerToRawData, section.SizeOfRawData, m_image_mapped.begin() + section.VirtualAddress);

			printf("copying [%s] 0x%p -> 0x%p [0x%04X]\n", &section.Name[0], (void*)source, (void*)target, section.SizeOfRawData);

		}
		//m_dos_header = (PIMAGE_DOS_HEADER)m_image_mapped.data();
		//m_nt_headers = (PIMAGE_NT_HEADERS64)((uintptr_t)m_dos_header + m_dos_header->e_lfanew);
	}

	PIMAGE_BASE_RELOCATION drv_image::process_relocation(uintptr_t va, uint32_t size_of_block, uint16_t* next_offset, intptr_t delta)
	{
		int32_t temp;

		while (size_of_block--)
		{
			const uint16_t offset = *next_offset & static_cast<uint16_t>(0xfff);
			uint8_t* fixupVa = reinterpret_cast<uint8_t*>(va + offset);

			// Apply the fixups.
			switch ((*next_offset) >> 12)
			{
				case IMAGE_REL_BASED_HIGHLOW:
					// HighLow - (32-bits) relocate the high and low half of an address.
					*reinterpret_cast<int32_t UNALIGNED*>(fixupVa) += static_cast<uint32_t>(delta);
					break;

				case IMAGE_REL_BASED_HIGH:
					// High - (16-bits) relocate the high half of an address.
					temp = *reinterpret_cast<uint16_t*>(fixupVa) << 16;
					temp += static_cast<uint32_t>(delta);
					*reinterpret_cast<uint16_t*>(fixupVa) = static_cast<USHORT>(temp >> 16);
					break;

				case IMAGE_REL_BASED_HIGHADJ:
					// Adjust high - (16-bits) relocate the high half of an address and adjust for sign extension of low half.
					// If the address has already been relocated then don't process it again now or information will be lost.
					if (offset & /*LDRP_RELOCATION_FINAL*/ 0x2)
					{
						++next_offset;
						--size_of_block;
						break;
					}

					temp = *reinterpret_cast<uint16_t*>(fixupVa) << 16;
					++next_offset;
					--size_of_block;
					temp += static_cast<int32_t>(*reinterpret_cast<int16_t*>(next_offset));
					temp += static_cast<uint32_t>(delta);
					temp += 0x8000;
					*reinterpret_cast<uint16_t*>(fixupVa) = static_cast<uint16_t>(temp >> 16);

					break;

				case IMAGE_REL_BASED_LOW:
					// Low - (16-bit) relocate the low half of an address.
					temp = *reinterpret_cast<int16_t*>(fixupVa);
					temp += static_cast<uint32_t>(delta);
					*reinterpret_cast<uint16_t*>(fixupVa) = static_cast<uint16_t>(temp);
					break;

				case IMAGE_REL_BASED_DIR64:
					*reinterpret_cast<uintptr_t UNALIGNED*>(fixupVa) += delta;
					break;

				case IMAGE_REL_BASED_ABSOLUTE:
					// Absolute - no fixup required
					break;

				default:
					// Illegal or unsupported relocation type
					return nullptr;
			}

			++next_offset;
		}

		return reinterpret_cast<PIMAGE_BASE_RELOCATION>(next_offset);
	}

	bool drv_image::relocate(uintptr_t base) const
	{
		ULONG relocation_size = 0;
		const auto nt_headers = ImageNtHeader((void*)m_image.data());

		auto next_block = (PIMAGE_BASE_RELOCATION)::ImageDirectoryEntryToData((void*)m_image.data(), FALSE, IMAGE_DIRECTORY_ENTRY_BASERELOC, &relocation_size);
		const intptr_t image_base_delta = static_cast<intptr_t>(base - nt_headers->OptionalHeader.ImageBase);

		// This should check (DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) too but lots of drivers do not have it set due to WDK defaults
		const bool doRelocations = image_base_delta != 0 && relocation_size > 0;

		if (!doRelocations) {
			printf("no relocations needed\n");
			return true;
		}

		if (next_block == nullptr || relocation_size == 0)
		{
			// If there is no relocation directory, this is only OK if it wasn't present at some point but stripped
			return (nt_headers->FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED) == 0;
		}

		// Process the relocation blocks
		while (relocation_size > 0)
		{
			uint32_t size_of_block = next_block->SizeOfBlock;
			if (size_of_block == 0)
				return false;

			relocation_size -= size_of_block;
			size_of_block -= sizeof(IMAGE_BASE_RELOCATION);
			size_of_block /= sizeof(uint16_t);
			uint16_t* next_offset = reinterpret_cast<uint16_t*>(reinterpret_cast<int8_t*>(next_block) + sizeof(IMAGE_BASE_RELOCATION));

			const uintptr_t offset = rva_to_offset(next_block->VirtualAddress);
			const uintptr_t va = reinterpret_cast<uintptr_t>(m_image.data()) + offset;

			next_block = process_relocation(va,
											size_of_block,
											next_offset,
											image_base_delta);
			if (next_block == nullptr)
			{
				printf("failed to relocate!\n");
				return false;
			}
		}

		// Set the new image base in the headers
		nt_headers->OptionalHeader.ImageBase = base;

		return true;
	}

	template<typename T>
	__forceinline T* ptr_add(void* base, uintptr_t offset)
	{
		return (T*)(uintptr_t)base + offset;
	}

	void drv_image::fix_imports(const std::function<uintptr_t(std::string_view)> get_module, const std::function<uintptr_t(uintptr_t, const char*)> get_function, const std::function<uintptr_t(uintptr_t, uint16_t)> get_function_ord){

		ULONG size;
		auto import_descriptors = static_cast<PIMAGE_IMPORT_DESCRIPTOR>(::ImageDirectoryEntryToData(m_image.data(), FALSE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size));

		if (import_descriptors == nullptr) {
			printf("no imports!\n");
			return;
		}

		for (; import_descriptors->Name; import_descriptors++)
		{
			IMAGE_THUNK_DATA *image_thunk_data;

			const auto module_name = get_rva<char>(import_descriptors->Name);
			const auto module_base = get_module(module_name);
			assert(module_base != 0);

			printf("processing module: %s [0x%I64X]\n", module_name, module_base);

			if (import_descriptors->OriginalFirstThunk)
			{
				image_thunk_data = get_rva<IMAGE_THUNK_DATA>(import_descriptors->OriginalFirstThunk);
			}
			else
			{
				image_thunk_data = get_rva<IMAGE_THUNK_DATA>(import_descriptors->FirstThunk);
			}

			auto image_func_data = get_rva<IMAGE_THUNK_DATA64>(import_descriptors->FirstThunk);

			assert(image_thunk_data != nullptr);
			assert(image_func_data != nullptr);

			for (; image_thunk_data->u1.AddressOfData; image_thunk_data++, image_func_data++)
			{
				uintptr_t function_address;
				const auto ordinal = (image_thunk_data->u1.Ordinal & IMAGE_ORDINAL_FLAG64) != 0;

				if(ordinal)
				{
					const auto import_ordinal = static_cast<uint16_t>(image_thunk_data->u1.Ordinal & 0xffff);
					function_address = get_function_ord(module_base, import_ordinal);
					printf("function: %hu [0x%I64X]\n", import_ordinal, function_address);
				} else
				{
					const auto image_import_by_name = get_rva<IMAGE_IMPORT_BY_NAME>(*(DWORD*)image_thunk_data);
					const auto name_of_import = static_cast<char*>(image_import_by_name->Name);
					function_address = get_function(module_base, name_of_import);
					printf("function: %s [0x%I64X]\n", name_of_import, function_address);
				}

				assert(function_address != 0);

				image_func_data->u1.Function = function_address;
			}
		}


	}

	void drv_image::add_cookie(uintptr_t base)
	{
//TODO
	}

	void* drv_image::data()
	{
		return m_image_mapped.data();
	}

	size_t drv_image::header_size()
	{
		return m_nt_headers->OptionalHeader.SizeOfHeaders;
	}
}
