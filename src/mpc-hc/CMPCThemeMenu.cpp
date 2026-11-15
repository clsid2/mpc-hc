#include "stdafx.h"
#include "CMPCThemeMenu.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include <strsafe.h>
#include "AppSettings.h"
#include "PPageAccelTbl.h"
#include "mplayerc.h"

std::unordered_set<MenuObject*> CMPCThemeMenu::liveItems;
std::mutex CMPCThemeMenu::liveItemsMutex;
HBRUSH CMPCThemeMenu::bgBrush = 0;
HBRUSH CMPCThemeMenu::bgMenubarBrush = 0;
std::map<UINT, std::unique_ptr<CMPCThemeMenu::Dimensions>> CMPCThemeMenu::dimensionsByDpi;
CCritSec CMPCThemeMenu::resourceLock;

IMPLEMENT_DYNAMIC(CMPCThemeMenu, CMenu);
CMPCThemeMenu::CMPCThemeMenu()
{
}

CMPCThemeMenu::~CMPCThemeMenu()
{
    {
        std::lock_guard<std::mutex> guard(liveItemsMutex);
        for (u_int i = 0; i < allocatedItems.size(); i++) {
            liveItems.erase(allocatedItems[i]);
            delete allocatedItems[i];
        }
    }
    allocatedItems.clear();

    for (u_int i = 0; i < allocatedMenus.size(); i++) {
        allocatedMenus[i]->Detach();
        delete allocatedMenus[i];
    }
    allocatedMenus.clear();
}

void CMPCThemeMenu::clearDimensions()
{
    CAutoLock cAutoLock(&resourceLock);
    dimensionsByDpi.clear();
}

CMPCThemeMenu::Dimensions& CMPCThemeMenu::getDimensions(UINT dpiValue)
{
    std::unique_ptr<Dimensions>& d = dimensionsByDpi[dpiValue];
    if (!d) {
        d = std::make_unique<Dimensions>();
        DpiHelper dpi = DpiHelper();
        dpi.Override(dpiValue, dpiValue);

        d->subMenuPadding = dpi.ScaleX(20);
        d->iconSpacing = dpi.ScaleX(22);
        d->iconPadding = dpi.ScaleX(10);
        d->rowPadding = dpi.ScaleY(4) + 3; //windows 10 explorer has paddings of 7,8,9,9,11--this yields 7,8,9,10,11
        d->separatorPadding = dpi.ScaleX(8);
        d->separatorHeight = dpi.ScaleX(7);
        d->postTextSpacing = dpi.ScaleX(20);
        d->accelSpacing = dpi.ScaleX(30);
        d->hoverInsetX = dpi.ScaleX(5); //windows 11 insets the hover about 5px from the popup edge and rounds it by 4px, measured at 100%
        d->hoverInsetY = dpi.ScaleY(2);
        d->hoverRadius = dpi.ScaleX(4);
        CMPCThemeUtil::getFontByTypeForDpi(d->font, CMPCThemeUtil::MenuFont, dpiValue);
        CMPCThemeUtil::getFontByFaceForDpi(d->symbolFont, CMPCTheme::uiSymbolFont, 14, dpiValue, FW_BOLD);
        CMPCThemeUtil::getFontByFaceForDpi(d->bulletFont, CMPCTheme::uiSymbolFont, 6, dpiValue, FW_REGULAR);
        CMPCThemeUtil::getFontByFaceForDpi(d->checkFont, CMPCTheme::uiSymbolFont, 10, dpiValue, FW_REGULAR);
    }
    return *d;
}

UINT CMPCThemeMenu::getOwnerDpi()
{
    //a popup is measured for the window tracking it, which may be on another monitor than the main window
    //(e.g. an edit control in the options dialog); the menubar and system menu have no tracking window
    HWND hOwner = AfxGetThreadState()->m_hTrackingWindow;
    if (!hOwner) {
        hOwner = AfxGetMainWnd()->GetSafeHwnd();
    }
    DpiHelper dpi = DpiHelper();
    dpi.Override(hOwner);
    return dpi.DPIY();
}

UINT CMPCThemeMenu::findID(UINT& nPos, bool byCommand)
{
    int iMaxItems = GetMenuItemCount();

    UINT nID;
    if (byCommand) {
        nID = nPos;
        bool found = false;
        for (int j = 0; j < iMaxItems; j++) {
            if (nID == GetMenuItemID(j)) {
                nPos = j;
                found = true;
                break;
            }
        }
        if (!found) {
            return (UINT) - 1;
        }
    } else {
        nID = GetMenuItemID(nPos);
        if (nID == 0xFFFFFFFF) { //submenu, have to find the old-fashioned way
            MENUITEMINFO mii = { sizeof(mii) };
            mii.fMask = MIIM_ID;
            GetMenuItemInfo(nPos, &mii, TRUE);
            nID = mii.wID;
        }
    }
    return nID;
}

void CMPCThemeMenu::cleanupItem(UINT nPosition, UINT nFlags)
{
    if (AppIsThemeLoaded()) {
        MENUITEMINFO tInfo = { sizeof(MENUITEMINFO) };
        tInfo.fMask = MIIM_DATA;
        GetMenuItemInfo(nPosition, &tInfo, 0 != (nFlags & MF_BYPOSITION));
        MenuObject* pObject = (MenuObject*)tInfo.dwItemData;
        if (std::find(allocatedItems.begin(), allocatedItems.end(), pObject) != allocatedItems.end()) {
            removeItem(pObject);
            delete pObject;
        }
    }
}

void CMPCThemeMenu::addItem(MenuObject* pObject)
{
    allocatedItems.push_back(pObject);
    std::lock_guard<std::mutex> guard(liveItemsMutex);
    liveItems.insert(pObject);
}

void CMPCThemeMenu::removeItem(MenuObject* pObject)
{
    {
        std::lock_guard<std::mutex> guard(liveItemsMutex);
        liveItems.erase(pObject);
    }
    allocatedItems.erase(std::remove(allocatedItems.begin(), allocatedItems.end(), pObject), allocatedItems.end());
}

BOOL CMPCThemeMenu::DeleteMenu(UINT nPosition, UINT nFlags)
{
    cleanupItem(nPosition, nFlags);
    return CMenu::DeleteMenu(nPosition, nFlags);
}

BOOL CMPCThemeMenu::RemoveMenu(UINT nPosition, UINT nFlags)
{
    cleanupItem(nPosition, nFlags);
    if (nFlags & MF_BYPOSITION) {
        CMenu *t  = GetSubMenu(nPosition);
        //only release wrappers we own; a submenu owned elsewhere (e.g. a CMainFrame member) stays attached
        if (t && std::find(allocatedMenus.begin(), allocatedMenus.end(), t) != allocatedMenus.end()) {
            t->Detach();
            allocatedMenus.erase(std::remove(allocatedMenus.begin(), allocatedMenus.end(), t), allocatedMenus.end());
            delete t;
        }
    }
    return CMenu::RemoveMenu(nPosition, nFlags);
}

BOOL CMPCThemeMenu::DeleteThemedMenu(CMenu* menu, UINT nPosition, UINT nFlags)
{
    CMPCThemeMenu* tMenu = DYNAMIC_DOWNCAST(CMPCThemeMenu, menu);
    if (nullptr != tMenu) {
        return tMenu->DeleteMenu(nPosition, nFlags);
    }
    return menu->DeleteMenu(nPosition, nFlags);
}

BOOL CMPCThemeMenu::RemoveThemedMenu(CMenu* menu, UINT nPosition, UINT nFlags)
{
    CMPCThemeMenu* tMenu = DYNAMIC_DOWNCAST(CMPCThemeMenu, menu);
    if (nullptr != tMenu) {
        return tMenu->RemoveMenu(nPosition, nFlags);
    }
    return menu->RemoveMenu(nPosition, nFlags);
}

BOOL CMPCThemeMenu::SetThemedMenuItemInfo(UINT uItem, LPMENUITEMINFO lpMenuItemInfo, BOOL fByPos) {
    bool rebuildData = false;
    bool isMenuBar = false;
    if (AppIsThemeLoaded()) {
        MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
        mii.fMask = MIIM_DATA;
        CMenu::GetMenuItemInfo(uItem, &mii, fByPos);
        rebuildData = (0 != (lpMenuItemInfo->fMask & (MIIM_FTYPE | MIIM_SUBMENU | MIIM_STRING)));
        if (mii.dwItemData && rebuildData) {
            MenuObject* tm = (MenuObject*)mii.dwItemData;
            isMenuBar = tm->isMenubar;
            lpMenuItemInfo->fMask |= MIIM_DATA;
            lpMenuItemInfo->dwItemData = 0;
            cleanupItem(uItem, fByPos ? MF_BYPOSITION : MF_BYCOMMAND);
        }
    }

    BOOL ret = CMenu::SetMenuItemInfo(uItem, lpMenuItemInfo, fByPos);

    if (rebuildData) {
        fulfillThemeReqsItem((UINT)uItem, !fByPos, isMenuBar);
    }
    return ret;
}

BOOL CMPCThemeMenu::SetThemedMenuItemInfo(CMenu* menu, UINT uItem, LPMENUITEMINFO lpMenuItemInfo, BOOL fByPos) {
    if (menu) {
        if (AppIsThemeLoaded()) {
            CMPCThemeMenu *tMenu = DYNAMIC_DOWNCAST(CMPCThemeMenu, menu);
            if (nullptr != tMenu) {
                return tMenu->SetThemedMenuItemInfo(uItem, lpMenuItemInfo, fByPos);
            }
        } else {
            return menu->SetMenuItemInfo(uItem, lpMenuItemInfo, fByPos);
        }
    }
    return 0;
}

BOOL CMPCThemeMenu::AppendMenu(UINT nFlags, UINT_PTR nIDNewItem, LPCTSTR lpszNewItem)
{
    BOOL ret = CMenu::AppendMenu(nFlags, nIDNewItem, lpszNewItem);
    UINT numItems = GetMenuItemCount();
    if (numItems > 0) {
        //this guarantees we will find the item just inserted, in case id is not unique (0)
        fulfillThemeReqsItem(numItems - 1);
    }
    return ret;
}

void CMPCThemeMenu::resetBrushes()
{
    if (bgBrush) {
        ::DeleteObject(bgBrush);
        bgBrush = 0;
    }
    if (bgMenubarBrush) {
        ::DeleteObject(bgMenubarBrush);
        bgMenubarBrush = 0;
    }
}

void CMPCThemeMenu::fulfillThemeReqs(bool isMenubar)
{
    if (AppIsThemeLoaded()) {
        //with native menus only the menubar is drawn by us--popups are left to windows
        bool ownerDraw = isMenubar || AppNeedsThemedMenus();

        if (ownerDraw) {
            MENUINFO oldInfo = { sizeof(MENUINFO) };
            oldInfo.fMask = MIM_STYLE;
            GetMenuInfo(&oldInfo);

            MENUINFO MenuInfo = { 0 };
            MenuInfo.cbSize = sizeof(MENUINFO);
            MenuInfo.fMask = MIM_BACKGROUND | MIM_STYLE;
            if (AppNeedsThemedMenus()) {
                MenuInfo.fMask |= MIM_APPLYTOSUBMENUS;
            }
            MenuInfo.dwStyle = oldInfo.dwStyle;
            if (!bgBrush) {
                bgBrush = ::CreateSolidBrush(CMPCTheme::MenuBGColor);
            }
            if (!bgMenubarBrush) {
                bgMenubarBrush = ::CreateSolidBrush(CMPCTheme::MenubarBGColor);
            }
            if (isMenubar) {
                MenuInfo.hbrBack = bgMenubarBrush;
            } else {
                MenuInfo.hbrBack = bgBrush;
            }
            SetMenuInfo(&MenuInfo);
        }

        int iMaxItems = GetMenuItemCount();
        for (int i = 0; i < iMaxItems; i++) {
            cleanupItem(i, MF_BYPOSITION);
            if (!ownerDraw) {
                fulfillThemeReqsSubMenu(i);
                continue;
            }
            CString nameHolder;
            MenuObject* pObject = DEBUG_NEW MenuObject;
            addItem(pObject);
            pObject->m_hIcon = NULL;
            pObject->isMenubar = isMenubar;
            if (i == 0) {
                pObject->isFirstMenuInMenuBar = true;
            }

            GetMenuString(i, pObject->m_strCaption, MF_BYPOSITION);

            UINT nID = GetMenuItemID(i);
            if (!isOSMenu) {
                pObject->m_strAccel = CPPageAccelTbl::MakeAccelShortcutLabel(nID);
            }

            MENUITEMINFO tInfo;
            ZeroMemory(&tInfo, sizeof(MENUITEMINFO));
            tInfo.fMask = MIIM_FTYPE;
            tInfo.cbSize = sizeof(MENUITEMINFO);
            GetMenuItemInfo(i, &tInfo, true);

            if (tInfo.fType & MFT_SEPARATOR) {
                pObject->isSeparator = true;
            }

            MENUITEMINFO mInfo;
            ZeroMemory(&mInfo, sizeof(MENUITEMINFO));

            mInfo.fMask = MIIM_FTYPE | MIIM_DATA;
            mInfo.fType = MFT_OWNERDRAW | tInfo.fType;
            mInfo.cbSize = sizeof(MENUITEMINFO);
            mInfo.dwItemData = (ULONG_PTR)pObject;
            CMenu::SetMenuItemInfo(i, &mInfo, true);

            fulfillThemeReqsSubMenu(i);
        }
    }
}

void CMPCThemeMenu::fulfillThemeReqsSubMenu(UINT nPos)
{
    CMenu* t = GetSubMenu(nPos);
    if (nullptr != t) {
        CMPCThemeMenu* pSubMenu;
        pSubMenu = DYNAMIC_DOWNCAST(CMPCThemeMenu, t);
        if (!pSubMenu) {
            pSubMenu = DEBUG_NEW CMPCThemeMenu;
            pSubMenu->setOSMenu(isOSMenu);
            allocatedMenus.push_back(pSubMenu);
            pSubMenu->Attach(t->Detach());
        }
        pSubMenu->fulfillThemeReqs();
    }
}

void CMPCThemeMenu::fulfillThemeReqsItem(UINT i, bool byCommand, bool isMenuBar)
{
    if (AppIsThemeLoaded() && !isMenuBar && !AppNeedsThemedMenus()) {
        UINT nPos = i;
        if (findID(nPos, byCommand) != (UINT)-1) {
            fulfillThemeReqsSubMenu(nPos);
        }
    } else if (AppIsThemeLoaded()) {
        MENUITEMINFO tInfo = { sizeof(MENUITEMINFO) };
        tInfo.fMask = MIIM_DATA | MIIM_FTYPE;
        GetMenuItemInfo(i, &tInfo, !byCommand);
        if (NULL == tInfo.dwItemData) {
            CString nameHolder;
            MenuObject* pObject = DEBUG_NEW MenuObject;
            addItem(pObject);
            pObject->m_hIcon = NULL;
            pObject->isMenubar = isMenuBar;

            UINT posOrCmd = byCommand ? MF_BYCOMMAND : MF_BYPOSITION;

            GetMenuString(i, pObject->m_strCaption, posOrCmd);

            UINT nPos = i;
            UINT nID = findID(nPos, byCommand);
            if (nID == -1) {
                return;
            }

            pObject->m_strAccel = CPPageAccelTbl::MakeAccelShortcutLabel(nID);

            if (tInfo.fType & MFT_SEPARATOR) {
                pObject->isSeparator = true;
            }

            MENUITEMINFO mInfo;
            ZeroMemory(&mInfo, sizeof(MENUITEMINFO));

            mInfo.fMask = MIIM_FTYPE | MIIM_DATA;
            mInfo.fType = MFT_OWNERDRAW | tInfo.fType;
            mInfo.cbSize = sizeof(MENUITEMINFO);
            mInfo.dwItemData = (ULONG_PTR)pObject;
            CMenu::SetMenuItemInfo(nPos, &mInfo, true);

            fulfillThemeReqsSubMenu(nPos);
        }
    }
}

void CMPCThemeMenu::fulfillThemeReqsItem(CMenu* parent, UINT i, bool byCommand)
{
    CMPCThemeMenu* t;
    if ((t = DYNAMIC_DOWNCAST(CMPCThemeMenu, parent)) != nullptr) {
        t->fulfillThemeReqsItem(i, byCommand);
    }
}

UINT CMPCThemeMenu::getPosFromID(CMenu* parent, UINT nID)
{
    int iMaxItems = parent->GetMenuItemCount();
    for (int j = 0; j < iMaxItems; j++) {
        if (nID == parent->GetMenuItemID(j)) {
            return j;
        }
    }
    return (UINT) - 1;
}

void CMPCThemeMenu::GetRects(const Dimensions& dim, RECT rcItem, CRect& rectFull, CRect& rectM, CRect& rectIcon, CRect& rectText, CRect& rectArrow)
{
    rectFull.CopyRect(&rcItem);
    rectM = rectFull;
    rectIcon.SetRect(rectM.left, rectM.top, rectM.left + dim.iconSpacing, rectM.bottom);
    rectText.SetRect(rectM.left + dim.iconSpacing + dim.iconPadding, rectM.top, rectM.right - dim.subMenuPadding, rectM.bottom);
    rectArrow.SetRect(rectM.right - dim.subMenuPadding, rectM.top, rectM.right, rectM.bottom);
}

void CMPCThemeMenu::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CAutoLock cAutoLock(&resourceLock); //make sure our resources are protected
    std::lock_guard<std::mutex> guard(liveItemsMutex);
    MenuObject* menuObject = (MenuObject*)lpDrawItemStruct->itemData;
    if (!liveItems.count(menuObject)) { //not one of ours, or already deleted
        return;
    }
    Dimensions& dim = getDimensions(menuObject->dpi ? menuObject->dpi : getOwnerDpi());
    if (dim.font.m_hObject) {
        MENUITEMINFO mInfo = { sizeof(MENUITEMINFO) };

        mInfo.fMask = MIIM_FTYPE | MIIM_SUBMENU;
        if (lpDrawItemStruct->itemID) {
            GetMenuItemInfo(lpDrawItemStruct->itemID, &mInfo);
        } else {
            //itemID=0 is default for anything inserted without specifying ID (separator).
            //result can be finding the first separator rather than a valid item with id=0
            MENUITEMINFO byDataInfo = { sizeof(MENUITEMINFO) };
            byDataInfo.fMask = MIIM_DATA | MIIM_ID;
            for (int a = 0; a < GetMenuItemCount(); a++) {
                GetMenuItemInfo((UINT)a, &byDataInfo, true);
                if (byDataInfo.wID == 0 && byDataInfo.dwItemData == lpDrawItemStruct->itemData) {
                    GetMenuItemInfo((UINT)a, &mInfo, true);
                    break;
                }
            }
        }

        CRect rectFull;
        CRect rectM;
        CRect rectIcon;
        CRect rectText;
        CRect rectArrow;

        GetRects(dim, lpDrawItemStruct->rcItem, rectFull, rectM, rectIcon, rectText, rectArrow);

        UINT captionAlign = DT_LEFT;

        COLORREF ArrowColor = CMPCTheme::SubmenuColor;
        COLORREF TextFGColor;
        COLORREF TextBGColor;
        COLORREF TextSelectColor;

        if (menuObject->isMenubar) {
            TextBGColor = CMPCTheme::MenubarBGColor;
            TextSelectColor = CMPCTheme::MenubarSelectedBGColor;
        } else {
            TextBGColor = CMPCTheme::MenuBGColor;
            TextSelectColor = CMPCTheme::MenuSelectedColor;
        }

        CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

        if ((lpDrawItemStruct->itemState & ODS_DISABLED)) {
            TextFGColor = CMPCTheme::MenuItemDisabledColor;
            ArrowColor = CMPCTheme::MenuItemDisabledColor;
        } else if (menuObject->isMenubar && GetForegroundWindow() != AfxGetMainWnd()->m_hWnd) {
            TextFGColor = CMPCTheme::TextFGColorFade;
        } else {
            TextFGColor = CMPCTheme::TextFGColor;
        }

        int oldBKMode = pDC->SetBkMode(TRANSPARENT);
        pDC->FillSolidRect(&rectM, TextBGColor);

        if (menuObject->isMenubar) {
            if (menuObject->isFirstMenuInMenuBar) { //clean up white borders
                CRect wndSize;
                ::GetClientRect(AfxGetMainWnd()->m_hWnd, &wndSize);

                CRect rectBorder(rectM.left, rectM.bottom, rectM.left + wndSize.Width(), rectM.bottom + 1);
                pDC->FillSolidRect(&rectBorder, CMPCTheme::MainMenuBorderColor);
                ExcludeClipRect(lpDrawItemStruct->hDC, rectBorder.left, rectBorder.top, rectBorder.right, rectBorder.bottom);
            }
            rectM = rectFull;
            rectText = rectFull;
            captionAlign = DT_CENTER;
        }

        if (mInfo.fType & MFT_SEPARATOR) {
            int centerOffset = (dim.separatorHeight - 1) / 2;
            CRect rectSeparator(rectM.left + dim.separatorPadding, rectM.top + centerOffset, rectM.right - dim.separatorPadding, rectM.top + centerOffset + 1);
            pDC->FillSolidRect(&rectSeparator, CMPCTheme::MenuSeparatorColor);
        } else {
            COLORREF oldTextFGColor = pDC->SetTextColor(TextFGColor);

            CFont* pOldFont = pDC->GetCurrentFont();
            pDC->SelectObject(&dim.font);
            if ((lpDrawItemStruct->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) && (lpDrawItemStruct->itemAction & (ODA_SELECT | ODA_DRAWENTIRE))) {
                if (CMPCTheme::isWindows11Style && !menuObject->isMenubar) {
                    //windows 11 style: the hover is an inset rounded pill rather than the full row
                    Gdiplus::Graphics gfx(pDC->m_hDC);
                    gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias8x8);
                    gfx.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
                    const Gdiplus::REAL l = (Gdiplus::REAL)(rectM.left + dim.hoverInsetX), t = (Gdiplus::REAL)(rectM.top + dim.hoverInsetY);
                    const Gdiplus::REAL w = (Gdiplus::REAL)(rectM.Width() - 2 * dim.hoverInsetX), h = (Gdiplus::REAL)(rectM.Height() - 2 * dim.hoverInsetY);
                    const Gdiplus::REAL e = (Gdiplus::REAL)(2 * dim.hoverRadius);
                    Gdiplus::GraphicsPath path;
                    path.AddArc(l, t, e, e, 180, 90);
                    path.AddArc(l + w - e, t, e, e, 270, 90);
                    path.AddArc(l + w - e, t + h - e, e, e, 0, 90);
                    path.AddArc(l, t + h - e, e, e, 90, 90);
                    path.CloseFigure();
                    Gdiplus::SolidBrush brush(Gdiplus::Color(GetRValue(TextSelectColor), GetGValue(TextSelectColor), GetBValue(TextSelectColor)));
                    gfx.FillPath(&brush, &path);
                } else {
                    pDC->FillSolidRect(&rectM, TextSelectColor);
                }
            }
            CString left, right;
            GetStrings(menuObject, left, right);

            UINT accelStyle = 0;
            if (lpDrawItemStruct->itemState & ODS_NOACCEL) { //removing single &s before drawtext
                accelStyle = DT_HIDEPREFIX;
            }
            pDC->DrawTextW(left, rectText, DT_VCENTER | captionAlign | DT_SINGLELINE | accelStyle);

            if (!menuObject->isMenubar) {

                if (right.GetLength() > 0) {
                    pDC->DrawTextW(right, rectText, DT_VCENTER | DT_RIGHT | DT_SINGLELINE | accelStyle);
                }

                if (mInfo.hSubMenu) {
                    pDC->SelectObject(&dim.symbolFont);
                    pDC->SetTextColor(ArrowColor);
                    pDC->DrawTextW(TEXT(">"), rectArrow, DT_VCENTER | DT_CENTER | DT_SINGLELINE);
                }

                if (lpDrawItemStruct->itemState & ODS_CHECKED) {
                    CString check;
                    if (mInfo.fType & MFT_RADIOCHECK) {
                        check = TEXT("\u25CF"); //bullet
                        pDC->SelectObject(&dim.bulletFont);
                    } else {
                        check = TEXT("\u2714"); //checkmark
                        pDC->SelectObject(&dim.checkFont);
                    }
                    pDC->SetTextColor(TextFGColor);
                    pDC->DrawTextW(check, rectIcon, DT_VCENTER | DT_CENTER | DT_SINGLELINE);
                }
            }

            pDC->SetBkMode(oldBKMode);
            pDC->SetTextColor(oldTextFGColor);
            pDC->SelectObject(pOldFont);
        }
        ExcludeClipRect(lpDrawItemStruct->hDC, rectFull.left, rectFull.top, rectFull.right, rectFull.bottom);
    }
}

void CMPCThemeMenu::GetStrings(MenuObject* mo, CString& left, CString& right)
{
    if (mo->m_strAccel.GetLength() > 0) {
        left = mo->m_strCaption;
        right = mo->m_strAccel;
    } else {
        CString text = mo->m_strCaption;
        if (!AfxExtractSubString(left, text, 0, _T('\t'))) {
            left = _T("");
        }
        if (!AfxExtractSubString(right, text, 1, _T('\t'))) {
            right = _T("");
        }
    }
}

void CMPCThemeMenu::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
    if (!MeasureMenuItem(lpMeasureItemStruct)) {
        ASSERT(false);
    }
}

bool CMPCThemeMenu::MeasureMenuItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
    //uses nothing but itemData and the shared resources, so any menu's items can be measured here
    if (lpMeasureItemStruct && lpMeasureItemStruct->CtlType == ODT_MENU) {
        CAutoLock cAutoLock(&resourceLock); //make sure our resources are protected
        std::lock_guard<std::mutex> guard(liveItemsMutex);

        MenuObject* mo = (MenuObject*)lpMeasureItemStruct->itemData;
        if (!liveItems.count(mo)) { //not one of ours, or already deleted
            return false;
        }
        mo->dpi = getOwnerDpi(); //drawitem uses the same dpi, to fill the size windows caches
        Dimensions& dim = getDimensions(mo->dpi);

        if (mo->isSeparator) {
            lpMeasureItemStruct->itemWidth = 0;
            lpMeasureItemStruct->itemHeight = dim.separatorHeight;
        } else {
            HWND mainWnd = AfxGetMainWnd()->GetSafeHwnd();
            HDC hDC = ::GetDC(mainWnd);
            if (hDC) {
                CSize height = CMPCThemeUtil::GetTextSize(_T("W"), hDC, &dim.font);
                if (mo->isMenubar) {
                    CSize cs = CMPCThemeUtil::GetTextSize(mo->m_strCaption, hDC, &dim.font);
                    lpMeasureItemStruct->itemWidth = cs.cx;
                    lpMeasureItemStruct->itemHeight = height.cy + dim.rowPadding;
                } else {
                    CString left, right;
                    GetStrings(mo, left, right);
                    CSize cs = CMPCThemeUtil::GetTextSize(left, hDC, &dim.font);
                    lpMeasureItemStruct->itemHeight = height.cy + dim.rowPadding;
                    lpMeasureItemStruct->itemWidth = dim.iconSpacing + dim.postTextSpacing + dim.subMenuPadding + cs.cx;
                    if (right.GetLength() > 0) {
                        CSize csAccel = CMPCThemeUtil::GetTextSize(right, hDC, &dim.font);
                        lpMeasureItemStruct->itemWidth += dim.accelSpacing + csAccel.cx;
                    }
                }
                ::ReleaseDC(mainWnd, hDC);
            }
        }
        return true;
    }
    return false;
}

CMPCThemeMenu* CMPCThemeMenu::GetSubMenu(int nPos)
{
    return (CMPCThemeMenu*) CMenu::GetSubMenu(nPos);
}

void CMPCThemeMenu::updateItem(CCmdUI* pCmdUI)
{
    CMenu* cm = pCmdUI->m_pMenu;

    if (DYNAMIC_DOWNCAST(CMPCThemeMenu, cm)) {
        MENUITEMINFO mInfo = { sizeof(MENUITEMINFO) };
        mInfo.fMask = MIIM_DATA;
        VERIFY(cm->GetMenuItemInfo(pCmdUI->m_nID, &mInfo));

        MenuObject* menuObject = (MenuObject*)mInfo.dwItemData;
        if (menuObject) {
            cm->GetMenuString(pCmdUI->m_nID, menuObject->m_strCaption, MF_BYCOMMAND);
        }
    }
}
