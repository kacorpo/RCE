#include "stdafx.h"
#include "CNodePtr.h"

CNodePtr::CNodePtr( ) 
    : m_pClassNode( NULL )
{
    m_nodeType = nt_pointer;
}

void CNodePtr::Update( const PHOTSPOT Spot )
{
    StandardUpdate( Spot );
}

NODESIZE CNodePtr::Draw(const PVIEWINFO View, int x, int y)
{
    NODESIZE DrawSize;
    NODESIZE ChildDrawSize;
    ULONG_PTR* Data;
    int tx;

    if (m_bHidden)
        return DrawHidden(View, x, y);

    Data = (ULONG_PTR*)(View->Data + m_Offset);

    AddSelection(View, 0, y, g_FontHeight);
    AddDelete(View, x, y);
    AddTypeDrop(View, x, y);

    x = AddOpenClose(View, x, y);
    x = AddIcon(View, x, y, ICON_POINTER, -1, -1);

    tx = AddAddressOffset(View, x, y);
    tx = AddText(View, tx, y, g_clrType, HS_NONE, _T("Ptr "));
    tx = AddText(View, tx, y, g_clrName, HS_NAME, _T("%s"), m_strName);
    tx = AddText(View, tx, y, g_clrValue, HS_NONE, _T(" <%s>"), m_pClassNode->GetName());
    tx = AddIcon(View, tx, y, ICON_CHANGE, HS_CLICK, HS_CHANGE_A);

    tx += g_FontWidth;

    // *** Use the same format as provided in your example ***
    ULONG_PTR uintVal = *Data; // Get the pointer value
    CString strAddress(GetAddressName(uintVal, FALSE));

    if (strAddress.GetLength() > 0)
    {
        if (g_bPointers)
        {
            if (uintVal > 0x6FFFFFFF && uintVal < 0x7FFFFFFFFFFF)
            {
                tx = AddText(View, tx, y, g_clrOffset, HS_EDIT, _T("-> %s "), strAddress.GetString());

                if (g_bRTTI)
                    tx = ResolveRTTI(uintVal, tx, View, y);

                // Print out info from PDB at address
                if (g_bSymbolResolution)
                {
                    ULONG_PTR ModuleAddress = GetModuleBaseFromAddress(uintVal);
                    SymbolReader* pSymbols = nullptr;

                    if (ModuleAddress != 0)
                    {
                        pSymbols = g_ReClassApp.m_pSymbolLoader->GetSymbolsForModuleAddress(ModuleAddress);
                        if (!pSymbols)
                        {
                            CString moduleName = GetModuleName(uintVal);
                            if (!moduleName.IsEmpty())
                                pSymbols = g_ReClassApp.m_pSymbolLoader->GetSymbolsForModuleName(moduleName);
                        }

                        if (pSymbols != nullptr)
                        {
                            CString SymbolOut;
                            SymbolOut.Preallocate(1024);
                            if (pSymbols->GetSymbolStringFromVA(uintVal, SymbolOut))
                            {
                                tx = AddText(View, tx, y, g_clrOffset, HS_EDIT, _T("%s "), SymbolOut.GetString());
                            }
                        }
                    }
                }
            }
        }

        if (g_bString)
        {
            bool bAddStr = true;
            char txt[64];
            ReClassReadMemory((LPVOID)uintVal, txt, 64);

            for (int i = 0; i < 8; i++)
            {
                if (!(txt[i] > 0x1F && txt[i] < 0xFF && txt[i] != 0x7F))
                    bAddStr = false;
            }

            if (bAddStr)
            {
                txt[63] = '\0';
                tx = AddText(View, tx, y, g_clrChar, HS_NONE, _T("'%hs'"), txt);
            }
        }
    }

    tx = AddComment(View, tx, y);

    y += g_FontHeight;
    DrawSize.x = tx;

    if (m_LevelsOpen[View->Level])
    {
        DWORD NeededSize = m_pClassNode->GetMemorySize();
        m_Memory.SetSize(NeededSize);

        VIEWINFO NewView;
        memcpy(&NewView, View, sizeof(NewView));
        NewView.Data = m_Memory.Data();
        NewView.Address = *Data;

        ReClassReadMemory((LPVOID)NewView.Address, (LPVOID)NewView.Data, NeededSize);

        ChildDrawSize = m_pClassNode->Draw(&NewView, x, y);

        y = ChildDrawSize.y;
        if (ChildDrawSize.x > DrawSize.x)
            DrawSize.x = ChildDrawSize.x;
    }

    DrawSize.y = y;
    return DrawSize;
}