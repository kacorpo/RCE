#include "stdafx.h"

#include <memory>
#include <Psapi.h>

//Globals
HANDLE g_hProcess = NULL;
DWORD g_ProcessID = NULL;
ULONG_PTR g_AttachedProcessAddress = NULL;
DWORD g_AttachedProcessSize = NULL;
CString g_ProcessName;

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
std::map<ULONG_PTR, MemMapInfo>     g_MemMap;
std::vector<MemMapInfo>             g_MemMapCode;
std::vector<MemMapInfo>             g_MemMapData;
std::map<ULONG_PTR, MemMapInfo>     g_MemMapModules;
std::vector<AddressName>            g_Exports;
std::vector<AddressName>            g_CustomNames;

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

BOOL ReClassReadMemory( LPVOID Address, LPVOID Buffer, SIZE_T Size, PSIZE_T BytesRead )
{
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
    if (g_MemMap.size( ) > 1)
        return g_AttachedProcessAddress;
    #ifdef _WIN64
    return (ULONG_PTR)0x140000000;
    #else
    return (ULONG_PTR)0x400000;
    #endif
}

BOOLEAN IsCode( ULONG_PTR Address )
{    
    for (size_t i = 0; i < g_MemMapCode.size( ); i++)
    {
        if ((Address >= g_MemMapCode[i].Start) && (Address <= g_MemMapCode[i].End))
            return TRUE;
    }
    return FALSE;
}

BOOLEAN IsData( ULONG_PTR Address )
{
    for (size_t i = 0; i < g_MemMapData.size( ); i++)
    {
        if ((Address >= g_MemMapData[i].Start) && (Address <= g_MemMapData[i].End))
            return true;
    }
    return false;
}

BOOLEAN IsMemory( ULONG_PTR Address )
{
    auto containingBlock = g_MemMap.lower_bound(Address);
    if (containingBlock != g_MemMap.end()
        && containingBlock->second.Start <= Address) {
        return true;
    }
    return false;
}

BOOLEAN IsModule( ULONG_PTR Address )
{
    return GetModule(Address) != nullptr;
}

const MemMapInfo* GetModule(ULONG_PTR Address)
{
    auto containingModule = g_MemMapModules.lower_bound(Address);
    if (containingModule != g_MemMapModules.end()
        && containingModule->second.Start <= Address) {
        return &containingModule->second;
    }    
    return nullptr;
}

ULONG_PTR GetModuleBaseFromAddress( ULONG_PTR Address )
{
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

    IMAGE_DOS_HEADER dosHeader;
    if (!ReClassReadMemory((LPVOID)module->Start, &dosHeader, sizeof(IMAGE_DOS_HEADER)))
        return _T("");

    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        return _T("");

    ULONG_PTR ntHeadersAddress = module->Start + dosHeader.e_lfanew;
    IMAGE_NT_HEADERS ntHeaders;
    if (!ReClassReadMemory((LPVOID)ntHeadersAddress, &ntHeaders, sizeof(IMAGE_NT_HEADERS)))
        return _T("");

    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE)
        return _T("");

    PIMAGE_SECTION_HEADER sectionHeaders = new IMAGE_SECTION_HEADER[ntHeaders.FileHeader.NumberOfSections];
    ULONG_PTR sectionHeadersAddress = ntHeadersAddress + sizeof(IMAGE_NT_HEADERS);
    if (!ReClassReadMemory((LPVOID)sectionHeadersAddress, sectionHeaders, sizeof(IMAGE_SECTION_HEADER) * ntHeaders.FileHeader.NumberOfSections))
    {
        delete[] sectionHeaders;
        return _T("");
    }

	CString sectionName = _T("PE-HEADER");
    for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; i++)
    {
        ULONG_PTR sectionStart = module->Start + sectionHeaders[i].VirtualAddress;
        ULONG_PTR sectionEnd = sectionStart + sectionHeaders[i].Misc.VirtualSize;

        if (Address >= sectionStart && Address < sectionEnd)
        {
            sectionName = CString((char*)sectionHeaders[i].Name);
            break;
        }
    }

    delete[] sectionHeaders;
    return sectionName;
}

CString GetFunctionNameFromExports(const MemMapInfo* module, ULONG_PTR Address)
{
    if (module == nullptr)
        return _T("");

    // Read DOS header
    IMAGE_DOS_HEADER dosHeader;
    if (!ReClassReadMemory((LPVOID)module->Start, &dosHeader, sizeof(IMAGE_DOS_HEADER)))
        return _T("");

    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        return _T(""); // Not a valid DOS header

    // Read NT headers
    ULONG_PTR ntHeadersAddress = module->Start + dosHeader.e_lfanew;
    IMAGE_NT_HEADERS ntHeaders;
    if (!ReClassReadMemory((LPVOID)ntHeadersAddress, &ntHeaders, sizeof(IMAGE_NT_HEADERS)))
        return _T("");

    if (ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress == 0)
        return _T(""); // No export directory

    // Read export directory
    ULONG_PTR exportDirRVA = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    IMAGE_EXPORT_DIRECTORY exportDirectory;
    if (!ReClassReadMemory((LPVOID)(module->Start + exportDirRVA), &exportDirectory, sizeof(IMAGE_EXPORT_DIRECTORY)))
        return _T("");

    // Get the RVA of the functions, names, and ordinals
    ULONG_PTR addressOfFunctions = module->Start + exportDirectory.AddressOfFunctions;
    ULONG_PTR addressOfNames = module->Start + exportDirectory.AddressOfNames;
    ULONG_PTR addressOfNameOrdinals = module->Start + exportDirectory.AddressOfNameOrdinals;

    char functionNameBuffer[128] = { 0 };

    // Iterate over the export names to find the function corresponding to the address
    for (DWORD i = 0; i < exportDirectory.NumberOfNames; i++)
    {
        // Get the name ordinal and function address
        WORD nameOrdinal = 0;
        if (!ReClassReadMemory((LPVOID)(addressOfNameOrdinals + i * sizeof(WORD)), &nameOrdinal, sizeof(WORD)))
            continue;

        DWORD funcRVA = 0;
        if (!ReClassReadMemory((LPVOID)(addressOfFunctions + nameOrdinal * sizeof(DWORD)), &funcRVA, sizeof(DWORD)))
            continue;

        // Get the function name
        DWORD nameRVA = 0;
        if (!ReClassReadMemory((LPVOID)(addressOfNames + i * sizeof(DWORD)), &nameRVA, sizeof(DWORD)))
            continue;

        if (!ReClassReadMemory((LPVOID)(module->Start + nameRVA), functionNameBuffer, sizeof(functionNameBuffer)))
            continue;

        // Compare the function RVA with the provided address
        if ((module->Start + funcRVA) == Address)
        {
            return CString(functionNameBuffer); // Return the function name
        }
    }

    return _T(""); // No matching function found
}

CString GetAddressName(ULONG_PTR Address, BOOLEAN bJustAddress)
{
    CString txt;
    size_t i = 0;

    // Check Custom Names
    for (i = 0; i < g_CustomNames.size(); i++)
    {
        if (Address == g_CustomNames[i].Address)
        {
#ifdef _WIN64
            txt.Format(_T("%s %IX"), g_CustomNames[i].Name, Address);
#else
            txt.Format(_T("%s %X"), g_CustomNames[i].Name, Address);
#endif
            return txt;
        }
    }

    // Check Exports
    for (i = 0; i < g_Exports.size(); i++)
    {
        if (Address == g_Exports[i].Address)
        {
#ifdef _WIN64
            txt.Format(_T("%s %IX"), g_Exports[i].Name, Address);
#else
            txt.Format(_T("%s %X"), g_Exports[i].Name, Address);
#endif
            return txt;
        }
    }

    // Check Memory Maps
    for (i = 0; i < g_MemMapCode.size(); i++)
    {
        if ((Address >= g_MemMapCode[i].Start) && (Address <= g_MemMapCode[i].End))
        {
#ifdef _WIN64
            txt.Format(_T("<CODE> %s %IX"), g_MemMapCode[i].Name, Address);
#else
            txt.Format(_T("<CODE> %s %X"), g_MemMapCode[i].Name, Address);
#endif
            return txt;
        }
    }

    for (i = 0; i < g_MemMapData.size(); i++)
    {
        if ((Address >= g_MemMapData[i].Start) && (Address <= g_MemMapData[i].End))
        {
#ifdef _WIN64
            txt.Format(_T("<DATA> %s %IX"), g_MemMapData[i].Name, Address);
#else
            txt.Format(_T("<DATA> %s %X"), g_MemMapData[i].Name, Address);
#endif
            return txt;
        }
    }

    // Locate the module that contains the address
    const MemMapInfo* module = GetModule(Address);
    if (module != nullptr)
    {
        // Retrieve the section name and the function name from exports
        CString sectionName = GetSectionName(module, Address);
        CString functionName = GetFunctionNameFromExports(module, Address);

#ifdef _WIN64
        if (functionName.IsEmpty())
        {
            txt.Format(_T("%s %s %IX"), module->Name, sectionName, Address);
        }
        else
        {
            txt.Format(_T("%s %s %IX [%s]"), module->Name, sectionName, Address, functionName);
        }
#else
        if (functionName.IsEmpty())
        {
            txt.Format(_T("%s %s %X"), module->Name, sectionName, Address);
        }
        else
        {
            txt.Format(_T("%s %s %X [%s]"), module->Name, sectionName, Address, functionName);
        }
#endif
        return txt;
    }

    // Address belongs to an allocated memory region
    if (IsMemory(Address))
    {
#ifdef _WIN64
        txt.Format(_T("%IX"), Address);
#else
        txt.Format(_T("%X"), Address);
#endif
        return txt;
    }

    // Just the address without additional information
    if (bJustAddress)
    {
#ifdef _WIN64
        txt.Format(_T("%IX"), Address);
#else
        txt.Format(_T("%X"), Address);
#endif
    }

    return txt;
}

CString GetModuleName( ULONG_PTR Address )
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
    if (lastExitTime - entryTime < 50)
        return;

    while ((GetTickCount64() - entryTime < 10 || !mapInitialized))
    {
        SIZE_T vqBytes = VirtualQueryEx(g_hProcess, (LPCVOID)pMemory, &MemInfo, sizeof(MEMORY_BASIC_INFORMATION));
        if (vqBytes > 0)
        {
            if (MemInfo.State == MEM_COMMIT /*&& MemInfo.Type == MEM_PRIVATE*/)
            {
                MemMapInfo Mem;
                Mem.Start = (ULONG_PTR)pMemory;
                Mem.End = (ULONG_PTR)pMemory + MemInfo.RegionSize - 1;
                const MemMapInfo* containingModule = GetModule(Mem.Start);
                if (containingModule != nullptr)
                    Mem.Name = containingModule->Name;
                memMap[Mem.End] = Mem;
            }
            pMemory = (ULONG_PTR)MemInfo.BaseAddress + MemInfo.RegionSize;
        }
        else
        {
            // We've exhausted the memory space of the target process.  Assign our temp
            // map to the global.  
            g_MemMap = std::move(memMap);
            memMap.clear();
            pMemory = (ULONG_PTR)SysInfo.lpMinimumApplicationAddress;
            
            break;
        }
    }
    
    if (g_MemMap.size() > 0)
        mapInitialized = true;
    lastExitTime = GetTickCount64();
}

BOOLEAN UpdateMemoryMap( void )
{    
    g_MemMapCode.clear( );
    g_MemMapData.clear( );
    g_MemMapModules.clear( );
    g_Exports.clear( );
    g_CustomNames.clear( );

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

    // Try to allocate buffer 
    DWORD dwSize = sizeof( PROCESS_BASIC_INFORMATION );
    ProcessInfo = (PPROCESS_BASIC_INFORMATION)malloc( dwSize );
    if (!ProcessInfo)
    {
        #ifdef _DEBUG
        PrintOut( _T( "[UpdateMemoryMap]: Couldn't allocate process info buffer!" ) );
        #endif
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
            #ifdef _DEBUG
            PrintOut( _T( "[UpdateMemoryMap]: Couldn't allocate process info buffer!" ) );
            #endif
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
            #ifdef _DEBUG
            PrintOut( _T( "[UpdateMemoryMap]: PEB is null! Aborting UpdateExports!" ) );
            #endif
            if (ProcessInfo)
                free( ProcessInfo );
            return false;
        }

        // Read Process Environment Block (PEB)
        SIZE_T dwBytesRead = 0;
        if (ReClassReadMemory( (LPVOID)ProcessInfo->PebBaseAddress, &Peb, sizeof( PEB ), &dwBytesRead ) == 0)
        {
            #ifdef _DEBUG
            PrintOut( _T( "[UpdateMemoryMap]: Failed to read PEB! Aborting UpdateExports!" ) );
            #endif
            if (ProcessInfo)
                free( ProcessInfo );
            return false;
        }

        // Get Ldr
        dwBytesRead = 0;
        if (ReClassReadMemory( (LPVOID)Peb.Ldr, &LdrData, sizeof( LdrData ), &dwBytesRead ) == 0)
        {
            #ifdef _DEBUG
            PrintOut( _T( "[UpdateMemoryMap]: Failed to read PEB Ldr Data! Aborting UpdateExports!" ) );
            #endif
            if (ProcessInfo)
                free( ProcessInfo );
            return false;
        }

        LIST_ENTRY *pLdrListHead = (LIST_ENTRY *)LdrData.InLoadOrderModuleList.Flink;
        LIST_ENTRY *pLdrCurrentNode = LdrData.InLoadOrderModuleList.Flink;
        do
        {
            LDR_DATA_TABLE_ENTRY lstEntry = { 0 };
            dwBytesRead = 0;
            if (!ReClassReadMemory( (LPVOID)pLdrCurrentNode, &lstEntry, sizeof( LDR_DATA_TABLE_ENTRY ), &dwBytesRead ))
            {
                #ifdef _DEBUG
                PrintOut( _T( "[UpdateMemoryMap]: Could not read list entry from LDR list. Error = %s" ), Utils::GetLastErrorString( ).GetString( ) );
                #endif
                if (ProcessInfo)
                    free( ProcessInfo );
                return false;
            }

            pLdrCurrentNode = lstEntry.InLoadOrderLinks.Flink;

            if (lstEntry.DllBase != NULL /*&& lstEntry.SizeOfImage != 0*/)
            {
                UCHAR* ModuleBase = (UCHAR*)lstEntry.DllBase;
                DWORD ModuleSize = lstEntry.SizeOfImage;

                WCHAR wcsModulePath[MAX_PATH] = { 0 };
                WCHAR* wcsModuleName = NULL;

                if (lstEntry.FullDllName.Length > 0)
                {
                    dwBytesRead = 0;
                    if (ReClassReadMemory( (LPVOID)lstEntry.FullDllName.Buffer, &wcsModulePath, lstEntry.FullDllName.Length, &dwBytesRead ))
                    {
                        wcsModuleName = wcsrchr( wcsModulePath, L'\\' );
                        if (!wcsModuleName)
                            wcsModuleName = wcsrchr( wcsModulePath, L'/' );
                        wcsModuleName++;

                        if (g_AttachedProcessAddress == NULL)
                        {
                            WCHAR wcsModuleFilename[MAX_PATH] = { 0 };
                            GetModuleFileNameExW( g_hProcess, NULL, wcsModuleFilename, MAX_PATH );
                            if (_wcsicmp( wcsModuleFilename, wcsModulePath ) == 0)
                            {
                                g_AttachedProcessAddress = (ULONG_PTR)ModuleBase;
                                g_AttachedProcessSize = ModuleSize;
                            }
                        }
                    }
                }

                // module info
                MemMapInfo Mem;
                Mem.Start = (ULONG_PTR)ModuleBase;
                Mem.End = Mem.Start + ModuleSize;
                Mem.Size = ModuleSize;
                #ifdef UNICODE
                Mem.Name = wcsModuleName;
                Mem.Path = wcsModulePath;
                #else
                Mem.Name = CW2A( wcsModuleName );
                Mem.Path = CW2A( wcsModulePath );
                #endif
                g_MemMapModules[Mem.End] = Mem ;

                // module code
                IMAGE_DOS_HEADER DosHdr;
                IMAGE_NT_HEADERS NtHdr;

                ReClassReadMemory( ModuleBase, &DosHdr, sizeof( IMAGE_DOS_HEADER ), NULL );
                ReClassReadMemory( ModuleBase + DosHdr.e_lfanew, &NtHdr, sizeof( IMAGE_NT_HEADERS ), NULL );
                DWORD sectionsSize = (DWORD)NtHdr.FileHeader.NumberOfSections * sizeof( IMAGE_SECTION_HEADER );
                PIMAGE_SECTION_HEADER sections = (PIMAGE_SECTION_HEADER)malloc( sectionsSize );
                ReClassReadMemory( ModuleBase + DosHdr.e_lfanew + sizeof( IMAGE_NT_HEADERS ), sections, sectionsSize, NULL );
                for (int i = 0; i < NtHdr.FileHeader.NumberOfSections; i++)
                {
                    CString txt;
                    MemMapInfo Mem;
                    txt.Format( _T( "%.8s" ), sections[i].Name ); txt.MakeLower( );
                    if (txt == ".text" || txt == "code")
                    {
                        Mem.Start = (ULONG_PTR)ModuleBase + sections[i].VirtualAddress;
                        Mem.End = Mem.Start + sections[i].Misc.VirtualSize;
                        Mem.Name = wcsModuleName;
                        g_MemMapCode.push_back( Mem );
                    }
                    else if (txt == ".data" || txt == "data" || txt == ".rdata" || txt == ".idata")
                    {
                        Mem.Start = (ULONG_PTR)ModuleBase + sections[i].VirtualAddress;
                        Mem.End = Mem.Start + sections[i].Misc.VirtualSize;
                        Mem.Name = wcsModuleName;
                        g_MemMapData.push_back( Mem );
                    }
                }
                // Free sections
                free( sections );
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

    return true;
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
                PrintOutDbg( _T( "[UpdateExports]: Could not read list entry from LDR list. Error = %s" ), Utils::GetLastErrorString( ).GetString( ) );
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
