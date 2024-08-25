#include "stdafx.h"

#include <memory>
#include <Psapi.h>

#include <windows.h>
#include <dbghelp.h>
#include <cstdio>

//Globals
HANDLE g_hProcess = NULL;
DWORD g_ProcessID = NULL;
ULONG_PTR g_AttachedProcessAddress = NULL;
DWORD g_AttachedProcessSize = NULL;
CString g_ProcessName;
HANDLE g_UpdateCacheThread = NULL;

/*
    Memory and module map info
        
    ReClass makes it easy to tell at a glance whether a value could be a pointer.
    When a node is drawn and it hasn't been assigned a type, it will show an extra
    tag at the end ( e.g. '-> 0x0BADF00D') in red if the data can be interpreted 
    as a pointer to a mapped portion of memory.  

    To know whether a value is a valid address in the target process' memory space,
    we maintain a copy of the meta data of the target process' memory map.  This is
    represented by a map keyed on the last byte of each memory region, with the value
    at each key being a MemMapInfo class containing the start, end, and size of the 
    memory region.  The regions are keyed by last byte so that map->lower_bound(addr)
    will return an iterator to the region thst starts *BEFORE* addr, and then the
    addr need only be checked to come before the regions last byte to confirm that the
    addr is within the region.

    We use a map because we have to find what region things are in a *LOT*, and we 
    typically have a *LOT* of regions.  In my testing, I ended up with ~280,000 regions
    in our MemMap.  If we had to iterate through that list to find the region containing
    an address every time we drew a default node, ReClass's performance would slow to a
    crawl.
*/

std::mutex g_MemMapMutex;
std::map<ULONG_PTR, MemMapInfo>     g_MemMap;

std::mutex g_MemMapModulesMutex;
std::map<ULONG_PTR, MemMapInfo>     g_MemMapModules;

std::vector<AddressName>            g_Exports;

std::vector<HICON> g_Icons;

COLORREF g_clrBackground = RGB( 255, 255, 255 );
COLORREF g_clrSelect = RGB( 240, 240, 240 );
COLORREF g_clrHidden = RGB( 240, 240, 240 );

COLORREF g_clrOffset = RGB( 255, 0, 0 );
COLORREF g_clrAddress = RGB( 0, 200, 0 );
COLORREF g_clrType = RGB( 0, 0, 255 );
COLORREF g_clrName = RGB( 32, 32, 128 );
COLORREF g_clrIndex = RGB( 32, 200, 200 );
COLORREF g_clrValue = RGB( 255, 128, 0 );
COLORREF g_clrComment = RGB( 0, 200, 0 );

COLORREF g_clrVTable = RGB( 0, 255, 0 );
COLORREF g_clrFunction = RGB( 255, 0, 255 );
COLORREF g_clrChar = RGB( 0, 0, 255 );
COLORREF g_clrBits = RGB( 0, 0, 255 );
COLORREF g_clrCustom = RGB( 64, 128, 64 );
COLORREF g_clrHex = RGB( 0, 0, 0 );

CString g_ViewFontName;
CFont g_ViewFont;

int g_FontWidth;
int g_FontHeight;

bool g_bAddress = true;
bool g_bOffset = true;
bool g_bText = true;
bool g_bRTTI = true;
bool g_bRandomName = true;
bool g_bResizingFont = true;
bool g_bSymbolResolution = true;
bool g_bLoadModuleSymbol = false;

bool g_bFloat = true;
bool g_bInt = true;
bool g_bString = true;
bool g_bPointers = true;
bool g_bUnsignedHex = true;

bool g_bTop = true;
bool g_bClassBrowser = true;
bool g_bFilterProcesses = false;
bool g_bPrivatePadding = false;
bool g_bClipboardCopy = false;

RCTYPEDEFS g_Typedefs;

DWORD g_NodeCreateIndex = 0;

bool InitializeSymbolHandler()
{
    static bool initialized = false;
    static std::mutex initMutex;

    std::lock_guard<std::mutex> lock(initMutex);

    if (!initialized)
    {
        if (SymInitialize(GetCurrentProcess(), NULL, TRUE))
        {
            initialized = true;
        }
        else
        {
            printf("Failed to initialize symbol handler.\n");
            return false;
        }
    }

    return true;
}

void CleanupSymbolHandler()
{
    SymCleanup(GetCurrentProcess());
}

BOOL ReClassReadMemory(LPVOID Address, LPVOID Buffer, SIZE_T Size, PSIZE_T BytesRead)
{
    //if (!InitializeSymbolHandler())
    //    return FALSE;
    //
    //void* returnAddress = _ReturnAddress();
    //
    //// Symbol information
    //DWORD64 displacement = 0;
    //DWORD64 address = (DWORD64)returnAddress;
    //char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    //PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
    //symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    //symbol->MaxNameLen = MAX_SYM_NAME;
    //
    //if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbol))
    //{
    //    printf("ReClassReadMemory called from function: %s\n", symbol->Name);
    //}
    //else
    //{
    //    printf("ReClassReadMemory: Could not resolve caller function name.\n");
    //}
    //
    //
    if (g_PluginOverrideReadMemoryOperation != NULL)
        return g_PluginOverrideReadMemoryOperation( Address, Buffer, Size, BytesRead );

    BOOL return_val = ReadProcessMemory( g_hProcess, (LPVOID)Address, Buffer, Size, BytesRead );
    if (!return_val) SecureZeroMemory( Buffer, Size );
    return return_val;
}

BOOL ReClassWriteMemory( LPVOID Address, LPVOID Buffer, SIZE_T Size, PSIZE_T BytesWritten )
{
    if (g_PluginOverrideWriteMemoryOperation != NULL)
        return g_PluginOverrideWriteMemoryOperation( Address, Buffer, Size, BytesWritten );

    DWORD OldProtect;
    VirtualProtectEx( g_hProcess, (void*)Address, Size, PAGE_EXECUTE_READWRITE, &OldProtect );
    BOOL ret = WriteProcessMemory( g_hProcess, (PVOID)Address, Buffer, Size, BytesWritten );
    VirtualProtectEx( g_hProcess, (void*)Address, Size, OldProtect, NULL );
    return ret;
}

HANDLE ReClassOpenProcess( DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessID )
{
    if (g_PluginOverrideOpenProcessOperation != NULL)
        return g_PluginOverrideOpenProcessOperation( dwDesiredAccess, bInheritHandle, dwProcessID );
    return OpenProcess( dwDesiredAccess, bInheritHandle, dwProcessID );
}

HANDLE ReClassOpenThread( DWORD dwDesiredAccessFlags, BOOL bInheritHandle, DWORD dwThreadID )
{
    if (g_PluginOverrideOpenProcessOperation != NULL)
        return g_PluginOverrideOpenProcessOperation( dwDesiredAccessFlags, bInheritHandle, dwThreadID );
    return OpenThread( dwDesiredAccessFlags, bInheritHandle, dwThreadID );
}

CStringA ReadMemoryStringA( ULONG_PTR address, SIZE_T max )
{	
    SIZE_T bytesRead = 0;
    auto buffer = std::make_unique<char[]>( max + 1 );

    if (ReClassReadMemory( (PVOID)address, (LPVOID)buffer.get( ), max, &bytesRead ) != 0)
    {
        for (size_t i = 0; i < bytesRead; i++)
        {
            if (!(buffer[i] > 0x1F && buffer[i] < 0xFF && buffer[i] != 0x7F) && buffer[i] != '\0')
                buffer[i] = '.';
        }
        buffer[bytesRead] = '\0';
        return CStringA( buffer.get( ) );
    }
    else
    {
        #ifdef _DEBUG
        PrintOut( _T( "[ReadMemoryString]: Failed to read memory, GetLastError() = %s" ), Utils::GetLastErrorString( ).GetString( ) );
        #endif
        return CStringA( "...." );
    }
}

CStringW ReadMemoryStringW( ULONG_PTR address, SIZE_T max )
{
    SIZE_T bytesRead = 0;
    auto buffer = std::make_unique<wchar_t[]>( max + 1 );

    if (ReClassReadMemory( (PVOID)address, (LPVOID)buffer.get( ), max * sizeof( wchar_t ), &bytesRead ) != 0)
    {
        bytesRead /= sizeof( wchar_t );

        for (size_t i = 0; i < bytesRead; i++)
        {
            if (!(buffer[i] > 0x1F && buffer[i] < 0xFF && buffer[i] != 0x7F) && buffer[i] != '\0')
                buffer[i] = '.';
        }
        buffer[bytesRead] = '\0';
        return CStringW( buffer.get( ) );
    }
    else
    {
        #ifdef _DEBUG
        PrintOut( _T( "[ReadMemoryString]: Failed to read memory, GetLastError() = %s" ), Utils::GetLastErrorString( ).GetString( ) );
        #endif
        return CStringW( L".." );
    }
}

BOOLEAN PauseResumeThreadList( BOOL bResumeThread )
{
    if (g_hProcess == NULL)
        return FALSE;

    NTSTATUS status = STATUS_SUCCESS;
    ULONG bufferSize = 0x4000;
    DWORD ProcessId = GetProcessId( g_hProcess );
    PVOID SystemProcessInfo = malloc( bufferSize );
    if (!SystemProcessInfo)
    {
        #ifdef _DEBUG
        PrintOut( _T( "[PauseResumeThreadList]: Couldn't allocate system process info buffer!" ) );
        #endif
        return FALSE;
    }

    while (TRUE)
    {
        status = ntdll::NtQuerySystemInformation( SystemProcessInformation, SystemProcessInfo, bufferSize, &bufferSize );
        if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH)
        {
            if (SystemProcessInfo)
                free( SystemProcessInfo );
            SystemProcessInfo = malloc( bufferSize * 2 );
            if (!SystemProcessInfo)
            {
                #ifdef _DEBUG
                PrintOut( _T( "[PauseResumeThreadList]: Couldn't allocate system process info buffer!" ) );
                #endif
                return FALSE;
            }
        }
        else
        {
            break;
        }
    }

    if (!NT_SUCCESS( status ))
    {
        if (SystemProcessInfo)
            free( SystemProcessInfo );
        #ifdef _DEBUG
        PrintOut( _T( "[PauseResumeThreadList]: NtQuerySystemInformation failed with status 0x%08X" ), status );
        #endif
        return FALSE;
    }

    PSYSTEM_PROCESS_INFORMATION process;
    PSYSTEM_THREAD_INFORMATION threads;
    ULONG numberOfThreads;

    process = PROCESS_INFORMATION_FIRST_PROCESS( SystemProcessInfo );
    do
    {
        if (process->UniqueProcessId == (HANDLE)ProcessId)
            break;
    } while (process = PROCESS_INFORMATION_NEXT_PROCESS( process ));

    if (!process)
    {
        // The process doesn't exist anymore :(
        return 0;
    }

    threads = process->Threads;
    numberOfThreads = process->NumberOfThreads;

    for (ULONG i = 0; i < numberOfThreads; i++)
    {
        PSYSTEM_THREAD_INFORMATION thread = &threads[i];
        if (!thread)
            continue;
        DWORD thId = (DWORD)thread->ClientId.UniqueThread;
        if (!thId)
            continue;

        HANDLE hThread = ReClassOpenThread( THREAD_SUSPEND_RESUME, FALSE, thId );

        if (bResumeThread)
            ResumeThread( hThread );
        else
            SuspendThread( hThread );

        CloseHandle( hThread );
    }

    if (SystemProcessInfo)
        free( SystemProcessInfo );

    return 1;
}

ULONG_PTR GetBaseAddress( )
{
    std::lock_guard<std::mutex> lock(g_MemMapMutex);
    if (g_MemMap.size( ) > 1)
        return g_AttachedProcessAddress;
    return (ULONG_PTR)0x140000000;
}

BOOLEAN IsMemory(ULONG_PTR Address)
{
    std::lock_guard<std::mutex> lock(g_MemMapMutex);
    auto containingBlock = g_MemMap.lower_bound(Address);
    if (containingBlock != g_MemMap.end()
        && containingBlock->second.Start <= Address) {
        return true;
    }
    return false;
}

BOOLEAN IsModule( ULONG_PTR Address)
{
    return GetModule(Address) != nullptr;
}

const MemMapInfo* GetModule(ULONG_PTR Address)
{
    //std::lock_guard<std::mutex> lock(g_MemMapModulesMutex);
    auto containingModule = g_MemMapModules.lower_bound(Address);
    if (containingModule != g_MemMapModules.end()
        && containingModule->second.Start <= Address) {
        return &containingModule->second;
    }    
    return nullptr;
}

ULONG_PTR GetModuleBaseFromAddress( ULONG_PTR Address)
{
    std::lock_guard<std::mutex> lock(g_MemMapModulesMutex);
    auto containingModule = g_MemMapModules.lower_bound(Address);
    if (containingModule != g_MemMapModules.end()
        && containingModule->second.Start <= Address) {
        return containingModule->second.Start;
    }
    
    return 0;
}

CString GetSectionName(const MemMapInfo* module, ULONG_PTR Address)
{
    if (module == nullptr)
        return _T("");

    for (const auto& section : module->Sections)
    {
        if (Address >= section.Start && Address < section.End)
        {
            return section.Name;
        }
    }

    if (Address == module->Start) {
		return _T("BASE");
    }

    return _T("PE-HEADER");
}

CString GetFunctionNameFromExports(const MemMapInfo* module, ULONG_PTR Address)
{
    if (module == nullptr)
        return _T("");

    for (const auto& exportInfo : module->Exports)
    {
        if ((module->Start + exportInfo.FunctionRVA) == Address)
        {
            return exportInfo.FunctionName;
        }
    }

    return _T("");
}

CString GetAddressName(ULONG_PTR Address, BOOLEAN bJustAddress)
{
    CString txt;
    size_t i = 0;

    for (i = 0; i < g_Exports.size(); i++)
    {
        if (Address == g_Exports[i].Address)
        {
            txt.Format(_T("%s %IX"), g_Exports[i].Name, Address);
            return txt;
        }
    }

    const MemMapInfo* module = GetModule(Address);
    if (module != nullptr)
    {
        CString sectionName = GetSectionName(module, Address);
        CString functionName = GetFunctionNameFromExports(module, Address);

        if (functionName.IsEmpty())
        {
            txt.Format(_T("%s %s %IX"), module->Name, sectionName, Address);
        }
        else
        {
            txt.Format(_T("%s %s %IX [%s]"), module->Name, sectionName, Address, functionName);
        }
        return txt;
    }

    if (IsMemory(Address))
    {
        txt.Format(_T("%IX"), Address);
        return txt;
    }

    if (bJustAddress)
    {
        txt.Format(_T("%IX"), Address);
    }

    return txt;
}

CString GetModuleName( ULONG_PTR Address)
{
    const MemMapInfo* module = GetModule(Address);    
    if (module != nullptr)
        return module->Name;    
    return CString( "<unknown>" );
}

ULONG_PTR GetAddressFromName( CString moduleName )
{
    ULONG_PTR moduleAddress = 0;
    for (auto mi : g_MemMapModules)
    {
        if (mi.second.Name == moduleName)
        {
            moduleAddress = mi.second.Start;
            break;
        }
    }
    return moduleAddress;
}

BOOLEAN IsProcessHandleValid( HANDLE hProc )
{
    DWORD RetVal = WAIT_FAILED;
    if (!hProc)
        return FALSE;
    RetVal = WaitForSingleObject( hProc, 0 );
    if (RetVal == WAIT_FAILED)
        return FALSE;
    return (RetVal == WAIT_TIMEOUT) ? TRUE : FALSE;
}

void UpdateMemoryMapIncremental() {
    static std::map<ULONG_PTR, struct MemMapInfo> memMap;
    static bool structsInitialized = false;
    static bool mapInitialized = false;
    static SYSTEM_INFO SysInfo;
    static ULONGLONG lastExitTime = 0;
    MEMORY_BASIC_INFORMATION MemInfo;
    static ULONG_PTR pMemory;

    if (!structsInitialized) {
        structsInitialized = true;
        GetSystemInfo(&SysInfo);
        pMemory = (ULONG_PTR)SysInfo.lpMinimumApplicationAddress;
    }

    ULONGLONG entryTime = GetTickCount64();

    // Don't fire unless there's been at least 20ms since our last iteration
    if (entryTime - lastExitTime < 50) {
        return;
    }

    while ((GetTickCount64() - entryTime < 10 || !mapInitialized)) {
        SIZE_T vqBytes = VirtualQueryEx(g_hProcess, (LPCVOID)pMemory, &MemInfo, sizeof(MEMORY_BASIC_INFORMATION));
        if (vqBytes > 0) {
            if (MemInfo.State == MEM_COMMIT /*&& MemInfo.Type == MEM_PRIVATE*/) {
                MemMapInfo Mem;
                Mem.Start = (ULONG_PTR)pMemory;
                Mem.End = (ULONG_PTR)pMemory + MemInfo.RegionSize - 1;
                const MemMapInfo* containingModule = GetModule(Mem.Start);
                if (containingModule != nullptr) {
                    Mem.Name = containingModule->Name;
                }
                memMap[Mem.End] = Mem;
            }
            pMemory = (ULONG_PTR)MemInfo.BaseAddress + MemInfo.RegionSize;
        }
        else {
            // We've exhausted the memory space of the target process. Assign our temp
            // map to the global.
            {
                std::lock_guard<std::mutex> lock(g_MemMapMutex);
                g_MemMap = std::move(memMap);
            }

            memMap.clear();
            pMemory = (ULONG_PTR)SysInfo.lpMinimumApplicationAddress;

            break;
        }
    }

    if (!memMap.empty()) {
        mapInitialized = true;
    }
    lastExitTime = GetTickCount64();
}

BOOLEAN UpdateMemoryMap(void)
{
    std::lock_guard<std::mutex> lock(g_MemMapModulesMutex);

    g_MemMapModules.clear();
    g_Exports.clear();

    if (g_hProcess == NULL)
        return FALSE;

    if (!IsProcessHandleValid( g_hProcess ))
    {
        g_hProcess = NULL;
        return FALSE;
    }

    UpdateMemoryMapIncremental();

    PPROCESS_BASIC_INFORMATION ProcessInfo = NULL;
    PEB Peb;
    PEB_LDR_DATA LdrData;

    DWORD dwSize = sizeof(PROCESS_BASIC_INFORMATION);
    ProcessInfo = (PPROCESS_BASIC_INFORMATION)malloc(dwSize);
    if (!ProcessInfo)
    {
#ifdef _DEBUG
        PrintOut(_T("[UpdateMemoryMap]: Couldn't allocate process info buffer!"));
#endif
        return FALSE;
    }

    ULONG dwSizeNeeded = 0;
    NTSTATUS status = ntdll::NtQueryInformationProcess(g_hProcess, ProcessBasicInformation, ProcessInfo, dwSize, &dwSizeNeeded);
    if (status >= 0 && dwSize < dwSizeNeeded)
    {
        if (ProcessInfo)
            free(ProcessInfo);

        ProcessInfo = (PPROCESS_BASIC_INFORMATION)malloc(dwSizeNeeded);
        if (!ProcessInfo)
        {
#ifdef _DEBUG
            PrintOut(_T("[UpdateMemoryMap]: Couldn't allocate process info buffer!"));
#endif
            return FALSE;
        }

        status = ntdll::NtQueryInformationProcess(g_hProcess, ProcessBasicInformation, ProcessInfo, dwSizeNeeded, &dwSizeNeeded);
    }

    if (NT_SUCCESS(status))
    {
        if (!ProcessInfo->PebBaseAddress)
        {
#ifdef _DEBUG
            PrintOut(_T("[UpdateMemoryMap]: PEB is null! Aborting UpdateExports!"));
#endif
            if (ProcessInfo)
                free(ProcessInfo);
            return false;
        }

        SIZE_T dwBytesRead = 0;
        if (ReClassReadMemory((LPVOID)ProcessInfo->PebBaseAddress, &Peb, sizeof(PEB), &dwBytesRead) == 0)
        {
#ifdef _DEBUG
            PrintOut(_T("[UpdateMemoryMap]: Failed to read PEB! Aborting UpdateExports!"));
#endif
            if (ProcessInfo)
                free(ProcessInfo);
            return false;
        }

        dwBytesRead = 0;
        if (ReClassReadMemory((LPVOID)Peb.Ldr, &LdrData, sizeof(LdrData), &dwBytesRead) == 0)
        {
#ifdef _DEBUG
            PrintOut(_T("[UpdateMemoryMap]: Failed to read PEB Ldr Data! Aborting UpdateExports!"));
#endif
            if (ProcessInfo)
                free(ProcessInfo);
            return false;
        }

        LIST_ENTRY* pLdrListHead = (LIST_ENTRY*)LdrData.InLoadOrderModuleList.Flink;
        LIST_ENTRY* pLdrCurrentNode = LdrData.InLoadOrderModuleList.Flink;
        do
        {
            LDR_DATA_TABLE_ENTRY lstEntry = { 0 };
            dwBytesRead = 0;
            if (!ReClassReadMemory((LPVOID)pLdrCurrentNode, &lstEntry, sizeof(LDR_DATA_TABLE_ENTRY), &dwBytesRead))
            {
#ifdef _DEBUG
                PrintOut(_T("[UpdateMemoryMap]: Could not read list entry from LDR list. Error = %s"), Utils::GetLastErrorString().GetString());
#endif
                if (ProcessInfo)
                    free(ProcessInfo);
                return false;
            }

            pLdrCurrentNode = lstEntry.InLoadOrderLinks.Flink;

            if (lstEntry.DllBase != NULL)
            {
                UCHAR* ModuleBase = (UCHAR*)lstEntry.DllBase;
                DWORD ModuleSize = lstEntry.SizeOfImage;

                WCHAR wcsModulePath[MAX_PATH] = { 0 };
                WCHAR* wcsModuleName = NULL;

                if (lstEntry.FullDllName.Length > 0)
                {
                    dwBytesRead = 0;
                    if (ReClassReadMemory((LPVOID)lstEntry.FullDllName.Buffer, &wcsModulePath, lstEntry.FullDllName.Length, &dwBytesRead))
                    {
                        wcsModuleName = wcsrchr(wcsModulePath, L'\\');
                        if (!wcsModuleName)
                            wcsModuleName = wcsrchr(wcsModulePath, L'/');
                        wcsModuleName++;

                        if (g_AttachedProcessAddress == NULL)
                        {
                            WCHAR wcsModuleFilename[MAX_PATH] = { 0 };
                            GetModuleFileNameExW(g_hProcess, NULL, wcsModuleFilename, MAX_PATH);
                            if (_wcsicmp(wcsModuleFilename, wcsModulePath) == 0)
                            {
                                g_AttachedProcessAddress = (ULONG_PTR)ModuleBase;
                                g_AttachedProcessSize = ModuleSize;
                            }
                        }
                    }
                }

                MemMapInfo Mem;
                Mem.Start = (ULONG_PTR)ModuleBase;
                Mem.End = Mem.Start + ModuleSize;
                Mem.Size = ModuleSize;
                Mem.Name = wcsModuleName;
                Mem.Path = wcsModulePath;
                g_MemMapModules[Mem.End] = Mem;

                IMAGE_DOS_HEADER DosHdr;
                IMAGE_NT_HEADERS NtHdr;

                ReClassReadMemory(ModuleBase, &DosHdr, sizeof(IMAGE_DOS_HEADER), NULL);
                ReClassReadMemory(ModuleBase + DosHdr.e_lfanew, &NtHdr, sizeof(IMAGE_NT_HEADERS), NULL);

                DWORD sectionsSize = (DWORD)NtHdr.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
                PIMAGE_SECTION_HEADER sections = (PIMAGE_SECTION_HEADER)malloc(sectionsSize);
                ReClassReadMemory(ModuleBase + DosHdr.e_lfanew + sizeof(IMAGE_NT_HEADERS), sections, sectionsSize, NULL);

                for (int i = 0; i < NtHdr.FileHeader.NumberOfSections; i++)
                {
                    SectionInfo sectionInfo;
                    char sectionNameBuffer[9] = { 0 };
                    memcpy(sectionNameBuffer, sections[i].Name, 8);

                    sectionInfo.Name = CString(sectionNameBuffer);
                    sectionInfo.Name.MakeLower();

                    sectionInfo.Start = (ULONG_PTR)ModuleBase + sections[i].VirtualAddress;
                    sectionInfo.End = sectionInfo.Start + sections[i].Misc.VirtualSize;

                    g_MemMapModules[Mem.End].Sections.push_back(sectionInfo);
                }
                free(sections);

                if (NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress != 0)
                {
                    ULONG_PTR exportDirRVA = NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
                    IMAGE_EXPORT_DIRECTORY exportDirectory;
                    if (ReClassReadMemory((LPVOID)(ModuleBase + exportDirRVA), &exportDirectory, sizeof(IMAGE_EXPORT_DIRECTORY)))
                    {
                        ULONG_PTR addressOfFunctions = (ULONG_PTR)ModuleBase + exportDirectory.AddressOfFunctions;
                        ULONG_PTR addressOfNames = (ULONG_PTR)ModuleBase + exportDirectory.AddressOfNames;
                        ULONG_PTR addressOfNameOrdinals = (ULONG_PTR)ModuleBase + exportDirectory.AddressOfNameOrdinals;

                        for (DWORD i = 0; i < exportDirectory.NumberOfNames; i++)
                        {
                            ExportInfo exportInfo;
                            WORD nameOrdinal = 0;
                            if (ReClassReadMemory((LPVOID)(addressOfNameOrdinals + i * sizeof(WORD)), &nameOrdinal, sizeof(WORD)))
                            {
                                DWORD funcRVA = 0;
                                if (ReClassReadMemory((LPVOID)(addressOfFunctions + nameOrdinal * sizeof(DWORD)), &funcRVA, sizeof(DWORD)))
                                {
                                    exportInfo.FunctionRVA = funcRVA;

                                    DWORD nameRVA = 0;
                                    if (ReClassReadMemory((LPVOID)(addressOfNames + i * sizeof(DWORD)), &nameRVA, sizeof(DWORD)))
                                    {
                                        char functionNameBuffer[128] = { 0 };
                                        if (ReClassReadMemory((LPVOID)(ModuleBase + nameRVA), functionNameBuffer, sizeof(functionNameBuffer)))
                                        {
                                            exportInfo.FunctionName = CString(functionNameBuffer);
                                            g_MemMapModules[Mem.End].Exports.push_back(exportInfo);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            } while (pLdrListHead != pLdrCurrentNode);
        }
    else
    {
        PrintOutDbg(_T("[UpdateExports]: NtQueryInformationProcess failed! Aborting..."));
        if (ProcessInfo)
            free(ProcessInfo);
        return 0;
    }

    if (ProcessInfo)
        free(ProcessInfo);

    return true;
}

void UpdateMemoryMapThread() {
    while (true) {
        UpdateMemoryMap();

        Sleep(100);
    }
}

BOOLEAN UpdateExports( )
{
    g_Exports.clear( );

    //if (!g_bExports) 
    //	return;

    PPROCESS_BASIC_INFORMATION ProcessInfo = NULL;
    PEB Peb;
    PEB_LDR_DATA LdrData;

    // Try to allocate buffer 
    DWORD dwSize = sizeof( PROCESS_BASIC_INFORMATION );
    ProcessInfo = (PPROCESS_BASIC_INFORMATION)malloc( dwSize );
    if (!ProcessInfo)
    {
        PrintOutDbg( _T( "[UpdateExports]: Couldn't allocate heap buffer!" ) );
        return FALSE;
    }

    ULONG dwSizeNeeded = 0;
    NTSTATUS status = ntdll::NtQueryInformationProcess( g_hProcess, ProcessBasicInformation, ProcessInfo, dwSize, &dwSizeNeeded );
    if (status >= 0 && dwSize < dwSizeNeeded)
    {
        if (ProcessInfo)
            free( ProcessInfo );

        ProcessInfo = (PPROCESS_BASIC_INFORMATION)malloc( dwSizeNeeded );
        if (!ProcessInfo)
        {
            PrintOutDbg( _T( "[UpdateExports]: Couldn't allocate heap buffer!" ) );
            return FALSE;
        }

        status = ntdll::NtQueryInformationProcess( g_hProcess, ProcessBasicInformation, ProcessInfo, dwSizeNeeded, &dwSizeNeeded );
    }

    // Did we successfully get basic info on process
    if (NT_SUCCESS( status ))
    {
        // Check for PEB
        if (!ProcessInfo->PebBaseAddress)
        {
            PrintOutDbg( _T( "[UpdateExports]: PEB is null! Aborting..." ) );
            if (ProcessInfo)
                free( ProcessInfo );
            return FALSE;
        }

        // Read Process Environment Block (PEB)
        SIZE_T dwBytesRead = 0;
        if (ReClassReadMemory( (LPVOID)ProcessInfo->PebBaseAddress, &Peb, sizeof( PEB ), &dwBytesRead ) == 0)
        {
            PrintOutDbg( _T( "[UpdateExports]: Failed to read PEB! Aborting UpdateExports." ) );
            if (ProcessInfo)
                free( ProcessInfo );
            return FALSE;
        }

        // Get Ldr
        dwBytesRead = 0;
        if (ReClassReadMemory( (LPVOID)Peb.Ldr, &LdrData, sizeof( LdrData ), &dwBytesRead ) == 0)
        {
            PrintOutDbg( _T( "[UpdateExports]: Failed to read PEB Ldr Data! Aborting UpdateExports." ) );
            if (ProcessInfo)
                free( ProcessInfo );
            return 0;
        }

        LIST_ENTRY *pLdrListHead = (LIST_ENTRY *)LdrData.InLoadOrderModuleList.Flink;
        LIST_ENTRY *pLdrCurrentNode = LdrData.InLoadOrderModuleList.Flink;
        do
        {
            LDR_DATA_TABLE_ENTRY lstEntry = { 0 };
            dwBytesRead = 0;
            if (!ReClassReadMemory( (void*)pLdrCurrentNode, &lstEntry, sizeof( LDR_DATA_TABLE_ENTRY ), &dwBytesRead ))
            {
                if (ProcessInfo)
                    free( ProcessInfo );
                return 0;
            }

            pLdrCurrentNode = lstEntry.InLoadOrderLinks.Flink;

            if (lstEntry.DllBase != nullptr && lstEntry.SizeOfImage != 0)
            {
                unsigned char* ModuleHandle = (unsigned char*)lstEntry.DllBase;
                wchar_t wcsDllName[MAX_PATH] = { 0 };
                wchar_t ModuleName[MAX_PATH] = { 0 };
                if (lstEntry.BaseDllName.Length > 0)
                {
                    dwBytesRead = 0;
                    if (ReClassReadMemory( (LPVOID)lstEntry.BaseDllName.Buffer, &wcsDllName, lstEntry.BaseDllName.Length, &dwBytesRead ))
                    {
                        wcscpy_s( ModuleName, wcsDllName );
                    }
                }

                IMAGE_DOS_HEADER DosHdr;
                IMAGE_NT_HEADERS NtHdr;

                ReClassReadMemory( ModuleHandle, &DosHdr, sizeof( DosHdr ), NULL );
                ReClassReadMemory( ModuleHandle + DosHdr.e_lfanew, &NtHdr, sizeof( IMAGE_NT_HEADERS ), NULL );
                if (NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress != 0)
                {
                    IMAGE_EXPORT_DIRECTORY ExpDir;
                    ReClassReadMemory( (LPVOID)(ModuleHandle + NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress), &ExpDir, sizeof( ExpDir ), NULL );
                    PVOID pName = (PVOID)(ModuleHandle + ExpDir.AddressOfNames);
                    PVOID pOrd = (PVOID)(ModuleHandle + ExpDir.AddressOfNameOrdinals);
                    PVOID pAddress = (PVOID)(ModuleHandle + ExpDir.AddressOfFunctions);

                    ULONG aNames[MAX_EXPORTS];
                    WORD aOrds[MAX_EXPORTS];
                    ULONG aAddresses[MAX_EXPORTS];
                    ReClassReadMemory( (LPVOID)pName, aNames, ExpDir.NumberOfNames * sizeof( aNames[0] ), NULL );
                    ReClassReadMemory( (LPVOID)pOrd, aOrds, ExpDir.NumberOfNames * sizeof( aOrds[0] ), NULL );
                    ReClassReadMemory( (LPVOID)pAddress, aAddresses, ExpDir.NumberOfFunctions * sizeof( aAddresses[0] ), NULL );

                    for (DWORD i = 0; i < ExpDir.NumberOfNames; i++)
                    {
                        char ExportName[256];
                        ReClassReadMemory( (LPVOID)(ModuleHandle + aNames[i]), ExportName, 256, NULL );
                        DWORD_PTR Address = (DWORD_PTR)ModuleHandle + aAddresses[aOrds[i]];

                        AddressName Entry;
                        Entry.Name.Format( _T( "%ls.%hs" ), ModuleName, ExportName );
                        Entry.Address = Address;
                        // Add export entry to array
                        g_Exports.push_back( Entry );
                    }

                }
            }
        } while (pLdrListHead != pLdrCurrentNode);
    }
    else
    {
        PrintOutDbg( _T( "[UpdateExports]: NtQueryInformationProcess failed! Aborting..." ) );
        if (ProcessInfo)
            free( ProcessInfo );
        return 0;
    }

    if (ProcessInfo)
        free( ProcessInfo );

    return 1;
}

int SplitString( const CString& input, const CString& delimiter, CStringArray& results )
{
    int iPos = 0;
    int newPos = -1;
    int sizeS2 = delimiter.GetLength( );
    int isize = input.GetLength( );

    CArray<INT, int> positions;

    newPos = input.Find( delimiter, 0 );
    if (newPos < 0)
        return 0;

    int numFound = 0;
    while (newPos > iPos)
    {
        numFound++;
        positions.Add( newPos );
        iPos = newPos;
        newPos = input.Find( delimiter, iPos + sizeS2 + 1 );
    }

    for (int i = 0; i <= positions.GetSize( ); i++)
    {
        CString s;
        if (i == 0)
        {
            s = input.Mid( i, positions[i] );
        }
        else
        {
            int offset = positions[i - 1] + sizeS2;
            if (offset < isize)
            {
                if (i == positions.GetSize( ))
                    s = input.Mid( offset );
                else if (i > 0)
                    s = input.Mid( positions[i - 1] + sizeS2, positions[i] - positions[i - 1] - sizeS2 );
            }
        }

        if (s.GetLength( ) > 0)
        {
            results.Add( s );
        }
    }
    return numFound;
}

ULONG_PTR ConvertStrToAddress( CString str )
{
    CStringArray chunks;
    if (SplitString( str, "+", chunks ) == 0)
        chunks.Add( str );

    ULONG_PTR Final = 0;

    for (UINT i = 0; i < (UINT)chunks.GetCount( ); i++)
    {
        CString a = chunks[i];

        a.MakeLower( );  // Make all lowercase
        a.Trim( );		// Trim whitespace
        a.Remove( _T( '\"' ) ); // Remove quotes

        bool bPointer = false;
        bool bMod = false;

        if (a.Find( _T( ".exe" ) ) != -1 || a.Find( _T( ".dll" ) ) != -1)
        {
            bMod = true;
        }

        if (a[0] == _T( '*' ))
        {
            bPointer = true;
            a = a.Mid( 1 );
        }
        else if (a[0] == _T( '&' ))
        {
            bMod = true;
            a = a.Mid( 1 );
        }

        ULONG_PTR curadd = 0;

        if (bMod)
        {
            for (auto mi : g_MemMapModules)
            {
                CString ModName = mi.second.Name;
                ModName.MakeLower( );
                if ( ModName.CompareNoCase( a ) == 0 )
                {
                    curadd = mi.second.Start;
                    bMod = true;
                    break;
                }
            }
        }
        else
        {
            curadd = (ULONG_PTR)_tcstoui64( a.GetBuffer( ), NULL, 16 ); //StrToNum
        }

        Final += curadd;

        if (bPointer)
        {
            if (!ReClassReadMemory( (LPVOID)Final, &Final, sizeof( Final ), NULL ))
            {
                // Causing memory leaks when Final doesnt point to a valid address.
                //#ifdef _DEBUG
                // PrintOut(_T("[ConvertStrToAddress]: Failed to read memory. Error: %s"), Utils::GetLastErrorString().c_str());
                //#endif
                return 0xDEADBEEF;
            }
        }
    }

    return Final;
}
