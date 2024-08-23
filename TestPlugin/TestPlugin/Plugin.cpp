#include "Plugin.h"
#include "resource.h"
#include "vmmdll.h"

BOOL gPluginState = FALSE;

HANDLE g_DeviceHandle = INVALID_HANDLE_VALUE;

typedef enum _REQUEST_TYPE : UINT {
    WRITE,
    READ,
    MAINBASE,
    PEBBASE,
} REQUEST_TYPE;

typedef struct _REQUEST_DATA {
    UINT Type;
    PVOID Arguments;
    NTSTATUS* Status;
} REQUEST_DATA, * PREQUEST_DATA;

typedef struct _REQUEST_BASE {
    DWORD ProcessId;
    PBYTE* OutAddress;
} REQUEST_BASE, * PREQUEST_BASE;

typedef struct _REQUEST_READ {
    DWORD ProcessId;
    PVOID Dest;
    PVOID Src;
    DWORD Size;
} REQUEST_READ, * PREQUEST_READ;

typedef struct _REQUEST_WRITE {
    DWORD ProcessId;
    PVOID Dest;
    PVOID Src;
    DWORD Size;
} REQUEST_WRITE, * PREQUEST_WRITE;


const bool SendRequest(UINT type, PVOID args) {
    REQUEST_DATA req;
    NTSTATUS status;
    req.Type = type;
    req.Arguments = args;
    req.Status = &status;

    __int64 InBuffer = (__int64)&req;
    HANDLE OutBuffer = 0;
    DWORD BytesReturned;

    BOOL ok = DeviceIoControl(g_DeviceHandle, 0x83350050, &InBuffer, 8u, &OutBuffer, 8u, &BytesReturned, 0i64);

	if (!ok) {
		RCPrintConsole(L"[RCEPlugin] DeviceIoControl failed with error %d\n", GetLastError());
        
        return false;
	}

	return NT_SUCCESS(status);
}

bool setPrivilege(
    HANDLE hToken,          // token handle 
    LPCTSTR lpszPrivilege,  // privilege to enable/disable 
    BOOL bEnablePrivilege   // to enable or disable privilege
) {
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(
        NULL,            // lookup privilege on local system
        lpszPrivilege,   // privilege to lookup 
        &luid)) {        // receives LUID of privilege
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege) {
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    }
    else {
        tp.Privileges[0].Attributes = 0;
    }

    if (!AdjustTokenPrivileges(
        hToken,
        FALSE,
        &tp,
        sizeof(TOKEN_PRIVILEGES),
        (PTOKEN_PRIVILEGES)NULL,
        (PDWORD)NULL)) {
        printf("AdjustTokenPrivileges error: %u\n", GetLastError());
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        return FALSE;
    }

    return TRUE;
}

bool enableSeDebugPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    if (!setPrivilege(hToken, SE_DEBUG_NAME, TRUE)) {
        return false;
    }

    return true;
}

BOOL 
PLUGIN_CC 
PluginInit( 
    OUT LPRECLASS_PLUGIN_INFO lpRCInfo 
)
{
    wcscpy_s( lpRCInfo->Name, L"RCEPlugin" );
    wcscpy_s( lpRCInfo->Version, L"1.2.3.5" );
    wcscpy_s( lpRCInfo->About, L"PluginEx" );
    lpRCInfo->DialogId = IDD_SETTINGS_DLG;

	enableSeDebugPrivilege();

    g_DeviceHandle = CreateFileW((L"\\\\.\\Global\\PROCEXP152"), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

    RCPrintConsole(L"[RCEPlugin] g_DeviceHandle: %x", g_DeviceHandle);

	if (g_DeviceHandle == INVALID_HANDLE_VALUE) {
		RCPrintConsole(L"[RCEPlugin] Failed to open device handle with error %d", GetLastError());
		MessageBoxW(RCMainWindow(), L"[RCEPlugin] Failed to open device handle!", L"RCEPlugin", MB_ICONERROR);
		return false;
	}


    //LPCSTR szParams[] = { "", "-device", "fpga", "-norefresh" };
    //bool result = VMMDLL_Initialize(3, (LPSTR*)szParams);
    //
    //if (!result) {
    //    RCPrintConsole(L"[RCEPlugin] Failed to initialize memory process file system in call to vmm.dll!VMMDLL_Initialize (Error: %d)", GetLastError());
    //    //return false;
    //}
    //else {
    //    RCPrintConsole(L"[RCEPlugin] vmm.dll!VMMDLL_Initialize success");
    //}
    
   if (!RCIsReadMemoryOverriden() && !RCIsWriteMemoryOverriden())
   {
       if (RCOverrideMemoryOperations( ReadCallback, WriteCallback ) == FALSE)
       {
           RCPrintConsole( L"[RCEPlugin] Failed to register read/write callbacks, failing PluginInit" );
           return FALSE;
       }
   }

   MessageBoxW(RCMainWindow(), L"[RCEPlugin] Loaded successfully!", L"RCEPlugin", MB_ICONINFORMATION);

    gPluginState = TRUE;

    return TRUE;
}

VOID 
PLUGIN_CC 
PluginStateChange( 
    IN BOOL State 
)
{
    //
    // Update global state variable
    //
    gPluginState = State;


    if (State)
    {
        RCPrintConsole( L"[RCEPlugin] Enabled" );

        //
        // Nothing for now.
        //
    }
    else
    {
        RCPrintConsole( L"[RCEPlugin] Disabled" );

        //
        // Remove our overrides if we're disabling/disabled.
        //
        if (RCGetCurrentReadMemory( ) == &ReadCallback)
            RCRemoveReadMemoryOverride( );

        if (RCGetCurrentWriteMemory( ) == &WriteCallback)
            RCRemoveWriteMemoryOverride( );
    }
}

INT_PTR 
PLUGIN_CC 
PluginSettingsDlg( 
    IN HWND hWnd, 
    IN UINT Msg, 
    IN WPARAM wParam, 
    IN LPARAM lParam 
)
{
    switch (Msg)
    {

    case WM_INITDIALOG:
    {
        if (gPluginState)
        {
            //
            // Apply checkboxes appropriately if we're in anm enabled state.
            //
            BOOL ReadChecked = (RCGetCurrentReadMemory( ) == &ReadCallback) ? BST_CHECKED : BST_UNCHECKED;
            BOOL WriteChecked = (RCGetCurrentWriteMemory( ) == &WriteCallback) ? BST_CHECKED : BST_UNCHECKED;

            SendMessage( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), BM_SETCHECK, MAKEWPARAM( ReadChecked, 0 ), 0 );
            EnableWindow( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), TRUE );

            SendMessage( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ), BM_SETCHECK, MAKEWPARAM( WriteChecked, 0 ), 0 );
            EnableWindow( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ), TRUE );
        }
        else
        {
            //
            // Make sure we can't touch the settings if we're in a disabled state.
            //
            SendMessage( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), BM_SETCHECK, MAKEWPARAM( BST_UNCHECKED, 0 ), 0 );
            EnableWindow( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), FALSE );

            SendMessage( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ), BM_SETCHECK, MAKEWPARAM( BST_UNCHECKED, 0 ), 0 );
            EnableWindow( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ), FALSE );
        }
    }
    return TRUE;

    case WM_COMMAND:
    {
        WORD NotificationCode = HIWORD( wParam );
        WORD ControlId = LOWORD( wParam );
        HWND hControlWnd = (HWND)lParam;
        
        if (NotificationCode == BN_CLICKED)
        {
            BOOLEAN bChecked = (SendMessage( hControlWnd, BM_GETCHECK, 0, 0 ) == BST_CHECKED);

            if (ControlId == IDC_CHECK_READ_MEMORY_OVERRIDE)
            {
                if (bChecked)
                {
                    //
                    // Make sure the read memory operation is not already overriden.
                    //
                    if (!RCIsReadMemoryOverriden( ))
                    {
                        RCOverrideReadMemoryOperation( &ReadCallback );
                    }
                    else
                    {
                        //
                        // Make sure it's not us!
                        //
                        if (RCGetCurrentReadMemory( ) != &ReadCallback)
                        {
                            //
                            // Ask the user whether or not they want to overwrite the other operation.
                            //
                            if (MessageBoxW( RCMainWindow( ),
                                L"Another plugin has already overriden the read operation.\n"
                                L"Would you like to overwrite their read override?",
                                L"Test Plugin", MB_YESNO ) == IDYES)
                            {
                                RCOverrideReadMemoryOperation( &ReadCallback );
                            }
                            else
                            {
                                //
                                // If the user chose no, then make sure our checkbox is unchecked.
                                //
                                SendMessage( GetDlgItem( hWnd, IDC_CHECK_READ_MEMORY_OVERRIDE ), 
                                    BM_SETCHECK, MAKEWPARAM( BST_UNCHECKED, 0 ), 0 );
                            }
                        }
                        else
                        {
                            //
                            // This shouldn't happen!
                            //
                            MessageBoxW( RCMainWindow( ), 
                                L"WTF! Plugin memory read operation is already set as the active override!", 
                                L"Test Plugin", MB_ICONERROR );
                        }
                    }
                }
                else
                {
                    //
                    // Only remove the read memory operation if it's ours!
                    //
                    if (RCGetCurrentReadMemory( ) == &ReadCallback)
                    {
                        RCRemoveReadMemoryOverride( );
                    }
                }			
            }
            else if (ControlId == IDC_CHECK_WRITE_MEMORY_OVERRIDE)
            {
                if (bChecked)
                {
                    //
                    // Make sure the write memory operation is not already overriden.
                    //
                    if (!RCIsWriteMemoryOverriden( ))
                    {
                        //
                        // We're all good to set our write memory operation!
                        //
                        RCOverrideWriteMemoryOperation( &WriteCallback );
                    }
                    else
                    {
                        //
                        // Make sure it's not us!
                        //
                        if (RCGetCurrentWriteMemory( ) != &WriteCallback)
                        {
                            //
                            // Ask the user whether or not they want to overwrite the other operation.
                            //
                            if (MessageBoxW( RCMainWindow( ),
                                L"Another plugin has already overriden the write operation.\n"
                                L"Would you like to overwrite their write override?",
                                L"Test Plugin", MB_YESNO ) == IDYES)
                            {
                                RCOverrideWriteMemoryOperation( &WriteCallback );
                            }
                            else
                            {
                                //
                                // If the user chose no, then make sure our checkbox is unchecked.
                                //
                                SendMessage( GetDlgItem( hWnd, IDC_CHECK_WRITE_MEMORY_OVERRIDE ),
                                    BM_SETCHECK, MAKEWPARAM( BST_UNCHECKED, 0 ), 0 );
                            }
                        }
                        else
                        {
                            //
                            // This shouldn't happen!
                            //
                            MessageBoxW( RCMainWindow( ),
                                L"WTF! Plugin memory write operation is already set as the active override!",
                                L"Test Plugin", MB_ICONERROR );
                        }
                    }
                }
                else
                {
                    //
                    // Only remove the read memory operation if it's ours!
                    //
                    if (RCGetCurrentWriteMemory( ) == &WriteCallback)
                    {
                        RCRemoveWriteMemoryOverride( );
                    }
                }
            }
        }	
    }
    break;

    case WM_CLOSE:
    {
        EndDialog( hWnd, 0 );
    }
    break;

    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
WriteCallback( 
    IN LPVOID Address, 
    IN LPVOID Buffer, 
    IN SIZE_T Size, 
    OUT PSIZE_T BytesWritten 
)
{
    DWORD PID = RCGetProcessId();
    //if (VMMDLL_MemWrite((DWORD)PID, (ULONG64)Address, (PBYTE)Buffer, Size)) {
    //    
    //    return true;
    //}

    //REQUEST_WRITE req{};
    //req.ProcessId = PID;
    //req.Src = (PVOID)Address;
    //req.Dest = (PVOID)&Buffer;
    //req.Size = Size;
    //
    //return SendRequest(REQUEST_TYPE::WRITE, &req);

    return false;

    /*DWORD OldProtect;
    VirtualProtectEx(ProcessHandle, (PVOID)Address, Size, PAGE_EXECUTE_READWRITE, &OldProtect);
    BOOL Retval = WriteProcessMemory( ProcessHandle, (PVOID)Address, Buffer, Size, BytesWritten );
    VirtualProtectEx( ProcessHandle, (PVOID)Address, Size, OldProtect, NULL );
    return Retval;*/
}

BOOL 
PLUGIN_CC 
ReadCallback( 
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesRead 
)
{
    DWORD PID = RCGetProcessId( );
    //RCPrintConsole(L"[RCEPlugin] Read:");
    //RCPrintConsole(L"[RCEPlugin] Address: %x %p", Address, Address);
    //RCPrintConsole(L"[RCEPlugin] Buffer: %x %p", Buffer, Buffer);
    //RCPrintConsole(L"[RCEPlugin] Size: %x", Size);
    //RCPrintConsole(L"[RCEPlugin] PID: %d", PID);

    //if (VMMDLL_MemRead((DWORD)PID, (ULONG64)Address, (PBYTE)Buffer, Size)) {
    //    return true;
    //}
    //else {
	//	SecureZeroMemory(Buffer, Size);
	//	return false;
    //}

    REQUEST_READ req;
    req.ProcessId = PID;
    req.Src = (PVOID)Address;
    req.Dest = Buffer;
    req.Size = Size;
    
    bool result = SendRequest(REQUEST_TYPE::READ, &req);
    
    if (!result) {
        SecureZeroMemory(Buffer, Size);
    }
    
    return result;

    /*BOOL Retval = ReadProcessMemory(ProcessHandle, (LPVOID)Address, Buffer, Size, BytesRead);
    if (!Retval)
        ZeroMemory( Buffer, Size );
    return Retval;*/
}
