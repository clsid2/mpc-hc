#include "stdafx.h"

#pragma once

struct MemoryDCBuffer {
    CDC memDC;
    CBitmap memBitmap;
    CSize memSize;
    bool bValid = false;

    MemoryDCBuffer() : memSize(0, 0) {}

    ~MemoryDCBuffer() {
        if (memBitmap.GetSafeHandle()) {
            memDC.SelectObject((CBitmap*)NULL);
            memBitmap.DeleteObject();
        }
        // memDC will be automatically cleaned up by CDC destructor
    }

    void EnsureInitialized(CDC* sourceDC, const CSize& requiredSize, CFont* font = nullptr) {
        // Initialize DC if needed
        if (memDC.GetSafeHdc() == NULL) {
            memDC.CreateCompatibleDC(sourceDC);
        }

        // Resize if needed
        if (requiredSize.cx > memSize.cx || requiredSize.cy > memSize.cy) {
            if (memBitmap.GetSafeHandle()) {
                memDC.SelectObject((CBitmap*)NULL);
                memBitmap.DeleteObject();
            }

            CSize newSize(std::max(requiredSize.cx, memSize.cx) + 50,
                std::max(requiredSize.cy, memSize.cy) + 50);

            memBitmap.CreateCompatibleBitmap(sourceDC, newSize.cx, newSize.cy);
            memDC.SelectObject(&memBitmap);
            memSize = newSize;
            bValid = false;
        }

        // Configure DC
        if (sourceDC) {
            memDC.SetWindowOrg(0, 0);
            memDC.SetViewportOrg(0, 0);
            memDC.SetBkColor(sourceDC->GetBkColor());
            memDC.SetTextColor(sourceDC->GetTextColor());
            memDC.SetBkMode(sourceDC->GetBkMode());
            if (font) {
                memDC.SelectObject(font);
            }
        }
    }
};
