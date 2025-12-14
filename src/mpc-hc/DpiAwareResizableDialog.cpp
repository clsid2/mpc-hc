#include "stdafx.h"
#include "DpiAwareResizableDialog.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"

// Forward declarations for MFC internal functions
AFX_STATIC DLGITEMTEMPLATE* AFXAPI _AfxFindFirstDlgItem(const DLGTEMPLATE* pTemplate);
AFX_STATIC DLGITEMTEMPLATE* AFXAPI _AfxFindNextDlgItem(DLGITEMTEMPLATE* pItem, BOOL bDialogEx);

CDpiAwareResizableDialog::CDpiAwareResizableDialog()
    : m_currentDpi(96), m_inDpiChange(false), m_cachedTemplate(nullptr), m_bGripVisible(true)
{
}

CDpiAwareResizableDialog::CDpiAwareResizableDialog(UINT nIDTemplate, CWnd* pParent) : CResizableDialog(nIDTemplate, pParent), m_currentDpi(96), m_inDpiChange(false), m_cachedTemplate(nullptr), m_bGripVisible(true)
{
}

CDpiAwareResizableDialog::CDpiAwareResizableDialog(LPCTSTR lpszTemplateName, CWnd* pParent) : CResizableDialog(lpszTemplateName, pParent), m_currentDpi(96), m_inDpiChange(false), m_cachedTemplate(nullptr), m_bGripVisible(true)
{
}

CDpiAwareResizableDialog::~CDpiAwareResizableDialog()
{
}

BOOL CDpiAwareResizableDialog::OnInitDialog() {
    BOOL ret = __super::OnInitDialog();

    m_currentDpi = DpiHelper::GetDPIForWindow(m_hWnd);

    // Must set track sizes before resizing (required by ResizableLib)
    UpdateMinMaxTrackSizeForDPI();

    CSize clientSize;
    if (CalculateDialogSize(clientSize)) {
        CRect windowRect(0, 0, clientSize.cx, clientSize.cy);
        DpiHelper::AdjustWindowRectExForDpi(&windowRect, GetStyle(), FALSE, GetExStyle(), m_currentDpi);

        CRect currentRect;
        GetWindowRect(&currentRect);
        int x = currentRect.left + (currentRect.Width() - windowRect.Width()) / 2;
        int y = currentRect.top + (currentRect.Height() - windowRect.Height()) / 2;
        SetWindowPos(NULL, x, y, windowRect.Width(), windowRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);

        RepositionControlsFromTemplate();
    }

    // Refresh grip state (initialized with primary monitor DPI, not current monitor)
    CWnd* pGrip = GetSizeGripWnd();
    if (pGrip && ::IsWindow(pGrip->GetSafeHwnd())) {
        pGrip->SendMessage(WM_SETTINGCHANGE, 0, 0);
    }

    UpdateSizeGripDPI();

    return ret;
}

BEGIN_MESSAGE_MAP(CDpiAwareResizableDialog, CResizableDialog)
    ON_WM_SIZE()
    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

void CDpiAwareResizableDialog::UpdateSizeGripDPI()
{
    CWnd* pGrip = GetSizeGripWnd();
    if (!pGrip || !::IsWindow(pGrip->GetSafeHwnd())) {
        return;
    }

    CRect clientRect;
    GetClientRect(&clientRect);

    // Skip if client rect not yet valid
    if (clientRect.Width() <= 0 || clientRect.Height() <= 0) {
        return;
    }

    // Update DPI if it changed (happens before OnInitDialog sets it)
    UINT actualDpi = DpiHelper::GetDPIForWindow(m_hWnd);
    if (actualDpi > 0 && actualDpi != m_currentDpi) {
        m_currentDpi = actualDpi;
    }

    DpiHelper dpiHelper;
    dpiHelper.Override(m_currentDpi, m_currentDpi);
    int gripCx = dpiHelper.GetSystemMetricsDPI(SM_CXVSCROLL);
    int gripCy = dpiHelper.GetSystemMetricsDPI(SM_CYHSCROLL);

    int gripX = clientRect.right - gripCx;
    int gripY = clientRect.bottom - gripCy;

    pGrip->SetWindowPos(&CWnd::wndBottom,
                       gripX, gripY,
                       gripCx, gripCy,
                       SWP_NOACTIVATE | SWP_NOREPOSITION |
                       (m_bGripVisible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
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

    // Hide grip when maximized, show otherwise
    if (nType == SIZE_MAXIMIZED) {
        m_bGripVisible = false;
    } else {
        m_bGripVisible = true;
    }

    UpdateSizeGripDPI();
    ArrangeLayout();
}

LRESULT CDpiAwareResizableDialog::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{

    if (m_inDpiChange) {
        return 0;
    }

    if (!IsWindowVisible()) {
        return 0;
    }

    m_inDpiChange = true;

    UINT oldDPI = LOWORD(wParam);
    UINT newDPI = HIWORD(wParam);
    RECT* suggestedRect = (RECT*)lParam;

    UINT actualDPI = DpiHelper::GetDPIForWindow(m_hWnd);
    UINT realNewDPI = newDPI;

    if (oldDPI == newDPI) {
        if (actualDPI != m_currentDpi) {
            realNewDPI = actualDPI;
        } else {
            return 0;
        }
    }

    if (realNewDPI == m_currentDpi) {
        return 0;
    }

    CRect oldClientRect;
    GetClientRect(&oldClientRect);

    CSize templateDluSize;
    bool hasTemplate = GetDialogBaseUnits(templateDluSize);

    CString fontFace;
    int fontSize;
    bool hasFontInfo = GetDialogFontInfo(fontFace, fontSize);

    double dluWidthRatio = 1.0;
    double dluHeightRatio = 1.0;

    if (hasTemplate && hasFontInfo) {
        CalculateCurrentDluRatio(oldClientRect, templateDluSize, fontFace, fontSize, dluWidthRatio, dluHeightRatio);
    }

    m_currentDpi = realNewDPI;

    RECT customRect;
    RECT* rectToUse = suggestedRect;

    if (hasTemplate && hasFontInfo && (dluWidthRatio != 1.0 || dluHeightRatio != 1.0)) {
        if (CalculateCustomDpiRect(templateDluSize, dluWidthRatio, dluHeightRatio, fontFace, fontSize, suggestedRect, customRect)) {
            rectToUse = &customRect;
        }
    }

    UpdateMinMaxTrackSizeForDPI();
    RemoveAllAnchors();

    DefWindowProc(WM_DPICHANGED, wParam, (LPARAM)rectToUse);

    RepositionControlsFromTemplate();
    CMPCThemeUtil::RefreshBitmapIconControls(this);

    m_inDpiChange = false;

    if (hasTemplate && hasFontInfo && (dluWidthRatio != 1.0 || dluHeightRatio != 1.0)) {
        int newAvgWidth, newAvgHeight;
        if (DpiHelper::GetDialogFontMetricsForDPI(m_currentDpi, fontFace, fontSize, newAvgWidth, newAvgHeight)) {
            int templatePixelWidth = MulDiv(templateDluSize.cx, newAvgWidth, 4);
            int templatePixelHeight = MulDiv(templateDluSize.cy, newAvgHeight, 8);

            CRect actualClientRect;
            GetClientRect(&actualClientRect);

            int deltaWidth = actualClientRect.Width() - templatePixelWidth;
            int deltaHeight = actualClientRect.Height() - templatePixelHeight;

            AdjustControlPositionsForResize(templateDluSize, deltaWidth, deltaHeight);
        }
    }

    // Refresh grip state and reposition for new DPI
    CWnd* pGrip = GetSizeGripWnd();
    if (pGrip && ::IsWindow(pGrip->GetSafeHwnd())) {
        pGrip->SendMessage(WM_SETTINGCHANGE, 0, 0);
    }
    UpdateSizeGripDPI();

    SetupAnchors();

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
    CString fontFace;
    int fontSize;

    if (GetDialogFontInfo(fontFace, fontSize)) {
        // Use custom dialog font
        CFont dlgFont;
        if (CMPCThemeUtil::getFontByFace(dlgFont, this, const_cast<wchar_t*>((LPCTSTR)fontFace), fontSize)) {
            CMPCThemeUtil::MapDialogRectInternal(this, r, &dlgFont);
        }
    } else {
        // Fallback: DS_SETFONT not set, use system message font
        CMPCThemeUtil::MapDialogRectMessageFont(this, r);
    }
}

bool CDpiAwareResizableDialog::CalculateDialogSize(CSize& clientSize)
{
    CSize dluSize;
    if (!GetDialogBaseUnits(dluSize)) {
        return false;
    }

    CRect rect(0, 0, dluSize.cx, dluSize.cy);
    MapDialogRectAuto(rect);

    clientSize = rect.Size();

    return true;
}

void CDpiAwareResizableDialog::UpdateMinMaxTrackSizeForDPI()
{
    CSize clientSize;
    if (!CalculateDialogSize(clientSize)) {
        return;
    }

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

    // Build a list of all static controls (-1 ID) in window order
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
        // Get control ID and DLU position from template
        CRect dluRect;
        DWORD ctrlID;
        GetItemDimensions(pTemplate, pItem, dluRect, ctrlID);

        // Convert DLUs to pixels using dialog font or fallback to message font
        MapDialogRectAuto(dluRect);

        // Get the actual control
        HWND hControl = NULL;
        bool isStaticBySequence = false;

        // For static controls with ID -1/0xFFFF, match by sequence order
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
            // Check if this is a combobox (preserve current height)
            TCHAR className[256];
            ::GetClassName(hControl, className, 256);
            bool isComboBox = (_tcsicmp(className, WC_COMBOBOX) == 0);

            if (isComboBox) {
                // For comboboxes, preserve the current height
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

void CDpiAwareResizableDialog::CalculateCurrentDluRatio(const CRect& clientRect, const CSize& templateDluSize, LPCTSTR fontFace, int fontSize, double& widthRatio, double& heightRatio)
{
    widthRatio = 1.0;
    heightRatio = 1.0;

    int oldAvgWidth, oldAvgHeight;
    if (DpiHelper::GetDialogFontMetricsForDPI(m_currentDpi, fontFace, fontSize, oldAvgWidth, oldAvgHeight)) {
        int actualDluWidth = MulDiv(clientRect.Width(), 4, oldAvgWidth);
        int actualDluHeight = MulDiv(clientRect.Height(), 8, oldAvgHeight);
        widthRatio = (double)actualDluWidth / templateDluSize.cx;
        heightRatio = (double)actualDluHeight / templateDluSize.cy;
    }
}

bool CDpiAwareResizableDialog::CalculateCustomDpiRect(const CSize& templateDluSize, double widthRatio, double heightRatio, LPCTSTR fontFace, int fontSize, const RECT* suggestedRect, RECT& customRect)
{
    int newAvgWidth, newAvgHeight;
    if (!DpiHelper::GetDialogFontMetricsForDPI(m_currentDpi, fontFace, fontSize, newAvgWidth, newAvgHeight)) {
        return false;
    }

    int targetDluWidth = (int)(templateDluSize.cx * widthRatio);
    int targetDluHeight = (int)(templateDluSize.cy * heightRatio);

    int targetPixelWidth = MulDiv(targetDluWidth, newAvgWidth, 4);
    int targetPixelHeight = MulDiv(targetDluHeight, newAvgHeight, 8);

    CRect tempRect(0, 0, targetPixelWidth, targetPixelHeight);
    DpiHelper::AdjustWindowRectExForDpi(&tempRect, GetStyle(), FALSE, GetExStyle(), m_currentDpi);

    customRect.left = suggestedRect->left;
    customRect.top = suggestedRect->top;
    customRect.right = customRect.left + tempRect.Width();
    customRect.bottom = customRect.top + tempRect.Height();

    return true;
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

            bool inBottomRight = (dluRect.left > templateDluSize.cx * 0.8) && (dluRect.top > templateDluSize.cy * 0.8);

            if (inBottomRight) {
                pChild = pChild->GetWindow(GW_HWNDNEXT);
                continue;
            }
            else if (dluRect.left > templateDluSize.cx / 2) {
                rectNew.OffsetRect(deltaWidth, 0);
                moved = true;
            }
            else if (dluRect.Width() > templateDluSize.cx * 0.6) {
                rectNew.right += deltaWidth;
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

const DLGTEMPLATE* CDpiAwareResizableDialog::LoadDialogTemplate() const {
    if (m_cachedTemplate) {
        return m_cachedTemplate;
    }

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

    m_cachedTemplate = (const DLGTEMPLATE*)LockResource(hglb);
    return m_cachedTemplate;
}
