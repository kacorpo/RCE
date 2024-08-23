#include "stdafx.h"
#include "CNodeHex64.h"

CNodeHex64::CNodeHex64()
{
    m_nodeType = nt_hex64;
    m_lastChangeTime = std::chrono::steady_clock::now();
    memset(m_lastValue, 0, sizeof(m_lastValue));
}

void CNodeHex64::Update(const PHOTSPOT Spot)
{
    StandardUpdate(Spot);
    unsigned char v = (unsigned char)(_tcstoul(Spot->Text.GetString(), NULL, 16) & 0xFF);
    if (Spot->Id >= 0 && Spot->Id < 8)
        ReClassWriteMemory((LPVOID)(Spot->Address + Spot->Id), &v, 1);

    m_valueChanged = true;
    m_lastChangeTime = std::chrono::steady_clock::now();
}

NODESIZE CNodeHex64::Draw(const PVIEWINFO View, int x, int y)
{
    int tx;
    NODESIZE DrawSize;
    const UCHAR* Data;

    if (m_bHidden)
        return DrawHidden(View, x, y);

    Data = (const UCHAR*)(View->Data + m_Offset);
    AddSelection(View, 0, y, g_FontHeight);
    AddDelete(View, x, y);
    AddTypeDrop(View, x, y);

    tx = x + TXOFFSET + 16;
    tx = AddAddressOffset(View, tx, y);

    if (g_bText)
    {
        CStringA AsciiMemory = GetStringFromMemoryA((const char*)Data, 8) + " ";
        tx = AddText(View, tx, y, g_clrChar, HS_NONE, "%s", AsciiMemory.GetBuffer());
    }

    // Check if value has changed and adjust the color
    for (int i = 0; i < 8; i++)
    {
        COLORREF color = g_clrHex;
        if (Data[i] != m_lastValue[i])
        {
            m_valueChanged = true;
            m_lastValue[i] = Data[i];
            m_lastChangeTime = std::chrono::steady_clock::now();
            color = RGB(255, 0, 0); // Red color for changed values
        }

        // Revert color back to original after 2 seconds
        if (m_valueChanged && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - m_lastChangeTime).count() > 2)
        {
            m_valueChanged = false;
            color = g_clrHex;
        }

        tx = AddText(View, tx, y, color, i, _T("%0.2X"), Data[i]) + g_FontWidth;
    }

    tx = AddComment(View, tx, y);

    DrawSize.x = tx;
    DrawSize.y = y + g_FontHeight;
    return DrawSize;
}
