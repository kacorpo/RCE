#include "stdafx.h"

#include "ReClassEx.h"

#include "afxwinappex.h"
#include "afxdialogex.h"

#include "CMainFrame.h"
#include "CClassFrame.h"

#include "DialogEdit.h"
#include "DialogClasses.h"
#include "DialogModules.h"
#include "DialogPlugins.h"


// The one and only CReClassExApp object
CReClassExApp g_ReClassApp;

// CReClassExApp
BEGIN_MESSAGE_MAP( CReClassExApp, CWinAppEx )
    ON_COMMAND( ID_FILE_NEW, &CReClassExApp::OnFileNew )
    ON_COMMAND( ID_FILE_SAVE, &CReClassExApp::OnFileSave )
    ON_COMMAND( ID_FILE_SAVE_AS, &CReClassExApp::OnFileSaveAs )
    ON_COMMAND( ID_FILE_OPEN, &CReClassExApp::OnFileOpen )
    ON_COMMAND( ID_FILE_OPEN_PDB, &CReClassExApp::OnOpenPdb )
    ON_COMMAND( ID_RC_PLUGINS, &CReClassExApp::OnButtonPlugins )
    ON_COMMAND( ID_BUTTON_NEWCLASS, &CReClassExApp::OnButtonNewClass )
    ON_COMMAND( ID_BUTTON_NOTES, &CReClassExApp::OnButtonNotes )
    ON_COMMAND( ID_BUTTON_SEARCH, &CReClassExApp::OnButtonSearch )
    ON_COMMAND( ID_BUTTON_CONSOLE, &CReClassExApp::OnButtonConsole )
    ON_COMMAND( ID_BUTTON_MODULES, &CReClassExApp::OnButtonModules )
    ON_COMMAND( ID_BUTTON_PARSER, &CReClassExApp::OnButtonParser )
    ON_COMMAND( ID_BUTTON_HEADER, &CReClassExApp::OnButtonHeader )
    ON_COMMAND( ID_BUTTON_FOOTER, &CReClassExApp::OnButtonFooter )
    ON_COMMAND( ID_BUTTON_RESET, &CReClassExApp::OnButtonReset )
    ON_COMMAND( ID_BUTTON_PAUSE, &CReClassExApp::OnButtonPause )
    ON_COMMAND( ID_BUTTON_RESUME, &CReClassExApp::OnButtonResume )
    ON_COMMAND( ID_BUTTON_KILL, &CReClassExApp::OnButtonKill )
    ON_UPDATE_COMMAND_UI( ID_BUTTON_PAUSE, &CReClassExApp::OnUpdateButtonPause )
    ON_UPDATE_COMMAND_UI( ID_BUTTON_RESUME, &CReClassExApp::OnUpdateButtonResume )
    ON_UPDATE_COMMAND_UI( ID_BUTTON_KILL, &CReClassExApp::OnUpdateButtonKill )
    ON_UPDATE_COMMAND_UI( ID_BUTTON_SEARCH, &CReClassExApp::OnUpdateButtonSearch )
    ON_UPDATE_COMMAND_UI( ID_BUTTON_MODULES, &CReClassExApp::OnUpdateButtonModules )
    ON_UPDATE_COMMAND_UI( ID_RC_PLUGINS, &CReClassExApp::OnUpdateButtonPlugins )
    ON_COMMAND( ID_BUTTON_GENERATE, &CReClassExApp::OnButtonGenerate )
    ON_COMMAND( ID_BUTTON_CLEAN, &CReClassExApp::OnButtonClean )
    ON_UPDATE_COMMAND_UI( ID_BUTTON_CLEAN, &CReClassExApp::OnUpdateButtonClean )
    ON_UPDATE_COMMAND_UI( ID_FILE_OPEN_PDB, &CReClassExApp::OnUpdateOpenPdb )
END_MESSAGE_MAP( )

CReClassExApp::CReClassExApp( )
{
    TCHAR AppId[256] = { 0 };

    LoadString( NULL, AFX_IDS_APP_ID, AppId, 256 );
    SetAppID( AppId );

    m_bHiColorIcons = TRUE;
    m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

    g_FontWidth = FONT_DEFAULT_WIDTH;
    g_FontHeight = FONT_DEFAULT_HEIGHT;

}

void CReClassExApp::ResizeMemoryFont( int fontWidth, int fontHeight )
{
    g_ViewFont.DeleteObject( );

    HMODULE hShcoreBase = (HMODULE)Utils::GetLocalModuleBase( _T( "shcore.dll" ) );
    if (hShcoreBase)
    {
        auto pfnGetProcessDpiAwareness = reinterpret_cast<decltype(&GetProcessDpiAwareness)>(Utils::GetLocalProcAddress( hShcoreBase, _T( "GetProcessDpiAwareness" ) ));
        auto pfnGetDpiForMonitor = reinterpret_cast<decltype(&GetDpiForMonitor)>(Utils::GetLocalProcAddress( hShcoreBase, _T( "GetDpiForMonitor" ) ));

        if (pfnGetProcessDpiAwareness && pfnGetDpiForMonitor)
        {
            PROCESS_DPI_AWARENESS dpiValue;
            UINT dpiX, dpiY;
            HMONITOR hMonitor;

            pfnGetProcessDpiAwareness( NULL, &dpiValue );
            if (dpiValue == PROCESS_DPI_AWARENESS::PROCESS_PER_MONITOR_DPI_AWARE || 
                dpiValue == PROCESS_DPI_AWARENESS::PROCESS_SYSTEM_DPI_AWARE)
            {           
                hMonitor = ::MonitorFromWindow( m_pMainWnd->GetSafeHwnd( ), MONITOR_DEFAULTTONEAREST );
                pfnGetDpiForMonitor( hMonitor, MONITOR_DPI_TYPE::MDT_EFFECTIVE_DPI, &dpiX, &dpiY );
                g_FontWidth = MulDiv( fontWidth, MulDiv( dpiX, 100, 96 ), 100 );
                g_FontHeight = MulDiv( fontHeight, MulDiv( dpiY, 100, 96 ), 100 );
            }
        }

        FreeLibrary( hShcoreBase );
    }

    g_ViewFont.CreateFont( g_FontHeight, g_FontWidth, 0, 0, 
        FW_NORMAL, FALSE, FALSE, FALSE, 0, 
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
        CLEARTYPE_QUALITY, FIXED_PITCH, g_ViewFontName.GetBuffer( ) );
}

BOOL CReClassExApp::InitInstance( )
{
    #ifdef _DEBUG
    Utils::CreateDbgConsole( _T( "dbg" ) );
    #endif

	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	freopen("CONIN$", "r", stdin);

    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof( InitCtrls );
    InitCtrls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx( &InitCtrls );

    CWinAppEx::InitInstance( );

    if (!ntdll::Init( ))
    {
        AfxMessageBox( IDP_NTDLL_INIT_FAILED );
        return FALSE;
    }

    if (!AfxOleInit( ))
    {
        AfxMessageBox( IDP_OLE_INIT_FAILED );
        return FALSE;
    }

    AfxEnableControlContainer( );
    EnableTaskbarInteraction( FALSE );
    InitContextMenuManager( );
    InitKeyboardManager( );
    InitTooltipManager( );

    CMFCToolTipInfo toolTipInfo;
    toolTipInfo.m_bVislManagerTheme = TRUE;
    GetTooltipManager( )->SetTooltipParams( AFX_TOOLTIP_TYPE_ALL, RUNTIME_CLASS( CMFCToolTipCtrl ), &toolTipInfo );

    // Get registry entries
    SetRegistryKey( _T( "RCEX" ) );

    //Typedefs
    g_Typedefs.Hex      = GetProfileString( _T( "Typedefs" ), _T( "Hex" ), _T( "char" ) );
    g_Typedefs.Int64    = GetProfileString( _T( "Typedefs" ), _T( "Int64" ), _T( "__int64" ) );
    g_Typedefs.Int32    = GetProfileString( _T( "Typedefs" ), _T( "Int32" ), _T( "__int32" ) );
    g_Typedefs.Int16    = GetProfileString( _T( "Typedefs" ), _T( "Int16" ), _T( "__int16" ) );
    g_Typedefs.Int8     = GetProfileString( _T( "Typedefs" ), _T( "Int8" ), _T( "__int8" ) );
    g_Typedefs.Qword    = GetProfileString( _T( "Typedefs" ), _T( "Qword" ), _T( "DWORD64" ) );
    g_Typedefs.Dword    = GetProfileString( _T( "Typedefs" ), _T( "Dword" ), _T( "DWORD" ) );
    g_Typedefs.Word     = GetProfileString( _T( "Typedefs" ), _T( "Word" ), _T( "WORD" ) );
    g_Typedefs.Byte     = GetProfileString( _T( "Typedefs" ), _T( "Byte" ), _T( "unsigned char" ) );
    g_Typedefs.Float    = GetProfileString( _T( "Typedefs" ), _T( "Float" ), _T( "float" ) );
    g_Typedefs.Double   = GetProfileString( _T( "Typedefs" ), _T( "Double" ), _T( "double" ) );
    g_Typedefs.Vec2     = GetProfileString( _T( "Typedefs" ), _T( "Vec2" ), _T( "Vector2" ) );
    g_Typedefs.Vec3     = GetProfileString( _T( "Typedefs" ), _T( "Vec3" ), _T( "Vector3" ) );
    g_Typedefs.Quat     = GetProfileString( _T( "Typedefs" ), _T( "Quat" ), _T( "Vector4" ) );
    g_Typedefs.Matrix   = GetProfileString( _T( "Typedefs" ), _T( "Matrix" ), _T( "matrix3x4_t" ) );
    g_Typedefs.PChar    = GetProfileString( _T( "Typedefs" ), _T( "PChar" ), _T( "char*" ) );
    g_Typedefs.PWChar   = GetProfileString( _T( "Typedefs" ), _T( "PWChar" ), _T( "wchar_t*" ) );

    g_clrBackground     = GetProfileInt( _T( "Colors" ), _T( "Background" ), g_clrBackground );
    g_clrSelect         = GetProfileInt( _T( "Colors" ), _T( "Select" ), g_clrSelect );
    g_clrHidden         = GetProfileInt( _T( "Colors" ), _T( "Hidden" ), g_clrHidden );
    g_clrOffset         = GetProfileInt( _T( "Colors" ), _T( "Offset" ), g_clrOffset );
    g_clrAddress        = GetProfileInt( _T( "Colors" ), _T( "Address" ), g_clrAddress);
    g_clrType           = GetProfileInt( _T( "Colors" ), _T( "Type" ), g_clrType );
    g_clrName           = GetProfileInt( _T( "Colors" ), _T( "Name" ), g_clrName );
    g_clrIndex          = GetProfileInt( _T( "Colors" ), _T( "Index" ), g_clrIndex );
    g_clrValue          = GetProfileInt( _T( "Colors" ), _T( "Value" ), g_clrValue );
    g_clrComment        = GetProfileInt( _T( "Colors" ), _T( "Comment" ), g_clrComment );
    g_clrVTable			= GetProfileInt( _T( "Colors" ), _T( "VTable" ), g_clrVTable );
    g_clrFunction		= GetProfileInt( _T( "Colors" ), _T( "Function" ), g_clrFunction );
    g_clrChar           = GetProfileInt( _T( "Colors" ), _T( "Char" ), g_clrChar );
    g_clrCustom			= GetProfileInt( _T( "Colors" ), _T( "Custom" ), g_clrCustom );
    g_clrHex            = GetProfileInt( _T( "Colors" ), _T( "Hex" ), g_clrHex );

    g_bOffset           = GetProfileInt( _T( "Display" ), _T( "Offset" ), g_bOffset ) > 0 ? true : false;
    g_bAddress          = GetProfileInt( _T( "Display" ), _T( "Address" ), g_bAddress) > 0 ? true : false;
    g_bText             = GetProfileInt( _T( "Display" ), _T( "Text" ), g_bText ) > 0 ? true : false;
    g_bFloat            = GetProfileInt( _T( "Display" ), _T( "Float" ), g_bFloat ) > 0 ? true : false;
    g_bInt              = GetProfileInt( _T( "Display" ), _T( "Int" ), g_bInt ) > 0 ? true : false;
    g_bString           = GetProfileInt( _T( "Display" ), _T( "String" ), g_bString ) > 0 ? true : false;
    g_bPointers         = GetProfileInt( _T( "Display" ), _T( "Pointers" ), g_bPointers ) > 0 ? true : false;
    g_bClassBrowser     = GetProfileInt( _T( "Display" ), _T( "ClassBrowser" ), g_bClassBrowser ) > 0 ? true : false;
    g_bFilterProcesses  = GetProfileInt( _T( "Display" ), _T( "FilterProcesses" ), g_bFilterProcesses ) > 0 ? true : false;

    g_bRTTI             = GetProfileInt( _T( "Misc" ), _T( "RTTI" ), g_bRTTI ) > 0 ? true : false;
    g_bRandomName       = GetProfileInt( _T( "Misc" ), _T( "RandomName" ), g_bRandomName ) > 0 ? true : false;
    g_bLoadModuleSymbol = GetProfileInt( _T( "Misc" ), _T( "LoadModuleSymbols" ), g_bLoadModuleSymbol ) > 0 ? true : false;

    g_bPrivatePadding   = GetProfileInt( _T( "Class Generation" ), _T( "PrivatePadding" ), g_bPrivatePadding ) > 0 ? true : false;
    g_bClipboardCopy    = GetProfileInt( _T( "Class Generation" ), _T( "ClipboardCopy" ), g_bClipboardCopy ) > 0 ? true : false;

    g_bTop = false; //GetProfileInt("Display", "g_bTop", g_bTop) > 0 ? true : false;

    g_ViewFontName = _T( "Terminal" );

    HINSTANCE hInst = AfxGetInstanceHandle( );

    m_hMdiMenu = ::LoadMenu( hInst, MAKEINTRESOURCE( IDR_ReClassExTYPE ) );
    m_hMdiAccel = ::LoadAccelerators( hInst, MAKEINTRESOURCE( IDR_ReClassExTYPE ) );

    #define PushIcon(id) g_Icons.emplace_back(::LoadIcon(hInst, MAKEINTRESOURCE(id)));
    PushIcon( IDI_ICON_OPEN );
    PushIcon( IDI_ICON_CLOSED );
    PushIcon( IDI_ICON_CLASS );
    PushIcon( IDI_ICON_METHOD );
    PushIcon( IDI_ICON_VTABLE );
    PushIcon( IDI_ICON_DELETE );
    PushIcon( IDI_ICON_ADD );
    PushIcon( IDI_ICON_RANDOM );
    PushIcon( IDI_ICON_DROPARROW );
    PushIcon( IDI_ICON_POINTER );
    PushIcon( IDI_ICON_ARRAY );
    PushIcon( IDI_ICON_CUSTOM );
    PushIcon( IDI_ICON_ENUM );
    PushIcon( IDI_ICON_FLOAT );
    PushIcon( IDI_ICON_LEFT );
    PushIcon( IDI_ICON_RIGHT );
    PushIcon( IDI_ICON_MATRIX );
    PushIcon( IDI_ICON_INTEGER );
    PushIcon( IDI_ICON_TEXT );
    PushIcon( IDI_ICON_UNSIGNED );
    PushIcon( IDI_ICON_VECTOR );
    PushIcon( IDI_ICON_CHANGE );
    PushIcon( IDI_ICON_CAMERA );
    #undef PushIcon

    CMainFrame* pMainFrame = new CMainFrame( );
    if (!pMainFrame || !pMainFrame->LoadFrame( IDR_MAINFRAME ))
        return FALSE;

    m_pMainWnd = pMainFrame;

    pMainFrame->m_hMenuDefault = m_hMdiMenu;
    pMainFrame->m_hAccelTable = m_hMdiAccel;

    pMainFrame->ShowWindow( m_nCmdShow );
    pMainFrame->UpdateWindow( );

    //
    // Fix for 4k monitors
    //
    ResizeMemoryFont( g_FontWidth, g_FontHeight );

    g_hProcess = NULL;
    g_ProcessID = NULL;
    g_AttachedProcessAddress = NULL;

    //
    // Initialize the Scintilla editor
    //
    if (!Scintilla_RegisterClasses( m_hInstance ))
    {
        AfxMessageBox( _T( "Scintilla failed to initiailze" ) );
        return FALSE;
    }

    m_pConsole = new CDialogConsole( _T( "Console" ) );
    if (m_pConsole->Create( CDialogConsole::IDD, CWnd::GetDesktopWindow( ) ))
        m_pConsole->ShowWindow( SW_HIDE );

    m_pSymbolLoader = new (std::nothrow) Symbols;
    if (m_pSymbolLoader)
    {
        PrintOut( _T( "Symbol resolution enabled" ) );
        g_bSymbolResolution = true;
    }
    else
    {
        PrintOut( _T( "Failed to init symbol loader, disabling globally" ) );
        g_bSymbolResolution = false;
    }

    LoadPlugins( );

    return TRUE;
}

int CReClassExApp::ExitInstance( )
{
    //
    // Unload any loaded plugins
    //
    UnloadPlugins( );

    //
    // Free resources
    //
    if (m_hMdiMenu != NULL)
    {
        FreeResource( m_hMdiMenu );
        m_hMdiMenu = NULL;
    }	

    if (m_hMdiAccel != NULL)
    {
        FreeResource( m_hMdiAccel );
        m_hMdiAccel = NULL;
    }
        
    if (m_pConsole)
    {
        delete m_pConsole;
        m_pConsole = NULL;
    }

    if (m_pSymbolLoader) 
    { 
        delete m_pSymbolLoader;
        m_pSymbolLoader = NULL;
    }

    AfxOleTerm( FALSE );

    //
    // Release Scintilla
    //
    Scintilla_ReleaseResources( );

    //
    // Write settings to profile
    //
    WriteProfileString( _T( "Typedefs" ),   _T( "Hex" ),    g_Typedefs.Hex );
    WriteProfileString( _T( "Typedefs" ),   _T( "Int64" ),  g_Typedefs.Int64 );
    WriteProfileString( _T( "Typedefs" ),   _T( "Int32" ),  g_Typedefs.Int32 );
    WriteProfileString( _T( "Typedefs" ),   _T( "Int16" ),  g_Typedefs.Int16 );
    WriteProfileString( _T( "Typedefs" ),   _T( "Int8" ),   g_Typedefs.Int8 );
    WriteProfileString( _T( "Typedefs" ),   _T( "QWORD" ),  g_Typedefs.Qword );
    WriteProfileString( _T( "Typedefs" ),   _T( "DWORD" ),  g_Typedefs.Dword );
    WriteProfileString( _T( "Typedefs" ),   _T( "WORD" ),   g_Typedefs.Word );
    WriteProfileString( _T( "Typedefs" ),   _T( "BYTE" ),   g_Typedefs.Byte );
    WriteProfileString( _T( "Typedefs" ),   _T( "Float" ),  g_Typedefs.Float );
    WriteProfileString( _T( "Typedefs" ),   _T( "Double" ), g_Typedefs.Double );
    WriteProfileString( _T( "Typedefs" ),   _T( "Vec2" ),   g_Typedefs.Vec2 );
    WriteProfileString( _T( "Typedefs" ),   _T( "Vec3" ),   g_Typedefs.Vec3 );
    WriteProfileString( _T( "Typedefs" ),   _T( "Quat" ),   g_Typedefs.Quat );
    WriteProfileString( _T( "Typedefs" ),   _T( "Matrix" ), g_Typedefs.Matrix );
    WriteProfileString( _T( "Typedefs" ),   _T( "PChar" ),  g_Typedefs.PChar );
    WriteProfileString( _T( "Typedefs" ),   _T( "PWChar" ), g_Typedefs.PWChar );

    WriteProfileInt( _T( "Colors" ),    _T( "Background" ), g_clrBackground );
    WriteProfileInt( _T( "Colors" ),    _T( "Select" ),     g_clrSelect );
    WriteProfileInt( _T( "Colors" ),    _T( "Hidden" ),     g_clrHidden );
    WriteProfileInt( _T( "Colors" ),    _T( "Offset" ),     g_clrOffset );
    WriteProfileInt( _T( "Colors" ),    _T( "Address" ),    g_clrAddress);
    WriteProfileInt( _T( "Colors" ),    _T( "Type" ),       g_clrType );
    WriteProfileInt( _T( "Colors" ),    _T( "Name" ),       g_clrName );
    WriteProfileInt( _T( "Colors" ),    _T( "Index" ),      g_clrIndex );
    WriteProfileInt( _T( "Colors" ),    _T( "Value" ),      g_clrValue );
    WriteProfileInt( _T( "Colors" ),    _T( "Comment" ),    g_clrComment );
    WriteProfileInt( _T( "Colors" ),    _T( "VTable" ),     g_clrVTable );
    WriteProfileInt( _T( "Colors" ),    _T( "Function" ),   g_clrFunction );
    WriteProfileInt( _T( "Colors" ),    _T( "Char" ),       g_clrChar );
    WriteProfileInt( _T( "Colors" ),    _T( "Custom" ),     g_clrCustom );
    WriteProfileInt( _T( "Colors" ),    _T( "Hex" ),        g_clrHex );

    WriteProfileInt( _T( "Display" ),   _T( "Offset" ),     g_bOffset );
    WriteProfileInt( _T( "Display" ),   _T( "Address" ),    g_bAddress);
    WriteProfileInt( _T( "Display" ),   _T( "Text" ),       g_bText );
    WriteProfileInt( _T( "Display" ),   _T( "Float" ),      g_bFloat );
    WriteProfileInt( _T( "Display" ),   _T( "Int" ),        g_bInt );
    WriteProfileInt( _T( "Display" ),   _T( "String" ),     g_bString );
    WriteProfileInt( _T( "Display" ),   _T( "Pointers" ),   g_bPointers );
    WriteProfileInt( _T( "Display" ),   _T( "Top" ),        g_bTop );
    WriteProfileInt( _T( "Display" ),   _T( "ClassBrowser" ), g_bClassBrowser );
    WriteProfileInt( _T( "Display" ),   _T( "FilterProcesses" ), g_bFilterProcesses );

    WriteProfileInt( _T( "Misc" ),  _T( "RTTI" ),           g_bRTTI );
    WriteProfileInt( _T( "Misc" ),  _T( "RandomName" ),     g_bRandomName );
    WriteProfileInt( _T( "Misc" ),  _T( "LoadModuleSymbols" ), g_bLoadModuleSymbol );

    WriteProfileInt( _T( "Class Generation" ), _T( "PrivatePadding" ), g_bPrivatePadding );
    WriteProfileInt( _T( "Class Generation" ), _T( "ClipboardCopy" ), g_bClipboardCopy );
    
    //
    // Exit application instance.
    //
    return CWinAppEx::ExitInstance( );
}

void CReClassExApp::OnButtonReset( )
{
    CloseHandle( g_hProcess );

    g_hProcess = NULL;
    g_ProcessID = 0;
    g_AttachedProcessAddress = NULL;

    if (g_UpdateCacheThread)
    {
        TerminateThread(g_UpdateCacheThread, 0);
        CloseHandle(g_UpdateCacheThread);
        g_UpdateCacheThread = NULL;
    }

    CMDIFrameWnd* pFrame = STATIC_DOWNCAST( CMDIFrameWnd, m_pMainWnd );
    CMDIChildWnd* pChildWnd = pFrame->MDIGetActive( );

    while (pChildWnd)
    {
        pChildWnd->SendMessage( WM_CLOSE, 0, 0 );
        pChildWnd = pFrame->MDIGetActive( );
    }

    m_Classes.clear( );
    g_NodeCreateIndex = 0;

    m_strHeader = _T( "" );
    m_strFooter = _T( "" );
    m_strNotes = _T( "" );
    m_strCurrentFilePath = _T("");
}

void CReClassExApp::OnButtonPause( )
{
    PauseResumeThreadList( FALSE );
}

void CReClassExApp::OnUpdateButtonPause( CCmdUI *pCmdUI )
{
    pCmdUI->Enable( g_hProcess != NULL );
}

void CReClassExApp::OnButtonResume( )
{
    PauseResumeThreadList( TRUE );
}

void CReClassExApp::OnUpdateButtonResume( CCmdUI *pCmdUI )
{
    pCmdUI->Enable( g_hProcess != NULL );
}

void CReClassExApp::OnButtonKill( )
{
    TerminateProcess( g_hProcess, 0 );
    g_hProcess = NULL;
}

void CReClassExApp::OnUpdateButtonKill( CCmdUI* pCmdUI )
{
    pCmdUI->Enable( g_hProcess != NULL );
}

void CReClassExApp::CalcOffsets( CNodeClass* pClass )
{
    size_t offset = 0;
    for (UINT i = 0; i < pClass->NodeCount( ); i++)
    {
        pClass->GetNode( i )->SetOffset( offset );
        offset += pClass->GetNode( i )->GetMemorySize( );
    }
}

void CReClassExApp::CalcAllOffsets( )
{
    for (UINT i = 0; i < m_Classes.size( ); i++)
        CalcOffsets( m_Classes[i] );
}

void CReClassExApp::OnFileNew( )
{
    CMainFrame* pMainFrame = STATIC_DOWNCAST( CMainFrame, m_pMainWnd );
    
    CClassFrame* pChildClassFrame = STATIC_DOWNCAST( CClassFrame, 
        pMainFrame->CreateNewChild( RUNTIME_CLASS( CClassFrame ), IDR_ReClassExTYPE, m_hMdiMenu, m_hMdiAccel ) );
    
    CNodeClass* pClass = new CNodeClass;
    
    pClass->m_pChildClassFrame = pChildClassFrame;
    pChildClassFrame->SetClass( pClass );
    g_ReClassApp.m_Classes.push_back( pClass );
    
    //CNodeCustom* pCust = new CNodeCustom;
    //pCust->memsize = 18;
    //pCust->pParent = pClass;
    //pClass->Nodes.push_back(pCust);

    for (int i = 0; i < 64 / sizeof( size_t ); i++)
    {
        CNodeHex* pNode = new CNodeHex;
        pNode->SetParent( pClass );
        pClass->AddNode( pNode );
    }

    CalcOffsets( pClass );
}

void CReClassExApp::PreLoadState( )
{
    CString strName;
    ASSERT( strName.LoadString( IDS_EDIT_MENU ) );
    GetContextMenuManager( )->AddMenu( strName, IDR_POPUP_EDIT );
}

void CReClassExApp::LoadCustomState( )
{
}

void CReClassExApp::SaveCustomState( )
{
}

// TODO: Actually finish this
void CReClassExApp::OnFileImport( )
{
    return;
}

void CReClassExApp::ClearSelection( )
{
    for (size_t c = 0; c < m_Classes.size( ); c++)
    {
        m_Classes[c]->Unselect( );
        for (size_t n = 0; n < m_Classes[c]->NodeCount( ); n++)
        {
            CNodeBase* pNode = m_Classes[c]->GetNode( n );
            pNode->Unselect( );

            NodeType nodeType = pNode->GetType( );
            if (nodeType == nt_vtable)
            {
                CNodeVTable* pVTable = static_cast<CNodeVTable*>(pNode);
                for (size_t f = 0; f < pVTable->NodeCount( ); f++)
                {
                    pVTable->GetNode( f )->Unselect( );
                }
            }
            if (nodeType == nt_array)
            {
                static_cast<CNodeArray*>(pNode)->Unselect( );
            }
            if (nodeType == nt_pointer)
            {
                static_cast<CNodePtr*>(pNode)->Unselect( );
            }
        }
    }
}

void CReClassExApp::ClearHidden( )
{
    for (size_t c = 0; c < m_Classes.size( ); c++)
    {
        m_Classes[c]->Show( );
        for (size_t n = 0; n < m_Classes[c]->NodeCount( ); n++)
        {
            CNodeBase* pNode = m_Classes[c]->GetNode( n );
            pNode->Show( );

            NodeType nt = pNode->GetType( );
            if (nt == nt_vtable)
            {
                CNodeVTable* pVTable = static_cast<CNodeVTable*>(pNode);
                for (size_t f = 0; f < pVTable->NodeCount( ); f++)
                {
                    pVTable->GetNode( f )->Show( );
                }          
            }
            if (nt == nt_array)
            {
                static_cast<CNodeArray*>(pNode)->Show( );
            }
            if (nt == nt_pointer)
            {
                static_cast<CNodePtr*>(pNode)->Show( );
            }
        }
    }
}

bool CReClassExApp::IsNodeValid( CNodeBase* pCheckNode )
{
    for (size_t c = 0; c < m_Classes.size( ); c++)
    {
        for (size_t n = 0; n < m_Classes[c]->NodeCount( ); n++)
        {
            CNodeBase* pNode = m_Classes[c]->GetNode( n );
            if (pNode == pCheckNode)
                return true;

            NodeType nodeType = pNode->GetType( );
            if (nodeType == nt_vtable)
            {
                CNodeVTable* pVTable = static_cast<CNodeVTable*>(pNode);
                for (size_t f = 0; f < pVTable->NodeCount( ); f++)
                {
                    if (pVTable->GetNode( f ) == pCheckNode)
                        return true;
                }
            }
            if (nodeType == nt_array)
            {
                if (static_cast<CNodeArray*>(pNode)->GetClass( ) == pCheckNode)
                    return true;
            }
            if (nodeType == nt_pointer)
            {
                if (static_cast<CNodePtr*>(pNode)->GetClass( ) == pCheckNode)
                    return true;
            }
        }
    }
    return false;
}


//////////////// OnButtonNewClass /////////////////
void CReClassExApp::OnButtonNewClass( )
{
    CMainFrame* pMainFrame = STATIC_DOWNCAST( CMainFrame, m_pMainWnd );
    
    CClassFrame* pChildClassFrame = STATIC_DOWNCAST( CClassFrame, 
        pMainFrame->CreateNewChild( RUNTIME_CLASS( CClassFrame ), IDR_ReClassExTYPE, m_hMdiMenu, m_hMdiAccel ) );
    
    CNodeClass* pNewClass = new CNodeClass;

    pNewClass->SetChildClassFrame( pChildClassFrame );
    pNewClass->m_Idx = g_ReClassApp.m_Classes.size( );
    pChildClassFrame->SetClass( pNewClass );

    g_ReClassApp.m_Classes.push_back( pNewClass );

    for (int i = 0; i < 64 / sizeof( size_t ); i++)
    {
        CNodeHex* pNode = new CNodeHex;
        pNode->SetParent( pNewClass );
        pNewClass->AddNode( pNode );
    }

    CalcOffsets( pNewClass );
}

void CReClassExApp::OnButtonSearch( )
{
    GetMainWnd( )->MessageBox( _T( "Coming Soon!" ), _T( "WubbaLubbaDubDub" ) );
}

void CReClassExApp::OnUpdateButtonSearch( CCmdUI *pCmdUI )
{
    pCmdUI->Enable( g_hProcess != NULL );
}

void CReClassExApp::OnButtonConsole( )
{
    m_pConsole->ShowWindow( SW_SHOW );
    m_pConsole->SetForegroundWindow( );
}

void CReClassExApp::OnButtonModules( )
{
    CDialogModules dlg;
    dlg.DoModal( );
}

void CReClassExApp::OnUpdateButtonModules( CCmdUI * pCmdU )
{
    pCmdU->Enable( g_hProcess != NULL );
}

void CReClassExApp::OnButtonNotes( )
{
    CDialogEdit dlg;
    dlg.Title = _T( "Notes" );
    dlg.Text = m_strNotes;
    dlg.DoModal( );
    m_strNotes = dlg.Text;
}

void CReClassExApp::OnButtonParser( )
{
    CDialogClasses dlg;
    dlg.DoModal( );
}

void CReClassExApp::OnButtonHeader( )
{
    CDialogEdit dlg;
    dlg.Title = _T( "Header" );
    dlg.Text = m_strHeader;
    dlg.DoModal( );
    m_strHeader = dlg.Text;
}

void CReClassExApp::OnButtonFooter( )
{
    CDialogEdit dlg;
    dlg.Title = _T( "Footer" );
    dlg.Text = m_strFooter;
    dlg.DoModal( );
    m_strFooter = dlg.Text;
}

CMainFrame* CReClassExApp::GetMainFrame( )
{
    return STATIC_DOWNCAST( CMainFrame, m_pMainWnd );
}

CMFCRibbonBar* CReClassExApp::GetRibbonBar( )
{
    return (CMFCRibbonBar*)&GetMainFrame( )->m_RibbonBar;
}

CStatusBar* CReClassExApp::GetStatusBar( )
{
    return (CStatusBar*)&GetMainFrame( )->m_StatusBar;
}

CNodeBase* CReClassExApp::CreateNewNode( NodeType Type )
{
    switch ( Type )
    {
    case nt_class:			return new CNodeClass;

    case nt_hex64:			return new CNodeHex64;
    case nt_hex32:			return new CNodeHex32;
    case nt_hex16:			return new CNodeHex16;
    case nt_hex8:			return new CNodeHex8;
    case nt_bits:			return new CNodeBits;

    case nt_int64:			return new CNodeInt64;
    case nt_int32:			return new CNodeInt32;
    case nt_int16:			return new CNodeInt16;
    case nt_int8:			return new CNodeInt8;

    case nt_uint64:			return new CNodeQword;
    case nt_uint32:			return new CNodeDword;
    case nt_uint16:			return new CNodeWord;
    case nt_uint8:			return new CNodeByte;

    case nt_vec2:			return new CNodeVec2;
    case nt_vec3:			return new CNodeVec3;
    case nt_quat:			return new CNodeQuat;
    case nt_matrix:			return new CNodeMatrix;

    case nt_float:			return new CNodeFloat;
    case nt_double:			return new CNodeDouble;

    case nt_custom:			return new CNodeCustom;
    case nt_text:			return new CNodeText;
    case nt_pchar:			return new CNodeCharPtr;
    case nt_pwchar:			return new CNodeWCharPtr;
    case nt_unicode:		return new CNodeUnicode;

    case nt_vtable:			return new CNodeVTable;
    case nt_functionptr:	return new CNodeFunctionPtr;
    case nt_function:		return new CNodeFunction;

    case nt_pointer:		return new CNodePtr;
    case nt_array:			return new CNodeArray;
    case nt_ptrarray:		return new CNodePtrArray;

    case nt_instance:		return new CNodeClassInstance;
    }
    return NULL;
}

void CReClassExApp::SaveXML( TCHAR* FileName )
{
    PrintOutDbg( _T( "SaveXML(\"%s\") called" ), FileName );

    tinyxml2::XMLDocument XmlDoc;

    XMLDeclaration* decl = XmlDoc.NewDeclaration(/*"xml version = \"1.0\" encoding=\"UTF-8\""*/ );
    XmlDoc.LinkEndChild( decl );

    XMLElement* root = XmlDoc.NewElement( "RC" );
    XmlDoc.LinkEndChild( root );

    XMLComment* comment = XmlDoc.NewComment( "RCE" );
    root->LinkEndChild( comment );
    //---------------------------------------------
    XMLElement* settings = XmlDoc.NewElement( "TypeDef" );
#ifdef UNICODE
    settings->SetAttribute( "tdHex",    CW2A( g_Typedefs.Hex ) );
    settings->SetAttribute( "tdInt64",  CW2A( g_Typedefs.Int64 ) );
    settings->SetAttribute( "tdInt32",  CW2A( g_Typedefs.Int32 ) );
    settings->SetAttribute( "tdInt16",  CW2A( g_Typedefs.Int16 ) );
    settings->SetAttribute( "tdInt8",   CW2A( g_Typedefs.Int8 ) );
    settings->SetAttribute( "tdQword",  CW2A( g_Typedefs.Qword ) );
    settings->SetAttribute( "tdDword",  CW2A( g_Typedefs.Dword ) );
    settings->SetAttribute( "tdWord",   CW2A( g_Typedefs.Word ) );
    settings->SetAttribute( "tdByte",   CW2A( g_Typedefs.Byte ) );
    settings->SetAttribute( "tdFloat",  CW2A( g_Typedefs.Float ) );
    settings->SetAttribute( "tdDouble", CW2A( g_Typedefs.Double ) );
    settings->SetAttribute( "tdVec2",   CW2A( g_Typedefs.Vec2 ) );
    settings->SetAttribute( "tdVec3",   CW2A( g_Typedefs.Vec3 ) );
    settings->SetAttribute( "tdQuat",   CW2A( g_Typedefs.Quat ) );
    settings->SetAttribute( "tdMatrix", CW2A( g_Typedefs.Matrix ) );
    settings->SetAttribute( "tdPChar",  CW2A( g_Typedefs.PChar ) );
    settings->SetAttribute( "tdPWChar", CW2A( g_Typedefs.PWChar ) );
#else
    settings->SetAttribute( "tdHex",    g_Typedefs.Hex );
    settings->SetAttribute( "tdInt64",  g_Typedefs.Int64 );
    settings->SetAttribute( "tdInt32",  g_Typedefs.Int32 );
    settings->SetAttribute( "tdInt16",  g_Typedefs.Int16 );
    settings->SetAttribute( "tdInt8",   g_Typedefs.Int8 );
    settings->SetAttribute( "tdQword",  g_Typedefs.Qword );
    settings->SetAttribute( "tdDword",  g_Typedefs.Dword );
    settings->SetAttribute( "tdWord",   g_Typedefs.Word );
    settings->SetAttribute( "tdByte",   g_Typedefs.Byte );
    settings->SetAttribute( "tdFloat",  g_Typedefs.Float );
    settings->SetAttribute( "tdDouble", g_Typedefs.Double );
    settings->SetAttribute( "tdVec2",   g_Typedefs.Vec2 );
    settings->SetAttribute( "tdVec3",   g_Typedefs.Vec3 );
    settings->SetAttribute( "tdQuat",   g_Typedefs.Quat );
    settings->SetAttribute( "tdMatrix", g_Typedefs.Matrix );
    settings->SetAttribute( "tdPChar",  g_Typedefs.PChar );
    settings->SetAttribute( "tdPWChar", g_Typedefs.PWChar );
#endif
    root->LinkEndChild( settings );

    settings = XmlDoc.NewElement( "Header" );
#ifdef UNICODE
    settings->SetAttribute( "Text", CW2A( m_strHeader ) );
    root->LinkEndChild( settings );

    settings = XmlDoc.NewElement( "Footer" );
    settings->SetAttribute( "Text", CW2A( m_strFooter ) );
    root->LinkEndChild( settings );

    settings = XmlDoc.NewElement( "Notes" );
    settings->SetAttribute( "Text", CW2A( m_strNotes ) );
    root->LinkEndChild( settings );
#else
    settings->SetAttribute( "Text", m_strHeader );
    root->LinkEndChild( settings );

    settings = XmlDoc.NewElement( "Footer" );
    settings->SetAttribute( "Text", m_strFooter );
    root->LinkEndChild( settings );

    settings = XmlDoc.NewElement( "Notes" );
    settings->SetAttribute( "Text", m_strNotes );
    root->LinkEndChild( settings );
#endif

    for (UINT i = 0; i < m_Classes.size( ); i++)
    {
        CNodeClass* pClass = m_Classes[i];

        #ifdef UNICODE
        CStringA strClassName = CW2A( pClass->GetName( ) );
        CStringA strClassComment = CW2A( pClass->GetComment( ) );
        CStringA strClassOffset = CW2A( pClass->GetOffsetString( ) );
        CStringA strClassCode = CW2A( pClass->m_Code );
        #else
        CStringA strClassName = pClass->GetName( );
        CStringA strClassComment = pClass->GetComment( );
        CStringA strClassOffset = pClass->GetOffsetString( );
        CStringA strClassCode = pClass->m_Code;
        #endif

        XMLElement* classNode = XmlDoc.NewElement( "Class" );
        classNode->SetAttribute( "Name", strClassName );
        classNode->SetAttribute( "Type", pClass->GetType( ) );
        classNode->SetAttribute( "Comment", strClassComment );
        classNode->SetAttribute( "Offset", (int)pClass->GetOffset( ) );
        classNode->SetAttribute( "strOffset", strClassOffset );
        classNode->SetAttribute( "Code", strClassCode );
        root->LinkEndChild( classNode );

        for (UINT n = 0; n < pClass->NodeCount( ); n++)
        {
            CNodeBase* pNode = pClass->GetNode( n );
            if (!pNode)
                continue;

            #ifdef UNICODE
            CStringA strNodeName = CW2A( pNode->GetName( ) );
            CStringA strNodeComment = CW2A( pNode->GetComment( ) );
            #else
            CStringA strNodeName = pNode->GetName( );
            CStringA strNodeComment = pNode->GetComment( );
            #endif

            XMLElement* pXmlNode = XmlDoc.NewElement( "Node" );
            pXmlNode->SetAttribute( "Name", strNodeName );
            pXmlNode->SetAttribute( "Type", pNode->GetType( ) );
            pXmlNode->SetAttribute( "Size", (UINT)pNode->GetMemorySize( ) );
            pXmlNode->SetAttribute( "bHidden", pNode->IsHidden( ) );
            pXmlNode->SetAttribute( "Comment", strNodeComment );

            classNode->LinkEndChild( pXmlNode );

            if (pNode->GetType( ) == nt_array)
            {
                CNodeArray* pArray = (CNodeArray*)pNode;
                pXmlNode->SetAttribute( "Total", (UINT)pArray->GetTotal( ) );

                #ifdef UNICODE
                CStringA strArrayNodeName = CW2A( pArray->GetClass( )->GetName( ) );
                CStringA strArrayNodeComment = CW2A( pArray->GetClass( )->GetComment( ) );
                #else
                CStringA strArrayNodeName = pArray->GetClass( )->GetName( );
                CStringA strArrayNodeComment = pArray->GetClass( )->GetComment( );
                #endif

                XMLElement *item = XmlDoc.NewElement( "Array" );
                item->SetAttribute( "Name", strArrayNodeName );
                item->SetAttribute( "Type", pArray->GetClass( )->GetType( ) );
                item->SetAttribute( "Size", (UINT)pArray->GetClass( )->GetMemorySize( ) );
                item->SetAttribute( "Comment", strArrayNodeComment );
                pXmlNode->LinkEndChild( item );
            }
            else if (pNode->GetType() == nt_ptrarray)
            { 
                CNodePtrArray* pArray = (CNodePtrArray*)pNode;
                pXmlNode->SetAttribute( "Count", (UINT)pArray->Count( ) );

                #ifdef UNICODE
                CStringA strArrayNodeName = CW2A( pArray->GetClass()->GetName() );
                CStringA strArrayNodeComment = CW2A( pArray->GetClass()->GetComment() );
                #else
                CStringA strArrayNodeName = pArray->GetClass()->GetName();
                CStringA strArrayNodeComment = pArray->GetClass()->GetComment();
                #endif

                XMLElement *item = XmlDoc.NewElement( "Array" );
                item->SetAttribute( "Name", strArrayNodeName );
                item->SetAttribute( "Type", pArray->GetClass( )->GetType( ) );
                item->SetAttribute( "Size", (UINT)pArray->GetClass( )->GetMemorySize( ) );
                item->SetAttribute( "Comment", strArrayNodeComment );
                pXmlNode->LinkEndChild( item );
            }
            else if (pNode->GetType( ) == nt_pointer)
            {
                CNodePtr* pPointer = (CNodePtr*)pNode;
                #ifdef UNICODE
                CStringA strPtrNodeName = CW2A( pPointer->GetClass( )->GetName( ) );
                #else
                CStringA strPtrNodeName = pPointer->GetClass( )->GetName( );
                #endif

                pXmlNode->SetAttribute( "Pointer", strPtrNodeName );
            }
            else if (pNode->GetType( ) == nt_instance)
            {
                CNodeClassInstance* pClassInstance = (CNodeClassInstance*)pNode;
                #ifdef UNICODE
                CStringA strInstanceNodeName = CW2A( pClassInstance->GetClass( )->GetName( ) );
                #else
                CStringA strInstanceNodeName = pClassInstance->GetClass( )->GetName( );
                #endif
                pXmlNode->SetAttribute( "Instance", strInstanceNodeName );
            }
            else if (pNode->GetType( ) == nt_vtable)
            {
                CNodeVTable* pVTable = (CNodeVTable*)pNode;
                for (UINT f = 0; f < pVTable->NodeCount( ); f++)
                {
                    CNodeFunctionPtr* pFunctionPtr = (CNodeFunctionPtr*)pVTable->GetNode( f );
                    #ifdef UNICODE
                    CStringA strFunctionNodeName = CW2A( pFunctionPtr->GetName( ) );
                    CStringA strFunctionNodeComment = CW2A( pFunctionPtr->GetComment( ) );
                    #else
                    CStringA strFunctionNodeName = pFunctionPtr->GetName( );
                    CStringA strFunctionNodeComment = pFunctionPtr->GetComment( );
                    #endif

                    XMLElement *pXmlFunctionElement = XmlDoc.NewElement( "Function" );
                    pXmlFunctionElement->SetAttribute( "Name", strFunctionNodeName );
                    pXmlFunctionElement->SetAttribute( "Comment", strFunctionNodeComment );
                    pXmlFunctionElement->SetAttribute( "bHidden", pFunctionPtr->IsHidden( ) );
                    pXmlNode->LinkEndChild( pXmlFunctionElement );

                    CStringA strFunctionAssembly;
                    strFunctionAssembly.Preallocate( 2048 );
                    for (UINT as = 0; as < pFunctionPtr->m_Assembly.size( ); as++)
                    {
                        strFunctionAssembly += pFunctionPtr->m_Assembly[as];
                    }

                    XMLElement* pXmlCodeElement = XmlDoc.NewElement( "Code" );
                    pXmlCodeElement->SetAttribute( "Assembly", strFunctionAssembly );
                    pXmlFunctionElement->LinkEndChild( pXmlCodeElement );
                }
            }
        }
    }

    FILE* fp;
    _tfopen_s( &fp, FileName, _T( "wb" ) );
    XMLError err = XmlDoc.SaveFile( fp );
    fclose( fp );

    if (err != XML_SUCCESS)
    {
        PrintOut( _T( "Failed to save file to \"%s\". Error %d" ), FileName, err );   
        return;
    }

    PrintOut( _T( "RC files saved successfully to \"%s\"" ), FileName ); 
}

void CReClassExApp::OnFileSave( )
{
    if (m_strCurrentFilePath.IsEmpty( ))
    {
        OnFileSaveAs( );
    }
    else
    {
        SaveXML( m_strCurrentFilePath.GetBuffer( ) );
    }
}

void CReClassExApp::OnFileSaveAs( )
{
    TCHAR Filters[] = _T( "RC (*.rc)|*.rc|All Files (*.*)|*.*||" );
    CFileDialog fileDlg( FALSE, _T( "rc" ), _T( "" ), OFN_HIDEREADONLY, Filters, NULL );
    if (fileDlg.DoModal( ) != IDOK)
        return;

    CString pathName = fileDlg.GetPathName( );
    m_strCurrentFilePath = pathName;
    SaveXML( pathName.GetBuffer( ) );
}

void CReClassExApp::OnFileOpen( )
{
    TCHAR Filters[] = _T( "RC (*.rc)|*.rc|All Files (*.*)|*.*||" );
    
    CFileDialog fileDlg( TRUE, _T( "rc" ), _T( "" ), OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, Filters );
    if (fileDlg.DoModal( ) != IDOK)
        return;

    CString pathName = fileDlg.GetPathName( );

    CMDIFrameWnd* pFrame = STATIC_DOWNCAST( CMDIFrameWnd, m_pMainWnd );
    CMDIChildWnd* pChildWnd = pFrame->MDIGetActive( );

    while (pChildWnd)
    {
        pChildWnd->SendMessage( WM_CLOSE, 0, 0 );
        pChildWnd = pFrame->MDIGetActive( );
    }

    m_Classes.clear( );

    m_strHeader = _T( "" );
    m_strFooter = _T( "" );
    m_strNotes = _T( "" );
    m_strCurrentFilePath = _T( "" );

    tinyxml2::XMLDocument XmlDoc;

    #ifdef UNICODE
    #define _CA2W(psz) CA2W(psz)
    #else
    #define _CA2W(psz) (psz)
    #endif

    FILE* fp = NULL;
    #ifdef UNICODE
    _wfopen_s( &fp, pathName, L"rb" );
    #else
    fopen_s( &fp, pathName, "rb" );
    #endif
    XMLError ret = XmlDoc.LoadFile( fp );
    fclose( fp );

    if (ret != XML_SUCCESS)
        return;

    m_strCurrentFilePath = pathName;

    XMLHandle hDoc( &XmlDoc );
    XMLHandle hRoot( 0 );
    XMLElement* XmlCurrentElement;
    typedef std::pair<CString, CNodeBase*> Link;
    std::vector<Link> links;

    XmlCurrentElement = hDoc.FirstChildElement( ).ToElement( );
    if (!XmlCurrentElement)
        return;

    const char* v = XmlCurrentElement->Value( );

    if (_stricmp( v, "RC" ) != 0) // The root element value is 'ReClass'
        return; // Not a Reclass file

    hRoot = XMLHandle( XmlCurrentElement );

    XmlCurrentElement = hRoot.FirstChildElement( "Header" ).ToElement( );
    if (XmlCurrentElement)
    {
        m_strHeader.SetString( _CA2W( XmlCurrentElement->Attribute( "Text" ) ) );
    }

    XmlCurrentElement = hRoot.FirstChildElement( "Footer" ).ToElement( );
    if (XmlCurrentElement)
    {
        m_strFooter.SetString( _CA2W( XmlCurrentElement->Attribute( "Text" ) ) );
    }

    XmlCurrentElement = hRoot.FirstChildElement( "Notes" ).ToElement( );
    if (XmlCurrentElement)
    {
        m_strNotes.SetString( _CA2W( XmlCurrentElement->Attribute( "Text" ) ) );
    }

    XmlCurrentElement = hRoot.FirstChildElement( "Class" ).ToElement( );
    while (XmlCurrentElement)
    {
        CNodeClass* pClass = new CNodeClass;
        pClass->SetName( _CA2W( XmlCurrentElement->Attribute( "Name" ) ) );
        pClass->SetComment( _CA2W( XmlCurrentElement->Attribute( "Comment" ) ) );
        pClass->SetOffset( atoi( XmlCurrentElement->Attribute( "Offset" ) ) );
        pClass->SetOffsetString( _CA2W( XmlCurrentElement->Attribute( "strOffset" ) ) );
        pClass->SetCodeString( _CA2W( XmlCurrentElement->Attribute( "Code" ) ) );

        if (pClass->GetOffsetString( ) == "")
            pClass->SetOffsetString( _CA2W( XmlCurrentElement->Attribute( "Offset" ) ) );

        XMLElement* XmlClassElement = XmlCurrentElement->FirstChildElement( );
        while (XmlClassElement)
        {
            int Type = nt_none;
            XmlClassElement->QueryIntAttribute( "Type", &Type );

            if (Type != nt_none)
            {
                CNodeBase* pNode = CreateNewNode( (NodeType)Type );
                int Size = -1;
                
                pNode->SetName( _CA2W( XmlClassElement->Attribute( "Name" ) ) );
                pNode->SetComment( _CA2W( XmlClassElement->Attribute( "Comment" ) ) );
                pNode->SetHidden( atoi( XmlClassElement->Attribute( "bHidden" ) ) > 0 ? true : false );
                pNode->SetParent( pClass );
                pClass->AddNode( pNode );

                XmlClassElement->QueryIntAttribute( "Size", &Size );

                if (Type == nt_custom)
                {
                    static_cast<CNodeCustom*>(pNode)->SetSize( Size );
                }
                else if (Type == nt_text)
                {
                    static_cast<CNodeText*>(pNode)->SetSize( Size );
                }
                else if (Type == nt_unicode)
                {
                    static_cast<CNodeUnicode*>(pNode)->SetSize( Size );
                }
                else if (Type == nt_function)
                {
                    static_cast<CNodeFunction*>(pNode)->SetSize( Size );
                }
                else if (Type == nt_vtable)
                {
                    XMLElement* XmlVTableFunctionPtrElement = XmlClassElement->FirstChildElement( );
                    while (XmlVTableFunctionPtrElement)
                    {
                        CNodeVTable* VMTNode = static_cast<CNodeVTable*>(pNode);
                        CNodeFunctionPtr* FunctionPtrNode;

                        VMTNode->Initialize( GetMainFrame( ) );

                        FunctionPtrNode = new CNodeFunctionPtr;
                        FunctionPtrNode->SetName( _CA2W( XmlVTableFunctionPtrElement->Attribute( "Name" ) ) );
                        FunctionPtrNode->SetComment( _CA2W( XmlVTableFunctionPtrElement->Attribute( "Comment" ) ) );
                        FunctionPtrNode->SetHidden( atoi( XmlVTableFunctionPtrElement->Attribute( "bHidden" ) ) > 0 ? true : false );
                        FunctionPtrNode->SetParent( VMTNode );

                        VMTNode->AddNode( FunctionPtrNode );

                        XMLElement* XmlCodeElement = XmlVTableFunctionPtrElement->FirstChildElement( );
                        while (XmlCodeElement)
                        {
                            CStringA AssemblyString = XmlCodeElement->Attribute( "Assembly" );
                            FunctionPtrNode->m_Assembly.push_back( AssemblyString );
                            XmlCodeElement = XmlCodeElement->NextSiblingElement( );
                        }

                        XmlVTableFunctionPtrElement = XmlVTableFunctionPtrElement->NextSiblingElement( );
                    }
                }
                else if (Type == nt_array)
                {
                    //<Node Name="N4823" Type="23" Size="64" bHidden="0" Comment="" Total="1">
                    //<Array Name="N12DB" Type="24" Size="64" Comment="" />
                    CNodeArray* pArray = (CNodeArray*)pNode;
                    pArray->SetTotal( (DWORD)atoi( XmlClassElement->Attribute( "Total" ) ) );

                    XMLElement* XmlArrayElement = XmlClassElement->FirstChildElement( );
                    if (XmlArrayElement)
                    {
                        CString Name = _CA2W( XmlArrayElement->Attribute( "Name" ) );
                        CString Comment = _CA2W( XmlArrayElement->Attribute( "Comment" ) );
                        int ArrayType = nt_none;
                        int ArraySize = 0;

                        XmlArrayElement->QueryIntAttribute( "Type", &ArrayType );
                        XmlClassElement->QueryIntAttribute( "Size", &ArraySize );

                        if (ArrayType == nt_class)
                        {
                            links.push_back( Link( Name, pNode ) );
                        }
                        // TODO: Handle other type of arrays....
                    }
                }
                else if ( Type == nt_ptrarray )
                {
                    CNodePtrArray* pArray = (CNodePtrArray*) pNode;
                    pArray->SetCount( (size_t)atoi( XmlClassElement->Attribute( "Count" ) ) );

                    XMLElement* XmlArrayElement = XmlClassElement->FirstChildElement( );
                    if (XmlArrayElement)
                    {
                        CString Name = _CA2W( XmlArrayElement->Attribute( "Name" ) );
                        CString Comment = _CA2W( XmlArrayElement->Attribute( "Comment" ) );
                        int ArrayType = nt_none;
                        int ArraySize = 0;

                        XmlArrayElement->QueryIntAttribute( "Type", &ArrayType );
                        XmlClassElement->QueryIntAttribute( "Size", &ArraySize );

                        if (ArrayType == nt_class) 
                        {
                            links.push_back( Link( Name, pNode ) );
                        }
                    }
                }
                else if (Type == nt_pointer)
                {
                    CString PointerString = _CA2W( XmlClassElement->Attribute( "Pointer" ) );
                    links.push_back( Link( PointerString, pNode ) );
                }
                else if (Type == nt_instance)
                {
                    CString strInstance = _CA2W( XmlClassElement->Attribute( "Instance" ) );
                    links.push_back( Link( strInstance, pNode ) );
                }
            }

            XmlClassElement = XmlClassElement->NextSiblingElement( );
        }

        m_Classes.push_back( pClass );

        XmlCurrentElement = XmlCurrentElement->NextSiblingElement( "Class" );
    }

    //Fix Links... very ghetto this whole thing is just fucked
    for (auto it = links.begin( ); it != links.end( ); it++)
    {
        for (size_t i = 0; i < m_Classes.size( ); i++)
        {
            if (it->first == m_Classes[i]->GetName( ))
            {
                NodeType Type = it->second->GetType( );
                if (Type == nt_pointer)
                {
                    static_cast<CNodePtr*>(it->second)->SetClass( m_Classes[i] );
                }
                else if (Type == nt_instance)
                {
                    static_cast<CNodeClassInstance*>(it->second)->SetClass( m_Classes[i] );
                }
                else if (Type == nt_array)
                {
                    static_cast<CNodeArray*>(it->second)->SetClass( m_Classes[i] );
                }
                else if ( Type == nt_ptrarray )
                {
                    static_cast<CNodePtrArray*>(it->second)->SetClass( m_Classes[i] );
                }
            }
        }
    }

    CalcAllOffsets( );
}

void CReClassExApp::OnButtonGenerate( )
{
    CString strGeneratedText, t;
    strGeneratedText += _T( "// Generated using RC\r\n\r\n" );

    if (!m_strHeader.IsEmpty( ))
        strGeneratedText += m_strHeader + _T( "\r\n\r\n" );
    std::set<const CNodeClass*>  forwardDeclarations; 
    std::vector<const CNodeClass*>  orderedClassDefinitions;
    ClassDependencyGraph            depGraph;
    // Add each class as a node to the graph before adding dependency edges
    for (auto cNode : this->m_Classes) {
        depGraph.AddNode(cNode);
    }
    for (auto cNode : this->m_Classes) {
        for (size_t n = 0; n < cNode->NodeCount(); n++)
        {
            CNodeBase* pNode = (CNodeBase*)cNode->GetNode(n);
            NodeType Type = pNode->GetType();
            switch (Type) {
            case(nt_pointer):
            {
                CNodePtr* pPointer = (CNodePtr*)pNode;
                CNodeClass* pointerClass = pPointer->GetClass();
                depGraph.AddEdge(cNode, pointerClass, DependencyType::POINTER);
                break;
            }
            case(nt_instance):
            {
                CNodeClassInstance* pClassInstance = (CNodeClassInstance*)pNode;
                CNodeClass* instanceClass = pClassInstance->GetClass();
                depGraph.AddEdge(cNode, instanceClass, DependencyType::INSTANCE);
                break;
            }

            case(nt_array):
            {
                CNodeArray* pArray = (CNodeArray*)pNode;
                CNodeClass* instanceClass = pArray->GetClass();
                depGraph.AddEdge(cNode, instanceClass, DependencyType::INSTANCE);
                break;
            }

            case(nt_ptrarray):
            {
                CNodePtrArray* pArray = (CNodePtrArray*)pNode;
                CNodeClass* pointerClass = pArray->GetClass();
                depGraph.AddEdge(cNode, pointerClass, DependencyType::POINTER);
                break;
            }
            }
        }
    }

    depGraph.OrderClassesForGeneration(forwardDeclarations, orderedClassDefinitions);
    ASSERT(orderedClassDefinitions.size() == m_Classes.size());
    for (auto forwardDeclared : forwardDeclarations)
    {
        CNodeClass* pClass = (CNodeClass*)forwardDeclared;
        t.Format( _T( "class %s;\r\n" ), pClass->GetName( ) );
        strGeneratedText += t;
    }

    strGeneratedText += _T( "\r\n" );

    std::vector<CString> vfun;
    std::vector<CString> var;

    CString ClassName;

    for (auto constantClass : orderedClassDefinitions)
    {
        CNodeClass* pClass = (CNodeClass*)constantClass;

        CalcOffsets( pClass );

        vfun.clear( );
        var.clear( );

        ClassName.Format( _T( "class %s" ), pClass->GetName( ) );

        int fill = 0;
        int fillStart = 0;

        for (size_t n = 0; n < pClass->NodeCount( ); n++)
        {
            CNodeBase* pNode = (CNodeBase*)pClass->GetNode( n );
            NodeType Type = pNode->GetType( );

            if ((Type == nt_hex64) || (Type == nt_hex32) || (Type == nt_hex16) || (Type == nt_hex8))
            {
                if (fill == 0)
                    fillStart = (int)pNode->GetOffset( );
                fill += pNode->GetMemorySize( );
            }
            else
            {
                if (fill > 0)
                {
                    if (g_bPrivatePadding)
                        t.Format( _T( "private:\r\n\t%s pad_0x%0.4X[0x%X]; //0x%0.4X\r\npublic:\r\n" ), g_Typedefs.Hex, fillStart, fill, fillStart );
                    else
                        t.Format( _T( "\t%s pad_0x%0.4X[0x%X]; //0x%0.4X\r\n" ), g_Typedefs.Hex, fillStart, fill, fillStart );
                    var.push_back( t );
                }
                fill = 0;

                if (Type == nt_vtable)
                {
                    CNodeVTable* pVTable = (CNodeVTable*)pNode;
                    for (size_t f = 0; f < pVTable->NodeCount( ); f++)
                    {
                        CString fn( pVTable->GetNode( f )->GetName( ) );
                        if (fn.GetLength( ) == 0)
                            fn.Format( _T( "void Function%i()" ), f );
                        t.Format( _T( "\tvirtual %s; //%s\r\n" ), fn, pVTable->GetNode( f )->GetComment( ) );
                        vfun.push_back( t );
                    }
                }
                else if (Type == nt_int64)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Int64, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_int32)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Int32, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_int16)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Int16, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_int8)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Int8, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_uint64)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Qword, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_uint32)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Dword, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_uint16)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Word, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_uint8)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Byte, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }

                else if (Type == nt_vec2)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Vec2, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_vec3)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Vec3, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_quat)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Quat, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_matrix)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Matrix, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }

                else if (Type == nt_pchar)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.PChar, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_pwchar)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.PWChar, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_text)
                {
                    CNodeText* pText = (CNodeText*)pNode;
                    t.Format( _T( "\tchar %s[%i]; //0x%0.4X %s\r\n" ), pText->GetName( ), pText->GetMemorySize( ), pText->GetComment( ), pText->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_unicode)
                {
                    CNodeUnicode* pText = (CNodeUnicode*)pNode;
                    t.Format( _T( "\twchar_t %s[%i]; //0x%0.4X %s\r\n" ), pText->GetName( ), pText->GetMemorySize( ) / sizeof( wchar_t ), pText->GetOffset( ), pText->GetComment( ) );
                    var.push_back( t );
                }

                else if (Type == nt_float)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Float, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }
                else if (Type == nt_double)
                {
                    t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), g_Typedefs.Double, pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }

                else if (Type == nt_custom)
                {
                    t.Format( _T( "\t%s; //0x%0.4X %s\r\n" ), pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }

                else if (Type == nt_functionptr)
                {
                    t.Format( _T( "\t%s; //0x%0.4X %s\r\n" ), pNode->GetName( ), pNode->GetOffset( ), pNode->GetComment( ) );
                    var.push_back( t );
                }

                else if (Type == nt_pointer)
                {
                    CNodePtr* pPointer = (CNodePtr*)pNode;
                    t.Format( _T( "\t%s* %s; //0x%0.4X %s\r\n" ), pPointer->GetClass( )->GetName( ), pPointer->GetName( ), pPointer->GetOffset( ), pPointer->GetComment( ) );
                    var.push_back( t );
                }

                else if (Type == nt_instance)
                {
                    CNodeClassInstance* pClassInstance = (CNodeClassInstance*)pNode;
                    if (pClassInstance->GetOffset( ) == 0)
                    {
                        t.Format( _T( " : public %s" ), pClassInstance->GetClass( )->GetName( ) ); // Inheritance
                        ClassName += t;
                    }
                    else
                    {
                        t.Format( _T( "\t%s %s; //0x%0.4X %s\r\n" ), pClassInstance->GetClass( )->GetName( ), pClassInstance->GetName( ), pClassInstance->GetOffset( ), pClassInstance->GetComment( ) );
                        var.push_back( t );
                    }
                }

                else if (Type == nt_array)
                {
                    CNodeArray* pArray = (CNodeArray*)pNode;
                    t.Format( _T( "\t%s %s[%i]; //0x%0.4X %s\r\n" ), pArray->GetClass( )->GetName( ), pArray->GetName( ), pArray->GetTotal( ), pArray->GetOffset( ), pArray->GetComment( ) );
                    var.push_back( t );
                }

                else if (Type == nt_ptrarray)
                {
                    CNodePtrArray* pArray = (CNodePtrArray*)pNode;
                    t.Format( _T( "\t%s* %s[%i]; //0x%0.4X %s\r\n" ), pArray->GetClass( )->GetName( ), pArray->GetName( ), pArray->Count( ), pArray->GetOffset( ), pArray->GetComment( ) );
                    var.push_back( t );
                }
            }
        }

        if (fill > 0)
        {
            if (g_bPrivatePadding)
            {
                t.Format( _T( "private:\r\n\t%s pad_0x%0.4X[0x%X]; //0x%0.4X\r\n" ), g_Typedefs.Hex, fillStart, fill, fillStart );
                // TODO: Maybe add public at the end for user impl of class inline functions?: public:\r\n
            }
            else
            {
                t.Format( _T( "\t%s pad_0x%0.4X[0x%X]; //0x%0.4X\r\n" ), g_Typedefs.Hex, fillStart, fill, fillStart );
            }

            var.push_back( t );
        }

        t.Format( _T( "%s\r\n{\r\npublic:\r\n" ), ClassName );
        strGeneratedText += t;

        for (size_t i = 0; i < vfun.size( ); i++)
            strGeneratedText += vfun[i];

        if (vfun.size( ) > 0)
            strGeneratedText += _T( "\r\n" );

        for (size_t i = 0; i < var.size( ); i++)
            strGeneratedText += var[i];

        if (var.size( ) > 0)
            strGeneratedText += _T( "\r\n" );

        if (pClass->m_Code.GetLength( ) > 0)
        {
            strGeneratedText += pClass->m_Code;
            strGeneratedText += _T( "\r\n" );
        }

        t.Format( _T( "}; //Size=0x%0.4X\r\n\r\n" ), pClass->GetMemorySize( ) );
        strGeneratedText += t;
    }

    if (!m_strFooter.IsEmpty( ))
    {
        strGeneratedText += (m_strFooter + _T( "\r\n" ));
    }

    if (g_bClipboardCopy)
    {
        int stringSize = 0;
        HGLOBAL hMemBlob = NULL;

        ::OpenClipboard( NULL );
        ::EmptyClipboard( );

        stringSize = strGeneratedText.GetLength( ) * sizeof( CString::StrTraits::XCHAR );
        hMemBlob = ::GlobalAlloc( GMEM_FIXED, stringSize );
        memcpy( hMemBlob, strGeneratedText.GetBuffer( ), stringSize );

        #ifdef _UNICODE
        ::SetClipboardData( CF_UNICODETEXT, hMemBlob );
        #else
        ::SetClipboardData( CF_TEXT, hMemBlob );
        #endif

        ::CloseClipboard( );
        
        GetMainWnd( )->MessageBox( _T( "Copied generated code to clipboard" ), _T( "RC" ), MB_OK | MB_ICONINFORMATION );
    }
    else
    {
        CDialogEdit dlg;
        dlg.Title = _T( "Class Code Generated" );
        dlg.Text = strGeneratedText;
        dlg.DoModal( );
    }
}

void CReClassExApp::OnButtonPlugins( )
{
    CDialogPlugins plugin_dlg;
    plugin_dlg.DoModal( );
}

void CReClassExApp::OnUpdateButtonPlugins( CCmdUI * pCmdUI )
{
    pCmdUI->Enable( !g_LoadedPlugins.empty( ) );
}

void CReClassExApp::OnOpenPdb( )
{
    CString strConcatProcessName = g_ProcessName;
    if (strConcatProcessName.ReverseFind( '.' ) != -1)
        strConcatProcessName.Truncate( strConcatProcessName.ReverseFind( '.' ) );

    CFileDialog fileDlg( TRUE, _T( "pdb" ), strConcatProcessName, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, _T( "PDB (*.pdb)|*.pdb|All Files (*.*)|*.*||" ), NULL );
    if (fileDlg.DoModal( ) != IDOK)
        return;

    m_pSymbolLoader->LoadSymbolsForPdb( fileDlg.GetPathName( ) );
}

void CReClassExApp::OnUpdateOpenPdb( CCmdUI *pCmdUI )
{
    pCmdUI->Enable( (g_ProcessID != NULL) );
}

void CReClassExApp::DeleteClass( CNodeClass* pClass )
{
    PrintOutDbg( _T( "DeleteClass(\"%s\") called" ), pClass->GetName( ).GetString( ) );

    if (pClass->m_pChildClassFrame != NULL && ::IsWindow(pClass->m_pChildClassFrame->GetSafeHwnd()))
    {
        pClass->m_pChildClassFrame->SendMessage( WM_CLOSE, 0, 0 );
        pClass->m_pChildClassFrame = NULL;
    }

    CNodeBase* pNode = IsNodeRef( pClass ); 
    if (pNode)
    {
        //PrintOutDbg( _T( "Class still has a reference in %s.%s" ), pNode->GetParent( )->GetName( ).GetString( ), pNode->GetName( ).GetString( ) );
        CString msg; 
        msg.Format( _T( "Class still has a reference in %s.%s" ), 
            pNode->GetParent( )->GetName( ).GetString( ), pNode->GetName( ).GetString( ) );
        GetMainWnd( )->MessageBox( msg );
        return;
    }

    for (size_t i = 0; i < m_Classes.size( ); i++)
    {
        if (m_Classes[i] == pClass)
        {
            m_Classes.erase( m_Classes.begin( ) + i );
            return;
        }
    }
}

CNodeBase* CReClassExApp::IsNodeRef( CNodeBase* pTestNode )
{
    for (size_t c = 0; c < m_Classes.size( ); c++)
    {
        for (size_t n = 0; n < m_Classes[c]->NodeCount( ); n++)
        {
            CNodeBase* pNode = m_Classes[c]->GetNode( n );
            if (!pNode)
                continue;

            NodeType nodeType = pNode->GetType( );
            if (nodeType == nt_instance)
            {
                if (static_cast<CNodeClassInstance*>(pNode)->GetClass( ) == pTestNode)
                    return pNode;
            }
            else if (nodeType == nt_pointer)
            {
                if (static_cast<CNodePtr*>(pNode)->GetClass( ) == pTestNode)
                    return pNode;
            }
            else if (nodeType == nt_array)
            {
                if (static_cast<CNodePtr*>(pNode)->GetClass( ) == pTestNode)
                    return pNode;
            }
        }
    }
    return NULL;
}

void CReClassExApp::OnButtonClean( )
{
    size_t i, j, Count;
    std::vector<CNodeClass*> ClassesToCheck;
    CMDIFrameWnd* pMainFrame;
    CMDIChildWndEx* pChildWnd;

    pMainFrame = STATIC_DOWNCAST( CMDIFrameWnd, m_pMainWnd );
    pChildWnd = STATIC_DOWNCAST( CMDIChildWndEx, pMainFrame->MDIGetActive( ) );
    while (pChildWnd)
    {
        pChildWnd->SendMessage( WM_CLOSE, 0, 0 );
        pChildWnd = STATIC_DOWNCAST( CMDIChildWndEx, pMainFrame->MDIGetActive( ) );
    }
    
    for (i = 0; i < m_Classes.size( ); i++)
    {
        if (IsNodeRef( m_Classes[i] ) == NULL)
            ClassesToCheck.push_back( m_Classes[i] );
    }

    for (i = 0, Count = 0; i < ClassesToCheck.size( ); i++)
    {
        CNodeClass* pClass = ClassesToCheck[i];
        bool bCanDelete = true;

        for (j = 0; j < pClass->NodeCount( ); j++)
        {
            CNodeBase* pNode = pClass->GetNode( j );
            NodeType Type = pNode->GetType( );

            if (Type == nt_hex64 || Type == nt_hex32 || Type == nt_hex16 || Type == nt_hex8)
                continue;

            bCanDelete = false;
            break;
        }

        if (bCanDelete)
        {
            Count++;
            DeleteClass( pClass );
        }
    }
    
    //PrintOutDbg( _T( "Unused Classes removed: %i" ), Count );
    CString msg; msg.Format( _T( "Unused Classes removed: %i" ), Count );
    MessageBox( GetMainWnd( )->GetSafeHwnd( ), msg.GetString( ), _T( "Cleaner" ), MB_OK );
}

void CReClassExApp::OnUpdateButtonClean( CCmdUI *pCmdUI )
{
    pCmdUI->Enable( (g_ReClassApp.m_Classes.size( ) > 0) );
}

