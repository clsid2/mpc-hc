#include "stdafx.h"
#include "CMPCThemeEdit.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"
#include "CMPCThemeMenu.h"
#include <memory>

std::unique_ptr<CMPCThemeMenu> editMenu;
CMPCThemeEdit::CMPCThemeEdit() : CEdit(), CMPCThemeScrollBarRenderer()
{
    buddy = nullptr;
    isFileDialogChild = false;
}

CMPCThemeEdit::~CMPCThemeEdit()
{
}

IMPLEMENT_DYNAMIC(CMPCThemeEdit, CEdit)
BEGIN_MESSAGE_MAP(CMPCThemeEdit, CEdit)
    ON_WM_NCPAINT()
    ON_REGISTERED_MESSAGE(WMU_RESIZESUPPORT, ResizeSupport)
    ON_MESSAGE(WM_CONTEXTMENU, OnContextMenu)
    ON_WM_MEASUREITEM()
END_MESSAGE_MAP()

void CMPCThemeEdit::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct) {
    if (lpMeasureItemStruct->CtlType == ODT_MENU && editMenu) {
        editMenu->MeasureItem(lpMeasureItemStruct);
        return;
    }

    CEdit::OnMeasureItem(nIDCtl, lpMeasureItemStruct);
}

LRESULT CMPCThemeEdit::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (AppNeedsThemedControls()) {
        CMPCThemeScrollBarRenderer::ProcessMessage(m_hWnd, message, wParam, lParam);
    }
    LRESULT result = __super::WindowProc(message, wParam, lParam);
    return result;
}

//on Edit class, cbWndExtra allocates space for a pointer, which points to this data
struct PRIVATEWNDDATA {
    LONG_PTR ptr1;
    DWORD D04, D08;
    DWORD textLength;
    DWORD D10;
    DWORD startSelection, endSelection, cursorPosition;
    DWORD DW1[5];
    LONG_PTR ptr2;
    DWORD hwnd;        //32-bit storage, even on 64-bit (HWND is wrong size)
    DWORD DW2[7];
    DWORD parentHwnd;  //32-bit storage, even on 64-bit (HWND is wrong size)
    DWORD D3;
    LONG_PTR ptr3;
    DWORD D4;
    DWORD flags;       //EDIT control flags
};

//ContextMenu constants
#define ENABLE_UNICODE_CONTROL_CHARS 0x40000000
#define IDS_OPEN_IME 0x1052
#define IDS_CLOSE_IME 0x1053
#define IDS_IME_RECONVERSION 0x1056
#define ID_CONTEXT_MENU_IME 0x2711
#define ID_CONTEXT_MENU_RECONVERSION 0x2713
#define SCS_FEATURES_NEEDED (SCS_CAP_MAKEREAD | SCS_CAP_SETRECONVERTSTRING)
#define EDIT_CONTEXT_MENU 1

bool IsBidiLocale() {
    DWORD layout;
    //see https://web.archive.org/web/20131013052748/http://blogs.msdn.com/b/michkap/archive/2012/01/13/10256391.aspx
    if (GetLocaleInfoW(GetUserDefaultUILanguage(), LOCALE_IREADINGLAYOUT | LOCALE_RETURN_NUMBER, (LPWSTR)&layout, 2)) {
        if (layout == 1) {
            return true;
        }
    }
    return false;
}

std::array<int, 17> UnicodeChars = {
    0x200D, //ZWJ  -- Zero width &joiner
    0x200C, //ZWNJ -- Zero width &non-joiner
    0x200E, //RLM  -- &Right-to-left mark 
    0x200F, //LRM  -- &Left-to-right mark
    0x202A, //LRE  -- Start of left-to-right embedding 
    0x202B, //RLE  -- Start of right-to-left embedding 
    0x202D, //LRO  -- Start of left-to-right override 
    0x202E, //RLO  -- Start of right-to-left override 
    0x202C, //PDF  -- Pop directional formatting 
    0x206E, //NADS -- National digit shapes substitution 
    0x206F, //NODS -- Nominal (European) digit shapes 
    0x206B, //ASS  -- Activate symmetric swapping 
    0x206A, //ISS  -- Inhibit symmetric swapping 
    0x206D, //AAFS -- Activate Arabic form shaping 
    0x206C, //IAFS -- Inhibit Arabic form shaping 
    0x001E, //RS   -- Record Separator (Block separator) 
    0x001F, //US   -- Unit Separator (Segment separator) 
};

void CMPCThemeEdit::SetCompWindowPos(HIMC himc, UINT start) {
    COMPOSITIONFORM cf = { CFS_POINT, PosFromChar(start), {0} };
    ImmSetCompositionWindow(himc, &cf);
}

LRESULT CMPCThemeEdit::OnContextMenu(WPARAM wParam, LPARAM lParam) {
    if (AppIsThemeLoaded()) {
        if (GetFocus() != this) {
            SetFocus();
        }
        HMODULE g_hinst = GetModuleHandleW(L"comctl32.dll");
        if (g_hinst) {
            HMENU menu = LoadMenuW(g_hinst, (LPCWSTR)EDIT_CONTEXT_MENU);
            if (menu) {
                HMENU popup = GetSubMenu(menu, 0);
                if (popup) {
                    CPoint pt;
                    pt.x = GET_X_LPARAM(lParam);
                    pt.y = GET_Y_LPARAM(lParam);

                    DWORD rtlStyle = WS_EX_RTLREADING | WS_EX_RIGHT;
                    bool isRTL = (GetExStyle() & rtlStyle);

                    bool supportUnicodeControl = false, showControlChars = false;
                    PRIVATEWNDDATA *pwd = (PRIVATEWNDDATA *)GetWindowLongPtrW(m_hWnd, 0);
                    if (pwd && pwd->hwnd == static_cast<DWORD>((LONG_PTR)m_hWnd)) {       //sanity check
                        supportUnicodeControl = true;
                        showControlChars = (pwd->flags & ENABLE_UNICODE_CONTROL_CHARS);
                    }

                    if (pt.x == -1 && pt.y == -1) { //VK_APPS
                        CRect rc;
                        GetClientRect(&rc);

                        pt.x = rc.left + (rc.right - rc.left) / 2;
                        pt.y = rc.top + (rc.bottom - rc.top) / 2;
                        ClientToScreen(&pt);
                    }

                    UINT start = HIWORD(GetSel());
                    UINT end = LOWORD(GetSel());
                    DWORD style = GetStyle();
                    bool isPasswordField = (style & ES_PASSWORD);
                    CStringW str;
                    GetWindowTextW(str);


                    if (end < start) {
                        std::swap(end, start);
                    }
                    int selectionLength = end - start;

                    editMenu = std::make_unique<CMPCThemeMenu>();
                    editMenu->setOSMenu(true);
                    editMenu->Attach(popup);
                    editMenu->EnableMenuItem(EM_UNDO, MF_BYCOMMAND | (CanUndo() && !(style & ES_READONLY) ? MF_ENABLED : MF_GRAYED));
                    editMenu->EnableMenuItem(WM_CUT, MF_BYCOMMAND | (selectionLength && !isPasswordField && !(style & ES_READONLY) ? MF_ENABLED : MF_GRAYED));
                    editMenu->EnableMenuItem(WM_COPY, MF_BYCOMMAND | (selectionLength && !isPasswordField ? MF_ENABLED : MF_GRAYED));
                    editMenu->EnableMenuItem(WM_PASTE, MF_BYCOMMAND | (IsClipboardFormatAvailable(CF_UNICODETEXT) && !(style & ES_READONLY) ? MF_ENABLED : MF_GRAYED));
                    editMenu->EnableMenuItem(WM_CLEAR, MF_BYCOMMAND | (selectionLength && !(style & ES_READONLY) ? MF_ENABLED : MF_GRAYED)); //delete
                    editMenu->EnableMenuItem(EM_SETSEL, MF_BYCOMMAND | (start || (end != str.GetLength()) ? MF_ENABLED : MF_GRAYED)); //select all
                    editMenu->EnableMenuItem(WM_APP, MF_BYCOMMAND | MF_ENABLED); //RTL
                    editMenu->CheckMenuItem(WM_APP, MF_BYCOMMAND | (isRTL ? MF_CHECKED : MF_UNCHECKED));

                    if (supportUnicodeControl) {
                        editMenu->EnableMenuItem(WM_APP + 1, MF_BYCOMMAND | MF_ENABLED); //show unicode control chars
                        editMenu->CheckMenuItem(WM_APP + 1, MF_BYCOMMAND | (showControlChars ? MF_CHECKED : MF_UNCHECKED));
                    }

                    //enable all unicode char inserts 
                    for (UINT idx = WM_APP + 0x2; idx <= WM_APP + 0x13; idx++) {
                        editMenu->EnableMenuItem(idx, MF_BYCOMMAND | MF_ENABLED);
                    }

                    HIMC himc = nullptr;
                    HKL hkl = GetKeyboardLayout(NULL);
                    if (ImmIsIME(hkl)) {
                        himc = ImmGetContext(this->m_hWnd);
                        if (himc) {
                            bool checked = false, enabled = true;
                            MENUITEMINFOW mii = { sizeof(MENUITEMINFOW) };
                            mii.fMask = MIIM_ID | MIIM_STRING;
                            mii.wID = ID_CONTEXT_MENU_IME;
                            CStringW miS;
                            bool strLoaded;

                            if (::ImmGetOpenStatus(himc)) {
                                strLoaded = miS.LoadStringW(g_hinst, IDS_CLOSE_IME);
                            } else {
                                strLoaded = miS.LoadStringW(g_hinst, IDS_OPEN_IME);
                            }

                            if (strLoaded) {
                                mii.dwTypeData = (LPWSTR)miS.GetString();
                                mii.cch = (UINT)wcslen(mii.dwTypeData);

                                editMenu->InsertMenuItemW(editMenu->GetMenuItemCount(), &mii);
                            }

                            strLoaded = miS.LoadStringW(g_hinst, IDS_IME_RECONVERSION);
                            if (strLoaded) {
                                mii.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
                                mii.dwTypeData = (LPWSTR)miS.GetString();
                                mii.cch = (UINT)wcslen(mii.dwTypeData);
                                mii.wID = ID_CONTEXT_MENU_RECONVERSION;
                                bool supportsReconversion = (ImmGetProperty(hkl, IGP_SETCOMPSTR) & SCS_FEATURES_NEEDED) == SCS_FEATURES_NEEDED;
                                if (!supportsReconversion || !selectionLength || isPasswordField) {
                                    mii.fState = MFS_DISABLED;
                                } else {
                                    mii.fState = MFS_ENABLED;
                                }
                                editMenu->InsertMenuItemW(editMenu->GetMenuItemCount(), &mii);
                            }
                        }
                    }
                    editMenu->fulfillThemeReqs();

                    UINT cmd = editMenu->TrackPopupMenu((IsBidiLocale() ? TPM_LAYOUTRTL : TPM_LEFTALIGN) | TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, this);

                    switch (cmd) {
                    case EM_UNDO:
                    case WM_CUT:
                    case WM_COPY:
                    case WM_PASTE:
                    case WM_CLEAR:
                        SendMessageW(cmd, 0, 0);
                        break;
                    case EM_SETSEL:
                        SendMessageW(EM_SETSEL, 0, -1);
                        break;
                    case WM_APP: //RTL
                        ModifyStyleEx(isRTL ? rtlStyle : 0, isRTL ? 0 : rtlStyle);
                        break;
                    case WM_APP + 1: //show unicode control chars
                        if (pwd) {
                            pwd->flags ^= ENABLE_UNICODE_CONTROL_CHARS;
                            Invalidate();
                        }
                        break;
                    case ID_CONTEXT_MENU_IME: //IME
                        if (himc) {
                            ImmSetOpenStatus(himc, !ImmGetOpenStatus(himc));
                        }
                        break;
                    case ID_CONTEXT_MENU_RECONVERSION:
                        if (himc) {
                            CStringW selected;
                            selected = str.Mid(start, end - start);
                            const size_t text_memory_byte = sizeof(wchar_t) * (selected.GetLength() + 1);
                            const size_t memory_block_size = sizeof(RECONVERTSTRING) + text_memory_byte;
                            std::unique_ptr<BYTE[]> memory_block{ new (std::nothrow) BYTE[memory_block_size] };
                            if (memory_block) {
                                SetCompWindowPos(himc, start);
                                RECONVERTSTRING* reconv{ reinterpret_cast<RECONVERTSTRING*>(memory_block.get()) };
                                reconv->dwSize = (DWORD)memory_block_size;
                                reconv->dwVersion = 0;
                                reconv->dwStrLen = selected.GetLength();
                                reconv->dwStrOffset = sizeof(RECONVERTSTRING);
                                reconv->dwCompStrLen = selected.GetLength();
                                reconv->dwCompStrOffset = 0;
                                reconv->dwTargetStrLen = selected.GetLength();
                                reconv->dwTargetStrOffset = reconv->dwCompStrOffset;
                                wchar_t* text{ reinterpret_cast<wchar_t*>(memory_block.get() + sizeof(RECONVERTSTRING)) };
                                memcpy_s(text, text_memory_byte, selected.GetBuffer(), text_memory_byte);
                                ImmSetCompositionStringW(himc, SCS_QUERYRECONVERTSTRING, (LPVOID)memory_block.get(), (DWORD)memory_block_size, nullptr, 0);
                                ImmSetCompositionStringW(himc, SCS_SETRECONVERTSTRING, (LPVOID)memory_block.get(), (DWORD)memory_block_size, nullptr, 0);
                            }
                        }
                    default:
                        int idx = cmd - (WM_APP + 0x2);
                        if (idx >= 0 && idx < UnicodeChars.size()) {
                            SendMessageW(WM_CHAR, UnicodeChars[idx], 0); break; //US   -- Unit Separator (Segment separator)
                        }
                        break;
                    }

                    if (himc) {
                        ::ImmReleaseContext(this->m_hWnd, himc);
                    }
                    editMenu.reset();
                    return 0;
                }
            }
        }
    }
    return Default();
}

bool CMPCThemeEdit::IsScrollable() {
    return 0 != (GetStyle() & (WS_VSCROLL | WS_HSCROLL));
}

//this message is sent by resizablelib
//we prevent clipping for multi-line edits due to using regions which conflict with resizablelib clipping
LRESULT CMPCThemeEdit::ResizeSupport(WPARAM wParam, LPARAM lParam) {
    if (AppNeedsThemedControls() && IsScrollable()) {
        if (wParam == RSZSUP_QUERYPROPERTIES) {
            LPRESIZEPROPERTIES props = (LPRESIZEPROPERTIES)lParam;
            props->bAskClipping = false;
            props->bCachedLikesClipping = false;
            return TRUE;
        }
    }
    return FALSE;
}

void CMPCThemeEdit::PreSubclassWindow()
{
    if (AppIsThemeLoaded()) {
        CRect r;
        GetClientRect(r);
        r.DeflateRect(2, 2); //some default padding for those spaceless fonts
        SetRect(r);
        if (CMPCThemeUtil::canUseWin10DarkTheme()) {
            SetWindowTheme(GetSafeHwnd(), L"DarkMode_Explorer", NULL);
        } else {
            SetWindowTheme(GetSafeHwnd(), L"", NULL);
        }
    } else {
        __super::PreSubclassWindow();
    }
}



void CMPCThemeEdit::OnNcPaint()
{
    if (AppNeedsThemedControls()) {
        if (IsScrollable()) {  //scrollable edit will be treated like a window, not a field
            HandleNcPaint(m_hWnd);
        } else {
            CWindowDC dc(this);

            CRect rect;
            GetWindowRect(&rect);
            rect.OffsetRect(-rect.left, -rect.top);

            //note: rc file with style "NOT WS_BORDER" will remove the default of WS_EX_CLIENTEDGE from EDITTEXT
            //WS_BORDER itself is not typically present
            auto stEx = GetExStyle();
            if (0 != (GetStyle() & WS_BORDER) || 0 != (GetExStyle() & WS_EX_CLIENTEDGE)) {
                CBrush brush;
                if (isFileDialogChild) {//special case for edits injected to file dialog
                    brush.CreateSolidBrush(CMPCTheme::W10DarkThemeFileDialogInjectedEditBorderColor);
                } else {
                    brush.CreateSolidBrush(CMPCTheme::EditBorderColor);
                }

                dc.FrameRect(&rect, &brush);
                brush.DeleteObject();
            }

            //added code to draw the inner rect for the border.  we shrunk the draw rect for border spacing earlier
            //normally, the bg of the dialog is sufficient, but in the case of ResizableDialog, it clips the anchored
            //windows, which leaves unpainted area just inside our border
            rect.DeflateRect(1, 1);
            CMPCThemeUtil::drawParentDialogBGClr(this, &dc, rect, false);

            if (nullptr != buddy) {
                buddy->Invalidate();
            }
        }

    } else {
        __super::OnNcPaint();
    }
}

void CMPCThemeEdit::SetFixedWidthFont(CFont& f)
{
    if (AppIsThemeLoaded()) {
        CWindowDC dc(this);
        if (CMPCThemeUtil::getFixedFont(font, &dc, this)) {
            SetFont(&font);
        } else {
            SetFont(&f);
        }
    } else {
        SetFont(&f);
    }
}
