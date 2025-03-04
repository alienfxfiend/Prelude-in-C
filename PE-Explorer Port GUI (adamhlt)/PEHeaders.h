#pragma once
#ifndef PE_ANALYZER_STRUCT_H
#define PE_ANALYZER_STRUCT_H
#include <Windows.h>
#include <winternl.h>
#include <memory>
class FileMapper;

// Core PE analysis structure
struct PEAnalyzer {
    LPVOID lpFileContent;
    DWORD fileSize;
    bool is64Bit;
    PIMAGE_NT_HEADERS32 pNtHeaders32;
    PIMAGE_NT_HEADERS64 pNtHeaders64;
    PIMAGE_DOS_HEADER pDosHeader;
    FileMapper* pMapper; // <<-- inserted

    PEAnalyzer() : lpFileContent(nullptr), fileSize(0), is64Bit(false),
        pNtHeaders32(nullptr), pNtHeaders64(nullptr), pDosHeader(nullptr), pMapper(nullptr) {}

    // Remove destructor as FileMapper will handle cleanup
};
//struct PEAnalyzer;  // Add this before the namespace
// Declare functions that use PEAnalyzer
    template<typename T>
    bool IsSafeToRead(const PEAnalyzer& analyzer, const T* ptr, size_t size);
#endif // PE_ANALYZER_STRUCT_H
namespace PEHelpers {
//ifndef OUTPUT was here

    // Inline function to prevent multiple definition
    inline bool Is64BitPE(PIMAGE_NT_HEADERS pNtHeaders) {
        return pNtHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    }
    //bool ValidateFileSize(const PEAnalyzer& analyzer, DWORD expectedSize, DWORD fileSize);
    bool ValidateFileSize(DWORD expectedSize, DWORD fileSize);
    // Add the template function declaration here
    //template<typename T>
    //bool IsSafeToRead(const PEAnalyzer& analyzer, const T* ptr, size_t size);
    bool IsRvaValid(DWORD rva, DWORD fileSize, PIMAGE_NT_HEADERS pNtHeaders);
    DWORD_PTR GetImageBase(PIMAGE_NT_HEADERS pNtHeaders);
    DWORD GetSizeOfImage(PIMAGE_NT_HEADERS pNtHeaders);
    DWORD_PTR GetRvaPtr(DWORD rva, PIMAGE_SECTION_HEADER pSectionHeader,
        WORD numberOfSections, LPVOID baseAddress);
    DWORD RvaToOffset(DWORD rva, PIMAGE_SECTION_HEADER pSectionHeader,
        WORD numberOfSections);
    std::wstring GetImageCharacteristics(DWORD characteristics);
    std::wstring GetSubsystem(WORD subsystem);
    std::wstring GetDataDirectoryName(int DirectoryNumber);
    std::wstring GetSectionProtection(DWORD characteristics);

    //    using namespace std;
    //    using namespace PEAnalyzer;
        // Core PE validation functions
    // Ensure this function is defined exactly once
    /*inline bool Is64BitPE(PIMAGE_NT_HEADERS pNtHeaders) {
        return pNtHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    }*/

/*bool Is64BitPE(PIMAGE_NT_HEADERS pNtHeaders) {
    return pNtHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
}*/

bool ValidateFileSize(DWORD expectedSize, DWORD fileSize) {
    return expectedSize > 0 && expectedSize <= fileSize;
}

template<typename T>
bool IsSafeToRead(const PEAnalyzer& analyzer, const T* ptr, size_t size = sizeof(T)) {
    // Check if pointer is null
    if (!ptr) {
        OUTPUT("[-] Null pointer detected\n");
        return false;
    }

    // Calculate pointer ranges
    DWORD_PTR startAddress = reinterpret_cast<DWORD_PTR>(analyzer.lpFileContent);
    DWORD_PTR endAddress = startAddress + analyzer.fileSize;
    DWORD_PTR ptrAddress = reinterpret_cast<DWORD_PTR>(ptr);
    DWORD_PTR ptrEndAddress = ptrAddress + size;

    // Comprehensive pointer validation
    bool isValidPointer = (
        ptr != nullptr &&                  // Not null
        ptrAddress >= startAddress &&       // Above file start
        ptrEndAddress <= endAddress &&      // Below file end
        ptrAddress < ptrEndAddress          // Prevent integer overflow
        );

    if (!isValidPointer) {
        OUTPUT("[-] Pointer validation failed\n");
        OUTPUT("    Pointer: 0x%p\n", ptr);
        OUTPUT("    File Start: 0x%p\n", reinterpret_cast<void*>(startAddress));
        OUTPUT("    File End: 0x%p\n", reinterpret_cast<void*>(endAddress));
        OUTPUT("    Pointer Start: 0x%p\n", reinterpret_cast<void*>(ptrAddress));
        OUTPUT("    Pointer End: 0x%p\n", reinterpret_cast<void*>(ptrEndAddress));
        return false;
    }

    return true;
}

bool IsRvaValid(DWORD rva, DWORD fileSize, PIMAGE_NT_HEADERS pNtHeaders) {
    if (!pNtHeaders) {
        OUTPUT("[-] Invalid NT Headers for RVA validation\n");
        return false;
    }

    DWORD sizeOfImage = PEHelpers::Is64BitPE(pNtHeaders) ?
        reinterpret_cast<PIMAGE_NT_HEADERS64>(pNtHeaders)->OptionalHeader.SizeOfImage :
        reinterpret_cast<PIMAGE_NT_HEADERS32>(pNtHeaders)->OptionalHeader.SizeOfImage;

    bool isValid = (
        rva > 0 &&                  // RVA is non-zero
        rva < sizeOfImage&&         // RVA is within image size
        rva < fileSize               // RVA is within file size
        );

    if (!isValid) {
        OUTPUT("[-] Invalid RVA: 0x%X\n", rva);
        OUTPUT("    Size of Image: 0x%X\n", sizeOfImage);
        OUTPUT("    File Size: 0x%X\n", fileSize);
    }

    return isValid;
}

/*bool IsRvaValid(DWORD rva, DWORD fileSize, PIMAGE_NT_HEADERS pNtHeaders) {
    return rva < pNtHeaders->OptionalHeader.SizeOfImage&& rva < fileSize;
}*/

DWORD_PTR GetImageBase(PIMAGE_NT_HEADERS pNtHeaders) {
    if (PEHelpers::Is64BitPE(pNtHeaders)) {  // Add PEHelpers:: namespace qualifier
        auto pNtHeaders64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(pNtHeaders);
        return static_cast<DWORD_PTR>(pNtHeaders64->OptionalHeader.ImageBase);
    }
    auto pNtHeaders32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(pNtHeaders);
    return static_cast<DWORD_PTR>(pNtHeaders32->OptionalHeader.ImageBase);
}

DWORD GetSizeOfImage(PIMAGE_NT_HEADERS pNtHeaders) {
    if (PEHelpers::Is64BitPE(pNtHeaders)) {  // Add PEHelpers:: namespace qualifier
        auto pNtHeaders64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(pNtHeaders);
        return pNtHeaders64->OptionalHeader.SizeOfImage;
    }
    auto pNtHeaders32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(pNtHeaders);
    return pNtHeaders32->OptionalHeader.SizeOfImage;
}

/* // RVA and address conversion helpers
DWORD_PTR GetImageBase(PIMAGE_NT_HEADERS pNtHeaders) {
    if (Is64BitPE(pNtHeaders)) {
        auto pNtHeaders64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(pNtHeaders);
        return static_cast<DWORD_PTR>(pNtHeaders64->OptionalHeader.ImageBase);
    }
    auto pNtHeaders32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(pNtHeaders);
    return static_cast<DWORD_PTR>(pNtHeaders32->OptionalHeader.ImageBase);
}

DWORD GetSizeOfImage(PIMAGE_NT_HEADERS pNtHeaders) {
    if (Is64BitPE(pNtHeaders)) {
        auto pNtHeaders64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(pNtHeaders);
        return pNtHeaders64->OptionalHeader.SizeOfImage;
    }
    auto pNtHeaders32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(pNtHeaders);
    return pNtHeaders32->OptionalHeader.SizeOfImage;
}*/

DWORD_PTR GetRvaPtr(
    DWORD rva,
    PIMAGE_SECTION_HEADER pSectionHeader,
    WORD numberOfSections,
    LPVOID baseAddress,
    DWORD fileSize  // Add file size as a parameter
) {
    if (!rva || !pSectionHeader || !baseAddress) {
        OUTPUT("[-] Invalid parameters in GetRvaPtr\n");
        return 0;
    }

    for (WORD i = 0; i < numberOfSections; i++) {
        // Check if RVA falls within this section
        if (rva >= pSectionHeader[i].VirtualAddress &&
            rva < (pSectionHeader[i].VirtualAddress + pSectionHeader[i].SizeOfRawData)) {

            // Calculate pointer
            DWORD_PTR delta = reinterpret_cast<DWORD_PTR>(baseAddress) +
                pSectionHeader[i].PointerToRawData;

            DWORD_PTR result = delta + (rva - pSectionHeader[i].VirtualAddress);

            // Additional validation
            if (result < reinterpret_cast<DWORD_PTR>(baseAddress) ||
                result >= (reinterpret_cast<DWORD_PTR>(baseAddress) + fileSize)) {
                OUTPUT("[-] RVA pointer out of bounds\n");
                return 0;
            }

            return result;
        }
    }

    OUTPUT("[-] RVA not found in any section: 0x%X\n", rva);
    return 0;
}
//}
//}

/* DWORD_PTR GetRvaPtr(DWORD rva, PIMAGE_SECTION_HEADER pSectionHeader,
    WORD numberOfSections, LPVOID baseAddress) {
    if (!rva || !pSectionHeader || !baseAddress) return 0;

    for (WORD i = 0; i < numberOfSections; i++) {
        if (rva >= pSectionHeader[i].VirtualAddress &&
            rva < (pSectionHeader[i].VirtualAddress + pSectionHeader[i].SizeOfRawData)) {
            DWORD_PTR delta = (DWORD_PTR)baseAddress + pSectionHeader[i].PointerToRawData;
            return delta + (rva - pSectionHeader[i].VirtualAddress);
        }
    }
    OUTPUT("[-] Failed to map RVA: 0x%X\n", rva);
    return 0;
} */

DWORD RvaToOffset(DWORD rva, PIMAGE_SECTION_HEADER pSectionHeader, WORD numberOfSections) {
    if (!rva || !pSectionHeader) return 0;

    for (WORD i = 0; i < numberOfSections; i++) {
        if (rva >= pSectionHeader[i].VirtualAddress &&
            rva < (pSectionHeader[i].VirtualAddress + pSectionHeader[i].SizeOfRawData)) {
            return (rva - pSectionHeader[i].VirtualAddress) + pSectionHeader[i].PointerToRawData;
        }
    }
    OUTPUT("[-] Failed to map RVA: 0x%X\n", rva);
    return 0;
}

// PE information helper functions
std::wstring GetImageCharacteristics(DWORD characteristics) {
    if (characteristics & IMAGE_FILE_DLL) return L"(DLL)";
    if (characteristics & IMAGE_FILE_SYSTEM) return L"(DRIVER)";
    if (characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) return L"(EXE)";
    return L"(UNKNOWN)";
}

std::wstring GetSubsystem(WORD subsystem) {
    switch (subsystem) {
    case IMAGE_SUBSYSTEM_NATIVE: return L"(NATIVE/DRIVER)";
    case IMAGE_SUBSYSTEM_WINDOWS_GUI: return L"(GUI)";
    case IMAGE_SUBSYSTEM_WINDOWS_CUI: return L"(CONSOLE)";
    default: return L"(UNKNOWN)";
    }
}

std::wstring GetDataDirectoryName(int DirectoryNumber) {
    static const wchar_t* names[] = {
        L"Export Table", L"Import Table", L"Resource Table",
        L"Exception Entry", L"Security Entry", L"Relocation Table",
        L"Debug Entry", L"Copyright Entry", L"Global PTR Entry",
        L"TLS Entry", L"Configuration Entry", L"Bound Import Entry",
        L"IAT", L"Delay Import Descriptor", L"COM Descriptor"
    };
    return (DirectoryNumber < 15) ? names[DirectoryNumber] : L"Unknown";
}

std::wstring GetSectionProtection(DWORD characteristics) {
    std::wstring protection = L"(";
    bool needsSeparator = false;

    if (characteristics & IMAGE_SCN_MEM_EXECUTE) {
        protection += L"EXECUTE";
        needsSeparator = true;
    }
    if (characteristics & IMAGE_SCN_MEM_READ) {
        if (needsSeparator) protection += L" | ";
        protection += L"READ";
        needsSeparator = true;
    }
    if (characteristics & IMAGE_SCN_MEM_WRITE) {
        if (needsSeparator) protection += L" | ";
        protection += L"WRITE";
    }
    protection += L")";
    return protection;
}
}