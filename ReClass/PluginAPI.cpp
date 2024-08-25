#include "stdafx.h"

#include "PluginAPI.h"


PPLUGIN_READ_MEMORY_OPERATION g_PluginOverrideReadMemoryOperation = nullptr;
PPLUGIN_WRITE_MEMORY_OPERATION g_PluginOverrideWriteMemoryOperation = nullptr;
PPLUGIN_OPEN_PROCESS_OPERATION g_PluginOverrideOpenProcessOperation = nullptr;
PPLUGIN_OPEN_THREAD_OPERATION g_PluginOverrideOpenThreadOperation = nullptr;

std::vector<PRECLASS_PLUGIN> g_LoadedPlugins;

VOID 
LoadPlugins( 
    VOID 
)
{
    PRECLASS_PLUGIN Plugin;
    HMODULE PluginBase;

    HANDLE FileTreeHandle;
    WIN32_FIND_DATA FileData;

    PPLUGIN_INIT PluginInitFunction;
    PPLUGIN_STATE_CHANGE PluginStateChangeFunction;
    DLGPROC PluginSettingDlgFunction;

    TCHAR ModulePath[MAX_PATH];
    GetModuleFileName(GetModuleHandle(NULL), ModulePath, MAX_PATH);
    PathRemoveFileSpec(ModulePath);

    TCHAR PluginPath[MAX_PATH];
    _stprintf_s(PluginPath, _T("%s\\plugins\\*.rc-plugin64"), ModulePath);

    FileTreeHandle = FindFirstFile(PluginPath, &FileData);

    if (FileTreeHandle != INVALID_HANDLE_VALUE)
    {
        do
        {
            CString strPluginFileName( FileData.cFileName );
            CString strPluginFilePath( _T( "plugins\\" ) );
            strPluginFilePath.Append( strPluginFileName );

            PluginBase = ::LoadLibrary( strPluginFilePath.GetString( ) );
            if (PluginBase == NULL)
            {
                PrintOut( _T( "plugin %s was not able to be loaded!" ), strPluginFileName.GetString( ) );
                continue;
            }

            PluginInitFunction = (PPLUGIN_INIT)Utils::GetLocalProcAddress( PluginBase, _T( "PluginInit" ) );
            if (!PluginInitFunction)
            {
                PrintOut( _T( "%s is not a RC plugin!" ), strPluginFileName.GetString( ) );
                FreeLibrary( PluginBase );
                continue;
            }

            PluginStateChangeFunction = (PPLUGIN_STATE_CHANGE)Utils::GetLocalProcAddress( PluginBase, _T( "PluginStateChange" ) );
            if (!PluginStateChangeFunction)
            {
                PrintOut( _T( "%s doesnt have exported state change function! Unable to disable plugin on request, "
                    "so you must stop RC and delete the plugin to disable it!" ), strPluginFileName.GetString( ) );
            }

            PluginSettingDlgFunction = (DLGPROC)Utils::GetLocalProcAddress( PluginBase, _T( "PluginSettingsDlg" ) );

            Plugin = (PRECLASS_PLUGIN)_aligned_malloc( sizeof( RECLASS_PLUGIN ), 16 );
#ifdef _UNICODE
            wcscpy_s( Plugin->FileName, strPluginFileName.GetString( ) );
#else
            wcscpy_s( Plugin->FileName, CA2W( strPluginFileName ).m_psz );
#endif
            Plugin->LoadedBase = PluginBase;
            Plugin->InitFunction = PluginInitFunction;
            Plugin->SettingDlgFunction = PluginSettingDlgFunction;
            Plugin->StateChangeFunction = PluginStateChangeFunction;

            if (PluginInitFunction( &Plugin->Info ))
            {
#ifdef _UNICODE
                Plugin->State = g_ReClassApp.GetProfileInt( _T( "PluginState" ), Plugin->Info.Name, 1 ) == 1;
#else
                Plugin->State = g_ReClassApp.GetProfileInt( _T( "PluginState" ), CW2A( Plugin->Info.Name ), 1 ) == 1;
#endif

                if (Plugin->Info.DialogId == -1)
                    Plugin->SettingDlgFunction = NULL;

                PrintOut( _T( "Loaded plugin %s (%ls version %ls) - %ls" ), strPluginFileName.GetString( ), Plugin->Info.Name, Plugin->Info.Version, Plugin->Info.About );
                if (Plugin->StateChangeFunction)
                    Plugin->StateChangeFunction( Plugin->State );

                g_LoadedPlugins.push_back( Plugin );
            }
            else
            {
                PrintOut( _T( "Failed to load plugin %s" ), FileData.cFileName );
                FreeLibrary( PluginBase );
            }
        } while (FindNextFile( FileTreeHandle, &FileData ));
    }
}

VOID 
UnloadPlugins( 
    VOID 
)
{
    for (PRECLASS_PLUGIN Plugin : g_LoadedPlugins)
    {
        FreeLibrary( Plugin->LoadedBase );
        _aligned_free( Plugin );
    }
    g_LoadedPlugins.clear( );
}


//
// Plugin API Routines Implementation
//

BOOL 
PLUGIN_CC 
RCOverrideReadMemoryOperation( 
    IN PPLUGIN_READ_MEMORY_OPERATION ReadMemoryOperation 
)
{
    if (ReadMemoryOperation != nullptr)
    {
        g_PluginOverrideReadMemoryOperation = ReadMemoryOperation;
        return TRUE;
    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
RCOverrideWriteMemoryOperation( 
    IN PPLUGIN_WRITE_MEMORY_OPERATION WriteMemoryOperation
)
{
    if (WriteMemoryOperation != nullptr)
    {
        g_PluginOverrideWriteMemoryOperation = WriteMemoryOperation;
        return TRUE;
    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
RCOverrideMemoryOperations( 
    IN PPLUGIN_READ_MEMORY_OPERATION ReadMemoryOperation,
    IN PPLUGIN_WRITE_MEMORY_OPERATION WriteMemoryOperation
)
{
    if (ReadMemoryOperation == nullptr && WriteMemoryOperation == nullptr)
        return FALSE;
    if (ReadMemoryOperation != nullptr)
        g_PluginOverrideReadMemoryOperation = ReadMemoryOperation;
    if (WriteMemoryOperation != nullptr)
        g_PluginOverrideWriteMemoryOperation = WriteMemoryOperation;
    return TRUE;
}

BOOL 
PLUGIN_CC 
RCRemoveReadMemoryOverride( 
    VOID 
)
{
    if (g_PluginOverrideReadMemoryOperation != nullptr)
    {
        g_PluginOverrideReadMemoryOperation = nullptr;
        return TRUE;
    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
RCRemoveWriteMemoryOverride( 
    VOID 
)
{
    if (g_PluginOverrideWriteMemoryOperation != nullptr)
    {
        g_PluginOverrideWriteMemoryOperation = nullptr;
        return TRUE;
    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
RCIsReadMemoryOverriden( 
    VOID 
)
{
    return (g_PluginOverrideReadMemoryOperation != nullptr) ? TRUE : FALSE;
}

BOOL 
PLUGIN_CC 
RCIsWriteMemoryOverriden( 
    VOID 
)
{
    return (g_PluginOverrideWriteMemoryOperation != nullptr) ? TRUE : FALSE;
}

PPLUGIN_READ_MEMORY_OPERATION 
PLUGIN_CC 
RCGetCurrentReadMemory( 
    VOID 
)
{
    return g_PluginOverrideReadMemoryOperation;
}

PPLUGIN_WRITE_MEMORY_OPERATION 
PLUGIN_CC 
RCGetCurrentWriteMemory( 
    VOID 
)
{
    return g_PluginOverrideWriteMemoryOperation;
}

BOOL 
PLUGIN_CC 
RCOverrideOpenProcessOperation( 
    IN PPLUGIN_OPEN_PROCESS_OPERATION OpenProcessOperation 
)
{
    if (OpenProcessOperation != nullptr)
    {
        g_PluginOverrideOpenProcessOperation = OpenProcessOperation;
        return TRUE;
    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
RCOverrideOpenThreadOperation( 
    IN PPLUGIN_OPEN_THREAD_OPERATION OpenThreadOperation 
)
{
    if (OpenThreadOperation != nullptr)
    {
        g_PluginOverrideOpenThreadOperation = OpenThreadOperation;
        return TRUE;
    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
RCOverrideHandleOperations( 
    IN PPLUGIN_OPEN_PROCESS_OPERATION OpenProcessOperation, 
    IN PPLUGIN_OPEN_THREAD_OPERATION OpenThreadOperation )
{
    if (OpenProcessOperation == nullptr && OpenThreadOperation == nullptr)
        return FALSE;
    if (OpenProcessOperation != nullptr)
        g_PluginOverrideOpenProcessOperation = OpenProcessOperation;
    if (OpenThreadOperation != nullptr)
        g_PluginOverrideOpenThreadOperation = OpenThreadOperation;
    return TRUE;
}

BOOL 
PLUGIN_CC 
RCRemoveOpenProcessOverride( 
    VOID 
)
{
    if (g_PluginOverrideOpenProcessOperation != nullptr)
    {
        g_PluginOverrideOpenProcessOperation = nullptr;
        return TRUE;
    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
RCRemoveOpenThreadOverride( 
    VOID 
)
{
    if (g_PluginOverrideOpenThreadOperation != nullptr)
    {
        g_PluginOverrideOpenThreadOperation = nullptr;
        return TRUE;
    }
    return FALSE;
}

BOOL 
PLUGIN_CC 
RCIsOpenProcessOverriden( 
    VOID 
)
{
    return (g_PluginOverrideOpenProcessOperation != nullptr) ? TRUE : FALSE;
}

BOOL 
PLUGIN_CC 
RCIsOpenThreadOverriden( 
    VOID 
)
{
    return (g_PluginOverrideOpenThreadOperation != nullptr) ? TRUE : FALSE;
}

PPLUGIN_OPEN_PROCESS_OPERATION 
PLUGIN_CC 
RCGetCurrentOpenProcess( 
    VOID 
)
{
    return g_PluginOverrideOpenProcessOperation;
}

PPLUGIN_OPEN_THREAD_OPERATION 
PLUGIN_CC 
RCGetCurrentOpenThread( 
    VOID 
)
{
    return g_PluginOverrideOpenThreadOperation;
}

VOID
CDECL 
RCPrintConsole( 
    IN const wchar_t *Format,
    ...
)
{
    wchar_t Buffer[2048];
    //ZeroMemory( Buffer, 2048 );
    
    va_list Args;
    va_start( Args, Format );
    _vsnwprintf_s( Buffer, 2048, Format, Args );
    va_end( Args );

    #if defined(_UNICODE)
    g_ReClassApp.m_pConsole->PrintText( Buffer );
    #else
    g_RCApp.m_pConsole->PrintText( CW2A( Buffer ) );
    #endif
}

HANDLE 
PLUGIN_CC 
RCGetProcessHandle( 
    VOID 
)
{
    return g_hProcess;
}

DWORD 
PLUGIN_CC 
RCGetProcessId( 
    VOID 
)
{
    return g_ProcessID;
}

HWND 
PLUGIN_CC 
RCMainWindow( 
    VOID 
)
{
    return *g_ReClassApp.GetMainWnd( );
}

CMFCRibbonBar* 
PLUGIN_CC 
RCRibbonInterface( 
    VOID 
)
{
    return g_ReClassApp.GetRibbonBar( );
}