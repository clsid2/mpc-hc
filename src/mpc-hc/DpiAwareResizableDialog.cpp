#include "stdafx.h"
#include "DpiAwareResizableDialog.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"
#include "CMPCThemePlayerListCtrl.h"

// ResizableLib constants (copied from ResizableWndState.cpp:42-43)
#define PLACEMENT_ENT _T("WindowPlacement")
#define PLACEMENT_FMT _T("%ld,%ld,%ld,%ld,%u,%u,%ld,%ld")

// DLU size persistence constants
#define DLUSIZE_ENT _T("DLUSize")
#define DLUSIZE_FMT _T("%d,%d")

// Forward declarations for MFC internal functions
AFX_STATIC DLGITEMTEMPLATE* AFXAPI _AfxFindFirstDlgItem(const DLGTEMPLATE* pTemplate);
AFX_STATIC DLGITEMTEMPLATE* AFXAPI _AfxFindNextDlgItem(DLGITEMTEMPLATE* pItem, BOOL bDialogEx);

CDpiAwareResizableDialog::CDpiAwareResizableDialog()
    : m_currentDpi(96), m_inDpiChange(false), m_bMaximized(false), m_bSaveRestoreEnabled(false), m_bRestorationPending(false), m_currentDluSize(0, 0)
{
}

CDpiAwareResizableDialog::CDpiAwareResizableDialog(UINT nIDTemplate, CWnd* pParent) : CResizableDialog(nIDTemplate, pParent), m_currentDpi(96), m_inDpiChange(false), m_bMaximized(false), m_bSaveRestoreEnabled(false), m_bRestorationPending(false), m_currentDluSize(0, 0)
{
}

CDpiAwareResizableDialog::CDpiAwareResizableDialog(LPCTSTR lpszTemplateName, CWnd* pParent) : CResizableDialog(lpszTemplateName, pParent), m_currentDpi(96), m_inDpiChange(false), m_bMaximized(false), m_bSaveRestoreEnabled(false), m_bRestorationPending(false), m_currentDluSize(0, 0)
{
}

CDpiAwareResizableDialog::~CDpiAwareResizableDialog()
{
}

void CDpiAwareResizableDialog::EnableSaveRestoreKey(LPCTSTR pszKey, BOOL bRectOnly)
{
    m_bSaveRestoreEnabled = true;
    SetStateStore(pszKey);
    EnableSaveRestore(L"", bRectOnly);
}

UINT CDpiAwareResizableDialog::GetTargetDpiForRestoration()
{
    UINT currentDpi = DpiHelper::GetDPIForWindow(m_hWnd);
    m_bRestorationPending = false;

    if (!m_bSaveRestoreEnabled) {
        return currentDpi;
    }

    LoadWindowDLUSize();

    CString data;
    if (!ReadState(CString(PLACEMENT_ENT), data)) {
        return currentDpi;
    }

    RECT rc;
    UINT showCmd, flags;
    POINT ptMin;
    if (_stscanf_s(data, PLACEMENT_FMT, &rc.left, &rc.top, &rc.right, &rc.bottom, &showCmd, &flags, &ptMin.x, &ptMin.y) == 8) {
        UINT targetDpi = DpiHelper::GetDPIForRect(&rc);
        if (targetDpi > 0 && targetDpi != currentDpi) {
            // WM_DPICHANGED will handle correction after ResizableLib restores position
            m_bRestorationPending = true;
        }
    }

    return currentDpi;
}

void CDpiAwareResizableDialog::SaveWindowDLUSize()
{
    if (!m_bSaveRestoreEnabled) {
        return;
    }

    if (m_currentDluSize.cx > 0 && m_currentDluSize.cy > 0) {
        CString data;
        data.Format(DLUSIZE_FMT, m_currentDluSize.cx, m_currentDluSize.cy);
        WriteState(CString(DLUSIZE_ENT), data);
    }
}

void CDpiAwareResizableDialog::LoadWindowDLUSize()
{
    if (!m_bSaveRestoreEnabled) {
        return;
    }

    CString data;
    if (!ReadState(CString(DLUSIZE_ENT), data)) {
        return;
    }

    int dluWidth = 0, dluHeight = 0;
    if (_stscanf_s(data, DLUSIZE_FMT, &dluWidth, &dluHeight) == 2) {
        m_currentDluSize.cx = dluWidth;
        m_currentDluSize.cy = dluHeight;
    }
}

CSize CDpiAwareResizableDialog::GetTargetDluSize()
{
    CSize targetDluSize;
    if (m_currentDluSize.cx > 0 && m_currentDluSize.cy > 0) {
        targetDluSize = m_currentDluSize;
    } else {
        GetDialogBaseUnits(targetDluSize);
    }
    return targetDluSize;
}

void CDpiAwareResizableDialog::ApplyDialogSizeAndDpi(UINT targetDpi, CSize targetDluSize)
{
    CString fontFace;
    int fontSize;
    if (!GetDialogFontInfo(fontFace, fontSize)) {
        return;
    }

    if (m_dialogFont.m_hObject) {
        m_dialogFont.DeleteObject();
    }
    if (!CMPCThemeUtil::getFontByFaceForDpi(m_dialogFont, fontFace, fontSize, targetDpi)) {
        return;
    }

    SetFont(&m_dialogFont, FALSE);
    CWnd* pChild = GetWindow(GW_CHILD);
    while (pChild) {
        pChild->SetFont(&m_dialogFont, FALSE);
        pChild = pChild->GetWindow(GW_HWNDNEXT);
    }

    int avgWidth, avgHeight;
    if (!DpiHelper::GetDialogFontMetricsForDPI(targetDpi, fontFace, fontSize, avgWidth, avgHeight)) {
        return;
    }

    CSize targetClientSize = DluToPixels(targetDluSize, avgWidth, avgHeight);

    // Check if window is already the right size (might have been set in OnDpiChanged)
    CRect currentWindowRect;
    GetWindowRect(&currentWindowRect);

    CRect targetRect(0, 0, targetClientSize.cx, targetClientSize.cy);
    DpiHelper::AdjustWindowRectExForDpi(&targetRect, GetStyle(), FALSE, GetExStyle(), targetDpi);

    if (currentWindowRect.Width() != targetRect.Width() || currentWindowRect.Height() != targetRect.Height()) {
        SetWindowPos(NULL, 0, 0, targetRect.Width(), targetRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
    }

    CRect actualClientRect;
    GetClientRect(&actualClientRect);

    RepositionControlsFromTemplate();

    CSize templateDluSize;
    if (GetDialogBaseUnits(templateDluSize)) {
        CSize templatePixelSize = DluToPixels(templateDluSize, avgWidth, avgHeight);

        // Calculate resize delta from template size
        int deltaWidth = actualClientRect.Width() - templatePixelSize.cx;
        int deltaHeight = actualClientRect.Height() - templatePixelSize.cy;

        m_currentDluSize = targetDluSize;

        if (deltaWidth != 0 || deltaHeight != 0) {
            AdjustControlPositionsForResize(templateDluSize, deltaWidth, deltaHeight);
        }
    } else {
        m_currentDluSize = targetDluSize;
    }

    UpdateSizeGripDPI();
}

BOOL CDpiAwareResizableDialog::OnInitDialog() {
    BOOL ret = __super::OnInitDialog();

    m_currentDpi = GetTargetDpiForRestoration();
    UpdateMinMaxTrackSizeForDPI();
    ApplyDialogSizeAndDpi(m_currentDpi, GetTargetDluSize());

    // Refresh grip (initialized with primary monitor DPI, not current monitor)
    CWnd* pGrip = GetSizeGripWnd();
    if (pGrip && ::IsWindow(pGrip->GetSafeHwnd())) {
        pGrip->SendMessage(WM_SETTINGCHANGE, 0, 0);
    }

    return ret;
}

BEGIN_MESSAGE_MAP(CDpiAwareResizableDialog, CResizableDialog)
    ON_WM_SIZE()
    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
    ON_MESSAGE_VOID(WM_KICKIDLE, OnKickIdle)
    ON_WM_INITMENUPOPUP()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

LRESULT CDpiAwareResizableDialog::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT ret = __super::DefWindowProc(message, wParam, lParam);

    if (message == WM_INITDIALOG) {
        SendMessage(WM_KICKIDLE);
    }

    return ret;
}

void CDpiAwareResizableDialog::OnKickIdle()
{
    UpdateDialogControls(this, false);
}

void CDpiAwareResizableDialog::OnInitMenuPopup(CMenu* pPopupMenu, UINT /*nIndex*/, BOOL /*bSysMenu*/)
{
    ASSERT(pPopupMenu != nullptr);

    CCmdUI state;
    state.m_pMenu = pPopupMenu;
    ASSERT(state.m_pOther == nullptr);
    ASSERT(state.m_pParentMenu == nullptr);

    if (AfxGetThreadState()->m_hTrackingMenu == pPopupMenu->m_hMenu) {
        state.m_pParentMenu = pPopupMenu;
    } else if (::GetMenu(m_hWnd) != nullptr) {
        HMENU hParentMenu;
        CWnd* pParent = this;
        if (pParent != nullptr &&
            (hParentMenu = ::GetMenu(pParent->m_hWnd)) != nullptr) {
            int nIndexMax = ::GetMenuItemCount(hParentMenu);
            for (int nIndex = 0; nIndex < nIndexMax; nIndex++) {
                if (::GetSubMenu(hParentMenu, nIndex) == pPopupMenu->m_hMenu) {
                    state.m_pParentMenu = CMenu::FromHandle(hParentMenu);
                    break;
                }
            }
        }
    }

    state.m_nIndexMax = pPopupMenu->GetMenuItemCount();
    for (state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax; state.m_nIndex++) {
        state.m_nID = pPopupMenu->GetMenuItemID(state.m_nIndex);
        if (state.m_nID == 0) {
            continue;
        }

        ASSERT(state.m_pOther == nullptr);
        ASSERT(state.m_pMenu != nullptr);
        if (state.m_nID == UINT(-1)) {
            state.m_pSubMenu = pPopupMenu->GetSubMenu(state.m_nIndex);
            if (state.m_pSubMenu == nullptr ||
                (state.m_nID = state.m_pSubMenu->GetMenuItemID(0)) == 0 ||
                state.m_nID == UINT(-1)) {
                continue;
            }
            state.DoUpdate(this, TRUE);
        } else {
            state.m_pSubMenu = nullptr;
            state.DoUpdate(this, FALSE);
        }

        UINT nCount = pPopupMenu->GetMenuItemCount();
        if (nCount < state.m_nIndexMax) {
            state.m_nIndex -= (state.m_nIndexMax - nCount);
            while (state.m_nIndex < nCount &&
                pPopupMenu->GetMenuItemID(state.m_nIndex) == state.m_nID) {
                state.m_nIndex++;
            }
        }
        state.m_nIndexMax = nCount;
    }
}

void CDpiAwareResizableDialog::UpdateSizeGripDPI()
{
    CWnd* pGrip = GetSizeGripWnd();
    if (!pGrip || !::IsWindow(pGrip->GetSafeHwnd())) {
        return;
    }

    CRect clientRect;
    GetClientRect(&clientRect);

    if (clientRect.Width() <= 0 || clientRect.Height() <= 0) {
        return;
    }

    // Don't update m_currentDpi here - only OnDpiChanged should update it
    // Updating here causes incorrect DLU calculations in OnSize when dragging between monitors
    UINT actualDpi = m_currentDpi;

    DpiHelper dpiHelper;
    dpiHelper.Override(m_currentDpi, m_currentDpi);
    int gripCx = dpiHelper.GetSystemMetricsDPI(SM_CXVSCROLL);
    int gripCy = dpiHelper.GetSystemMetricsDPI(SM_CYHSCROLL);

    int gripX = clientRect.right - gripCx;
    int gripY = clientRect.bottom - gripCy;

    pGrip->SetWindowPos(&CWnd::wndBottom, gripX, gripY, gripCx, gripCy, SWP_NOACTIVATE | SWP_NOREPOSITION | (!m_bMaximized && IsSizeGripVisible() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
}

//see CResizableDialog::OnSize -- we override and do not call parent because private implementation is not dpi aware
void CDpiAwareResizableDialog::OnSize(UINT nType, int cx, int cy) {
    if (m_inDpiChange) {
        return;
    }

    CDialog::OnSize(nType, cx, cy);

    if (nType == SIZE_MAXHIDE || nType == SIZE_MAXSHOW) {
        return;
    }

    if (nType == SIZE_MAXIMIZED) {
        m_bMaximized = true;
    } else {
        m_bMaximized = false;
    }

    UpdateSizeGripDPI();
    ArrangeLayout();

    // Update tracked DLU size when user resizes (skip during initialization)
    if (IsWindowVisible() && cx > 0 && cy > 0) {
        CString fontFace;
        int fontSize;
        if (GetDialogFontInfo(fontFace, fontSize)) {
            int avgWidth, avgHeight;
            if (DpiHelper::GetDialogFontMetricsForDPI(m_currentDpi, fontFace, fontSize, avgWidth, avgHeight)) {
                m_currentDluSize = PixelsToDlu(CSize(cx, cy), avgWidth, avgHeight);
            }
        }
    }
}

void CDpiAwareResizableDialog::OnDestroy()
{
    for (const auto& info : m_staticImages) {
        CWnd* pControl = GetDlgItem(info.controlID);
        if (pControl) {
            CStatic* pStatic = DYNAMIC_DOWNCAST(CStatic, pControl);
            if (pStatic) {
                HICON hIcon = pStatic->GetIcon();
                if (hIcon) {
                    DestroyIcon(hIcon);
                }
            }
        }
    }

    SaveWindowDLUSize();
    __super::OnDestroy();
}

LRESULT CDpiAwareResizableDialog::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    if (m_inDpiChange) {
        return 0;
    }

    if (!IsWindowVisible() && !m_bRestorationPending) {
        return 0;
    }

    UINT oldDPI = LOWORD(wParam);
    UINT newDPI = HIWORD(wParam);
    RECT* suggestedRect = (RECT*)lParam;

    UINT actualDPI = DpiHelper::GetDPIForWindow(m_hWnd);
    UINT realNewDPI = newDPI;

    if (oldDPI == newDPI) {
        if (actualDPI != m_currentDpi) {
            realNewDPI = actualDPI;
        } else {
            if (!m_bRestorationPending) {
                return 0;
            }
        }
    }

    if (realNewDPI == m_currentDpi && !m_bRestorationPending) {
        return 0;
    }

    // Set flag AFTER all early exit checks to prevent leaking it as true
    m_inDpiChange = true;
    m_bRestorationPending = false;

    // Get target size before updating m_currentDpi
    CSize targetDluSize = GetTargetDluSize();

    m_currentDpi = realNewDPI;

    UpdateMinMaxTrackSizeForDPI();
    RemoveAllAnchors();

    // On Windows 10/11, we MUST resize the window during WM_DPICHANGED handler
    // Calculate the target client size and window rect
    CString fontFace;
    int fontSize;
    if (GetDialogFontInfo(fontFace, fontSize)) {
        int avgWidth, avgHeight;
        if (DpiHelper::GetDialogFontMetricsForDPI(m_currentDpi, fontFace, fontSize, avgWidth, avgHeight)) {
            CSize targetClientSize = DluToPixels(targetDluSize, avgWidth, avgHeight);
            CRect targetRect(0, 0, targetClientSize.cx, targetClientSize.cy);
            DpiHelper::AdjustWindowRectExForDpi(&targetRect, GetStyle(), FALSE, GetExStyle(), m_currentDpi);

            // Use suggested position if available (required by Windows for proper DPI handling)
            int x = suggestedRect ? suggestedRect->left : 0;
            int y = suggestedRect ? suggestedRect->top : 0;
            UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | (suggestedRect ? 0 : SWP_NOMOVE);

            SetWindowPos(NULL, x, y, targetRect.Width(), targetRect.Height(), flags);
        }
    }

    ApplyDialogSizeAndDpi(m_currentDpi, targetDluSize);
    RefreshStaticImages();

    CWnd* pGrip = GetSizeGripWnd();
    if (pGrip && ::IsWindow(pGrip->GetSafeHwnd())) {
        pGrip->SendMessage(WM_SETTINGCHANGE, 0, 0);
    }

    SetupAnchors();

    // Update any CMPCThemePlayerListCtrl children
    CWnd* pChild = GetWindow(GW_CHILD);
    while (pChild) {
        CMPCThemePlayerListCtrl* pListCtrl = DYNAMIC_DOWNCAST(CMPCThemePlayerListCtrl, pChild);
        if (pListCtrl) {
            pListCtrl->DoDPIChanged();
        }
        pChild = pChild->GetWindow(GW_HWNDNEXT);
    }

    RedrawWindow();

    m_inDpiChange = false;

    return 0;
}

bool CDpiAwareResizableDialog::GetDialogBaseUnits(CSize& baseSize)
{
    const DLGTEMPLATE* pTemplate = LoadDialogTemplate();
    if (!pTemplate) {
        return false;
    }

    _DialogSizeHelper::GetSizeInDialogUnits(pTemplate, &baseSize);
    return true;
}

bool CDpiAwareResizableDialog::GetDialogFontInfo(CString& fontFace, int& fontSize)
{
    const DLGTEMPLATE* pTemplate = LoadDialogTemplate();
    if (!pTemplate) {
        return false;
    }

    TCHAR szFace[LF_FACESIZE];
    WORD wFontSize;
    if (!_DialogSizeHelper::GetFont(pTemplate, szFace, &wFontSize)) {
        return false;
    }

    fontFace = szFace;
    fontSize = (int)wFontSize;
    return true;
}

void CDpiAwareResizableDialog::MapDialogRectAuto(CRect& r)
{
    // Use m_dialogFont if available (created for target DPI in ApplyDialogSizeAndDpi)
    // This ensures DLU->pixel conversion uses the correct DPI during DPI changes
    if (m_dialogFont.m_hObject) {
        CMPCThemeUtil::MapDialogRectInternal(this, r, &m_dialogFont);
        return;
    }

    CString fontFace;
    int fontSize;

    if (GetDialogFontInfo(fontFace, fontSize)) {
        CFont dlgFont;
        if (CMPCThemeUtil::getFontByFace(dlgFont, this, fontFace, fontSize)) {
            CMPCThemeUtil::MapDialogRectInternal(this, r, &dlgFont);
        }
    } else {
        // DS_SETFONT not set, use system message font
        CMPCThemeUtil::MapDialogRectMessageFont(this, r);
    }
}

void CDpiAwareResizableDialog::UpdateMinMaxTrackSizeForDPI()
{
    // Use template size for constraints (saved size is for restoring preferred size)
    CSize templateDluSize;
    if (!GetDialogBaseUnits(templateDluSize)) {
        return;
    }

    CString fontFace;
    int fontSize;
    if (!GetDialogFontInfo(fontFace, fontSize)) {
        return;
    }

    int avgWidth, avgHeight;
    if (!DpiHelper::GetDialogFontMetricsForDPI(m_currentDpi, fontFace, fontSize, avgWidth, avgHeight)) {
        return;
    }

    CSize clientSize = DluToPixels(templateDluSize, avgWidth, avgHeight);
    CRect windowRect(0, 0, clientSize.cx, clientSize.cy);
    DpiHelper::AdjustWindowRectExForDpi(&windowRect, GetStyle(), FALSE, GetExStyle(), m_currentDpi);

    TrackSizeConstraints constraints = GetTrackSizeConstraints();

    if (constraints.min.enabled) {
        CSize minSize(
            (int)(windowRect.Width() * constraints.min.xMultiplier),
            (int)(windowRect.Height() * constraints.min.yMultiplier)
        );
        SetMinTrackSize(minSize);
    }

    if (constraints.max.enabled) {
        CSize maxSize(
            (int)(windowRect.Width() * constraints.max.xMultiplier),
            (int)(windowRect.Height() * constraints.max.yMultiplier)
        );
        SetMaxTrackSize(maxSize);
    }
}

void CDpiAwareResizableDialog::RepositionControlsFromTemplate()
{
    const DLGTEMPLATE* pTemplate = LoadDialogTemplate();
    if (!pTemplate) {
        return;
    }

    const auto* pTemplateEx = (const DLGTEMPLATEEX*)pTemplate;
    BOOL bDialogEx = IsDialogEx(pTemplate);
    int itemCount = bDialogEx ? pTemplateEx->cDlgItems : pTemplate->cdit;

    struct StaticControlInfo {
        HWND hwnd;
        int sequence;
    };
    std::vector<StaticControlInfo> staticControls;

    HWND hChild = ::GetWindow(m_hWnd, GW_CHILD);
    int staticSequence = 0;
    while (hChild) {
        int id = ::GetDlgCtrlID(hChild);
        if (id == -1 || id == 0xFFFF) {
            StaticControlInfo info = { hChild, staticSequence++ };
            staticControls.push_back(info);
        }
        hChild = ::GetNextWindow(hChild, GW_HWNDNEXT);
    }

    DLGITEMTEMPLATE* pItem = _AfxFindFirstDlgItem(pTemplate);
    int templateStaticSequence = 0;

    for (int i = 0; i < itemCount && pItem; i++) {
        CRect dluRect;
        DWORD ctrlID;
        GetItemDimensions(pTemplate, pItem, dluRect, ctrlID);

        MapDialogRectAuto(dluRect);

        HWND hControl = NULL;
        bool isStaticBySequence = false;

        // Match static controls by sequence order
        if (ctrlID == 0xFFFF || ctrlID == (DWORD)-1) {
            if (templateStaticSequence < (int)staticControls.size()) {
                hControl = staticControls[templateStaticSequence].hwnd;
                isStaticBySequence = true;
                templateStaticSequence++;
            }
        } else {
            CWnd* pCtrl = GetDlgItem(ctrlID);
            if (pCtrl) {
                hControl = pCtrl->GetSafeHwnd();
            }
        }

        if (hControl) {
            TCHAR className[256];
            ::GetClassName(hControl, className, 256);
            bool isComboBox = (_tcsicmp(className, WC_COMBOBOX) == 0);

            if (isComboBox) {
                RECT currentRect;
                ::GetWindowRect(hControl, &currentRect);
                ::MapWindowPoints(NULL, m_hWnd, (LPPOINT)&currentRect, 2);
                int currentHeight = currentRect.bottom - currentRect.top;

                ::SetWindowPos(hControl, NULL, dluRect.left, dluRect.top, dluRect.Width(), currentHeight, SWP_NOZORDER | SWP_NOACTIVATE);
            } else {
                ::SetWindowPos(hControl, NULL, dluRect.left, dluRect.top, dluRect.Width(), dluRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }

        pItem = _AfxFindNextDlgItem(pItem, bDialogEx);
    }

}

void CDpiAwareResizableDialog::BuildControlTemplateDLUMap(std::map<int, CRect>& controlTemplateDLUs)
{
    const DLGTEMPLATE* pTemplate = LoadDialogTemplate();
    if (!pTemplate) {
        return;
    }

    const auto* pTemplateEx = (const DLGTEMPLATEEX*)pTemplate;
    BOOL bDialogEx = IsDialogEx(pTemplate);
    int itemCount = bDialogEx ? pTemplateEx->cDlgItems : pTemplate->cdit;
    DLGITEMTEMPLATE* pItem = _AfxFindFirstDlgItem(pTemplate);

    for (int i = 0; i < itemCount && pItem; i++) {
        CRect dluRect;
        DWORD ctrlID;
        GetItemDimensions(pTemplate, pItem, dluRect, ctrlID);

        if (ctrlID != 0xFFFF && ctrlID != (DWORD)-1) {
            controlTemplateDLUs[ctrlID] = dluRect;
        }

        pItem = _AfxFindNextDlgItem(pItem, bDialogEx);
    }
}

void CDpiAwareResizableDialog::AdjustControlPositionsForResize(const CSize& templateDluSize, int deltaWidth, int deltaHeight)
{
    std::map<int, CRect> controlTemplateDLUs;
    BuildControlTemplateDLUMap(controlTemplateDLUs);

    CWnd* pChild = GetWindow(GW_CHILD);
    while (pChild) {
        HWND hChild = pChild->GetSafeHwnd();
        int controlID = ::GetDlgCtrlID(hChild);
        CRect rectCurrent;
        ::GetWindowRect(hChild, &rectCurrent);
        ScreenToClient(&rectCurrent);

        if (rectCurrent.Width() <= 0 || rectCurrent.Height() <= 0) {
            pChild = pChild->GetWindow(GW_HWNDNEXT);
            continue;
        }

        CRect rectNew = rectCurrent;
        bool moved = false;

        auto it = controlTemplateDLUs.find(controlID);
        if (it != controlTemplateDLUs.end()) {
            const CRect& dluRect = it->second;

            if (dluRect.left > templateDluSize.cx / 2) {
                rectNew.OffsetRect(deltaWidth, 0);
                moved = true;
            }
            else if (dluRect.Width() > templateDluSize.cx * 0.6) {
                rectNew.right += deltaWidth;
                moved = true;
            }

            if (dluRect.top > templateDluSize.cy / 2) {
                rectNew.OffsetRect(0, deltaHeight);
                moved = true;
            }
            else if (dluRect.Height() > templateDluSize.cy * 0.6) {
                rectNew.bottom += deltaHeight;
                moved = true;
            }
        }

        if (moved) {
            ::SetWindowPos(hChild, NULL, rectNew.left, rectNew.top, rectNew.Width(), rectNew.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
        }

        pChild = pChild->GetWindow(GW_HWNDNEXT);
    }

    Invalidate();
    UpdateWindow();
}

void CDpiAwareResizableDialog::GetItemDimensions(const DLGTEMPLATE* pTemplate, DLGITEMTEMPLATE* pItem, CRect& rect, DWORD& id) {
    if (IsDialogEx(pTemplate)) {
        auto* pItemEx = (DLGITEMTEMPLATEEX*)pItem;
        rect.SetRect(pItemEx->x, pItemEx->y, pItemEx->x + pItemEx->cx, pItemEx->y + pItemEx->cy);
        id = pItemEx->id;
    } else {
        rect.SetRect(pItem->x, pItem->y, pItem->x + pItem->cx, pItem->y + pItem->cy);
        id = pItem->id;
    }
}

void CDpiAwareResizableDialog::LoadStaticIcon(int controlID, LPCTSTR iconResourceID, bool isSystemIcon)
{
    CWnd* pControl = GetDlgItem(controlID);
    if (!pControl) {
        return;
    }

    CStatic* pStatic = DYNAMIC_DOWNCAST(CStatic, pControl);
    if (!pStatic) {
        return;
    }

    CRect controlRect;
    pStatic->GetClientRect(&controlRect);
    int iconSize = std::min(controlRect.Width(), controlRect.Height());

    HICON hIcon = nullptr;
    HINSTANCE hInst = isSystemIcon ? nullptr : AfxGetResourceHandle();

    if (SUCCEEDED(LoadIconWithScaleDown(hInst, iconResourceID, iconSize, iconSize, &hIcon))) {
        pStatic->SetIcon(hIcon);
        m_staticImages.push_back({controlID, iconResourceID, isSystemIcon});
    }
}

void CDpiAwareResizableDialog::RefreshStaticImages()
{
    for (const auto& info : m_staticImages) {
        CWnd* pControl = GetDlgItem(info.controlID);
        if (!pControl) {
            continue;
        }

        CStatic* pStatic = DYNAMIC_DOWNCAST(CStatic, pControl);
        if (!pStatic) {
            continue;
        }

        CRect controlRect;
        pStatic->GetClientRect(&controlRect);
        int iconSize = std::min(controlRect.Width(), controlRect.Height());

        HICON hOldIcon = pStatic->GetIcon();
        HICON hIcon = nullptr;
        HINSTANCE hInst = info.isSystemIcon ? nullptr : AfxGetResourceHandle();

        if (SUCCEEDED(LoadIconWithScaleDown(hInst, info.resourceID, iconSize, iconSize, &hIcon))) {
            pStatic->SetIcon(hIcon);
            if (hOldIcon) {
                DestroyIcon(hOldIcon);
            }
        }
    }
}

const DLGTEMPLATE* CDpiAwareResizableDialog::LoadDialogTemplate() const {
    UINT templateID = GetDialogTemplateID();
    if (templateID == 0) {
        return nullptr;
    }

    HRSRC hrsrc = FindResource(AfxGetResourceHandle(), MAKEINTRESOURCE(templateID), RT_DIALOG);
    if (!hrsrc) {
        return nullptr;
    }

    HGLOBAL hglb = LoadResource(AfxGetResourceHandle(), hrsrc);
    if (!hglb) {
        return nullptr;
    }

    return (const DLGTEMPLATE*)LockResource(hglb);
}
