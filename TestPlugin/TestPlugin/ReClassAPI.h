#ifndef _RECLASS_API_H_
#define _RECLASS_API_H_

//#include <afxribbonbar.h> // Used for ribbon bar. comment this out if not used

#ifdef _WIN64
#ifdef _DEBUG
#pragma comment(lib, "RCE64_dbg.lib")
#else
#pragma comment(lib, "RCE64.lib")
#endif
#else
#ifdef _DEBUG
#pragma comment(lib, "ReClassEx_dbg.lib")
#else
#pragma comment(lib, "ReClassEx.lib")
#endif
#endif

#define PLUGIN_CC __stdcall

//
// Plugin Operation API Prototypes
//
typedef BOOL( PLUGIN_CC *PPLUGIN_READ_MEMORY_OPERATION )(
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesRead
    );
typedef BOOL( PLUGIN_CC *PPLUGIN_WRITE_MEMORY_OPERATION )(
    IN LPVOID Address,
    IN LPVOID Buffer,
    IN SIZE_T Size,
    OUT PSIZE_T BytesWritten
    );
typedef HANDLE( PLUGIN_CC *PPLUGIN_OPEN_PROCESS_OPERATION )(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN DWORD dwProcessId
    );
typedef HANDLE( PLUGIN_CC *PPLUGIN_OPEN_THREAD_OPERATION )(
    IN DWORD dwDesiredAccess,
    IN BOOL bInheritHandle,
    IN DWORD dwThreadId
    );

//
// Plugin info structure to be filled in during initialization
// which is passed back to RC to display in the plugins dialog
//
typedef DECLSPEC_ALIGN(16) struct _RECLASS_PLUGIN_INFO {
    wchar_t Name[256];      //< Name of the plugin
    wchar_t Version[256];   //< Plugin version
    wchar_t About[2048];    //< Small snippet about the plugin 
    int DialogId;           //< Identifier for the settings dialog
} RECLASS_PLUGIN_INFO, *PRECLASS_PLUGIN_INFO, *LPRECLASS_PLUGIN_INFO;

//
// Plugin initialization callback to fill in the RECLASS_PLUGIN_INFO struct,
// and initialize any other plugin resources
//
BOOL 
PLUGIN_CC 
PluginInit( 
    OUT LPRECLASS_PLUGIN_INFO lpRCInfo
    );

//
// Callback for when the plugin state is changed (enabled or disabled). 
// Plugins disabled and enabled state are dependent on the implementation inside the plugin. 
// All we do is send a state change to plugins for them to disable or enable their functionality.
//
VOID 
PLUGIN_CC 
PluginStateChange( 
    IN BOOL state 
    );

//
// Window Proc for the settings dialog
//
INT_PTR 
PLUGIN_CC 
PluginSettingsDlg( 
    IN HWND hWnd, 
    IN UINT Msg, 
    IN WPARAM wParam, 
    IN LPARAM lParam 
    );

// 
// Register, remove, get or check overrides for the read/write memory operations.
// 
BOOL 
PLUGIN_CC 
RCOverrideReadMemoryOperation( 
    IN PPLUGIN_READ_MEMORY_OPERATION ReadMemoryOperation 
    );

BOOL 
PLUGIN_CC 
RCOverrideWriteMemoryOperation( 
    IN PPLUGIN_WRITE_MEMORY_OPERATION WriteMemoryOperation 
    );

BOOL 
PLUGIN_CC 
RCOverrideMemoryOperations( 
    IN PPLUGIN_READ_MEMORY_OPERATION ReadMemoryOperation, 
    IN PPLUGIN_WRITE_MEMORY_OPERATION WriteMemoryOperation 
    );


BOOL 
PLUGIN_CC 
RCRemoveReadMemoryOverride( 
    VOID 
    );

BOOL 
PLUGIN_CC 
RCRemoveWriteMemoryOverride( 
    VOID 
    );


BOOL 
PLUGIN_CC 
RCIsReadMemoryOverriden( 
    VOID 
    );

BOOL 
PLUGIN_CC 
RCIsWriteMemoryOverriden( 
    VOID 
    );


PPLUGIN_READ_MEMORY_OPERATION 
PLUGIN_CC 
RCGetCurrentReadMemory( 
    VOID 
    );

PPLUGIN_WRITE_MEMORY_OPERATION 
PLUGIN_CC 
RCGetCurrentWriteMemory( 
    VOID 
    );

// 
// Register, remove, get or check overrides for the opening of handles 
// for various process/thread operations.
// 
BOOL
PLUGIN_CC
RCOverrideOpenProcessOperation( 
    IN PPLUGIN_OPEN_PROCESS_OPERATION OpenProcessOperation 
    );

BOOL
PLUGIN_CC
RCOverrideOpenThreadOperation( 
    IN PPLUGIN_OPEN_THREAD_OPERATION OpenThreadOperation 
    );

BOOL
PLUGIN_CC
RCOverrideHandleOperations( 
    IN PPLUGIN_OPEN_PROCESS_OPERATION OpenProcessOperation, 
    IN PPLUGIN_OPEN_THREAD_OPERATION OpenThreadOperation 
    );


BOOL
PLUGIN_CC
RCRemoveOpenProcessOverride( 
    VOID 
    );

BOOL
PLUGIN_CC
RCRemoveOpenThreadOverride( 
    VOID 
    );


BOOL 
PLUGIN_CC 
RCIsOpenProcessOverriden( 
    VOID 
    );

BOOL 
PLUGIN_CC 
RCIsOpenThreadOverriden( 
    VOID 
    );


PPLUGIN_OPEN_PROCESS_OPERATION 
PLUGIN_CC 
RCGetCurrentOpenProcess( 
    VOID 
    );

PPLUGIN_OPEN_THREAD_OPERATION 
PLUGIN_CC 
RCGetCurrentOpenThread( 
    VOID 
    );

// 
// Print text to the RC console window
// 
VOID 
PLUGIN_CC 
RCPrintConsole( 
    IN const wchar_t *Format, 
    ... 
    );

// 
// Gets the current attached process handle, null if not attached
// 
HANDLE 
PLUGIN_CC 
RCGetProcessHandle( 
    VOID 
    );

// 
// Gets the current attached process ID, 0 if not attached
// 
DWORD 
PLUGIN_CC 
RCGetProcessId( 
    VOID 
    );

// 
// Return the main window handle for RC
// 
HWND 
PLUGIN_CC 
RCMainWindow( 
    VOID 
    );

#ifdef _MFC_VER 
// 
// Get the ribbon interface for MFC (useful for adding custom buttons and such)
// 
CMFCRibbonBar* 
PLUGIN_CC 
RCRibbonInterface( 
    VOID 
    );
#endif


//
// Class Manipulation
//
// TODO: Implement this.
// 



#endif // _RECLASS_API_H_