#pragma once

#ifndef __AFXWIN_H__
#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "Resource.h"       // main symbols

// Console
#include "DialogConsole.h"
// Symbols
#include "Symbols.h"
// Class dependency graph
#include "ClassDependencyGraph.h"

class CReClassExApp : public CWinAppEx {
public:
    CReClassExApp( );

// App Globals
    CString m_strHeader;
    CString m_strFooter;
    CString m_strNotes;
    CString m_strParserInput;

    CString m_strCurrentFilePath;

    std::vector<CNodeClass*> m_Classes;
    //std::vector<CNodeBase*> m_Selected;

    CDialogConsole* m_pConsole;

    Symbols* m_pSymbolLoader;

// Overrides
    virtual BOOL InitInstance( );
    virtual int ExitInstance( );

// Implementation
    class CMainFrame* GetMainFrame( );
    class CMFCRibbonBar* GetRibbonBar( );
    class CStatusBar* GetStatusBar( );

    CNodeBase* CreateNewNode( NodeType Type );
    bool IsNodeValid( CNodeBase* pCheckNode );
    CNodeBase* IsNodeRef( CNodeBase* pTestNode );
    void DeleteClass( CNodeClass* pClass );

    void CalcOffsets( CNodeClass* pClass );
    void CalcAllOffsets( );
    void ClearSelection( );
    void ClearHidden( );

    void SaveXML( TCHAR* FileName );

    HMENU  m_hMdiMenu;
    HACCEL m_hMdiAccel;

    UINT  m_nAppLook;
    BOOL  m_bHiColorIcons;

    void ResizeMemoryFont( int font_width, int font_height );

    virtual void PreLoadState( );
    virtual void LoadCustomState( );
    virtual void SaveCustomState( );

    DECLARE_MESSAGE_MAP( )

    afx_msg void OnButtonReset( );
    afx_msg void OnFileNew( );
    afx_msg void OnFileOpen( );
    afx_msg void OnFileSave( );
    afx_msg void OnFileSaveAs( );
    afx_msg void OnFileImport( );
    afx_msg void OnButtonNewClass( );
    afx_msg void OnButtonNotes( );
    afx_msg void OnButtonConsole( );
    afx_msg void OnButtonParser( );
    afx_msg void OnButtonHeader( );
    afx_msg void OnButtonFooter( );
    afx_msg void OnButtonGenerate( );

    afx_msg void OnButtonPause( );
    afx_msg void OnUpdateButtonPause( CCmdUI *pCmdUI );
    afx_msg void OnButtonResume( );
    afx_msg void OnUpdateButtonResume( CCmdUI *pCmdUI );
    afx_msg void OnButtonKill( );
    afx_msg void OnUpdateButtonKill( CCmdUI *pCmdUI );
    afx_msg void OnButtonSearch( );
    afx_msg void OnUpdateButtonSearch( CCmdUI *pCmdUI );
    afx_msg void OnButtonClean( );
    afx_msg void OnUpdateButtonClean( CCmdUI *pCmdUI );
    afx_msg void OnButtonModules( );
    afx_msg void OnUpdateButtonModules( CCmdUI *pCmdUI );
    afx_msg void OnButtonPlugins( );
    afx_msg void OnUpdateButtonPlugins( CCmdUI *pCmdUI );
    afx_msg void OnOpenPdb( );
    afx_msg void OnUpdateOpenPdb( CCmdUI *pCmdUI );
};
