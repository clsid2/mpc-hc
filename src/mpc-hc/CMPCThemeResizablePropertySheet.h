#pragma once
#include "CMPCThemeUtil.h"
#include "ResizableLib/ResizableSheet.h"
#include <vector>

class CMPCThemeResizablePropertySheet : public CResizableSheet, public CMPCThemeUtil
{
public:
    DECLARE_DYNAMIC(CMPCThemeResizablePropertySheet)
    CMPCThemeResizablePropertySheet(UINT nIDCaption, CWnd* pParentWnd = nullptr, UINT iSelectPage = 0);
    CMPCThemeResizablePropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd = nullptr, UINT iSelectPage = 0);
    virtual ~CMPCThemeResizablePropertySheet();

    virtual BOOL OnInitDialog();
    void fulfillThemeReqs();
    virtual INT_PTR DoModal(); //override to handle RTL without using SetWindowLongPtr

    bool isModal;
    DECLARE_MESSAGE_MAP()
public:
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg BOOL OnNcCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
    afx_msg void OnDestroy();

protected:
    // These hide CResizableLayout::AddAnchor so the anchors a derived sheet adds can be added again after a DPI
    // change; those of CResizableSheet::PresetLayout come back from PresetLayout itself
    void AddAnchor(HWND hWnd, ANCHOR anchorTopLeft, ANCHOR anchorBottomRight);
    void AddAnchor(HWND hWnd, ANCHOR anchorTopLeft) {
        AddAnchor(hWnd, anchorTopLeft, anchorTopLeft);
    }
    void AddAnchor(UINT nID, ANCHOR anchorTopLeft, ANCHOR anchorBottomRight) {
        AddAnchor(::GetDlgItem(m_hWnd, nID), anchorTopLeft, anchorBottomRight);
    }
    void AddAnchor(UINT nID, ANCHOR anchorTopLeft) {
        AddAnchor(nID, anchorTopLeft, anchorTopLeft);
    }

    struct AnchorInfo {
        HWND hWnd;
        ANCHOR topLeft;
        ANCHOR bottomRight;
    };
    std::vector<AnchorInfo> m_anchors;
    UINT m_dpi = 0;
    UINT m_minTrackDpi = 0;
    CSize m_minTrackSize;
};

