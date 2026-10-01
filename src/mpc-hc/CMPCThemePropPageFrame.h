#pragma once
#include "TreePropSheet/PropPageFrameDefault.h"
class CMPCThemePropPageFrame : public TreePropSheet::CPropPageFrameDefault
{
public:
    CMPCThemePropPageFrame();
    virtual ~CMPCThemePropPageFrame();
    virtual BOOL Create(DWORD dwWindowStyle, const RECT& rect, CWnd* pwndParent, UINT nID);
    virtual CWnd* GetWnd();

    virtual void DrawCaption(CDC* pDc, CRect rect, LPCTSTR lpszCaption, HICON hIcon);
    static void resetBrush();

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
protected:
    static CBrush mpcThemeBorderBrush;
};

