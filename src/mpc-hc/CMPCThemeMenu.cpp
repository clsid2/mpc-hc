#include "stdafx.h"
#include "CMPCThemeMenu.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include <strsafe.h>
#include "AppSettings.h"
#include "PPageAccelTbl.h"
#include "mplayerc.h"

std::map<UINT, CMPCThemeMenu*> CMPCThemeMenu::subMenuIDs;
HBRUSH CMPCThemeMenu::bgBrush = 0;
HBRUSH CMPCThemeMenu::bgMenubarBrush = 0;
CFont CMPCThemeMenu::font;
CFont CMPCThemeMenu::symbolFont;
CFont CMPCThemeMenu::bulletFont;
CFont CMPCThemeMenu::checkFont;
bool CMPCThemeMenu::hasDimensions = false;
int CMPCThemeMenu::subMenuPadding;
int CMPCThemeMenu::iconSpacing;
int CMPCThemeMenu::iconPadding;
int CMPCThemeMenu::rowPadding;
int CMPCThemeMenu::separatorPadding;
int CMPCThemeMenu::separatorHeight;
int CMPCThemeMenu::postTextSpacing;
int CMPCThemeMenu::accelSpacing;
CCritSec CMPCThemeMenu::resourceLock;
std::mutex CMPCThemeMenu::submenuMutex;

IMPLEMENT_DYNAMIC(CMPCThemeMenu, CMenu);
CMPCThemeMenu::CMPCThemeMenu()
{
}

CMPCThemeMenu::~CMPCThemeMenu()
{
    {
        std::lock_guard<std::mutex> guard(submenuMutex);
        std::map<UINT, CMPCThemeMenu*>::iterator itr = subMenuIDs.begin();
        while (itr != subMenuIDs.end()) {
            if (itr->second == this) {
                itr = subMenuIDs.erase(itr);
            } else {
                ++itr;
            }
        }
    }

    for (u_int i = 0; i < allocatedItems.size(); i++) {
        delete allocatedItems[i];      
    }
    allocatedItems.clear();

    for (u_int i = 0; i < allocatedMenus.size(); i++) {
        allocatedMenus[i]->Detach();
        delete allocatedMenus[i];
    }
    allocatedMenus.clear();
}

void CMPCThemeMenu::initDimensions()
{
    if (!hasDimensions) {
        DpiHelper dpi = DpiHelper();
        dpi.Override(AfxGetMainWnd()->GetSafeHwnd());

        subMenuPadding = dpi.ScaleX(20);
        iconSpacing = dpi.ScaleX(22);
        iconPadding = dpi.ScaleX(10);
        rowPadding = dpi.ScaleY(4) + 3; //windows 10 explorer has paddings of 7,8,9,9,11--this yields 7,8,9,10,11
        separatorPadding = dpi.ScaleX(8);
        separatorHeight = dpi.ScaleX(7);
        postTextSpacing = dpi.ScaleX(20);
        accelSpacing = dpi.ScaleX(30);
        {
            CAutoLock cAutoLock(&resourceLock);
            if (font.m_hObject) {
                font.DeleteObject();
            }
            CMPCThemeUtil::getFontByType(font, AfxGetMainWnd(), CMPCThemeUtil::MenuFont);
            if (symbolFont.m_hObject) {
                symbolFont.DeleteObject();
            }
            CMPCThemeUtil::getFontByFace(symbolFont, AfxGetMainWnd(), CMPCTheme::uiSymbolFont, 14, FW_BOLD);
            if (bulletFont.m_hObject) {
                bulletFont.DeleteObject();
            }
            CMPCThemeUtil::getFontByFace(bulletFont, AfxGetMainWnd(), CMPCTheme::uiSymbolFont, 6, FW_REGULAR);
            if (checkFont.m_hObject) {
                checkFont.DeleteObject();
            }
            CMPCThemeUtil::getFontByFace(checkFont, AfxGetMainWnd(), CMPCTheme::uiSymbolFont, 10, FW_REGULAR);
        }
        hasDimensions = true;
    }
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
            allocatedItems.erase(std::remove(allocatedItems.begin(), allocatedItems.end(), pObject), allocatedItems.end());
            delete pObject;
        }
    }
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
        if (t) {
            t->Detach();
            if (std::find(allocatedMenus.begin(), allocatedMenus.end(), t) != allocatedMenus.end()) {
                allocatedMenus.erase(std::remove(allocatedMenus.begin(), allocatedMenus.end(), t), allocatedMenus.end());
                delete t;
            }
        }
    }
    return CMenu::RemoveMenu(nPosition, nFlags);
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

void CMPCThemeMenu::fulfillThemeReqs(bool isMenubar)
{
    if (AppIsThemeLoaded()) {
        MENUINFO oldInfo = { sizeof(MENUINFO) };
        oldInfo.fMask = MIM_STYLE;
        GetMenuInfo(&oldInfo);

        MENUINFO MenuInfo = { 0 };
        MenuInfo.cbSize = sizeof(MENUINFO);
        MenuInfo.fMask = MIM_BACKGROUND | MIM_STYLE | MIM_APPLYTOSUBMENUS;
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

        int iMaxItems = GetMenuItemCount();
        for (int i = 0; i < iMaxItems; i++) {
            CString nameHolder;
            MenuObject* pObject = DEBUG_NEW MenuObject;
            allocatedItems.push_back(pObject);
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

            subMenuIDs[nID] = this;

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

            CMenu* t = GetSubMenu(i);
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
    }
}

void CMPCThemeMenu::fulfillThemeReqsItem(UINT i, bool byCommand, bool isMenuBar)
{
    if (AppIsThemeLoaded()) {
        MENUITEMINFO tInfo = { sizeof(MENUITEMINFO) };
        tInfo.fMask = MIIM_DATA | MIIM_FTYPE;
        GetMenuItemInfo(i, &tInfo, !byCommand);
        if (NULL == tInfo.dwItemData) {
            CString nameHolder;
            MenuObject* pObject = DEBUG_NEW MenuObject;
            allocatedItems.push_back(pObject);
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

            subMenuIDs[nID] = this;

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

CMPCThemeMenu* CMPCThemeMenu::getParentMenu(UINT itemID)
{
    if (subMenuIDs.count(itemID) == 1) {
        CMPCThemeMenu* m = subMenuIDs.at(itemID);
        /* // checks if submenu for overriding of onmeasureitem (win32 limitation).
           // but mpc-hc doesn't set up some submenus until later
           // which is too late for measureitem to take place
           // so we return all items for measuring
        MENUITEMINFO mInfo;
        ZeroMemory(&mInfo, sizeof(MENUITEMINFO));
        mInfo.fMask = MIIM_SUBMENU;
        mInfo.cbSize = sizeof(MENUITEMINFO);
        m->GetMenuItemInfo(itemID, &mInfo);
        if (mInfo.hSubMenu)      //  */
        return m;
    }

    return nullptr;
}

void CMPCThemeMenu::GetRects(RECT rcItem, CRect& rectFull, CRect& rectM, CRect& rectIcon, CRect& rectText, CRect& rectArrow)
{
    rectFull.CopyRect(&rcItem);
    rectM = rectFull;
    rectIcon.SetRect(rectM.left, rectM.top, rectM.left + iconSpacing, rectM.bottom);
    rectText.SetRect(rectM.left + iconSpacing + iconPadding, rectM.top, rectM.right - subMenuPadding, rectM.bottom);
    rectArrow.SetRect(rectM.right - subMenuPadding, rectM.top, rectM.right, rectM.bottom);
}

void CMPCThemeMenu::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CAutoLock cAutoLock(&resourceLock); //make sure our resources are protected
    if (font.m_hObject) {
        MenuObject* menuObject = (MenuObject*)lpDrawItemStruct->itemData;

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

        GetRects(lpDrawItemStruct->rcItem, rectFull, rectM, rectIcon, rectText, rectArrow);

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
            int centerOffset = (separatorHeight - 1) / 2;
            CRect rectSeparator(rectM.left + separatorPadding, rectM.top + centerOffset, rectM.right - separatorPadding, rectM.top + centerOffset + 1);
            pDC->FillSolidRect(&rectSeparator, CMPCTheme::MenuSeparatorColor);
        } else {
            COLORREF oldTextFGColor = pDC->SetTextColor(TextFGColor);

            CFont* pOldFont = pDC->GetCurrentFont();
            pDC->SelectObject(&font);
            if ((lpDrawItemStruct->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) && (lpDrawItemStruct->itemAction & (ODA_SELECT | ODA_DRAWENTIRE))) {
                pDC->FillSolidRect(&rectM, TextSelectColor);
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
                    pDC->SelectObject(&symbolFont);
                    pDC->SetTextColor(ArrowColor);
                    pDC->DrawTextW(TEXT(">"), rectArrow, DT_VCENTER | DT_CENTER | DT_SINGLELINE);
                }

                if (lpDrawItemStruct->itemState & ODS_CHECKED) {
                    CString check;
                    if (mInfo.fType & MFT_RADIOCHECK) {
                        check = TEXT("\u25CF"); //bullet
                        pDC->SelectObject(&bulletFont);
                    } else {
                        check = TEXT("\u2714"); //checkmark
                        pDC->SelectObject(&checkFont);
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
    initDimensions(); //should happen before drawitem
    CAutoLock cAutoLock(&resourceLock); //make sure our resources are protected

    HWND mainWnd = AfxGetMainWnd()->GetSafeHwnd();
    HDC hDC = ::GetDC(mainWnd);
    MenuObject* mo = (MenuObject*)lpMeasureItemStruct->itemData;

    if (mo->isSeparator) {
        lpMeasureItemStruct->itemWidth = 0;
        lpMeasureItemStruct->itemHeight = separatorHeight;
    } else if (hDC) {
        CSize height = CMPCThemeUtil::GetTextSize(_T("W"), hDC, &font);
        if (mo->isMenubar) {
            CSize cs = CMPCThemeUtil::GetTextSize(mo->m_strCaption, hDC, &font);
            lpMeasureItemStruct->itemWidth = cs.cx;
            lpMeasureItemStruct->itemHeight = height.cy + rowPadding;
        } else {
            CString left, right;
            GetStrings(mo, left, right);
            CSize cs = CMPCThemeUtil::GetTextSize(left, hDC, &font);
            lpMeasureItemStruct->itemHeight = height.cy + rowPadding;
            lpMeasureItemStruct->itemWidth = iconSpacing + postTextSpacing + subMenuPadding + cs.cx;
            if (right.GetLength() > 0) {
                CSize csAccel = CMPCThemeUtil::GetTextSize(right, hDC, &font);
                lpMeasureItemStruct->itemWidth += accelSpacing + csAccel.cx;
            }
        }
    }
    ::ReleaseDC(mainWnd, hDC);
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
        cm->GetMenuString(pCmdUI->m_nID, menuObject->m_strCaption, MF_BYCOMMAND);
    }
}
