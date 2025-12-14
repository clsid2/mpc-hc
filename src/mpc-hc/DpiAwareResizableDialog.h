#pragma once
#include "ResizableLib/ResizableDialog.h"

struct TrackSizeConstraint {
    double xMultiplier = 1.0;
    double yMultiplier = 1.0;
    bool enabled = true;
};

struct TrackSizeConstraints {
    TrackSizeConstraint min{1.0, 1.0, true};
    TrackSizeConstraint max{1.0, 1.0, false};
};

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

    // Override in derived classes to customize resize constraints
    virtual TrackSizeConstraints GetTrackSizeConstraints() const { return TrackSizeConstraints(); }

    // Window state persistence
    void EnableSaveRestoreKey(LPCTSTR pszKey, BOOL bRectOnly = FALSE);

    // Static image/icon management
    void LoadStaticIcon(int controlID, LPCTSTR iconResourceID, bool isSystemIcon = false);
    void RefreshStaticImages();

protected:
    // DLU <-> Pixel conversion helpers
    static inline CSize DluToPixels(const CSize& dluSize, int avgWidth, int avgHeight) {
        return CSize(MulDiv(dluSize.cx, avgWidth, 4), MulDiv(dluSize.cy, avgHeight, 8));
    }
    static inline CSize PixelsToDlu(const CSize& pixelSize, int avgWidth, int avgHeight) {
        return CSize(MulDiv(pixelSize.cx, 4, avgWidth), MulDiv(pixelSize.cy, 8, avgHeight));
    }

    // Dialog sizing and positioning
    bool GetDialogBaseUnits(CSize& baseSize);
    bool GetDialogFontInfo(CString& fontFace, int& fontSize);
    void UpdateMinMaxTrackSizeForDPI();
    void MapDialogRectAuto(CRect& r);
    void RepositionControlsFromTemplate();

    // Size grip management
    void UpdateSizeGripDPI();

    // DPI change helpers
    void AdjustControlPositionsForResize(const CSize& templateDluSize, int deltaWidth, int deltaHeight);
    void BuildControlTemplateDLUMap(std::map<int, CRect>& controlTemplateDLUs);
    CSize GetTargetDluSize();

    // DPI restoration helper
    UINT GetTargetDpiForRestoration();

    // DLU size persistence
    void SaveWindowDLUSize();
    void LoadWindowDLUSize();

    // Common sizing and DPI application
    void ApplyDialogSizeAndDpi(UINT targetDpi, CSize targetDluSize);

    // Template utilities
    static void GetItemDimensions(const DLGTEMPLATE* pTemplate, DLGITEMTEMPLATE* pItem, CRect& rect, DWORD& id);
    const DLGTEMPLATE* LoadDialogTemplate() const;

    // Static image/icon tracking for DPI refresh
    struct StaticImageInfo {
        int controlID;
        LPCTSTR resourceID;
        bool isSystemIcon;  // true for IDI_*, false for app resources
    };
    std::vector<StaticImageInfo> m_staticImages;

    // State tracking
    UINT m_currentDpi;
    bool m_inDpiChange;
    mutable const DLGTEMPLATE* m_cachedTemplate;
    bool m_bGripVisible;
    bool m_bSaveRestoreEnabled;
    bool m_bRestorationPending;
    CSize m_currentDluSize;     // Current window size in DLU (always up-to-date, persisted to registry)
    CFont m_dialogFont;

/* code ported from CmdUI.h to support official CResizableDialog \/  */
    virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
public:
    afx_msg void OnKickIdle();
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
/* code ported from CmdUI.h to support official CResizableDialog /\ */

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
    afx_msg void OnDestroy();
};
