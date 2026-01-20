#pragma once
#include <afxwin.h>
#include "CMPCThemeScrollBarRenderer.h"
#include <imm.h>

class CMPCThemeEdit : public CEdit
    , public CMPCThemeScrollBarRenderer
{
public:
    DECLARE_DYNAMIC(CMPCThemeEdit)
    CMPCThemeEdit();
    virtual ~CMPCThemeEdit();
    void PreSubclassWindow();
    void setBuddy(CWnd* buddyWindow) { this->buddy = buddyWindow; };
    void setFileDialogChild(bool set) { isFileDialogChild = set; };
    void SetFixedWidthFont(CFont& f);
    bool IsScrollable();
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
protected:
    CWnd* buddy;
    CFont font;
    bool isFileDialogChild;
    void SetCompWindowPos(HIMC himc, UINT start);

    DECLARE_MESSAGE_MAP()

    afx_msg LRESULT ResizeSupport(WPARAM wParam, LPARAM lParam);
    afx_msg void OnNcPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
public:
    afx_msg LRESULT OnContextMenu(WPARAM wParam, LPARAM lParam);
    afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
};


