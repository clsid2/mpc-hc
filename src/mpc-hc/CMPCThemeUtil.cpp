#include "stdafx.h"
#include "CMPCThemeUtil.h"
#include "CMPCTheme.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "CMPCThemeStatic.h"
#include "CMPCThemeDialog.h"
#include "CMPCThemeSliderCtrl.h"
#include "CMPCThemeTabCtrl.h"
#include "VersionHelpersInternal.h"
#undef SubclassWindow

CBrush CMPCThemeUtil::contentBrush = CBrush();
CBrush CMPCThemeUtil::windowBrush = CBrush();
CBrush CMPCThemeUtil::controlAreaBrush = CBrush();
CBrush CMPCThemeUtil::W10DarkThemeFileDialogInjectedBGBrush = CBrush();

CFont CMPCThemeUtil::dialogFont = CFont();

CMPCThemeUtil::CMPCThemeUtil() {
}

CMPCThemeUtil::~CMPCThemeUtil() {
    for (u_int i = 0; i < allocatedWindows.size(); i++) {
        delete allocatedWindows[i];
    }
}


void CMPCThemeUtil::fulfillThemeReqs(CWnd* wnd) {
    if (AfxGetAppSettings().bMPCThemeLoaded) {

        initHelperObjects(wnd);

        CWnd* pChild = wnd->GetWindow(GW_CHILD);
        while (pChild) {
            LRESULT lRes = pChild->SendMessage(WM_GETDLGCODE, 0, 0);
            CWnd* tChild = pChild;
            pChild = pChild->GetNextWindow(); //increment before any unsubclassing
            CString runtimeClass = tChild->GetRuntimeClass()->m_lpszClassName;
            TCHAR windowClass[MAX_PATH];
            ::GetClassName(tChild->GetSafeHwnd(), windowClass, _countof(windowClass));
            DWORD style = tChild->GetStyle();
            DWORD buttonType = (style & BS_TYPEMASK);
            DWORD staticStyle = (style & SS_TYPEMASK);
            CString windowTitle;

            if (tChild->m_hWnd) {
                tChild->GetWindowText(windowTitle);
            }
            bool canSubclass = (CWnd::FromHandlePermanent(tChild->GetSafeHwnd()) == NULL); //refuse to subclass if already subclassed.  in this case the member class should be updated rather than dynamically subclassing

            if (canSubclass) {
                if (DLGC_BUTTON == (lRes & DLGC_BUTTON)) {
                    if (DLGC_DEFPUSHBUTTON == (lRes & DLGC_DEFPUSHBUTTON) || DLGC_UNDEFPUSHBUTTON == (lRes & DLGC_UNDEFPUSHBUTTON)) {
                        CMPCThemeButton* pObject = DEBUG_NEW CMPCThemeButton();
                        makeThemed(pObject, tChild);
                    } else if (DLGC_BUTTON == (lRes & DLGC_BUTTON) && (buttonType == BS_CHECKBOX || buttonType == BS_AUTOCHECKBOX)) {
                        CMPCThemeRadioOrCheck* pObject = DEBUG_NEW CMPCThemeRadioOrCheck();
                        makeThemed(pObject, tChild);
                    } else if (DLGC_BUTTON == (lRes & DLGC_BUTTON) && (buttonType == BS_3STATE || buttonType == BS_AUTO3STATE)) {
                        CMPCThemeRadioOrCheck* pObject = DEBUG_NEW CMPCThemeRadioOrCheck();
                        makeThemed(pObject, tChild);
                    } else if (DLGC_RADIOBUTTON == (lRes & DLGC_RADIOBUTTON) && (buttonType == BS_RADIOBUTTON || buttonType == BS_AUTORADIOBUTTON)) {
                        CMPCThemeRadioOrCheck* pObject = DEBUG_NEW CMPCThemeRadioOrCheck();
                        makeThemed(pObject, tChild);
                    } else { //what other buttons?
//                        int a = 1;
                    }
                } else if (0 == _tcsicmp(windowClass, WC_SCROLLBAR)) {
                } else if (0 == _tcsicmp(windowClass, WC_BUTTON) && buttonType == BS_GROUPBOX) {
                    CMPCThemeGroupBox* pObject = DEBUG_NEW CMPCThemeGroupBox();
                    makeThemed(pObject, tChild);
                    SetWindowTheme(tChild->GetSafeHwnd(), L"", L"");
                } else if (0 == _tcsicmp(windowClass, WC_STATIC) && SS_ICON == staticStyle) { //don't touch icons for now
                } else if (0 == _tcsicmp(windowClass, WC_STATIC) && SS_BITMAP == staticStyle) { //don't touch BITMAPS for now
                } else if (0 == _tcsicmp(windowClass, WC_STATIC) && SS_OWNERDRAW == staticStyle) { //don't touch OWNERDRAW for now
                } else if (0 == _tcsicmp(windowClass, WC_STATIC) && (staticStyle < SS_OWNERDRAW || SS_ETCHEDHORZ == staticStyle || SS_ETCHEDVERT == staticStyle || SS_ETCHEDFRAME == staticStyle)) {
                    tChild->SetFont(&dialogFont);
                    LITEM li = { 0 };
                    li.mask = LIF_ITEMINDEX | LIF_ITEMID;
                    if (::SendMessage(tChild->GetSafeHwnd(), LM_GETITEM, 0, (LPARAM)& li)) { //we appear to have a linkctrl
                        CMPCThemeLinkCtrl* pObject = DEBUG_NEW CMPCThemeLinkCtrl();
                        makeThemed(pObject, tChild);
                    } else {
                        CMPCThemeStatic* pObject = DEBUG_NEW CMPCThemeStatic();
                        if (0 == (style & SS_LEFTNOWORDWRAP) && 0 == windowTitle.Left(20).Compare(_T("Select which output "))) {
                            //this is a hack for LAVFilters to avoid wrapping the statics
                            //FIXME by upstreaming a change to dialog layout of lavfilters, or by developing a dynamic re-layout engine
                            CRect wr;
                            tChild->GetWindowRect(wr);
                            wnd->ScreenToClient(wr);
                            wr.right += 5;
                            tChild->MoveWindow(wr);
                        }
                        makeThemed(pObject, tChild);
                    }
                } else if (0 == _tcsicmp(windowClass, WC_EDIT)) {
                    CMPCThemeEdit* pObject = DEBUG_NEW CMPCThemeEdit();
                    makeThemed(pObject, tChild);
                } else if (0 == _tcsicmp(windowClass, UPDOWN_CLASS)) {
                    CMPCThemeSpinButtonCtrl* pObject = DEBUG_NEW CMPCThemeSpinButtonCtrl();
                    makeThemed(pObject, tChild);
                } else if (0 == _tcsicmp(windowClass, _T("#32770"))) { //dialog class
                    CMPCThemeDialog* pObject = DEBUG_NEW CMPCThemeDialog();
                    makeThemed(pObject, tChild);
                } else if (0 == _tcsicmp(windowClass, WC_COMBOBOX)) {
                    CMPCThemeComboBox* pObject = DEBUG_NEW CMPCThemeComboBox();
                    makeThemed(pObject, tChild);
                } else if (0 == _tcsicmp(windowClass, TRACKBAR_CLASS)) {
                    CMPCThemeSliderCtrl* pObject = DEBUG_NEW CMPCThemeSliderCtrl();
                    makeThemed(pObject, tChild);
                } else if (0 == _tcsicmp(windowClass, WC_TABCONTROL)) {
                    CMPCThemeTabCtrl* pObject = DEBUG_NEW CMPCThemeTabCtrl();
                    makeThemed(pObject, tChild);
                }
            }
            if (0 == _tcsicmp(windowClass, _T("#32770"))) { //dialog class
                fulfillThemeReqs(tChild);
            } else if (windowTitle == _T("CInternalPropertyPageWnd")) { //internal window encompassing property pages
                fulfillThemeReqs(tChild);
            }
        }
    }
}

void CMPCThemeUtil::initHelperObjects(CWnd* wnd) {
    if (contentBrush.m_hObject == nullptr) contentBrush.CreateSolidBrush(CMPCTheme::ContentBGColor);
    if (windowBrush.m_hObject == nullptr) windowBrush.CreateSolidBrush(CMPCTheme::WindowBGColor);
    if (controlAreaBrush.m_hObject == nullptr) controlAreaBrush.CreateSolidBrush(CMPCTheme::ControlAreaBGColor);
    if (W10DarkThemeFileDialogInjectedBGBrush.m_hObject == nullptr) W10DarkThemeFileDialogInjectedBGBrush.CreateSolidBrush(CMPCTheme::W10DarkThemeFileDialogInjectedBGColor);
    if (dialogFont.m_hObject == nullptr) {
        CDC* pDC = wnd->GetWindowDC();
        CMPCThemeUtil::getFontByType(dialogFont, pDC, CMPCThemeUtil::DialogFont);
        wnd->ReleaseDC(pDC);
    }
}

void CMPCThemeUtil::makeThemed(CWnd* pObject, CWnd* tChild) {
    allocatedWindows.push_back(pObject);
    pObject->SubclassWindow(tChild->GetSafeHwnd());
}

LRESULT CALLBACK wndProcFileDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    WNDPROC wndProcSink = NULL;
    wndProcSink = (WNDPROC)GetProp(hWnd, _T("WNDPROC_SINK"));
    VERIFY(wndProcSink);
    if (WM_CTLCOLOREDIT == uMsg) {
        return (LRESULT)CMPCThemeUtil::getCtlColorFileDialog((HDC)wParam, CTLCOLOR_EDIT);
    }
    if (!wndProcSink) return 0;
    return ::CallWindowProc(wndProcSink, hWnd, uMsg, wParam, lParam);
}


void CMPCThemeUtil::subClassFileDialog(CWnd* wnd, HWND hWnd, bool findSink) {
    if (AfxGetAppSettings().bWindows10DarkThemeActive) {
        initHelperObjects(wnd);
        HWND pChild = ::GetWindow(hWnd, GW_CHILD);

        while (pChild) {
            TCHAR childWindowClass[MAX_PATH];
            ::GetClassName(pChild, childWindowClass, _countof(childWindowClass));
            if (findSink) {
                if (0 == _tcsicmp(childWindowClass, _T("FloatNotifySink"))) { //children are the injected controls
                    subClassFileDialog(wnd, pChild, false); //recurse into the sinks
                }
            } else {
                if (0 == _tcsicmp(childWindowClass, WC_STATIC)) {
                    CWnd* c = CWnd::FromHandle(pChild);
                    c->UnsubclassWindow();
                    CMPCThemeStatic* pObject = DEBUG_NEW CMPCThemeStatic();
                    pObject->setFileDialogChild(true);
                    allocatedWindows.push_back(pObject);
                    pObject->SubclassWindow(pChild);
                } else if (0 == _tcsicmp(childWindowClass, WC_EDIT)) {
                    CWnd* c = CWnd::FromHandle(pChild);
                    c->UnsubclassWindow();
                    CMPCThemeEdit* pObject = DEBUG_NEW CMPCThemeEdit();
                    pObject->setFileDialogChild(true);
                    allocatedWindows.push_back(pObject);
                    pObject->SubclassWindow(pChild);
                    if (nullptr == GetProp(hWnd, _T("WNDPROC_SINK"))) {
                        LONG_PTR wndProcOld = ::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG)wndProcFileDialog);
                        SetProp(hWnd, _T("WNDPROC_SINK"), (HANDLE)wndProcOld);
                    }
                }
            }
            pChild = ::GetNextWindow(pChild, GW_HWNDNEXT);
        }
    }
}
AFX_STATIC DLGITEMTEMPLATE* AFXAPI _AfxFindNextDlgItem(DLGITEMTEMPLATE* pItem, BOOL bDialogEx);
AFX_STATIC DLGITEMTEMPLATE* AFXAPI _AfxFindFirstDlgItem(const DLGTEMPLATE* pTemplate);

AFX_STATIC inline BOOL IsDialogEx(const DLGTEMPLATE* pTemplate) {
    return ((_DialogSplitHelper::DLGTEMPLATEEX*)pTemplate)->signature == 0xFFFF;
}

static inline WORD& DlgTemplateItemCount(DLGTEMPLATE* pTemplate) {
    if (IsDialogEx(pTemplate))
        return reinterpret_cast<_DialogSplitHelper::DLGTEMPLATEEX*>(pTemplate)->cDlgItems;
    else
        return pTemplate->cdit;
}

static inline const WORD& DlgTemplateItemCount(const DLGTEMPLATE* pTemplate) {
    if (IsDialogEx(pTemplate))
        return reinterpret_cast<const _DialogSplitHelper::DLGTEMPLATEEX*>(pTemplate)->cDlgItems;
    else
        return pTemplate->cdit;
}

bool CMPCThemeUtil::ModifyTemplates(CPropertySheet* sheet, CRuntimeClass* pageClass, DWORD id, DWORD addStyle, DWORD removeStyle) {
    if (AfxGetAppSettings().bMPCThemeLoaded) {
        PROPSHEETHEADER m_psh = sheet->m_psh;
        for (int i = 0; i < sheet->GetPageCount(); i++) {
            CPropertyPage* pPage = sheet->GetPage(i);
            if (nullptr == AfxDynamicDownCast(pageClass, pPage)) {
                continue;
            }
            PROPSHEETPAGE* tpsp = &pPage->m_psp;

            const DLGTEMPLATE* pTemplate;
            if (tpsp->dwFlags & PSP_DLGINDIRECT) {
                pTemplate = tpsp->pResource;
            } else {
                HRSRC hResource = ::FindResource(tpsp->hInstance, tpsp->pszTemplate, RT_DIALOG);
                if (hResource == NULL) return false;
                HGLOBAL hTemplate = LoadResource(tpsp->hInstance, hResource);
                if (hTemplate == NULL) return false;
                pTemplate = (LPCDLGTEMPLATE)LockResource(hTemplate);
                if (pTemplate == NULL) return false;
            }

            if (afxOccManager != NULL) {
                DLGITEMTEMPLATE* pItem = _AfxFindFirstDlgItem(pTemplate);
                DLGITEMTEMPLATE* pNextItem;
                BOOL bDialogEx = IsDialogEx(pTemplate);

                int iItem, iItems = DlgTemplateItemCount(pTemplate);

                for (iItem = 0; iItem < iItems; iItem++) {
                    pNextItem = _AfxFindNextDlgItem(pItem, bDialogEx);
                    DWORD dwOldProtect, tp;
                    if (bDialogEx) {
                        _DialogSplitHelper::DLGITEMTEMPLATEEX* pItemEx = (_DialogSplitHelper::DLGITEMTEMPLATEEX*)pItem;
                        if (pItemEx->id == id) {
                            if (VirtualProtect(&pItemEx->style, sizeof(pItemEx->style), PAGE_READWRITE, &dwOldProtect)) {
                                pItemEx->style |= addStyle;
                                pItemEx->style &= ~removeStyle;
                                VirtualProtect(&pItemEx->style, sizeof(pItemEx->style), dwOldProtect, &tp);
                            }
                        }
                    } else {
                        if (pItem->id == id) {
                            if (VirtualProtect(&pItem->style, sizeof(pItem->style), PAGE_READWRITE, &dwOldProtect)) {
                                pItem->style |= addStyle;
                                pItem->style &= ~removeStyle;
                                VirtualProtect(&pItem->style, sizeof(pItem->style), dwOldProtect, &tp);
                            }
                        }
                    }
                    pItem = pNextItem;
                }
            }
        }
    }
    return true;
}

void CMPCThemeUtil::enableFileDialogHook() {
    CMainFrame* pMainFrame = AfxGetMainFrame();
    pMainFrame->enableFileDialogHook(this);
}

HBRUSH CMPCThemeUtil::getCtlColorFileDialog(HDC hDC, UINT nCtlColor) {
    if (CTLCOLOR_EDIT == nCtlColor) {
        ::SetTextColor(hDC, CMPCTheme::W10DarkThemeFileDialogInjectedTextColor);
        ::SetBkColor(hDC, CMPCTheme::W10DarkThemeFileDialogInjectedBGColor);
        return W10DarkThemeFileDialogInjectedBGBrush;
    } else if (CTLCOLOR_STATIC == nCtlColor) {
        ::SetTextColor(hDC, CMPCTheme::W10DarkThemeFileDialogInjectedTextColor);
        ::SetBkColor(hDC, CMPCTheme::W10DarkThemeFileDialogInjectedBGColor);
        return W10DarkThemeFileDialogInjectedBGBrush;
    } else {
        return NULL;
    }
}

HBRUSH CMPCThemeUtil::getCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    if (AfxGetAppSettings().bMPCThemeLoaded) {
        LRESULT lResult;
        if (pWnd->SendChildNotifyLastMsg(&lResult)) {
            return (HBRUSH)lResult;
        }
        if (CTLCOLOR_LISTBOX == nCtlColor) {
            pDC->SetTextColor(CMPCTheme::TextFGColor);
            pDC->SetBkColor(CMPCTheme::ContentBGColor);
            return contentBrush;
        } else {
            pDC->SetTextColor(CMPCTheme::TextFGColor);
            pDC->SetBkColor(CMPCTheme::WindowBGColor);
            return windowBrush;
        }
    }
    return nullptr;
}

bool CMPCThemeUtil::MPCThemeEraseBkgnd(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    if (AfxGetAppSettings().bMPCThemeLoaded) {
        CRect rect;
        pWnd->GetClientRect(rect);
        if (CTLCOLOR_DLG == nCtlColor) { //only supported "class" for now
            pDC->FillSolidRect(rect, CMPCTheme::WindowBGColor);
        } else {
            return false;
        }
        return true;
    } else {
        return false;
    }
}

void CMPCThemeUtil::getFontByFace(CFont& font, CDC* pDC, wchar_t* fontName, int size, LONG weight) {
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));

    DpiHelper dpiWindow;
    dpiWindow.Override(AfxGetMainWnd()->GetSafeHwnd());
    lf.lfHeight = -MulDiv(size, dpiWindow.DPIY(), 72);

    lf.lfQuality = CLEARTYPE_QUALITY;

    //lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfWeight = weight;
    wcsncpy_s(lf.lfFaceName, fontName, LF_FACESIZE);

    font.CreateFontIndirect(&lf);
}

void CMPCThemeUtil::getFontByType(CFont& font, CDC* pDC, int type, bool underline, bool bold) {
    NONCLIENTMETRICS m;
    GetMetrics(&m);

    LOGFONT* lf;
    if (type == CaptionFont) {
        lf = &m.lfCaptionFont;
    } else if (type == SmallCaptionFont) {
        lf = &m.lfSmCaptionFont;
    } else if (type == MenuFont) {
        lf = &m.lfMenuFont;
    } else if (type == StatusFont) {
        lf = &m.lfStatusFont;
    } else if (type == MessageFont) {
        lf = &m.lfMessageFont;
    } else if (type == DialogFont) { //hack for compatibility with MS SHell Dlg (8) used in dialogs
        DpiHelper dpiWindow;
        dpiWindow.Override(AfxGetMainWnd()->GetSafeHwnd());

        LOGFONT tlf;
        memset(&tlf, 0, sizeof(LOGFONT));
        tlf.lfHeight = -MulDiv(8, dpiWindow.DPIY(), 72);
        tlf.lfQuality = CLEARTYPE_QUALITY;
        tlf.lfWeight = FW_REGULAR;
        wcsncpy_s(tlf.lfFaceName, m.lfMessageFont.lfFaceName, LF_FACESIZE);
        //wcsncpy_s(tlf.lfFaceName, _T("MS Shell Dlg"), LF_FACESIZE);
        lf = &tlf;
    } else if (type == fixedFont) {
        DpiHelper dpiWindow;
        dpiWindow.Override(AfxGetMainWnd()->GetSafeHwnd());

        LOGFONT tlf;
        memset(&tlf, 0, sizeof(LOGFONT));
        tlf.lfHeight = -MulDiv(10, dpiWindow.DPIY(), 72);
        tlf.lfQuality = CLEARTYPE_QUALITY;
        tlf.lfWeight = FW_REGULAR;
        wcsncpy_s(tlf.lfFaceName, _T("Consolas"), LF_FACESIZE);
        lf = &tlf;
    } else {
        lf = &m.lfMessageFont;
    }
    if (underline || bold) {
        LOGFONT tlf;
        memset(&tlf, 0, sizeof(LOGFONT));
        tlf.lfHeight = lf->lfHeight;
        tlf.lfQuality = lf->lfQuality;
        tlf.lfWeight = lf->lfWeight;
        wcsncpy_s(tlf.lfFaceName, lf->lfFaceName, LF_FACESIZE);
        tlf.lfUnderline = underline;
        if (bold) tlf.lfWeight = FW_BOLD;
        font.CreateFontIndirect(&tlf);
    } else {
        font.CreateFontIndirect(lf);
    }
}

CSize CMPCThemeUtil::GetTextSize(CString str, HDC hDC, int type) {
    CDC* pDC = CDC::FromHandle(hDC);
    CFont font;
    getFontByType(font, pDC, type);
    CFont* pOldFont = pDC->SelectObject(&font);

    //CSize cs = pDC->GetTextExtent(str);
    CRect r = { 0 };
    pDC->DrawText(str, r, DT_SINGLELINE | DT_EXPANDTABS | DT_CALCRECT);
    CSize cs = r.Size();

    pDC->SelectObject(pOldFont);

    return cs;
}

CSize CMPCThemeUtil::GetTextSizeDiff(CString str, HDC hDC, int type, CFont* curFont) {
    CSize cs = GetTextSize(str, hDC, type);
    CDC* cDC = CDC::FromHandle(hDC);
    CFont* pOldFont = cDC->SelectObject(curFont);
    CSize curCs = cDC->GetTextExtent(str);
    cDC->SelectObject(pOldFont);

    return cs - curCs;
}

void CMPCThemeUtil::GetMetrics(NONCLIENTMETRICS *m) {
    m->cbSize = sizeof(NONCLIENTMETRICS);
    ::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), m, 0);
    DpiHelper dpi, dpiWindow;
    dpiWindow.Override(AfxGetMainWnd()->GetSafeHwnd());

    //getclientmetrics is ignorant of per window DPI
    if (dpi.ScaleFactorY() != dpiWindow.ScaleFactorY()) {
        m->lfCaptionFont.lfHeight = dpiWindow.ScaleSystemToOverrideY(m->lfCaptionFont.lfHeight);
        m->lfSmCaptionFont.lfHeight = dpiWindow.ScaleSystemToOverrideY(m->lfSmCaptionFont.lfHeight);
        m->lfMenuFont.lfHeight = dpiWindow.ScaleSystemToOverrideY(m->lfMenuFont.lfHeight);
        m->lfStatusFont.lfHeight = dpiWindow.ScaleSystemToOverrideY(m->lfStatusFont.lfHeight);
        m->lfMessageFont.lfHeight = dpiWindow.ScaleSystemToOverrideY(m->lfMessageFont.lfHeight);
    }
}

void CMPCThemeUtil::initMemDC(CDC* pDC, CDC& dcMem, CBitmap& bmMem, CRect rect) {
    dcMem.CreateCompatibleDC(pDC);
    dcMem.SetBkColor(pDC->GetBkColor());
    dcMem.SetTextColor(pDC->GetTextColor());
    dcMem.SetBkMode(pDC->GetBkMode());
    dcMem.SelectObject(pDC->GetCurrentFont());

    bmMem.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height());
    dcMem.SelectObject(&bmMem);
    dcMem.BitBlt(0, 0, rect.Width(), rect.Height(), pDC, rect.left, rect.top, SRCCOPY);
}

void CMPCThemeUtil::flushMemDC(CDC* pDC, CDC& dcMem, CRect rect) {
    pDC->BitBlt(rect.left, rect.top, rect.Width(), rect.Height(), &dcMem, 0, 0, SRCCOPY);
}


void CMPCThemeUtil::DrawBufferedText(CDC* pDC, CString text, CRect rect, UINT format) {
    CDC dcMem;
    dcMem.CreateCompatibleDC(pDC);
    dcMem.SetBkColor(pDC->GetBkColor());
    dcMem.SetTextColor(pDC->GetTextColor());
    dcMem.SetBkMode(pDC->GetBkMode());
    dcMem.SelectObject(pDC->GetCurrentFont());

    CBitmap bmMem;
    bmMem.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height());
    dcMem.SelectObject(&bmMem);
    dcMem.BitBlt(0, 0, rect.Width(), rect.Height(), pDC, rect.left, rect.top, SRCCOPY);

    CRect tr = rect;
    tr.OffsetRect(-tr.left, -tr.top);
    dcMem.DrawText(text, tr, format);

    pDC->BitBlt(rect.left, rect.top, rect.Width(), rect.Height(), &dcMem, 0, 0, SRCCOPY);
}

void CMPCThemeUtil::Draw2BitTransparent(CDC& dc, int left, int top, int width, int height, CBitmap& bmp, COLORREF fgColor) {
    COLORREF crOldTextColor = dc.GetTextColor();
    COLORREF crOldBkColor = dc.GetBkColor();

    CDC dcBMP;
    dcBMP.CreateCompatibleDC(&dc);

    dcBMP.SelectObject(bmp);
    dc.BitBlt(left, top, width, height, &dcBMP, 0, 0, SRCINVERT); //SRCINVERT works to create mask from 2-bit image. same result as bitblt with text=0 and bk=0xffffff
    dc.SetBkColor(fgColor);        //paint: foreground color (1 in 2 bit)
    dc.SetTextColor(RGB(0, 0, 0)); //paint: black where transparent
    dc.BitBlt(left, top, width, height, &dcBMP, 0, 0, SRCPAINT);

    dc.SetTextColor(crOldTextColor);
    dc.SetBkColor(crOldBkColor);
}

void CMPCThemeUtil::dbg(CString text, ...) {
    va_list args;
    va_start(args, text);
    CString output;
    output.FormatV(text, args);
    OutputDebugString(output);
    OutputDebugString(_T("\n"));
    va_end(args);
}

UINT CMPCThemeUtil::getResourceByDPI(CDC* pDC, const UINT* resources) {
    int index;
    int dpi = pDC->GetDeviceCaps(LOGPIXELSX);
    if (dpi < 120) index = 0;
    else if (dpi < 144) index = 1;
    else if (dpi < 168) index = 2;
    else if (dpi < 196) index = 3;
    else index = 4;

    return resources[index];
}

void CMPCThemeUtil::drawCheckBox(UINT checkState, bool isHover, bool useSystemSize, CRect rectCheck, CDC* pDC, bool isRadio) {
    COLORREF borderClr, bgClr;
    COLORREF oldBkClr = pDC->GetBkColor(), oldTextClr = pDC->GetTextColor();
    if (isHover) {
        borderClr = CMPCTheme::CheckboxBorderHoverColor;
        bgClr = CMPCTheme::CheckboxBGHoverColor;
    } else {
        borderClr = CMPCTheme::CheckboxBorderColor;
        bgClr = CMPCTheme::CheckboxBGColor;
    }

    if (useSystemSize) {
        CPngImage image;
        image.Load(getResourceByDPI(pDC, isRadio ? CMPCTheme::ThemeRadios : CMPCTheme::ThemeCheckBoxes), AfxGetInstanceHandle());
        BITMAP bm;
        image.GetBitmap(&bm);
        int size = bm.bmHeight;

        CDC mDC;
        mDC.CreateCompatibleDC(pDC);
        mDC.SelectObject(image);
        int index;
        if (isRadio) {
            index = RadioRegular;
            if (checkState) index += 1;
            if (isHover) index += 2;
        } else {
            index = CheckBoxRegular;
            if (isHover) index += 1;
        }
        CRect drawRect(0, 0, size, size);
        //drawRect.OffsetRect(rectCheck.left + (rectCheck.Width() - size) / 2, rectCheck.top + (rectCheck.Height() - size) / 2);
        drawRect.OffsetRect(rectCheck.left, rectCheck.top + (rectCheck.Height() - size) / 2);

        if (!isRadio && checkState != BST_CHECKED) { //we can draw this w/o BMPs
            CBrush brush(borderClr);
            pDC->FrameRect(drawRect, &brush);
            drawRect.DeflateRect(1, 1);
            pDC->FillSolidRect(drawRect, bgClr);
            if (checkState == BST_INDETERMINATE) {
                drawRect.DeflateRect(2, 2);
                pDC->FillSolidRect(drawRect, CMPCTheme::TextFGColor);
            }
        } else {
            int left = index * size;
            pDC->BitBlt(drawRect.left, drawRect.top, drawRect.Width(), drawRect.Height(), &mDC, left, 0, SRCCOPY);
        }
    } else {
        CBrush brush(borderClr);
        pDC->FrameRect(rectCheck, &brush);
        rectCheck.DeflateRect(1, 1);
        pDC->FillSolidRect(rectCheck, bgClr);
        if (checkState) {
            CBitmap checkBMP;
            CDC dcCheckBMP;
            dcCheckBMP.CreateCompatibleDC(pDC);

            int left, top, width, height;
            width = CMPCTheme::CheckWidth;
            height = CMPCTheme::CheckHeight;
            left = rectCheck.left + (rectCheck.Width() - width) / 2;
            top = rectCheck.top + (rectCheck.Height() - height) / 2;
            checkBMP.CreateBitmap(width, height, 1, 1, CMPCTheme::CheckBits);
            dcCheckBMP.SelectObject(&checkBMP);

            pDC->SetBkColor(CMPCTheme::CheckColor);
            pDC->SetTextColor(bgClr);
            pDC->BitBlt(left, top, width, height, &dcCheckBMP, 0, 0, SRCCOPY);
        }
    }
    pDC->SetBkColor(oldBkClr);
    pDC->SetTextColor(oldTextClr);
}

bool CMPCThemeUtil::canUseWin10DarkTheme() {
    if (AfxGetAppSettings().bMPCThemeLoaded) {
        //return false; //FIXME.  return false to test behavior for OS < Win10
        return IsWindows10OrGreater();
    }
    return false;
}

UINT CMPCThemeUtil::defaultLogo() {
    return IDF_LOGO4;
}

struct AFX_CTLCOLOR {
    HWND hWnd;
    HDC hDC;
    UINT nCtlType;
};

void CMPCThemeUtil::fillParentDialogBGClr(CWnd* wnd, CDC* pDC, CRect r) {
    CBrush brush;
    WPARAM w = (WPARAM)pDC;
    AFX_CTLCOLOR ctl;
    ctl.hWnd = wnd->GetSafeHwnd();
    ctl.nCtlType = CTLCOLOR_DLG;
    ctl.hDC = pDC->GetSafeHdc();
    CWnd* parent = wnd->GetParent();
    if (nullptr == parent) {
        parent = wnd;
    }
    HBRUSH bg = (HBRUSH)parent->SendMessage(WM_CTLCOLORDLG, w, (LPARAM)& ctl);
    brush.Attach(bg);
    pDC->FillRect(r, &brush);
    brush.Detach();
}

void CMPCThemeUtil::fulfillThemeReqs(CProgressCtrl* ctl) {
    if (AfxGetAppSettings().bMPCThemeLoaded) {
        SetWindowTheme(ctl->GetSafeHwnd(), _T(""), _T(""));
        ctl->SetBarColor(CMPCTheme::ProgressBarColor);
        ctl->SetBkColor(CMPCTheme::ProgressBarBGColor);
    }
    ctl->UpdateWindow();
}
