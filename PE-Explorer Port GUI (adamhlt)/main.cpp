#define STRICT
#define WIN32_LEAN_AND_MEAN
#define DEBUG_OUTPUT(format, ...) \
    do { \
        OUTPUT("[DEBUG] %ls:%d - " format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while(0)
#ifndef OUTPUT
#define OUTPUT(format, ...) wprintf(L##format, ##__VA_ARGS__)
#endif
#include <Windows.h>
#include <winternl.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <string>
#include <strsafe.h>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <vector>
#include <shellapi.h>
//#include "helpers.h"
#include "PEHeaders.h"
#include "resource.h"


std::wstring AnsiToWide(const char* str) {
    if (!str) return L"";
    int size = MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0);
    if (size <= 0) return L"";
    std::vector<wchar_t> buf(size);
    MultiByteToWideChar(CP_ACP, 0, str, -1, buf.data(), size);
    return std::wstring(buf.data());
}

std::wstring Utf8ToWide(const char* str) {
    if (!str) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    if (size <= 0) return L"";
    std::vector<wchar_t> buf(size);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, buf.data(), size);
    return std::wstring(buf.data());
}

#pragma comment(lib, "comctl32.lib")

//namespace was here

// Window defines
#define WINDOW_CLASS_NAME L"PEAnalyzerWindow"
#define OUTPUT(format, ...) AppendToOutput(L##format, ##__VA_ARGS__)
#define WINDOW_WIDTH    1024
#define WINDOW_HEIGHT   768
#define EDIT_MARGIN    10

// Global variables
HWND g_hMainWindow = NULL;
HWND g_hEditControl = NULL;
HWND g_hStatusBar = NULL;
HWND g_hProgressBar = NULL;
HFONT g_hFont = NULL;
std::wstringstream g_OutputText;
std::vector<wchar_t> g_OutputBuffer;
std::wstring tempBuffer;
WCHAR filePathW[MAX_PATH];
bool g_FileLoaded = false;

// Resource parsing constants
static const wchar_t* RESOURCE_TYPES[] = {
    L"Unknown",     L"Cursor",      L"Bitmap",      L"Icon",
    L"Menu",        L"Dialog",      L"String",      L"FontDir",
    L"Font",        L"Accelerator", L"RCData",      L"MessageTable",
    L"GroupCursor", L"GroupIcon",   L"Version",     L"DlgInclude"
};

//adding structs here now

enum class FileValidationResult {
    Valid,
    InvalidPath,
    FileNotFound,
    AccessDenied,
    UnsupportedType
};

// Forward declarations for global functions
void UpdateEditControl();
void AppendToOutput(const wchar_t* format, ...);
void SetStatusText(const wchar_t* text);
void ShowProgress(int percentage);
void OpenFileDialog(HWND hwnd);
void AnalyzePEFile(const wchar_t* filePathW);
//analyzepefile 4wd func
HANDLE GetFileContent(const wchar_t* lpFilePath);
void OpenFileDialog(HWND hwnd); // Add these in the global scope, before any namespace declarations

// GUI-related forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam,
    LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
void CreateMainWindow(HINSTANCE hInstance);
void InitializeControls(HWND hwnd);
void AddMenus(HWND hwnd);
//void SetStatusText(const wchar_t* text);
//void ShowProgress(int percentage);
//void UpdateEditControl();    
DWORD GetFileSizeCustom(const wchar_t* filePath);

//bool Is64BitPE(PIMAGE_NT_HEADERS pNtHeaders);


//using namespace std;
//using namespace PEParser;
//using namespace PEHelpers;

//namespace PEParser {

//struct PEAnalyzer;  // Forward declaration
class FileMapper;
FileValidationResult ValidatePEFile(const wchar_t* filePath);
bool InitializePEAnalyzer(PEAnalyzer& analyzer, const wchar_t* filePath);
//class FileValidationResult ValidatePEFile(const wchar_t* filePath);
void ParseSections(const PEAnalyzer& analyzer); //forwarddeclare parse+ bring namespace up
void ParseImportDirectory(const PEAnalyzer& analyzer);
void ParseExportDirectory(const PEAnalyzer& analyzer);
void ParseResourceDirectory(const PEAnalyzer& analyzer);
void ParseDebugDirectory(const PEAnalyzer& analyzer);
void AnalyzeHeaders(const PEAnalyzer& analyzer);
void AnalyzeDataDirectories(const PEAnalyzer& analyzer);
//bool InitializePEAnalyzer(PEAnalyzer& analyzer, const wchar_t* filePath);

//newglobals
void ParseNTHeader64(const PEAnalyzer& analyzer);
void ParseNTHeader32(const PEAnalyzer& analyzer);
void ParseImportFunctions32(const PEAnalyzer& analyzer, PIMAGE_IMPORT_DESCRIPTOR pImportDesc);
void ParseImportFunctions64(const PEAnalyzer& analyzer, PIMAGE_IMPORT_DESCRIPTOR pImportDesc);
void ParseExportedFunctions(const PEAnalyzer& analyzer, PIMAGE_EXPORT_DIRECTORY pExportDir,
    PDWORD pFunctions, PDWORD pNames, PWORD pOrdinals);
void ProcessResourceDirectory(
    PIMAGE_RESOURCE_DIRECTORY resDir,
    int level,
    const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer);
void ProcessNamedResourceEntry(const IMAGE_RESOURCE_DIRECTORY_ENTRY& entry,
    int level, PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer);
void ProcessIdResourceEntry(const IMAGE_RESOURCE_DIRECTORY_ENTRY& entry,
    int level, const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer);
void ProcessResourceSubdirectory(const IMAGE_RESOURCE_DIRECTORY_ENTRY& entry,
    int level, const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer);
void ProcessResourceData(const IMAGE_RESOURCE_DIRECTORY_ENTRY& entry,
    int level, const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer);
void ProcessDebugEntry(const IMAGE_DEBUG_DIRECTORY& debugEntry, const PEAnalyzer& analyzer);
void ProcessRSDSDebugInfo(const IMAGE_DEBUG_DIRECTORY& debugEntry, DWORD* pCVHeader,
    const PEAnalyzer& analyzer);
void ProcessCodeViewDebugInfo(const IMAGE_DEBUG_DIRECTORY& debugEntry, const PEAnalyzer& analyzer);
void ProcessNB10DebugInfo(const IMAGE_DEBUG_DIRECTORY& debugEntry, DWORD* pCVHeader,
    const PEAnalyzer& analyzer);
//void ParseImportDirectory(const PEAnalyzer& analyzer);
//void ParseExportDirectory(const PEAnalyzer& analyzer);
//DWORD GetFileSizeCustom(const wchar_t* filePath);
//endnewglobals

//using namespace std;
//using namespace PEHelpers;
//struct was here


// File mapping helper class
class FileMapper {
private:
    HANDLE hFile;
    HANDLE hMapping;
    LPVOID lpView;

public:
    FileMapper() : hFile(INVALID_HANDLE_VALUE), hMapping(nullptr), lpView(nullptr) {}
    ~FileMapper() { Cleanup(); }

    void Cleanup() {
        if (lpView) {
            UnmapViewOfFile(lpView);
            lpView = nullptr;
        }
        if (hMapping) {
            CloseHandle(hMapping);
            hMapping = nullptr;
        }
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }
    }

        bool Initialize(const wchar_t* path) {
        Cleanup();
        
        hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            OUTPUT("[-] Failed to open file\n");
            return false;
        }

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize)) {
            OUTPUT("[-] Failed to get file size\n");
            Cleanup();
            return false;
        }

        // Create mapping without SEC_IMAGE first
        hMapping = CreateFileMappingW(hFile, nullptr, PAGE_READONLY,
            fileSize.HighPart, fileSize.LowPart, nullptr);
        if (!hMapping) {
            OUTPUT("[-] Failed to create file mapping\n");
            Cleanup();
            return false;
        }

        lpView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if (!lpView) {
            OUTPUT("[-] Failed to map view of file\n");
            Cleanup();
            return false;
        }

        return true;
        }

        LPVOID GetView() const { return lpView; }
};

FileValidationResult ValidatePEFile(const wchar_t* filePath) {
    if (!filePath || wcslen(filePath) == 0) {
        OUTPUT("[-] Null or empty file path\n");
        return FileValidationResult::InvalidPath;
    }

    // Check file extension
    const wchar_t* ext = wcsrchr(filePath, L'.');
    if (!ext || (wcscmp(ext, L".exe") != 0 && wcscmp(ext, L".dll") != 0)) {
        OUTPUT("[-] Unsupported file type. Only .exe and .dll are supported\n");
        return FileValidationResult::UnsupportedType;
    }

    // Detailed file access check
    HANDLE hFile = CreateFileW(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        switch (error) {
        case ERROR_FILE_NOT_FOUND:
            OUTPUT("[-] File not found: %ls\n", filePath);
            return FileValidationResult::FileNotFound;
        case ERROR_ACCESS_DENIED:
            OUTPUT("[-] Access denied. Check file permissions: %ls\n", filePath);
            return FileValidationResult::AccessDenied;
        default:
            OUTPUT("[-] File open error %d: %ls\n", error, filePath);
            return FileValidationResult::InvalidPath;
        }
    }

    // Additional checks
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        OUTPUT("[-] Cannot determine file size\n");
        return FileValidationResult::InvalidPath;
    }

    CloseHandle(hFile);
    return FileValidationResult::Valid;
}

// Initialization function
bool InitializePEAnalyzer(PEAnalyzer& analyzer, const wchar_t* filePath) {
    FileValidationResult validationResult = ValidatePEFile(filePath);
    if (validationResult != FileValidationResult::Valid) {
        OUTPUT("[-] File validation failed with code: %d\n", static_cast<int>(validationResult));
        return false;
    }

        /*if (!analyzer.lpFileContent) {
            OUTPUT("[-] No file content available\n");
            return false;
        }*/ //chatgpt4omitted

    analyzer.pMapper = new FileMapper();
    if (!analyzer.pMapper->Initialize(filePath)) {
        OUTPUT("[-] Failed to map file. Detailed error in FileMapper\n");
        return false;
    }

    analyzer.lpFileContent = analyzer.pMapper->GetView();
    analyzer.fileSize = GetFileSizeCustom(filePath);
    if (!analyzer.lpFileContent) {
        OUTPUT("[-] Memory mapping failed\n");
        return false;
    }

    // Validate DOS Header
    analyzer.pDosHeader = static_cast<PIMAGE_DOS_HEADER>(analyzer.lpFileContent);
    if (!IsSafeToRead(analyzer, analyzer.pDosHeader, sizeof(IMAGE_DOS_HEADER)) ||
        analyzer.pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        OUTPUT("[-] Invalid DOS signature: 0x%X (Expected 0x%X)\n",
            analyzer.pDosHeader->e_magic, IMAGE_DOS_SIGNATURE);
        return false;
    }

    // Validate NT Header Offset
    if (analyzer.pDosHeader->e_lfanew >= analyzer.fileSize ||
        analyzer.pDosHeader->e_lfanew < sizeof(IMAGE_DOS_HEADER)) {
        OUTPUT("[-] Invalid e_lfanew value: 0x%X\n", analyzer.pDosHeader->e_lfanew);
        return false;
    }

    /* // Comprehensive PE signature validation
        if (!analyzer.lpFileContent || analyzer.fileSize == 0) {
        OUTPUT("[-] Memory mapping failed or file size is zero\n");
        return false;
    }

        // Add bounds checking
        if (analyzer.fileSize < sizeof(IMAGE_DOS_HEADER)) {
            OUTPUT("[-] File too small to contain DOS header\n");
            return false;
        }

    // DOS Header validation with extensive checks
        analyzer.pDosHeader = static_cast<PIMAGE_DOS_HEADER>(analyzer.lpFileContent);

        // Validate DOS header
        //if (!IsSafeToRead(analyzer, analyzer.pDosHeader, sizeof(IMAGE_DOS_HEADER))) {
        //    OUTPUT("[-] Invalid DOS header access\n");
            return false;
        //}

    // More robust DOS signature check
        if (analyzer.pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            OUTPUT("[-] Invalid DOS signature: 0x%X (Expected 0x%X)\n",
                analyzer.pDosHeader->e_magic, IMAGE_DOS_SIGNATURE);
            return false;
        }

    // Validate e_lfanew with more checks
    if (analyzer.pDosHeader->e_lfanew >= analyzer.fileSize ||
        analyzer.pDosHeader->e_lfanew < sizeof(IMAGE_DOS_HEADER)) {
        OUTPUT("[-] Invalid e_lfanew value: 0x%X\n", analyzer.pDosHeader->e_lfanew);
        return false;
    } */ //chatgpt4omitted

    /*analyzer.lpFileContent = mapper.GetView();
    if (!analyzer.lpFileContent) {
        OUTPUT("[-] Failed to get file view. Memory mapping issue.\n");
        return false;
    }

    analyzer.fileSize = GetFileSizeCustom(filePath);
    if (!analyzer.fileSize) {
        OUTPUT("[-] Unable to determine file size. File might be empty.\n");
        return false;
    }*/

    // Validate file content
    /*if (analyzer.fileSize < sizeof(IMAGE_DOS_HEADER)) {
        OUTPUT("[-] File too small to be a valid PE\n");
        return false;
    }

    analyzer.pDosHeader = static_cast<PIMAGE_DOS_HEADER>(analyzer.lpFileContent);
    if (!analyzer.pDosHeader || analyzer.pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        OUTPUT("[-] Invalid DOS signature\n");
        return false;
    }

    // Validate e_lfanew
    if (analyzer.pDosHeader->e_lfanew >= analyzer.fileSize ||
        analyzer.pDosHeader->e_lfanew < sizeof(IMAGE_DOS_HEADER)) {
        OUTPUT("[-] Invalid e_lfanew value\n");
        return false;
    }*/

    // Validate NT Headers
    LONG ntHeaderOffset = analyzer.pDosHeader->e_lfanew;
    auto pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
        static_cast<BYTE*>(analyzer.lpFileContent) + ntHeaderOffset);

    if (!IsSafeToRead(analyzer, pNtHeaders, sizeof(IMAGE_NT_HEADERS)) ||
        pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        OUTPUT("[-] Invalid NT signature: 0x%X (Expected 0x%X)\n",
            pNtHeaders->Signature, IMAGE_NT_SIGNATURE);
        return false;
    }

    /* // Validate NT Headers location
    LONG ntHeaderOffset = analyzer.pDosHeader->e_lfanew;
    if (ntHeaderOffset <= 0 ||
        ntHeaderOffset >= analyzer.fileSize ||
        ntHeaderOffset < sizeof(IMAGE_DOS_HEADER)) {
        OUTPUT("[-] Invalid NT Headers offset: 0x%X\n", ntHeaderOffset);
        return false;
    }

    // NT Headers signature validation
    auto pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
        static_cast<BYTE*>(analyzer.lpFileContent) + ntHeaderOffset);

    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        OUTPUT("[-] Invalid NT signature: 0x%X (Expected 0x%X)\n",
            pNtHeaders->Signature,
            IMAGE_NT_SIGNATURE);
        return false;
    } */ //chatgpt4omitted

    /*if (IsBadReadPtr(pNtHeaders, sizeof(IMAGE_NT_HEADERS)) ||
        pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        OUTPUT("[-] Invalid NT signature\n");
        return false;
    }*/

    // Determine architecture with additional safety checks
        // Determine Architecture
    /* WORD magicNumber = pNtHeaders->OptionalHeader.Magic;
    analyzer.is64Bit = (magicNumber == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    if (analyzer.is64Bit) {
        analyzer.pNtHeaders64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(pNtHeaders);
        analyzer.pNtHeaders32 = nullptr;
    }
    else {
        analyzer.pNtHeaders32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(pNtHeaders);
        analyzer.pNtHeaders64 = nullptr;
    }*/ //chatgpt4omitted

    // Determine Architecture
    WORD magicNumber = pNtHeaders->OptionalHeader.Magic;
    analyzer.is64Bit = (magicNumber == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    if (analyzer.is64Bit) {
        analyzer.pNtHeaders64 = reinterpret_cast<PIMAGE_NT_HEADERS64>(pNtHeaders);
        analyzer.pNtHeaders32 = nullptr;
    }
    else {
        analyzer.pNtHeaders32 = reinterpret_cast<PIMAGE_NT_HEADERS32>(pNtHeaders);
        analyzer.pNtHeaders64 = nullptr;
    }

    return true;
}


//    return true;
//}
/* catch (const std::exception& ex) {
    OUTPUT("[-] Initialization error: %s\n", ex.what());
    return false;
}
catch (...) {
    OUTPUT("[-] Unknown initialization error\n");
    return false;
}
} */ //chatgpt4omitted

// Helper function for file size
DWORD GetFileSizeCustom(const wchar_t* filePath) {
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    DWORD fileSize = ::GetFileSize(hFile, nullptr);
    CloseHandle(hFile);
    return fileSize;
} //removing other not this duplicate

// Main analysis function
void AnalyzePEFile(const wchar_t* filePathW) {
    if (!filePathW || wcslen(filePathW) == 0) {
        OUTPUT("[-] Invalid file path\n");
        SetStatusText(L"Invalid file path!");
        ShowProgress(0);
        return;
    }


    /* // Check file extension
    const wchar_t* ext = wcsrchr(filePathW, L'.');
    if (!ext || (wcscmp(ext, L".exe") != 0 && wcscmp(ext, L".dll") != 0)) {
        OUTPUT("[-] Unsupported file type. Use .exe or .dll\n");
        SetStatusText(L"Unsupported file type!");
        ShowProgress(0);
        return;
    } */

    /* // Check if file exists
    HANDLE hFile = CreateFileW(filePathW, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        OUTPUT("[-] Cannot open file. Error code: %d\n", error);
        SetStatusText(L"Failed to open file!");
        ShowProgress(0);
        return;
    }
    CloseHandle(hFile); */

    // Reset and prepare for analysis
        g_OutputText.str(L"");
        g_OutputText.clear();
        UpdateEditControl();

        OUTPUT("[+] Starting PE Analysis for: %ls\n\n", filePathW);
        SetStatusText(L"Analyzing PE File...");
        ShowProgress(10);
    //ShowProgress(10);

        PEAnalyzer analyzer;
        try {
            if (!InitializePEAnalyzer(analyzer, filePathW)) {
                OUTPUT("[-] PE Initialization Failed\n");
                SetStatusText(L"PE Analysis Failed");
                ShowProgress(0);
                UpdateEditControl();
                return;
            }

        // Add safety check
        /*if (!analyzer.lpFileContent || !analyzer.pDosHeader) {
            OUTPUT("[-] Invalid PE file structure\n");
            SetStatusText(L"Invalid PE File");
            ShowProgress(0);
            UpdateEditControl();
            return;
        }*/


        /* analyzer.lpFileContent = mapper.GetView();
        analyzer.fileSize = GetFileSizeCustom(filePathW);

        if (!InitializePEAnalyzer(analyzer, filePathW)) {
            OUTPUT("[-] PE Initialization Failed\n");
            SetStatusText(L"Analysis Failed - Invalid PE");
            ShowProgress(0);
            return;
        } */ //chatgpt4omitted

        // Detailed logging for architecture
        OUTPUT("[+] File Architecture: %ls\n", analyzer.is64Bit ? L"64-bit" : L"32-bit");

        // Show initial progress
        ShowProgress(10);
        /*
        // Add null checks before processing
        if (!analyzer.lpFileContent) {
            OUTPUT("[-] No file content available\n");
            SetStatusText(L"Failed to read file content!");
            ShowProgress(0);
            return;
        }

        // Inside AnalyzePEFile, before parsing headers
        if (analyzer.is64Bit) {
            if (!PEHelpers::ValidateFileSize(analyzer.pNtHeaders64->OptionalHeader.SizeOfImage, analyzer.fileSize)) {
                OUTPUT("[-] PE file size mismatch with 64-bit image size!\n");
                throw std::runtime_error("Invalid file size or PE image");
            }
        }
        else {
            if (!PEHelpers::ValidateFileSize(analyzer.pNtHeaders32->OptionalHeader.SizeOfImage, analyzer.fileSize)) {
                OUTPUT("[-] PE file size mismatch with 32-bit image size!\n");
                throw std::runtime_error("Invalid file size or PE image");
            }
        }
        */
        // Wrap each analysis step in its own try-catch
        // Basic headers analysis
        try {
            ShowProgress(20);
            AnalyzeHeaders(analyzer);
        }
        catch (const std::exception& ex) {
            OUTPUT("[-] Header Analysis Error: %ls\n", ex.what());
            throw;  // Re-throw to handle in the outer catch block
        }
        //AnalyzeHeaders(analyzer);

        // Data directories
        try {
            ShowProgress(30);
            AnalyzeDataDirectories(analyzer);
        }
        catch (const std::exception& ex) {
            OUTPUT("[-] Data Directories Analysis Error: %ls\n", ex.what());
            throw;  // Re-throw to handle in the outer catch block
        }
        //AnalyzeDataDirectories(analyzer);

        // Parse sections
        try {
            ShowProgress(40);
            ParseSections(analyzer);
        }
        catch (const std::exception& ex) {
            OUTPUT("[-] Sections Parsing Error: %ls\n", ex.what());
            throw;  // Re-throw to handle in the outer catch block
        }

        //ParseSections(analyzer);

        // Parse imports
        try {
            ShowProgress(50);
            ParseImportDirectory(analyzer);
        }
        catch (const std::exception& ex) {
            OUTPUT("[-] Import Directory Parsing Error: %ls\n", ex.what());
            throw;  // Re-throw to handle in the outer catch block
        }
        //ParseImportDirectory(analyzer);

        // Parse exports
        try {
            ShowProgress(60);            
            ParseExportDirectory(analyzer);
        }
        catch (const std::exception& ex) {
            OUTPUT("[-] Export Directory Parsing Error: %ls\n", ex.what());
            throw;  // Re-throw to handle in the outer catch block
        }
        //ParseExportDirectory(analyzer);

        // Parse resources
        try {
            ShowProgress(70);
            ParseResourceDirectory(analyzer);
        }
        catch (const std::exception& ex) {
            OUTPUT("[-] Resource Directory Parsing Error: %ls\n", ex.what());
            throw;  // Re-throw to handle in the outer catch block
        }
        //ParseResourceDirectory(analyzer);

        // Parse debug info
        try {
            ShowProgress(80);            
            ParseDebugDirectory(analyzer);
        }
        catch (const std::exception& ex) {
            OUTPUT("[-] Debug Directory Parsing Error: %ls\n", ex.what());
            throw;  // Re-throw to handle in the outer catch block
        }
        //ParseDebugDirectory(analyzer);
        
        ShowProgress(100);
        SetStatusText(L"Analysis Complete");

        // Mark that a file is loaded.
        g_FileLoaded = true;

        // Enable the "Select All" menu item.
        HMENU hMenu = GetMenu(g_hMainWindow);
        HMENU hFileMenu = GetSubMenu(hMenu, 0); // Assuming "File" is the first menu
        EnableMenuItem(hFileMenu, 2, MF_BYCOMMAND | MF_ENABLED);
        //EnableMenuItem(hFileMenu, ID_FILE_SELECTALL, MF_BYCOMMAND | MF_ENABLED);
        DrawMenuBar(g_hMainWindow);

        if (analyzer.pMapper) {
            delete analyzer.pMapper;
            analyzer.pMapper = nullptr;
        }
    }
    //}
    catch (const std::exception& ex) {
        OUTPUT("[-] Unexpected Exception: %ls\n", ex.what());
        SetStatusText(L"Analysis Failed");
        ShowProgress(0);
    }
    catch (...) {
        OUTPUT("[-] Unknown critical error during PE analysis\n");
        SetStatusText(L"Critical Failure");
        ShowProgress(0);
    }
    /*catch (const std::exception& ex) {
        OUTPUT("[-] Exception during analysis: %s\n", ex.what());
        SetStatusText(L"Analysis failed due to error.");
        ShowProgress(0);
        return;
    }*/ //newly commented out
    /*catch (const std::exception& e) {
        OUTPUT("[-] Error during analysis: %s\n", e.what());
        SetStatusText(L"Analysis failed!");
    }*/

    //ShowWindow(g_hProgressBar, SW_HIDE);
    // Ensure edit control is updated
    UpdateEditControl();
    analyzer.~PEAnalyzer(); // Explicit call to the destructor if not already invoked
}

//amalgamating parsing funcs here experimental

    // Header analysis function
void AnalyzeHeaders(const PEAnalyzer& analyzer) {
    if (!analyzer.lpFileContent || !analyzer.pDosHeader) {
        throw std::runtime_error("Invalid PE headers");
    }

    OUTPUT("[+] PE IMAGE INFORMATION\n\n");
    OUTPUT("[+] Architecture: %ls\n\n", analyzer.is64Bit ? L"x64" : L"x86");

    // Validate DOS Header
    if (!IsSafeToRead(analyzer, analyzer.pDosHeader, sizeof(IMAGE_DOS_HEADER))) {
        throw std::runtime_error("Invalid DOS header");
    }

    // DOS Header
    OUTPUT("[+] DOS HEADER\n");
    OUTPUT("\te_magic    : 0x%X\n", analyzer.pDosHeader->e_magic);
    OUTPUT("\te_cblp     : 0x%X\n", analyzer.pDosHeader->e_cblp);
    OUTPUT("\te_cp       : 0x%X\n", analyzer.pDosHeader->e_cp);
    OUTPUT("\te_crlc     : 0x%X\n", analyzer.pDosHeader->e_crlc);
    OUTPUT("\te_cparhdr  : 0x%X\n", analyzer.pDosHeader->e_cparhdr);
    OUTPUT("\te_minalloc : 0x%X\n", analyzer.pDosHeader->e_minalloc);
    OUTPUT("\te_maxalloc : 0x%X\n", analyzer.pDosHeader->e_maxalloc);
    OUTPUT("\te_ss       : 0x%X\n", analyzer.pDosHeader->e_ss);
    OUTPUT("\te_sp       : 0x%X\n", analyzer.pDosHeader->e_sp);
    OUTPUT("\te_csum     : 0x%X\n", analyzer.pDosHeader->e_csum);
    OUTPUT("\te_ip       : 0x%X\n", analyzer.pDosHeader->e_ip);
    OUTPUT("\te_cs       : 0x%X\n", analyzer.pDosHeader->e_cs);
    OUTPUT("\te_lfarlc   : 0x%X\n", analyzer.pDosHeader->e_lfarlc);
    OUTPUT("\te_ovno     : 0x%X\n", analyzer.pDosHeader->e_ovno);
    OUTPUT("\te_oemid    : 0x%X\n", analyzer.pDosHeader->e_oemid);
    OUTPUT("\te_oeminfo  : 0x%X\n", analyzer.pDosHeader->e_oeminfo);
    OUTPUT("\te_lfanew   : 0x%X\n\n", analyzer.pDosHeader->e_lfanew);

    // Validate NT Headers
    if (analyzer.is64Bit && analyzer.pNtHeaders64) {
        ParseNTHeader64(analyzer);
    }
    else if (analyzer.pNtHeaders32) {
        ParseNTHeader32(analyzer);
    }
    else {
        throw std::runtime_error("Invalid NT headers");
    }
}

void ParseNTHeader32(const PEAnalyzer& analyzer) {
    if (!analyzer.pNtHeaders32) {
        throw std::runtime_error("Invalid NT headers");
    }

    OUTPUT("[+] NT HEADER (32-bit)\n");
    OUTPUT("\tSignature: 0x%X\n\n", analyzer.pNtHeaders32->Signature);

    // File Header
    OUTPUT("[+] FILE HEADER\n");
    OUTPUT("\tMachine: 0x%X\n", analyzer.pNtHeaders32->FileHeader.Machine);
    OUTPUT("\tNumberOfSections: 0x%X\n", analyzer.pNtHeaders32->FileHeader.NumberOfSections);
    OUTPUT("\tTimeDateStamp: 0x%X\n", analyzer.pNtHeaders32->FileHeader.TimeDateStamp);
    OUTPUT("\tPointerToSymbolTable: 0x%X\n", analyzer.pNtHeaders32->FileHeader.PointerToSymbolTable);
    OUTPUT("\tNumberOfSymbols: 0x%X\n", analyzer.pNtHeaders32->FileHeader.NumberOfSymbols);
    OUTPUT("\tSizeOfOptionalHeader: 0x%X\n", analyzer.pNtHeaders32->FileHeader.SizeOfOptionalHeader);
    OUTPUT("\tCharacteristics: 0x%X\n\n", analyzer.pNtHeaders32->FileHeader.Characteristics);

    // Optional Header
    OUTPUT("[+] OPTIONAL HEADER\n");
    OUTPUT("\tMagic: 0x%X\n", analyzer.pNtHeaders32->OptionalHeader.Magic);
    OUTPUT("\tAddressOfEntryPoint: 0x%X\n", analyzer.pNtHeaders32->OptionalHeader.AddressOfEntryPoint);
    OUTPUT("\tImageBase: 0x%X\n", analyzer.pNtHeaders32->OptionalHeader.ImageBase);
    OUTPUT("\tSectionAlignment: 0x%X\n", analyzer.pNtHeaders32->OptionalHeader.SectionAlignment);
    OUTPUT("\tFileAlignment: 0x%X\n", analyzer.pNtHeaders32->OptionalHeader.FileAlignment);
    OUTPUT("\tSizeOfImage: 0x%X\n", analyzer.pNtHeaders32->OptionalHeader.SizeOfImage);
    OUTPUT("\tSizeOfHeaders: 0x%X\n", analyzer.pNtHeaders32->OptionalHeader.SizeOfHeaders);
    OUTPUT("\tSubsystem: 0x%X\n\n", analyzer.pNtHeaders32->OptionalHeader.Subsystem);
}

void ParseNTHeader64(const PEAnalyzer& analyzer) {
    if (!analyzer.pNtHeaders64) {
        throw std::runtime_error("Invalid NT headers");
    }

    OUTPUT("[+] NT HEADER (64-bit)\n");
    OUTPUT("\tSignature: 0x%X\n\n", analyzer.pNtHeaders64->Signature);

    // File Header
    OUTPUT("[+] FILE HEADER\n");
    OUTPUT("\tMachine: 0x%X\n", analyzer.pNtHeaders64->FileHeader.Machine);
    OUTPUT("\tNumberOfSections: 0x%X\n", analyzer.pNtHeaders64->FileHeader.NumberOfSections);
    OUTPUT("\tTimeDateStamp: 0x%X\n", analyzer.pNtHeaders64->FileHeader.TimeDateStamp);
    OUTPUT("\tPointerToSymbolTable: 0x%X\n", analyzer.pNtHeaders64->FileHeader.PointerToSymbolTable);
    OUTPUT("\tNumberOfSymbols: 0x%X\n", analyzer.pNtHeaders64->FileHeader.NumberOfSymbols);
    OUTPUT("\tSizeOfOptionalHeader: 0x%X\n", analyzer.pNtHeaders64->FileHeader.SizeOfOptionalHeader);
    OUTPUT("\tCharacteristics: 0x%X\n\n", analyzer.pNtHeaders64->FileHeader.Characteristics);

    // Optional Header
    OUTPUT("[+] OPTIONAL HEADER\n");
    OUTPUT("\tMagic: 0x%X\n", analyzer.pNtHeaders64->OptionalHeader.Magic);
    OUTPUT("\tAddressOfEntryPoint: 0x%X\n", analyzer.pNtHeaders64->OptionalHeader.AddressOfEntryPoint);
    OUTPUT("\tImageBase: 0x%llX\n", analyzer.pNtHeaders64->OptionalHeader.ImageBase);
    OUTPUT("\tSectionAlignment: 0x%X\n", analyzer.pNtHeaders64->OptionalHeader.SectionAlignment);
    OUTPUT("\tFileAlignment: 0x%X\n", analyzer.pNtHeaders64->OptionalHeader.FileAlignment);
    OUTPUT("\tSizeOfImage: 0x%X\n", analyzer.pNtHeaders64->OptionalHeader.SizeOfImage);
    OUTPUT("\tSizeOfHeaders: 0x%X\n", analyzer.pNtHeaders64->OptionalHeader.SizeOfHeaders);
    OUTPUT("\tSubsystem: 0x%X\n\n", analyzer.pNtHeaders64->OptionalHeader.Subsystem);
}
//}

        //russianheat (clipped peanalyzefile() code here)


        // Data Directories analysis
void AnalyzeDataDirectories(const PEAnalyzer& analyzer) {
    OUTPUT("[+] DATA DIRECTORIES\n");
    const IMAGE_DATA_DIRECTORY* dataDirectories = analyzer.is64Bit ?
        analyzer.pNtHeaders64->OptionalHeader.DataDirectory :
        analyzer.pNtHeaders32->OptionalHeader.DataDirectory;

    for (int i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++) {
        if (dataDirectories[i].VirtualAddress != 0) {
            OUTPUT("\t%ls:\n", PEHelpers::GetDataDirectoryName(i).c_str());
            OUTPUT("\t\tVirtualAddress: 0x%X\n", dataDirectories[i].VirtualAddress);
            OUTPUT("\t\tSize: 0x%X\n", dataDirectories[i].Size);
        }
    }
    OUTPUT("\n");
}

// Safe memory check helper
template<typename T>
bool IsSafeToRead(const PEAnalyzer& analyzer, const T* ptr, size_t size) {
    if (!ptr) return false;
    DWORD_PTR start = reinterpret_cast<DWORD_PTR>(analyzer.lpFileContent);
    DWORD_PTR end = start + analyzer.fileSize;
    DWORD_PTR ptrAddr = reinterpret_cast<DWORD_PTR>(ptr);
    return (ptrAddr >= start && (ptrAddr + size) <= end);
}

// Section parsing with safety checks
void ParseSections(const PEAnalyzer& analyzer) {
    OUTPUT("[+] SECTION HEADERS\n");

    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(
        analyzer.is64Bit ?
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders64) :
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32));

    WORD numberOfSections = analyzer.is64Bit ?
        analyzer.pNtHeaders64->FileHeader.NumberOfSections :
        analyzer.pNtHeaders32->FileHeader.NumberOfSections;

    if (!IsSafeToRead(analyzer, pSection,
        sizeof(IMAGE_SECTION_HEADER) * numberOfSections)) {
        throw std::runtime_error("Invalid section headers");
    }

    for (WORD i = 0; i < numberOfSections; i++, pSection++) {
        char sectionName[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
        memcpy(sectionName, pSection->Name, IMAGE_SIZEOF_SHORT_NAME);

        // Sanitize section name
        for (int j = 0; j < IMAGE_SIZEOF_SHORT_NAME; j++) {
            if (!isprint(static_cast<unsigned char>(sectionName[j]))) {
                sectionName[j] = '\0';
                break;
            }
        }

        // Output section information        
        OUTPUT("\tSECTION: %ls\n", AnsiToWide(sectionName).c_str());
        OUTPUT("\t\tVirtualSize: 0x%X\n", pSection->Misc.VirtualSize);
        OUTPUT("\t\tVirtualAddress: 0x%X\n", pSection->VirtualAddress);
        OUTPUT("\t\tSizeOfRawData: 0x%X\n", pSection->SizeOfRawData);
        OUTPUT("\t\tPointerToRawData: 0x%X\n", pSection->PointerToRawData);
        OUTPUT("\t\tPointerToRelocations: 0x%X\n", pSection->PointerToRelocations);
        OUTPUT("\t\tPointerToLinenumbers: 0x%X\n", pSection->PointerToLinenumbers);
        OUTPUT("\t\tNumberOfRelocations: 0x%X\n", pSection->NumberOfRelocations);
        OUTPUT("\t\tNumberOfLinenumbers: 0x%X\n", pSection->NumberOfLinenumbers);
        OUTPUT("\t\tCharacteristics: 0x%X %ls\n\n",
            pSection->Characteristics,
            PEHelpers::GetSectionProtection(pSection->Characteristics).c_str());
    }
} //end of parsesection here

    // Import Directory parsing with safety checks
void ParseImportDirectory(const PEAnalyzer& analyzer) {
    const auto& importDir = analyzer.is64Bit ?
        analyzer.pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] :
        analyzer.pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    if (!importDir.VirtualAddress || !importDir.Size) {
        OUTPUT("\n[-] No import directory found.\n");
        return;
    }

    OUTPUT("\n[+] IMPORT DIRECTORY (%ls)\n", analyzer.is64Bit ? L"64-bit" : L"32-bit");

    auto pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)PEHelpers::GetRvaPtr(
        importDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.is64Bit ?
            (PIMAGE_NT_HEADERS)analyzer.pNtHeaders64 :
            (PIMAGE_NT_HEADERS)analyzer.pNtHeaders32),
        analyzer.is64Bit ?
        analyzer.pNtHeaders64->FileHeader.NumberOfSections :
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!IsSafeToRead(analyzer, pImportDesc, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
        throw std::runtime_error("Invalid import descriptor");
    }

    while (pImportDesc->Name != 0) {
        const char* dllName = (const char*)PEHelpers::GetRvaPtr(
            pImportDesc->Name,
            IMAGE_FIRST_SECTION(analyzer.is64Bit ?
                reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders64) :
                reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32)),
            analyzer.is64Bit ?
            analyzer.pNtHeaders64->FileHeader.NumberOfSections :
            analyzer.pNtHeaders32->FileHeader.NumberOfSections,
            analyzer.lpFileContent,
            analyzer.fileSize);

        if (!IsSafeToRead(analyzer, dllName, 1)) {
            break;
        }

        if (dllName && !IsBadReadPtr(dllName, 1)) {
            OUTPUT("\n\tDLL NAME: %ls\n", AnsiToWide(dllName).c_str());
            OUTPUT("\tCharacteristics: 0x%X\n", pImportDesc->Characteristics);
            OUTPUT("\tTimeDateStamp: 0x%X\n", pImportDesc->TimeDateStamp);
            OUTPUT("\tForwarderChain: 0x%X\n", pImportDesc->ForwarderChain);
            OUTPUT("\tFirstThunk: 0x%X\n\n", pImportDesc->FirstThunk);
            OUTPUT("\tImported Functions:\n");
        }

        // Parse functions based on architecture
        if (analyzer.is64Bit) {
            ParseImportFunctions64(analyzer, pImportDesc);
        }
        else {
            ParseImportFunctions32(analyzer, pImportDesc);
        }

        pImportDesc++;
        if (!IsSafeToRead(analyzer, pImportDesc, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
            break;
        }
    }
}

void ParseImportFunctions32(const PEAnalyzer& analyzer, PIMAGE_IMPORT_DESCRIPTOR pImportDesc) {
    auto pThunk = (PIMAGE_THUNK_DATA32)PEHelpers::GetRvaPtr(
        pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk : pImportDesc->FirstThunk,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    while (pThunk && pThunk->u1.AddressOfData) {
        if (!(pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32)) {
            auto pImportByName = (PIMAGE_IMPORT_BY_NAME)PEHelpers::GetRvaPtr(
                pThunk->u1.AddressOfData,
                IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
                analyzer.pNtHeaders32->FileHeader.NumberOfSections,
                analyzer.lpFileContent,
                analyzer.fileSize);

            if (pImportByName && !IsBadReadPtr(pImportByName, sizeof(IMAGE_IMPORT_BY_NAME))) {
                wchar_t wFuncName[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, pImportByName->Name, -1, wFuncName, MAX_PATH);
                OUTPUT("\t\t%ls\n", wFuncName);
            }
        }
        else {
            OUTPUT("\t\tOrdinal: %d\n", pThunk->u1.Ordinal & 0xFFFF);
        }
        pThunk++;
    }
}

void ParseImportFunctions64(const PEAnalyzer& analyzer, PIMAGE_IMPORT_DESCRIPTOR pImportDesc) {
    auto pThunk = (PIMAGE_THUNK_DATA64)PEHelpers::GetRvaPtr(
        pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk : pImportDesc->FirstThunk,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    while (pThunk && pThunk->u1.AddressOfData) {
        if (!(pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
            auto pImportByName = (PIMAGE_IMPORT_BY_NAME)PEHelpers::GetRvaPtr(
                (DWORD)pThunk->u1.AddressOfData,
                IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
                analyzer.pNtHeaders64->FileHeader.NumberOfSections,
                analyzer.lpFileContent,
                analyzer.fileSize);

            if (pImportByName && !IsBadReadPtr(pImportByName, sizeof(IMAGE_IMPORT_BY_NAME))) {
                wchar_t wFuncName[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, pImportByName->Name, -1, wFuncName, MAX_PATH);
                OUTPUT("\t\t%ls\n", wFuncName);
            }
        }
        else {
            OUTPUT("\t\tOrdinal: %lld\n", pThunk->u1.Ordinal & 0xFFFF);
        }
        pThunk++;
    }
}

// Export Directory parsing with safety checks
void ParseExportDirectory(const PEAnalyzer& analyzer) {
    const auto& exportDir = analyzer.is64Bit ?
        analyzer.pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT] :
        analyzer.pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

    if (!exportDir.VirtualAddress || !exportDir.Size) {
        OUTPUT("\n[-] No export directory found.\n");
        return;
    }

    OUTPUT("\n[+] EXPORT DIRECTORY (%ls)\n", analyzer.is64Bit ? L"64-bit" : L"32-bit");

    auto pExportDir = (PIMAGE_EXPORT_DIRECTORY)PEHelpers::GetRvaPtr(
        exportDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.is64Bit ?
            reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders64) :
            reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32)),
        analyzer.is64Bit ?
        analyzer.pNtHeaders64->FileHeader.NumberOfSections :
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!IsSafeToRead(analyzer, pExportDir, sizeof(IMAGE_EXPORT_DIRECTORY))) {
        throw std::runtime_error("Invalid export directory");
    }

    // Output export directory information
    OUTPUT("\tCharacteristics: 0x%X\n", pExportDir->Characteristics);
    OUTPUT("\tTimeDateStamp: 0x%X\n", pExportDir->TimeDateStamp);
    OUTPUT("\tMajorVersion: %d\n", pExportDir->MajorVersion);
    OUTPUT("\tMinorVersion: %d\n", pExportDir->MinorVersion);
    OUTPUT("\tName: 0x%X\n", pExportDir->Name);
    OUTPUT("\tBase: %d\n", pExportDir->Base);
    OUTPUT("\tNumberOfFunctions: %d\n", pExportDir->NumberOfFunctions);
    OUTPUT("\tNumberOfNames: %d\n", pExportDir->NumberOfNames);
    OUTPUT("\tAddressOfFunctions: 0x%X\n", pExportDir->AddressOfFunctions);
    OUTPUT("\tAddressOfNames: 0x%X\n", pExportDir->AddressOfNames);
    OUTPUT("\tAddressOfNameOrdinals: 0x%X\n\n", pExportDir->AddressOfNameOrdinals);

    // Get export tables with safety checks

    // Add IMAGE_FIRST_SECTION and section count parameters
    auto pFunctions = (PDWORD)PEHelpers::GetRvaPtr(pExportDir->AddressOfFunctions,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    auto pNames = (PDWORD)PEHelpers::GetRvaPtr(pExportDir->AddressOfNames,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    auto pOrdinals = (PWORD)PEHelpers::GetRvaPtr(pExportDir->AddressOfNameOrdinals,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    //auto pFunctions = (PDWORD)PEHelpers::GetRvaPtr(pExportDir->AddressOfFunctions, /*...*/);
    //auto pNames = (PDWORD)PEHelpers::GetRvaPtr(pExportDir->AddressOfNames, /*...*/);
    //auto pOrdinals = (PWORD)PEHelpers::GetRvaPtr(pExportDir->AddressOfNameOrdinals, /*...*/);

    if (!IsSafeToRead(analyzer, pFunctions, sizeof(DWORD) * pExportDir->NumberOfFunctions) ||
        !IsSafeToRead(analyzer, pNames, sizeof(DWORD) * pExportDir->NumberOfNames) ||
        !IsSafeToRead(analyzer, pOrdinals, sizeof(WORD) * pExportDir->NumberOfNames)) {
        throw std::runtime_error("Invalid export tables");
    }

    // Parse and output exported functions
    ParseExportedFunctions(analyzer, pExportDir, pFunctions, pNames, pOrdinals);
}

void ParseExportedFunctions(const PEAnalyzer& analyzer, PIMAGE_EXPORT_DIRECTORY pExportDir,
    PDWORD pFunctions, PDWORD pNames, PWORD pOrdinals) {

    OUTPUT("\tExported Functions:\n\n");
    for (DWORD i = 0; i < pExportDir->NumberOfNames; i++) {
        if (IsBadReadPtr(pNames + i, sizeof(DWORD)) ||
            IsBadReadPtr(pOrdinals + i, sizeof(WORD))) {
            break;
        }

        const char* functionName = (const char*)PEHelpers::GetRvaPtr(
            pNames[i],
            analyzer.is64Bit ? IMAGE_FIRST_SECTION((PIMAGE_NT_HEADERS64)analyzer.pNtHeaders64) : IMAGE_FIRST_SECTION((PIMAGE_NT_HEADERS32)analyzer.pNtHeaders32),
            analyzer.is64Bit ? analyzer.pNtHeaders64->FileHeader.NumberOfSections : analyzer.pNtHeaders32->FileHeader.NumberOfSections,
            analyzer.lpFileContent,
            analyzer.fileSize);

        /*const char* functionName = (const char*)PEHelpers::GetRvaPtr(
            pNames[i],
            IMAGE_FIRST_SECTION(analyzer.is64Bit ? analyzer.pNtHeaders64 : analyzer.pNtHeaders32),
            analyzer.is64Bit ? analyzer.pNtHeaders64->FileHeader.NumberOfSections :
            analyzer.pNtHeaders32->FileHeader.NumberOfSections,
            analyzer.lpFileContent);*/

        if (functionName && !IsBadReadPtr(functionName, 1)) {
            WORD ordinal = pOrdinals[i];
            if (ordinal < pExportDir->NumberOfFunctions) {
                DWORD functionRva = pFunctions[ordinal];
                wchar_t wFunctionName[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, functionName, -1, wFunctionName, MAX_PATH);
                OUTPUT("\t\t%ls (Ordinal: %d, RVA: 0x%08X)\n", wFunctionName, ordinal + pExportDir->Base, functionRva);
            }
        }
    }
}

void ParseResourceDirectory(const PEAnalyzer& analyzer) {
    const auto& resourceDir = analyzer.is64Bit ?
        analyzer.pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE] :
        analyzer.pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];

    if (!resourceDir.VirtualAddress || !resourceDir.Size) {
        OUTPUT("\n[-] No resource directory found.\n");
        return;
    }

    OUTPUT("\n[+] RESOURCE DIRECTORY (%ls)\n", analyzer.is64Bit ? L"64-bit" : L"32-bit");

    auto pResourceDir = (PIMAGE_RESOURCE_DIRECTORY)PEHelpers::GetRvaPtr(
        resourceDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.is64Bit ?
            reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders64) :
            reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32)),
        analyzer.is64Bit ?
        analyzer.pNtHeaders64->FileHeader.NumberOfSections :
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!IsSafeToRead(analyzer, pResourceDir, sizeof(IMAGE_RESOURCE_DIRECTORY))) {
        throw std::runtime_error("Invalid resource directory");
    }

    ProcessResourceDirectory(pResourceDir, 0, nullptr, pResourceDir, analyzer);
}

void ProcessResourceDirectory(
    PIMAGE_RESOURCE_DIRECTORY resDir,
    int level,
    const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer)
{
    if (!IsSafeToRead(analyzer, resDir, sizeof(IMAGE_RESOURCE_DIRECTORY))) {
        return;
    }

    auto entry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resDir + 1);
    WORD totalEntries = resDir->NumberOfNamedEntries + resDir->NumberOfIdEntries;

    for (WORD i = 0; i < totalEntries; i++) {
        if (!IsSafeToRead(analyzer, entry + i, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY))) {
            break;
        }

        // Indent based on level
        for (int indent = 0; indent < level; indent++) {
            OUTPUT("\t");
        }

        // Process named entries
        if (entry[i].NameIsString) {
            ProcessNamedResourceEntry(entry[i], level, baseResourceDir, analyzer);
        }
        // Process ID entries
        else {
            ProcessIdResourceEntry(entry[i], level, type, baseResourceDir, analyzer);
        }

        // Process subdirectories or data
        if (entry[i].DataIsDirectory) {
            ProcessResourceSubdirectory(entry[i], level, type, baseResourceDir, analyzer);
        }
        else {
            ProcessResourceData(entry[i], level, type, baseResourceDir, analyzer);
        }
    }
}

//newlyfrom You,com Claude
void ProcessNamedResourceEntry(const IMAGE_RESOURCE_DIRECTORY_ENTRY& entry,
    int level, PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer) {

    auto nameEntry = (PIMAGE_RESOURCE_DIR_STRING_U)((BYTE*)baseResourceDir + entry.NameOffset);
    if (!IsBadReadPtr(nameEntry, sizeof(IMAGE_RESOURCE_DIR_STRING_U))) {
        std::vector<wchar_t> resourceName(nameEntry->Length + 1);
        wcsncpy_s(resourceName.data(), nameEntry->Length + 1,
            nameEntry->NameString, nameEntry->Length);
        resourceName[nameEntry->Length] = L'\0';

        if (level == 0) {
            OUTPUT("Resource Type: Custom (%ls)\n", resourceName.data());
        }
        else {
            OUTPUT("Name: %ls\n", resourceName.data());
        }
    }
}

void ProcessIdResourceEntry(const IMAGE_RESOURCE_DIRECTORY_ENTRY& entry,
    int level, const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer) {

    if (level == 0) {
        DWORD resourceType = entry.Id;
        if (resourceType < 16) {
            OUTPUT("Resource Type: %ls (ID: %d)\n", RESOURCE_TYPES[resourceType], resourceType);
        }
        else {
            OUTPUT("Resource Type: Custom (ID: %d)\n", resourceType);
        }
    }
    else {
        OUTPUT("ID: %d\n", entry.Id);
    }
}

void ProcessResourceSubdirectory(const IMAGE_RESOURCE_DIRECTORY_ENTRY& entry,
    int level, const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer) {

    auto nextDir = (PIMAGE_RESOURCE_DIRECTORY)((BYTE*)baseResourceDir + entry.OffsetToDirectory);
    if (!IsBadReadPtr(nextDir, sizeof(IMAGE_RESOURCE_DIRECTORY))) {
        ProcessResourceDirectory(nextDir, level + 1,
            level == 0 ? RESOURCE_TYPES[min(entry.Id, 15)] : type,
            baseResourceDir, analyzer);
    }
}

void ProcessResourceData(const IMAGE_RESOURCE_DIRECTORY_ENTRY& entry,
    int level, const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const PEAnalyzer& analyzer) {

    auto dataEntry = (PIMAGE_RESOURCE_DATA_ENTRY)((BYTE*)baseResourceDir + entry.OffsetToData);
    if (!IsBadReadPtr(dataEntry, sizeof(IMAGE_RESOURCE_DATA_ENTRY))) {
        for (int indent = 0; indent < level + 1; indent++) {
            OUTPUT("\t");
        }
        OUTPUT("Size: %d bytes, RVA: 0x%X\n", dataEntry->Size, dataEntry->OffsetToData);
    }
}

void ParseDebugDirectory(const PEAnalyzer& analyzer) {
    const auto& debugDir = analyzer.is64Bit ?
        analyzer.pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG] :
        analyzer.pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

    if (!debugDir.VirtualAddress || !debugDir.Size) {
        OUTPUT("\n[-] No debug directory found.\n");
        return;
    }

    OUTPUT("\n[+] DEBUG DIRECTORY (%ls)\n", analyzer.is64Bit ? L"64-bit" : L"32-bit");

    auto pDebugDir = (PIMAGE_DEBUG_DIRECTORY)PEHelpers::GetRvaPtr(
        debugDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.is64Bit ?
            reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders64) :
            reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32)),
        analyzer.is64Bit ?
        analyzer.pNtHeaders64->FileHeader.NumberOfSections :
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!IsSafeToRead(analyzer, pDebugDir, sizeof(IMAGE_DEBUG_DIRECTORY))) {
        throw std::runtime_error("Invalid debug directory");
    }

    DWORD numEntries = min(debugDir.Size / sizeof(IMAGE_DEBUG_DIRECTORY), 16);
    for (DWORD i = 0; i < numEntries; i++) {
        ProcessDebugEntry(pDebugDir[i], analyzer);
    }
}

void ProcessDebugEntry(const IMAGE_DEBUG_DIRECTORY& debugEntry, const PEAnalyzer& analyzer) {
    OUTPUT("\tDebug Entry:\n");
    OUTPUT("\tCharacteristics: 0x%X\n", debugEntry.Characteristics);
    OUTPUT("\tTimeDateStamp: 0x%X\n", debugEntry.TimeDateStamp);
    OUTPUT("\tVersion: %d.%d\n", debugEntry.MajorVersion, debugEntry.MinorVersion);
    OUTPUT("\tType: 0x%X", debugEntry.Type);

    // Output debug type
    switch (debugEntry.Type) {
    case IMAGE_DEBUG_TYPE_COFF:
        OUTPUT(" (COFF)\n"); break;
    case IMAGE_DEBUG_TYPE_CODEVIEW:
        OUTPUT(" (CodeView)\n");
        ProcessCodeViewDebugInfo(debugEntry, analyzer);
        break;
        // ... other debug types ...
    default:
        OUTPUT(" (Unknown)\n"); break;
    }

    OUTPUT("\tSizeOfData: 0x%X\n", debugEntry.SizeOfData);
    OUTPUT("\tAddressOfRawData: 0x%X\n", debugEntry.AddressOfRawData);
    OUTPUT("\tPointerToRawData: 0x%X\n\n", debugEntry.PointerToRawData);
}

// Helper functions for processing specific debug info types
void ProcessCodeViewDebugInfo(const IMAGE_DEBUG_DIRECTORY& debugEntry, const PEAnalyzer& analyzer) {
    if (debugEntry.Type != IMAGE_DEBUG_TYPE_CODEVIEW ||
        !debugEntry.PointerToRawData ||
        debugEntry.SizeOfData < sizeof(DWORD)) {
        return;
    }

    auto pCVHeader = (DWORD*)((BYTE*)analyzer.lpFileContent + debugEntry.PointerToRawData);
    if (!IsSafeToRead(analyzer, pCVHeader, sizeof(DWORD))) {
        return;
    }

    // Process different CodeView formats
    switch (*pCVHeader) {
    case 0x53445352: // 'RSDS'
        ProcessRSDSDebugInfo(debugEntry, pCVHeader, analyzer);
        break;
    case 0x3031424E: // 'NB10'
        ProcessNB10DebugInfo(debugEntry, pCVHeader, analyzer);
        break;
    }
}

void ProcessRSDSDebugInfo(const IMAGE_DEBUG_DIRECTORY& debugEntry, DWORD* pCVHeader,
    const PEAnalyzer& analyzer) {

    if (debugEntry.SizeOfData >= (sizeof(DWORD) + sizeof(GUID) + sizeof(DWORD) + 1)) {
        auto pCVData = (char*)(pCVHeader + 1);
        if (!IsBadReadPtr(pCVData + 16, 1)) {
            auto guid = (GUID*)pCVData;
            DWORD age = *(DWORD*)(pCVData + 16);
            const char* pdbPath = pCVData + 20;

            OUTPUT("\tPDB Information:\n");
            OUTPUT("\tGUID: {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
                guid->Data1, guid->Data2, guid->Data3,
                guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
            OUTPUT("\tAge: %d\n", age);
            OUTPUT("\tPDB Path: %ls\n\n", AnsiToWide(pdbPath).c_str());
        }
    }
}

void ProcessNB10DebugInfo(const IMAGE_DEBUG_DIRECTORY& debugEntry, DWORD* pCVHeader,
    const PEAnalyzer& analyzer) {

    if (debugEntry.SizeOfData >= 16) {
        auto pNB10Data = (char*)(pCVHeader + 1);
        DWORD offset = *(DWORD*)pNB10Data;
        DWORD timestamp = *(DWORD*)(pNB10Data + 4);
        DWORD age = *(DWORD*)(pNB10Data + 8);
        const char* pdbPath = pNB10Data + 12;

        OUTPUT("\tPDB Information (NB10):\n");
        OUTPUT("\tOffset: 0x%X\n", offset);
        OUTPUT("\tTimestamp: 0x%X\n", timestamp);
        OUTPUT("\tAge: %d\n", age);
        OUTPUT("\tPDB Path: %ls\n\n", pdbPath);
    }
}

//colonelburton

void ParseImportDirectory32(const PEAnalyzer& analyzer) {
    const auto& importDir = analyzer.pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir.VirtualAddress || !importDir.Size) return;

    if (!PEHelpers::IsRvaValid(importDir.VirtualAddress, analyzer.fileSize,
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32))) {
        OUTPUT("[-] Invalid import directory RVA!\n");
        throw std::runtime_error("RVA out of bounds");
        return;
    }

    OUTPUT("\n[+] IMPORT DIRECTORY (32-bit)\n");
    auto pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)PEHelpers::GetRvaPtr(
        importDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pImportDesc) return;

    while (pImportDesc->Name != 0) {
        if (IsBadReadPtr(pImportDesc, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
            OUTPUT("[-] Invalid import descriptor detected!\n");
            break;
        }

        const char* dllName = (const char*)PEHelpers::GetRvaPtr(
            pImportDesc->Name,
            IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
            analyzer.pNtHeaders32->FileHeader.NumberOfSections,
            analyzer.lpFileContent,
            analyzer.fileSize);

        if (dllName && !IsBadReadPtr(dllName, 1)) {
            std::vector<wchar_t> wideDllName(MAX_PATH);
            MultiByteToWideChar(CP_UTF8, 0, dllName, -1, wideDllName.data(), MAX_PATH);

            OUTPUT("\n\tDLL NAME: %ls\n", wideDllName.data());
            OUTPUT("\tCharacteristics: 0x%X\n", pImportDesc->Characteristics);
            OUTPUT("\tTimeDateStamp: 0x%X\n", pImportDesc->TimeDateStamp);
            OUTPUT("\tForwarderChain: 0x%X\n", pImportDesc->ForwarderChain);
            OUTPUT("\tFirstThunk: 0x%X\n", pImportDesc->FirstThunk);
            OUTPUT("\n\tImported Functions:\n");

            auto pThunk = (PIMAGE_THUNK_DATA32)PEHelpers::GetRvaPtr(
                pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk : pImportDesc->FirstThunk,
                IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
                analyzer.pNtHeaders32->FileHeader.NumberOfSections,
                analyzer.lpFileContent,
                analyzer.fileSize);

            while (pThunk && pThunk->u1.AddressOfData) {
                if (!(pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32)) {
                    auto pImportByName = (PIMAGE_IMPORT_BY_NAME)PEHelpers::GetRvaPtr(
                        pThunk->u1.AddressOfData,
                        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
                        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
                        analyzer.lpFileContent,
                        analyzer.fileSize);

                    if (pImportByName && !IsBadReadPtr(pImportByName, sizeof(IMAGE_IMPORT_BY_NAME))) {
                        std::vector<wchar_t> wideFuncName(MAX_PATH);
                        MultiByteToWideChar(CP_ACP, 0, (char*)pImportByName->Name, -1,
                            wideFuncName.data(), MAX_PATH);
                        OUTPUT("\t\t%ls\n", wideFuncName.data());
                    }
                }
                else {
                    OUTPUT("\t\tOrdinal: %d\n", pThunk->u1.Ordinal & 0xFFFF);
                }
                pThunk++;
            }
        }
        pImportDesc++;
    }
}

void ParseImportDirectory64(const PEAnalyzer& analyzer) {
    const auto& importDir = analyzer.pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir.VirtualAddress || !importDir.Size) return;

    if (!PEHelpers::IsRvaValid(importDir.VirtualAddress, analyzer.fileSize,
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders64))) {
        OUTPUT("[-] Invalid import directory RVA!\n");
        throw std::runtime_error("RVA out of bounds");
        return;
    }

    OUTPUT("\n[+] IMPORT DIRECTORY (64-bit)\n");
    auto pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)PEHelpers::GetRvaPtr(
        importDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pImportDesc) return;

    while (pImportDesc->Name != 0) {
        if (IsBadReadPtr(pImportDesc, sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
            OUTPUT("[-] Invalid import descriptor detected!\n");
            break;
        }

        const char* dllName = (const char*)PEHelpers::GetRvaPtr(
            pImportDesc->Name,
            IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
            analyzer.pNtHeaders64->FileHeader.NumberOfSections,
            analyzer.lpFileContent,
            analyzer.fileSize);

        if (dllName && !IsBadReadPtr(dllName, 1)) {
            std::vector<wchar_t> wideDllName(MAX_PATH);
            MultiByteToWideChar(CP_UTF8, 0, dllName, -1, wideDllName.data(), MAX_PATH);

            OUTPUT("\n\tDLL NAME: %ls\n", wideDllName.data());
            OUTPUT("\tCharacteristics: 0x%X\n", pImportDesc->Characteristics);
            OUTPUT("\tTimeDateStamp: 0x%X\n", pImportDesc->TimeDateStamp);
            OUTPUT("\tForwarderChain: 0x%X\n", pImportDesc->ForwarderChain);
            OUTPUT("\tFirstThunk: 0x%X\n", pImportDesc->FirstThunk);
            OUTPUT("\n\tImported Functions:\n");

            auto pThunk = (PIMAGE_THUNK_DATA64)PEHelpers::GetRvaPtr(
                pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk : pImportDesc->FirstThunk,
                IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
                analyzer.pNtHeaders64->FileHeader.NumberOfSections,
                analyzer.lpFileContent,
                analyzer.fileSize);

            while (pThunk && pThunk->u1.AddressOfData) {
                if (!(pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                    auto pImportByName = (PIMAGE_IMPORT_BY_NAME)PEHelpers::GetRvaPtr(
                        (DWORD)pThunk->u1.AddressOfData,
                        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
                        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
                        analyzer.lpFileContent,
                        analyzer.fileSize);

                    if (pImportByName && !IsBadReadPtr(pImportByName, sizeof(IMAGE_IMPORT_BY_NAME))) {
                        std::vector<wchar_t> wideFuncName(MAX_PATH);
                        MultiByteToWideChar(CP_ACP, 0, (char*)pImportByName->Name, -1,
                            wideFuncName.data(), MAX_PATH);
                        OUTPUT("\t\t%ls\n", wideFuncName.data());
                    }
                }
                else {
                    OUTPUT("\t\tOrdinal: %lld\n", pThunk->u1.Ordinal & 0xFFFF);
                }
                pThunk++;
            }
        }
        pImportDesc++;
    }
}

/*void ParseImportDirectory(const PEAnalyzer& analyzer) {
    if (analyzer.is64Bit) {
        ParseImportDirectory64(analyzer);
    }
    else {
        ParseImportDirectory32(analyzer);
    }
}*/ //removing the empty content duplicates
//}

void ParseExportDirectory32(const PEAnalyzer& analyzer) {
    const auto& exportDir = analyzer.pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!exportDir.VirtualAddress || !exportDir.Size) return;

    if (!PEHelpers::IsRvaValid(exportDir.VirtualAddress, analyzer.fileSize,
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32))) {
        OUTPUT("[-] Invalid export directory RVA!\n");
        throw std::runtime_error("RVA out of bounds");
        return;
    }

    OUTPUT("\n[+] EXPORT DIRECTORY (32-bit)\n");
    auto pExportDir = (PIMAGE_EXPORT_DIRECTORY)PEHelpers::GetRvaPtr(
        exportDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pExportDir || IsBadReadPtr(pExportDir, sizeof(IMAGE_EXPORT_DIRECTORY))) {
        OUTPUT("[-] Invalid export directory structure!\n");
        return;
    }

    OUTPUT("\tCharacteristics: 0x%X\n", pExportDir->Characteristics);
    OUTPUT("\tTimeDateStamp: 0x%X\n", pExportDir->TimeDateStamp);
    OUTPUT("\tMajorVersion: %d\n", pExportDir->MajorVersion);
    OUTPUT("\tMinorVersion: %d\n", pExportDir->MinorVersion);
    OUTPUT("\tName: 0x%X\n", pExportDir->Name);
    OUTPUT("\tBase: %d\n", pExportDir->Base);
    OUTPUT("\tNumberOfFunctions: %d\n", pExportDir->NumberOfFunctions);
    OUTPUT("\tNumberOfNames: %d\n", pExportDir->NumberOfNames);
    OUTPUT("\tAddressOfFunctions: 0x%X\n", pExportDir->AddressOfFunctions);
    OUTPUT("\tAddressOfNames: 0x%X\n", pExportDir->AddressOfNames);
    OUTPUT("\tAddressOfNameOrdinals: 0x%X\n\n", pExportDir->AddressOfNameOrdinals);

    auto pFunctions = (PDWORD)PEHelpers::GetRvaPtr(
        pExportDir->AddressOfFunctions,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    auto pNames = (PDWORD)PEHelpers::GetRvaPtr(
        pExportDir->AddressOfNames,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    auto pNameOrdinals = (PWORD)PEHelpers::GetRvaPtr(
        pExportDir->AddressOfNameOrdinals,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pNames || !pNameOrdinals || !pFunctions) {
        OUTPUT("[-] Invalid export address tables!\n");
        return;
    }

    OUTPUT("\tExported Functions:\n\n");
    for (DWORD i = 0; i < pExportDir->NumberOfNames; i++) {
        if (IsBadReadPtr(pNames + i, sizeof(DWORD)) ||
            IsBadReadPtr(pNameOrdinals + i, sizeof(WORD))) {
            break;
        }

        const char* functionName = (const char*)PEHelpers::GetRvaPtr(
            pNames[i],
            IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
            analyzer.pNtHeaders32->FileHeader.NumberOfSections,
            analyzer.lpFileContent,
            analyzer.fileSize);

        if (functionName && !IsBadReadPtr(functionName, 1)) {
            WORD ordinal = pNameOrdinals[i];
            if (ordinal < pExportDir->NumberOfFunctions) {
                DWORD functionRva = pFunctions[ordinal];

                // Check for forwarded export
                if (functionRva >= exportDir.VirtualAddress &&
                    functionRva < (exportDir.VirtualAddress + exportDir.Size)) {

                    const char* forwardName = (const char*)PEHelpers::GetRvaPtr(
                        functionRva,
                        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
                        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
                        analyzer.lpFileContent,
                        analyzer.fileSize);

                    if (forwardName && !IsBadReadPtr(forwardName, 1)) {
                        std::vector<wchar_t> wideForwardName(MAX_PATH);
                        MultiByteToWideChar(CP_ACP, 0, forwardName, -1,
                            wideForwardName.data(), MAX_PATH);
                        OUTPUT("\t\t%ls (Ordinal: %d) -> Forward to: %ls\n",
                            functionName,
                            ordinal + pExportDir->Base,
                            wideForwardName.data());
                    }
                }
                else {
                    OUTPUT("\t\t%ls (Ordinal: %d, RVA: 0x%08X)\n",
                        functionName,
                        ordinal + pExportDir->Base,
                        functionRva);
                }
            }
        }
    }
}

void ParseExportDirectory64(const PEAnalyzer& analyzer) {
    const auto& exportDir = analyzer.pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!exportDir.VirtualAddress || !exportDir.Size) return;

    if (!PEHelpers::IsRvaValid(exportDir.VirtualAddress, analyzer.fileSize,
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders64))) {
        OUTPUT("[-] Invalid export directory RVA!\n");
        throw std::runtime_error("RVA out of bounds");
        return;
    }

    OUTPUT("\n[+] EXPORT DIRECTORY (64-bit)\n");
    // Rest of the implementation is identical to 32-bit version
    // Just using pNtHeaders64 instead of pNtHeaders64
    auto pExportDir = (PIMAGE_EXPORT_DIRECTORY)PEHelpers::GetRvaPtr(
        exportDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pExportDir || IsBadReadPtr(pExportDir, sizeof(IMAGE_EXPORT_DIRECTORY))) {
        OUTPUT("[-] Invalid export directory structure!\n");
        return;
    }

    OUTPUT("\tCharacteristics: 0x%X\n", pExportDir->Characteristics);
    OUTPUT("\tTimeDateStamp: 0x%X\n", pExportDir->TimeDateStamp);
    OUTPUT("\tMajorVersion: %d\n", pExportDir->MajorVersion);
    OUTPUT("\tMinorVersion: %d\n", pExportDir->MinorVersion);
    OUTPUT("\tName: 0x%X\n", pExportDir->Name);
    OUTPUT("\tBase: %d\n", pExportDir->Base);
    OUTPUT("\tNumberOfFunctions: %d\n", pExportDir->NumberOfFunctions);
    OUTPUT("\tNumberOfNames: %d\n", pExportDir->NumberOfNames);
    OUTPUT("\tAddressOfFunctions: 0x%X\n", pExportDir->AddressOfFunctions);
    OUTPUT("\tAddressOfNames: 0x%X\n", pExportDir->AddressOfNames);
    OUTPUT("\tAddressOfNameOrdinals: 0x%X\n\n", pExportDir->AddressOfNameOrdinals);

    auto pFunctions = (PDWORD)PEHelpers::GetRvaPtr(
        pExportDir->AddressOfFunctions,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    auto pNames = (PDWORD)PEHelpers::GetRvaPtr(
        pExportDir->AddressOfNames,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    auto pNameOrdinals = (PWORD)PEHelpers::GetRvaPtr(
        pExportDir->AddressOfNameOrdinals,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pNames || !pNameOrdinals || !pFunctions) {
        OUTPUT("[-] Invalid export address tables!\n");
        return;
    }

    OUTPUT("\tExported Functions:\n\n");
    for (DWORD i = 0; i < pExportDir->NumberOfNames; i++) {
        if (IsBadReadPtr(pNames + i, sizeof(DWORD)) ||
            IsBadReadPtr(pNameOrdinals + i, sizeof(WORD))) {
            break;
        }

        const char* functionName = (const char*)PEHelpers::GetRvaPtr(
            pNames[i],
            IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
            analyzer.pNtHeaders64->FileHeader.NumberOfSections,
            analyzer.lpFileContent,
            analyzer.fileSize);

        if (functionName && !IsBadReadPtr(functionName, 1)) {
            WORD ordinal = pNameOrdinals[i];
            if (ordinal < pExportDir->NumberOfFunctions) {
                DWORD functionRva = pFunctions[ordinal];

                // Check for forwarded export
                if (functionRva >= exportDir.VirtualAddress &&
                    functionRva < (exportDir.VirtualAddress + exportDir.Size)) {

                    const char* forwardName = (const char*)PEHelpers::GetRvaPtr(
                        functionRva,
                        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
                        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
                        analyzer.lpFileContent,
                        analyzer.fileSize);

                    if (forwardName && !IsBadReadPtr(forwardName, 1)) {
                        std::vector<wchar_t> wideForwardName(MAX_PATH);
                        MultiByteToWideChar(CP_ACP, 0, forwardName, -1,
                            wideForwardName.data(), MAX_PATH);
                        OUTPUT("\t\t%ls (Ordinal: %d) -> Forward to: %ls\n",
                            functionName,
                            ordinal + pExportDir->Base,
                            wideForwardName.data());
                    }
                }
                else {
                    OUTPUT("\t\t%ls (Ordinal: %d, RVA: 0x%08X)\n",
                        functionName,
                        ordinal + pExportDir->Base,
                        functionRva);
                }
            }
        }
    }
}
// Copying the same logic as ParseExportDirectory32 but with 64-bit headers
// ... [Same implementation as above, just using pNtHeaders64]

/*void ParseExportDirectory(const PEAnalyzer& analyzer) {
    if (analyzer.is64Bit) {
        ParseExportDirectory64(analyzer);
    }
    else {
        ParseExportDirectory32(analyzer);
    }
}*/ //removing this duplicate

void ProcessResourceDirectory32(
    PIMAGE_RESOURCE_DIRECTORY resDir,
    int level,
    const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const wchar_t* resourceTypes[],
    const PEAnalyzer& analyzer)
{
    if (IsBadReadPtr(resDir, sizeof(IMAGE_RESOURCE_DIRECTORY))) {
        return;
    }

    auto entry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resDir + 1);
    WORD totalEntries = resDir->NumberOfNamedEntries + resDir->NumberOfIdEntries;

    for (WORD i = 0; i < totalEntries; i++) {
        if (IsBadReadPtr(entry + i, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY))) {
            break;
        }

        for (int indent = 0; indent < level; indent++) {
            OUTPUT("\t");
        }

        if (entry[i].NameIsString) {
            auto nameEntry = (PIMAGE_RESOURCE_DIR_STRING_U)((BYTE*)baseResourceDir + entry[i].NameOffset);
            if (!IsBadReadPtr(nameEntry, sizeof(IMAGE_RESOURCE_DIR_STRING_U))) {
                std::vector<wchar_t> resourceName(nameEntry->Length + 1);
                wcsncpy_s(resourceName.data(), nameEntry->Length + 1,
                    nameEntry->NameString, nameEntry->Length);
                resourceName[nameEntry->Length] = L'\0';

                if (level == 0) {
                    OUTPUT("Resource Type: Custom (%ls)\n", resourceName.data());
                }
                else {
                    OUTPUT("Name: %ls\n", resourceName.data());
                }
            }
        }
        else {
            if (level == 0) {
                DWORD resourceType = entry[i].Id;
                if (resourceType < 16) {
                    OUTPUT("Resource Type: %ls (ID: %d)\n", resourceTypes[resourceType], resourceType);
                }
                else {
                    OUTPUT("Resource Type: Custom (ID: %d)\n", resourceType);
                }
            }
            else {
                OUTPUT("ID: %d\n", entry[i].Id);
            }
        }

        if (entry[i].DataIsDirectory) {
            auto nextDir = (PIMAGE_RESOURCE_DIRECTORY)((BYTE*)baseResourceDir + entry[i].OffsetToDirectory);
            if (!IsBadReadPtr(nextDir, sizeof(IMAGE_RESOURCE_DIRECTORY))) {
                ProcessResourceDirectory32(nextDir, level + 1,
                    level == 0 ? resourceTypes[min(entry[i].Id, 15)] : type,
                    baseResourceDir, resourceTypes, analyzer);
            }
        }
        else {
            auto dataEntry = (PIMAGE_RESOURCE_DATA_ENTRY)((BYTE*)baseResourceDir + entry[i].OffsetToData);
            if (!IsBadReadPtr(dataEntry, sizeof(IMAGE_RESOURCE_DATA_ENTRY))) {
                for (int indent = 0; indent < level + 1; indent++) {
                    OUTPUT("\t");
                }
                OUTPUT("Size: %d bytes, RVA: 0x%X\n", dataEntry->Size, dataEntry->OffsetToData);

                // Special handling for Version resources
                if (type && wcscmp(type, L"Version") == 0) {
                    auto versionData = (BYTE*)PEHelpers::GetRvaPtr(
                        dataEntry->OffsetToData,
                        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
                        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
                        analyzer.lpFileContent,
                        analyzer.fileSize);

                    if (versionData && !IsBadReadPtr(versionData, sizeof(VS_FIXEDFILEINFO))) {
                        auto versionInfo = (VS_FIXEDFILEINFO*)(versionData + 40);
                        if (versionInfo->dwSignature == 0xFEEF04BD) {
                            for (int indent = 0; indent < level + 2; indent++) {
                                OUTPUT("\t");
                            }
                            OUTPUT("File Version: %d.%d.%d.%d\n",
                                HIWORD(versionInfo->dwFileVersionMS),
                                LOWORD(versionInfo->dwFileVersionMS),
                                HIWORD(versionInfo->dwFileVersionLS),
                                LOWORD(versionInfo->dwFileVersionLS));
                        }
                    }
                }
            }
        }
    }
}

void ProcessResourceDirectory64(
    PIMAGE_RESOURCE_DIRECTORY resDir,
    int level,
    const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const wchar_t* resourceTypes[],
    const PEAnalyzer& analyzer)
{
    // Similar to ProcessResourceDirectory32 but using 64-bit structures
    // Main difference is using analyzer.pNtHeaders64 instead of analyzer.pNtHeaders32
    if (IsBadReadPtr(resDir, sizeof(IMAGE_RESOURCE_DIRECTORY))) {
        return;
        // Rest of the implementation follows the same pattern
    }

    auto entry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(resDir + 1);
    WORD totalEntries = resDir->NumberOfNamedEntries + resDir->NumberOfIdEntries;

    for (WORD i = 0; i < totalEntries; i++) {
        if (IsBadReadPtr(entry + i, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY))) {
            break;
        }

        for (int indent = 0; indent < level; indent++) {
            OUTPUT("\t");
        }

        if (entry[i].NameIsString) {
            auto nameEntry = (PIMAGE_RESOURCE_DIR_STRING_U)((BYTE*)baseResourceDir + entry[i].NameOffset);
            if (!IsBadReadPtr(nameEntry, sizeof(IMAGE_RESOURCE_DIR_STRING_U))) {
                std::vector<wchar_t> resourceName(nameEntry->Length + 1);
                wcsncpy_s(resourceName.data(), nameEntry->Length + 1,
                    nameEntry->NameString, nameEntry->Length);
                resourceName[nameEntry->Length] = L'\0';

                if (level == 0) {
                    OUTPUT("Resource Type: Custom (%ls)\n", resourceName.data());
                }
                else {
                    OUTPUT("Name: %ls\n", resourceName.data());
                }
            }
        }
        else {
            if (level == 0) {
                DWORD resourceType = entry[i].Id;
                if (resourceType < 16) {
                    OUTPUT("Resource Type: %ls (ID: %d)\n", resourceTypes[resourceType], resourceType);
                }
                else {
                    OUTPUT("Resource Type: Custom (ID: %d)\n", resourceType);
                }
            }
            else {
                OUTPUT("ID: %d\n", entry[i].Id);
            }
        }

        if (entry[i].DataIsDirectory) {
            auto nextDir = (PIMAGE_RESOURCE_DIRECTORY)((BYTE*)baseResourceDir + entry[i].OffsetToDirectory);
            if (!IsBadReadPtr(nextDir, sizeof(IMAGE_RESOURCE_DIRECTORY))) {
                ProcessResourceDirectory32(nextDir, level + 1,
                    level == 0 ? resourceTypes[min(entry[i].Id, 15)] : type,
                    baseResourceDir, resourceTypes, analyzer);
            }
        }
        else {
            auto dataEntry = (PIMAGE_RESOURCE_DATA_ENTRY)((BYTE*)baseResourceDir + entry[i].OffsetToData);
            if (!IsBadReadPtr(dataEntry, sizeof(IMAGE_RESOURCE_DATA_ENTRY))) {
                for (int indent = 0; indent < level + 1; indent++) {
                    OUTPUT("\t");
                }
                OUTPUT("Size: %d bytes, RVA: 0x%X\n", dataEntry->Size, dataEntry->OffsetToData);

                // Special handling for Version resources
                if (type && wcscmp(type, L"Version") == 0) {
                    auto versionData = (BYTE*)PEHelpers::GetRvaPtr(
                        dataEntry->OffsetToData,
                        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
                        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
                        analyzer.lpFileContent,
                        analyzer.fileSize);

                    if (versionData && !IsBadReadPtr(versionData, sizeof(VS_FIXEDFILEINFO))) {
                        auto versionInfo = (VS_FIXEDFILEINFO*)(versionData + 40);
                        if (versionInfo->dwSignature == 0xFEEF04BD) {
                            for (int indent = 0; indent < level + 2; indent++) {
                                OUTPUT("\t");
                            }
                            OUTPUT("File Version: %d.%d.%d.%d\n",
                                HIWORD(versionInfo->dwFileVersionMS),
                                LOWORD(versionInfo->dwFileVersionMS),
                                HIWORD(versionInfo->dwFileVersionLS),
                                LOWORD(versionInfo->dwFileVersionLS));
                        }
                    }
                }
            }
        }
    }
}

void ParseResourceDirectory32(const PEAnalyzer& analyzer) {
    const auto& resourceDir = analyzer.pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    if (!resourceDir.VirtualAddress || !resourceDir.Size) return;

    if (!PEHelpers::IsRvaValid(resourceDir.VirtualAddress, analyzer.fileSize,
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32))) {
        OUTPUT("[-] Invalid resource directory RVA!\n");
        throw std::runtime_error("RVA out of bounds");
        return;
    }

    OUTPUT("\n[+] RESOURCE DIRECTORY (32-bit)\n");
    auto pResourceDir = (PIMAGE_RESOURCE_DIRECTORY)PEHelpers::GetRvaPtr(
        resourceDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pResourceDir || IsBadReadPtr(pResourceDir, sizeof(IMAGE_RESOURCE_DIRECTORY))) {
        OUTPUT("[-] Invalid or corrupted resource directory\n");
        return;
    }

    const wchar_t* resourceTypes[] = {
        L"Unknown",     L"Cursor",      L"Bitmap",      L"Icon",
        L"Menu",        L"Dialog",      L"String",      L"FontDir",
        L"Font",        L"Accelerator", L"RCData",      L"MessageTable",
        L"GroupCursor", L"GroupIcon",   L"Version",     L"DlgInclude"
    };

    ProcessResourceDirectory32(pResourceDir, 0, nullptr, pResourceDir, resourceTypes, analyzer);
}

void ParseDebugDirectory32(const PEAnalyzer& analyzer) {
    const auto& debugDir = analyzer.pNtHeaders32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (!debugDir.VirtualAddress || !debugDir.Size) return;

    if (!PEHelpers::IsRvaValid(debugDir.VirtualAddress, analyzer.fileSize,
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders32))) {
        OUTPUT("[-] Invalid debug directory RVA!\n");
        throw std::runtime_error("RVA out of bounds");
        return;
    }

    OUTPUT("\n[+] DEBUG DIRECTORY (32-bit)\n");
    auto pDebugDir = (PIMAGE_DEBUG_DIRECTORY)PEHelpers::GetRvaPtr(
        debugDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders32),
        analyzer.pNtHeaders32->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pDebugDir || IsBadReadPtr(pDebugDir, sizeof(IMAGE_DEBUG_DIRECTORY))) {
        OUTPUT("[-] Invalid debug directory structure!\n");
        return;
    }

    DWORD numEntries = min(debugDir.Size / sizeof(IMAGE_DEBUG_DIRECTORY), 16);
    for (DWORD i = 0; i < numEntries; i++) {
        OUTPUT("\tDebug Entry %d:\n", i + 1);
        OUTPUT("\tCharacteristics: 0x%X\n", pDebugDir[i].Characteristics);
        OUTPUT("\tTimeDateStamp: 0x%X\n", pDebugDir[i].TimeDateStamp);
        OUTPUT("\tMajorVersion: %d\n", pDebugDir[i].MajorVersion);
        OUTPUT("\tMinorVersion: %d\n", pDebugDir[i].MinorVersion);
        OUTPUT("\tType: 0x%X", pDebugDir[i].Type);

        switch (pDebugDir[i].Type) {
        case IMAGE_DEBUG_TYPE_COFF:
            OUTPUT(" (COFF)\n"); break;
        case IMAGE_DEBUG_TYPE_CODEVIEW:
            OUTPUT(" (CodeView)\n"); break;
        case IMAGE_DEBUG_TYPE_FPO:
            OUTPUT(" (FPO)\n"); break;
        case IMAGE_DEBUG_TYPE_MISC:
            OUTPUT(" (Misc)\n"); break;
        case IMAGE_DEBUG_TYPE_EXCEPTION:
            OUTPUT(" (Exception)\n"); break;
        case IMAGE_DEBUG_TYPE_FIXUP:
            OUTPUT(" (Fixup)\n"); break;
        case IMAGE_DEBUG_TYPE_OMAP_TO_SRC:
            OUTPUT(" (OMAP to Src)\n"); break;
        case IMAGE_DEBUG_TYPE_OMAP_FROM_SRC:
            OUTPUT(" (OMAP from Src)\n"); break;
        case IMAGE_DEBUG_TYPE_BORLAND:
            OUTPUT(" (Borland)\n"); break;
        default:
            OUTPUT(" (Unknown)\n"); break;
        }

        OUTPUT("\tSizeOfData: 0x%X\n", pDebugDir[i].SizeOfData);
        OUTPUT("\tAddressOfRawData: 0x%X\n", pDebugDir[i].AddressOfRawData);
        OUTPUT("\tPointerToRawData: 0x%X\n\n", pDebugDir[i].PointerToRawData);

        // Special handling for CodeView debug information
        if (pDebugDir[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW &&
            pDebugDir[i].PointerToRawData != 0 &&
            pDebugDir[i].SizeOfData >= sizeof(DWORD)) {

            auto pCVHeader = (DWORD*)((BYTE*)analyzer.lpFileContent + pDebugDir[i].PointerToRawData);
            if (!IsBadReadPtr(pCVHeader, sizeof(DWORD))) {
                switch (*pCVHeader) {
                case 0x53445352: // 'RSDS'
                    if (pDebugDir[i].SizeOfData >= (sizeof(DWORD) + sizeof(GUID) + sizeof(DWORD) + 1)) {
                        auto pCVData = (char*)(pCVHeader + 1);
                        if (!IsBadReadPtr(pCVData + 16, 1)) {
                            auto guid = (GUID*)pCVData;
                            DWORD age = *(DWORD*)(pCVData + 16);
                            const char* pdbPath = pCVData + 20;

                            OUTPUT("\tPDB Information:\n");
                            OUTPUT("\tGUID: {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
                                guid->Data1, guid->Data2, guid->Data3,
                                guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                                guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
                            OUTPUT("\tAge: %d\n", age);
                            OUTPUT("\tPDB Path: %ls\n\n", pdbPath);
                        }
                    }
                    break;

                case 0x3031424E: // 'NB10'
                    if (pDebugDir[i].SizeOfData >= 16) {
                        auto pNB10Data = (char*)(pCVHeader + 1);
                        DWORD offset = *(DWORD*)pNB10Data;
                        DWORD timestamp = *(DWORD*)(pNB10Data + 4);
                        DWORD age = *(DWORD*)(pNB10Data + 8);
                        const char* pdbPath = pNB10Data + 12;

                        OUTPUT("\tPDB Information (NB10):\n");
                        OUTPUT("\tOffset: 0x%X\n", offset);
                        OUTPUT("\tTimestamp: 0x%X\n", timestamp);
                        OUTPUT("\tAge: %d\n", age);
                        OUTPUT("\tPDB Path: %ls\n\n", pdbPath);
                    }
                    break;
                }
            }
        }
    }
}

void ParseDebugDirectory64(const PEAnalyzer& analyzer) {
    const auto& debugDir = analyzer.pNtHeaders64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (!debugDir.VirtualAddress || !debugDir.Size) return;

    if (!PEHelpers::IsRvaValid(debugDir.VirtualAddress, analyzer.fileSize,
        reinterpret_cast<PIMAGE_NT_HEADERS>(analyzer.pNtHeaders64))) {
        OUTPUT("[-] Invalid debug directory RVA!\n");
        throw std::runtime_error("RVA out of bounds");
        return;
    }

    OUTPUT("\n[+] DEBUG DIRECTORY (64-bit)\n");
    // Rest of implementation follows same pattern as 32-bit version
    // Just using pNtHeaders64 instead of pNtHeaders32
    auto pDebugDir = (PIMAGE_DEBUG_DIRECTORY)PEHelpers::GetRvaPtr(
        debugDir.VirtualAddress,
        IMAGE_FIRST_SECTION(analyzer.pNtHeaders64),
        analyzer.pNtHeaders64->FileHeader.NumberOfSections,
        analyzer.lpFileContent,
        analyzer.fileSize);

    if (!pDebugDir || IsBadReadPtr(pDebugDir, sizeof(IMAGE_DEBUG_DIRECTORY))) {
        OUTPUT("[-] Invalid debug directory structure!\n");
        return;
    }

    DWORD numEntries = min(debugDir.Size / sizeof(IMAGE_DEBUG_DIRECTORY), 16);
    for (DWORD i = 0; i < numEntries; i++) {
        OUTPUT("\tDebug Entry %d:\n", i + 1);
        OUTPUT("\tCharacteristics: 0x%X\n", pDebugDir[i].Characteristics);
        OUTPUT("\tTimeDateStamp: 0x%X\n", pDebugDir[i].TimeDateStamp);
        OUTPUT("\tMajorVersion: %d\n", pDebugDir[i].MajorVersion);
        OUTPUT("\tMinorVersion: %d\n", pDebugDir[i].MinorVersion);
        OUTPUT("\tType: 0x%X", pDebugDir[i].Type);

        switch (pDebugDir[i].Type) {
        case IMAGE_DEBUG_TYPE_COFF:
            OUTPUT(" (COFF)\n"); break;
        case IMAGE_DEBUG_TYPE_CODEVIEW:
            OUTPUT(" (CodeView)\n"); break;
        case IMAGE_DEBUG_TYPE_FPO:
            OUTPUT(" (FPO)\n"); break;
        case IMAGE_DEBUG_TYPE_MISC:
            OUTPUT(" (Misc)\n"); break;
        case IMAGE_DEBUG_TYPE_EXCEPTION:
            OUTPUT(" (Exception)\n"); break;
        case IMAGE_DEBUG_TYPE_FIXUP:
            OUTPUT(" (Fixup)\n"); break;
        case IMAGE_DEBUG_TYPE_OMAP_TO_SRC:
            OUTPUT(" (OMAP to Src)\n"); break;
        case IMAGE_DEBUG_TYPE_OMAP_FROM_SRC:
            OUTPUT(" (OMAP from Src)\n"); break;
        case IMAGE_DEBUG_TYPE_BORLAND:
            OUTPUT(" (Borland)\n"); break;
        default:
            OUTPUT(" (Unknown)\n"); break;
        }

        OUTPUT("\tSizeOfData: 0x%X\n", pDebugDir[i].SizeOfData);
        OUTPUT("\tAddressOfRawData: 0x%X\n", pDebugDir[i].AddressOfRawData);
        OUTPUT("\tPointerToRawData: 0x%X\n\n", pDebugDir[i].PointerToRawData);

        // Special handling for CodeView debug information
        if (pDebugDir[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW &&
            pDebugDir[i].PointerToRawData != 0 &&
            pDebugDir[i].SizeOfData >= sizeof(DWORD)) {

            auto pCVHeader = (DWORD*)((BYTE*)analyzer.lpFileContent + pDebugDir[i].PointerToRawData);
            if (!IsBadReadPtr(pCVHeader, sizeof(DWORD))) {
                switch (*pCVHeader) {
                case 0x53445352: // 'RSDS'
                    if (pDebugDir[i].SizeOfData >= (sizeof(DWORD) + sizeof(GUID) + sizeof(DWORD) + 1)) {
                        auto pCVData = (char*)(pCVHeader + 1);
                        if (!IsBadReadPtr(pCVData + 16, 1)) {
                            auto guid = (GUID*)pCVData;
                            DWORD age = *(DWORD*)(pCVData + 16);
                            const char* pdbPath = pCVData + 20;

                            OUTPUT("\tPDB Information:\n");
                            OUTPUT("\tGUID: {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
                                guid->Data1, guid->Data2, guid->Data3,
                                guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                                guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
                            OUTPUT("\tAge: %d\n", age);
                            OUTPUT("\tPDB Path: %ls\n\n", pdbPath);
                        }
                    }
                    break;

                case 0x3031424E: // 'NB10'
                    if (pDebugDir[i].SizeOfData >= 16) {
                        auto pNB10Data = (char*)(pCVHeader + 1);
                        DWORD offset = *(DWORD*)pNB10Data;
                        DWORD timestamp = *(DWORD*)(pNB10Data + 4);
                        DWORD age = *(DWORD*)(pNB10Data + 8);
                        const char* pdbPath = pNB10Data + 12;

                        OUTPUT("\tPDB Information (NB10):\n");
                        OUTPUT("\tOffset: 0x%X\n", offset);
                        OUTPUT("\tTimestamp: 0x%X\n", timestamp);
                        OUTPUT("\tAge: %d\n", age);
                        OUTPUT("\tPDB Path: %ls\n\n", pdbPath);
                    }
                    break;
                }
            }
        }
    }
}
// ... [Same implementation as ParseDebugDirectory32]

//newfunctionshere (lateest fixes)


void ProcessResourceDirectory32(
    PIMAGE_RESOURCE_DIRECTORY resDir,
    int level,
    const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const wchar_t* resourceTypes[],
    const PEAnalyzer& analyzer);

void ProcessResourceDirectory64(
    PIMAGE_RESOURCE_DIRECTORY resDir,
    int level,
    const wchar_t* type,
    PIMAGE_RESOURCE_DIRECTORY baseResourceDir,
    const wchar_t* resourceTypes[],
    const PEAnalyzer& analyzer);

// Main window class name
//const char* const WINDOW_CLASS_NAME = "PEAnalyzerWindow";

//new inserted here >>

// Page Break

//here

// Page Break

void AddMenus(HWND hwnd) {
    HMENU hMenuBar = CreateMenu();
    HMENU hFileMenu = CreateMenu();
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    AppendMenu(hFileMenu, MF_STRING, 1, L"&Open\tCtrl+O");  // Updated to show shortcut
    AppendMenu(hFileMenu, MF_STRING, 2, L"&Select All\tCtrl+A");
    AppendMenu(hFileMenu, MF_STRING, 3, L"E&xit\tAlt+F4");
    EnableMenuItem(hFileMenu, 2, MF_BYCOMMAND | MF_GRAYED);
    //EnableMenuItem(hFileMenu, ID_FILE_SELECTALL, MF_BYCOMMAND | MF_GRAYED);
    SetMenu(hwnd, hMenuBar);
}

/*void AddMenus(HWND hwnd) {
    HMENU hMenuBar = CreateMenu();
    HMENU hFileMenu = CreateMenu();

    // Create the File menu using the defined IDs.
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_OPEN, L"&Open\tCtrl+O");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SELECTALL, L"&Select All");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_EXIT, L"E&xit");

    // Initially disable the "Select All" menu item.
    EnableMenuItem(hFileMenu, ID_FILE_SELECTALL, MF_BYCOMMAND | MF_GRAYED);
    SetMenu(hwnd, hMenuBar);
}*/

// First, add this helper function to determine if the PE file is 64-bit
bool Is64BitPE(PIMAGE_NT_HEADERS pNtHeaders) {
    return pNtHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;
} //removing this duplicate

// Helper function to safely get file size
/*DWORD GetFileSizeCustom(const wchar_t* filePath) {
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD fileSize = ::GetFileSize(hFile, nullptr);
    CloseHandle(hFile);
    return fileSize;
}*/ //removing this duplicate instead

// Add this helper function
void SafeOutput(const std::wstring& text) {
    try {
        g_OutputText << text;
        UpdateEditControl();
    }
    catch (...) {
        SetStatusText(L"Error writing output");
    }
}

void UpdateStatusBar(const wchar_t* text, int progress = -1) {
    if (text) {
        SetStatusText(text);
    }
    if (progress >= 0) {
        ShowProgress(progress);
    }
}

void ClearProgress() {
    ShowWindow(g_hProgressBar, SW_HIDE);
    SetStatusText(L"Ready");
}



void SetStatusText(const wchar_t* text) {
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)text);
}

void ShowProgress(int percentage) {
    if (percentage < 0 || percentage > 100) {
        OUTPUT("[-] Invalid progress percentage: %d\n", percentage);
        return;
    }

    SendMessage(g_hProgressBar, PBM_SETPOS, (WPARAM)percentage, 0);
    wchar_t status[256];
    swprintf_s(status, L"Analyzing... %d%%", percentage);
    SetStatusText(status);
    SendMessage(g_hStatusBar, SB_SETTEXT, 0, (LPARAM)status);
}

void OpenFileDialog(HWND hwnd) {
    WCHAR fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW), hwnd, NULL, L"Executable Files (*.exe;*.dll)\0*.exe;*.dll\0All Files (*.*)\0*.*\0", NULL, 0, 1, fileName, MAX_PATH, NULL, 0, NULL, NULL, OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, 0, 0, L"exe", NULL, NULL, NULL };
    if (GetOpenFileNameW(&ofn)) {
        SetWindowTextW(g_hEditControl, L"");
        g_OutputText.str(L"");
        g_OutputText.clear();
        AnalyzePEFile(ofn.lpstrFile);
        UpdateEditControl();
    }
}


// Modified AppendToOutput to handle newlines properly:
void AppendToOutput(const wchar_t* format, ...) {
    std::vector<wchar_t> buffer(4096); //recent 19/2/25 (1024)*
    va_list args;
    va_start(args, format);

    while (true) {
        va_list argsCopy;
        va_copy(argsCopy, args);
        int result = _vsnwprintf(buffer.data(), buffer.size(), format, argsCopy);
        va_end(argsCopy);
        if (result >= 0) break;
        buffer.resize(buffer.size() * 2);
    }
    va_end(args);

    // Convert \n to \r\n
    std::wstring output = buffer.data();
    size_t pos = 0;
    while ((pos = output.find(L'\n', pos)) != std::wstring::npos) {
        if (pos == 0 || output[pos - 1] != L'\r') {
            output.insert(pos, L"\r");
            pos += 2;
        }
        else {
            pos++;
        }
    }

    g_OutputText << output;
    UpdateEditControl();
}


//use vectors for unlimited size growth of buffer, use an alternative to editbox, check for fast loops overloading, in its primitive form it works properly! try w/o getimports3264 datadirectories getsections    

//adding here
// Line Break
// new code below starting here vv



DWORD GetFileSize(const wchar_t* filePath) {
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    DWORD fileSize = ::GetFileSize(hFile, nullptr);
    CloseHandle(hFile);
    return fileSize;
}
//}
// Page Break

//}

//UpdateEditControl();
//}

// new code upto here ending here ^^
// Line Break 
//  //end adding here



//filePathW
//lpFilePath
/*
HANDLE GetFileContent(const wchar_t* lpFilePath) {
    HANDLE hFile = CreateFileW(lpFilePath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return nullptr;
    DWORD fileSize = ::GetFileSize(hFile, nullptr);
    auto lpFileContent = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize);
    DWORD bytesRead;
    ReadFile(hFile, lpFileContent, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    return lpFileContent;
}
*/

void UpdateEditControl() {
    SetWindowTextW(g_hEditControl, g_OutputText.str().c_str());
    SendMessage(g_hEditControl, EM_SETSEL, -1, -1);
    SendMessage(g_hEditControl, EM_SCROLLCARET, 0, 0);
}
//}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    // Get command line parameters in Unicode
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    CreateMainWindow(hInstance);
    if (!g_hMainWindow) {
        LocalFree(argv);
        return -1;
    }

    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);
    SetWindowPos(g_hMainWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // If there's a command line parameter, process it
    if (argc > 1) {
        // Process the first parameter as file path
        SetWindowTextW(g_hEditControl, L"");
        g_OutputText.str(L"");
        g_OutputText.clear();
        AnalyzePEFile(argv[1]);
        UpdateEditControl();
    }

    LocalFree(argv);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_hFont) DeleteObject(g_hFont);
    return (int)msg.wParam;
}

void CreateMainWindow(HINSTANCE hInstance) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), 0, WindowProc, 0, 0, hInstance,
        LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)),
        LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        NULL, WINDOW_CLASS_NAME,
        LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)) };
    RegisterClassExW(&wc);

    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Calculate center position
    int windowX = (screenWidth - WINDOW_WIDTH) / 2;
    int windowY = (screenHeight - WINDOW_HEIGHT) / 2;

    // Remove WS_MAXIMIZEBOX and WS_THICKFRAME from the window style
    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX) & ~WS_THICKFRAME;
    //WS_OVERLAPPEDWINDOW ~~> style
    g_hMainWindow = CreateWindowExW(0, WINDOW_CLASS_NAME, L"PE File Analyzer v6.6",
        style, windowX, windowY, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr);
}

void InitializeControls(HWND hwnd) {
    // Create status bar
    g_hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, NULL,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Set up status bar parts
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int statusParts[2] = { 150, rcClient.right };
    SendMessage(g_hStatusBar, SB_SETPARTS, 2, (LPARAM)statusParts);

    // Create progress bar in the second part of status bar
    RECT rcPart;
    SendMessage(g_hStatusBar, SB_GETRECT, 1, (LPARAM)&rcPart);

    g_hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        rcPart.left + 5, rcPart.top + 2,
        rcPart.right - rcPart.left - 10, rcPart.bottom - rcPart.top - 4,
        g_hStatusBar, NULL,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Set blue color and range
    SendMessage(g_hProgressBar, PBM_SETBARCOLOR, 0, (LPARAM)RGB(0, 120, 215));  // Windows 10 blue
    SendMessage(g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(g_hProgressBar, PBM_SETSTEP, 1, 0);

    // Get status bar height for edit control positioning
    RECT rcStatus;
    GetWindowRect(g_hStatusBar, &rcStatus);
    int statusHeight = rcStatus.bottom - rcStatus.top;

    // Create edit control
    g_hEditControl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        EDIT_MARGIN, EDIT_MARGIN,
        rcClient.right - (2 * EDIT_MARGIN),
        rcClient.bottom - statusHeight - (2 * EDIT_MARGIN),
        hwnd, nullptr,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), nullptr);

    // Allow the edit control to accept dropped files.
    DragAcceptFiles(g_hEditControl, TRUE);

    // Create and set font
    g_hFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");

    if (g_hFont)
        SendMessage(g_hEditControl, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    SetWindowSubclass(g_hEditControl, EditSubclassProc, 0, 0);
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_KEYDOWN) {
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'O') {
            SendMessage(GetParent(hwnd), WM_COMMAND, 1, 0);
            return 0;
        }
        switch (wParam) {
        case VK_F1:
            SendMessage(GetParent(hwnd), WM_KEYDOWN, VK_F1, 0);
            return 0;
        case VK_ESCAPE:
            SendMessage(GetParent(hwnd), WM_KEYDOWN, VK_ESCAPE, 0);
            return 0;
        case VK_PRIOR:
            // Scroll up
            SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), 0);
            return 0;
        case VK_NEXT:
            // Scroll down
            SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), 0);
            return 0;
        case VK_UP:
            // Scroll one line up
            SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_LINEUP, 0), 0);
            return 0;
        case VK_DOWN:
            // Scroll one line down
            SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_LINEDOWN, 0), 0);
            return 0;
        }
    }
    if (uMsg == WM_MOUSEWHEEL) {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta > 0) {
            // Scroll up
            SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), 0);
        }
        else {
            // Scroll down
            SendMessage(hwnd, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), 0);
        }
        return 0;
    }
    // Check for Ctrl+A: the 'A' key (wParam == 'A') along with the CTRL key held down.
    if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        // Select all text in the edit control.
        SendMessage(hwnd, EM_SETSEL, 0, -1);
        return 0; // indicate that the message has been handled.
    }
    // Handle file drops
    if (uMsg == WM_DROPFILES) {
        HDROP hDrop = (HDROP)wParam;
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        if (fileCount > 0) {
            wchar_t filePath[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH)) {
                // Call your file analysis function with the dropped file path.
                AnalyzePEFile(filePath);
            }
        }
        DragFinish(hDrop);
        return 0; // Indicate that the message was handled.
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: InitializeControls(hwnd); AddMenus(hwnd); return 0;
    case WM_SIZE:
    {
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // Resize status bar
        SendMessage(g_hStatusBar, WM_SIZE, 0, 0);

        // Recalculate status bar parts
        int statusParts[2] = { 150, rcClient.right };
        SendMessage(g_hStatusBar, SB_SETPARTS, 2, (LPARAM)statusParts);

        // Reposition progress bar
        RECT rcPart;
        SendMessage(g_hStatusBar, SB_GETRECT, 1, (LPARAM)&rcPart);
        SetWindowPos(g_hProgressBar, NULL,
            rcPart.left + 5, rcPart.top + 2,
            rcPart.right - rcPart.left - 10, rcPart.bottom - rcPart.top - 4,
            SWP_NOZORDER);

        // Get status bar height
        RECT rcStatus;
        GetWindowRect(g_hStatusBar, &rcStatus);
        int statusHeight = rcStatus.bottom - rcStatus.top;

        // Resize edit control
        SetWindowPos(g_hEditControl, NULL,
            EDIT_MARGIN,
            EDIT_MARGIN,
            rcClient.right - (2 * EDIT_MARGIN),
            rcClient.bottom - statusHeight - (2 * EDIT_MARGIN),
            SWP_NOZORDER);

        if (wParam == SIZE_MINIMIZED)
        {
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }
        else
        {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }

        return 0;
    }

    case WM_KEYDOWN:
    {
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            switch (wParam) {
            case 'O':
                OpenFileDialog(hwnd);
                return 0;
            }
        }
        switch (wParam) {
        case VK_F1:
            MessageBoxW(hwnd,
                L"PE Header Parser 6.6 GUI-based Programmed in C++ Win32 API (2834+ lines of code) by Entisoft Software (c) Evans Thorpemorton.\nFiles can be #1 Drag & Dropped #2 File Menu > Open Dialog #3 Command-Line Argument FilePath #4 Ctrl+O Hotkey",
                L"About",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        case VK_ESCAPE:
            PostQuitMessage(0);
            return 0;
        case VK_PRIOR:
            // Scroll up
            SendMessage(g_hEditControl, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), 0);
            return 0;
        case VK_NEXT:
            // Scroll down
            SendMessage(g_hEditControl, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), 0);
            return 0;
        case VK_UP:
            // Scroll one line up
            SendMessage(g_hEditControl, WM_VSCROLL, MAKELONG(SB_LINEUP, 0), 0);
            return 0;
        case VK_DOWN:
            // Scroll one line down
            SendMessage(g_hEditControl, WM_VSCROLL, MAKELONG(SB_LINEDOWN, 0), 0);
            return 0;
        }
        break;
    }
    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta > 0) {
            // Scroll up
            SendMessage(g_hEditControl, WM_VSCROLL, MAKELONG(SB_PAGEUP, 0), 0);
        }
        else {
            // Scroll down
            SendMessage(g_hEditControl, WM_VSCROLL, MAKELONG(SB_PAGEDOWN, 0), 0);
        }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) OpenFileDialog(hwnd); if (LOWORD(wParam) == 2) { if (g_hEditControl) { SetFocus(g_hEditControl); SendMessage(g_hEditControl, EM_SETSEL, 0, -1); } } if (LOWORD(wParam) == 3) PostQuitMessage(0); return 0;

/*        switch (LOWORD(wParam)) {
        case ID_FILE_OPEN:
            OpenFileDialog(hwnd);
            break;
        case ID_FILE_SELECTALL:
            if (g_hEditControl) {
                // Set focus to the edit control if needed.
                SetFocus(g_hEditControl);
                // Select all text.
                SendMessage(g_hEditControl, EM_SETSEL, 0, -1);
                UpdateEditControl();
            }
            break;
        case ID_FILE_EXIT:
            PostQuitMessage(0);
            break;
        }
        return 0; */         
        
        /* {
        int cmd = LOWORD(wParam);
        if (cmd == 1) {
            OpenFileDialog(hwnd);
        }
        else if (cmd == 2) {  // This is your Select All command.
            if (g_hEditControl) {
                SetFocus(g_hEditControl);
                SendMessage(g_hEditControl, EM_SETSEL, 0, -1);
                UpdateEditControl();  // if necessary to refresh display
            }
        }
        else if (cmd == 3) {  // This is your Exit command.
            PostQuitMessage(0);
        }
        return 0;
    } */
        
/*        switch (LOWORD(wParam)) {
        case ID_FILE_OPEN:
            OpenFileDialog(hwnd);
            break;
        case ID_FILE_SELECTALL:
            if (g_hEditControl) {
                // Set focus to the edit control
                SetFocus(g_hEditControl);
                // Select all text
                SendMessage(g_hEditControl, EM_SETSEL, 0, -1);
                UpdateEditControl();
            }
            break;
        case ID_FILE_EXIT:
            PostQuitMessage(0);
            break;
        }
        return 0;
*/
    case WM_DESTROY:
        if (g_hStatusBar) DestroyWindow(g_hStatusBar);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}