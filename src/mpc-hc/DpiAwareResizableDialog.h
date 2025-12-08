#pragma once
#include "ResizableLib/ResizableDialog.h"

class CDpiAwareResizableDialog : public CResizableDialog, public _DialogSplitHelper
{
public:
    CDpiAwareResizableDialog();
    CDpiAwareResizableDialog(UINT nIDTemplate, CWnd* pParent = nullptr);
    CDpiAwareResizableDialog(LPCTSTR lpszTemplateName, CWnd* pParent = nullptr);
    virtual ~CDpiAwareResizableDialog();

    virtual BOOL OnInitDialog();

    // Override in derived classes to return the dialog template ID
    virtual UINT GetDialogTemplateID() const { return 0; }

    // Override in derived classes to setup control anchors
    virtual void SetupAnchors() {}

protected:
    // Dialog sizing and positioning
    bool GetDialogBaseUnits(CSize& baseSize);
    bool GetDialogFontInfo(CString& fontFace, int& fontSize);
    bool CalculateDialogSize(CSize& clientSize);
    void UpdateMinMaxTrackSizeForDPI();
    void MapDialogRectAuto(CRect& r);
    void RepositionControlsFromTemplate();

    // Size grip management
    void UpdateSizeGripDPI();

    // DPI change helpers
    void CalculateCurrentDluRatio(const CRect& clientRect, const CSize& templateDluSize, LPCTSTR fontFace, int fontSize, double& widthRatio, double& heightRatio);
    bool CalculateCustomDpiRect(const CSize& templateDluSize, double widthRatio, double heightRatio, LPCTSTR fontFace, int fontSize, const RECT* suggestedRect, RECT& customRect);
    void AdjustControlPositionsForResize(const CSize& templateDluSize, int deltaWidth, int deltaHeight);
    void BuildControlTemplateDLUMap(std::map<int, CRect>& controlTemplateDLUs);

    // Template utilities
    static void GetItemDimensions(const DLGTEMPLATE* pTemplate, DLGITEMTEMPLATE* pItem, CRect& rect, DWORD& id);
    const DLGTEMPLATE* LoadDialogTemplate() const;

    // State tracking
    UINT m_currentDpi;
    bool m_inDpiChange;
    mutable const DLGTEMPLATE* m_cachedTemplate;
    bool m_bGripVisible;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
};
