#pragma once
#include "CMPCThemeUtil.h"
#include "ResizableLib/ResizablePage.h"
#include <vector>
class CMPCThemeResizablePropertyPage : public CResizablePage, public CMPCThemeUtil
{
public:
    CMPCThemeResizablePropertyPage() {}; //for dynamic creation
    CMPCThemeResizablePropertyPage(UINT nIDTemplate, UINT nIDCaption);
    virtual ~CMPCThemeResizablePropertyPage();


    void fulfillThemeReqs() { CMPCThemeUtil::fulfillThemeReqs((CWnd*)this); };
    DECLARE_DYNCREATE(CMPCThemeResizablePropertyPage)

    // Used by the sheet on a DPI change: anchors hold pixel margins, so they are dropped while the controls are
    // scaled, then added again from the record at the new positions. The minimum size, which the sheet takes
    // into its own, is scaled before the sheet is resized.
    void RemoveAnchorsForDpiChange(UINT dpi);
    void RestoreAnchorsAfterDpiChange();

    DECLARE_MESSAGE_MAP()
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
protected:
    virtual BOOL OnInitDialog();

    // These hide CResizableLayout::AddAnchor so every anchor is recorded
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
    CSize m_minTrackSize;
    UINT m_minTrackDpi = 0;
};

