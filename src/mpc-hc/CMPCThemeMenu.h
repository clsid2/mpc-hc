#pragma once
#include <afxwin.h>
#include <unordered_set>

struct MenuObject {
    HICON m_hIcon;
    CString m_strCaption;
    CString m_strAccel;
    bool isMenubar = false;
    bool isSeparator = false;
    bool isFirstMenuInMenuBar = false;
};



class CMPCThemeMenu : public CMenu
{
    DECLARE_DYNAMIC(CMPCThemeMenu)
public:
    CMPCThemeMenu();
    virtual ~CMPCThemeMenu();

    void fulfillThemeReqs(bool menubar = false);
    void fulfillThemeReqsItem(UINT i, bool byCommand = false, bool isMenuBar = false);
    void fulfillThemeReqsSubMenu(UINT nPos);
    static void fulfillThemeReqsItem(CMenu* parent, UINT i, bool byCommand = false);
    static UINT getPosFromID(CMenu* parent, UINT nID);
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    static void GetStrings(MenuObject* mo, CString& left, CString& right);
    virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    static bool MeasureMenuItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    virtual BOOL AppendMenu(UINT nFlags, UINT_PTR nIDNewItem = 0, LPCTSTR lpszNewItem = NULL);
    virtual BOOL DeleteMenu(UINT nPosition, UINT nFlags);
    virtual BOOL RemoveMenu(UINT nPosition, UINT nFlags);
    static BOOL DeleteThemedMenu(CMenu* menu, UINT nPosition, UINT nFlags);
    static BOOL RemoveThemedMenu(CMenu* menu, UINT nPosition, UINT nFlags);
    virtual BOOL SetThemedMenuItemInfo(UINT uItem, LPMENUITEMINFO lpMenuItemInfo, BOOL fByPos = FALSE);
    static BOOL SetThemedMenuItemInfo(CMenu *menu, UINT uItem, LPMENUITEMINFO lpMenuItemInfo, BOOL fByPos = FALSE);
    CMPCThemeMenu* GetSubMenu(int nPos);
    static void updateItem(CCmdUI* pCmdUI);
    static void clearDimensions() { hasDimensions = false; };
    static void resetBrushes();
    void setOSMenu(bool isOSMenu) { this->isOSMenu = isOSMenu; };
protected:
    static std::unordered_set<MenuObject*> liveItems; //every MenuObject not yet deleted, to validate itemData from WM_MEASUREITEM/WM_DRAWITEM
    static std::mutex liveItemsMutex;
    std::vector<MenuObject*> allocatedItems;
    void addItem(MenuObject* pObject);
    void removeItem(MenuObject* pObject);
    std::vector<CMPCThemeMenu*> allocatedMenus;
    static void initDimensions();
    UINT findID(UINT& i, bool byCommand);
    void cleanupItem(UINT nPosition, UINT nFlags);
    bool isOSMenu = false;

    void GetRects(RECT rcItem, CRect& rectFull, CRect& rectM, CRect& rectIcon, CRect& rectText, CRect& rectArrow);
    static bool hasDimensions;
    static int subMenuPadding;
    static int iconSpacing;
    static int iconPadding;
    static int rowPadding;
    static int separatorPadding;
    static int separatorHeight;
    static int postTextSpacing;
    static int accelSpacing;
    static int hoverInsetX, hoverInsetY, hoverRadius; //windows 11 style hover pill
    static HBRUSH bgBrush, bgMenubarBrush;
    static CFont font, symbolFont, bulletFont, checkFont;
    static CCritSec resourceLock;
};

