#pragma once

#include "CNodeBase.h"
#include <chrono>

class CNodeHex64 : public CNodeBase {
public:
    CNodeHex64();

    virtual void Update(const PHOTSPOT Spot);

    virtual ULONG GetMemorySize() { return sizeof(__int64); }

    virtual NODESIZE Draw(const PVIEWINFO View, int x, int y);

private:
    bool m_valueChanged = false;
    std::chrono::time_point<std::chrono::steady_clock> m_lastChangeTime;
    UCHAR m_lastValue[8] = { 0 };
};
