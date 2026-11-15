#pragma once
#include <afxwin.h>
#include <unordered_set>
#include <map>
#include <memory>

struct MenuObject {
    HICON m_hIcon;
    CString m_strCaption;
    CString m_strAccel;
    bool isMenubar = false;
    bool isSeparator = false;
    bool isFirstMenuInMenuBar = false;
    UINT dpi = 0; //dpi the item was measured at, so it is drawn to the size windows cached
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
    static void clearDimensions();
    static void resetBrushes();
    void setOSMenu(bool isOSMenu) { this->isOSMenu = isOSMenu; };
protected:
    static std::unordered_set<MenuObject*> liveItems; //every MenuObject not yet deleted, to validate itemData from WM_MEASUREITEM/WM_DRAWITEM
    static std::mutex liveItemsMutex;
    std::vector<MenuObject*> allocatedItems;
    void addItem(MenuObject* pObject);
    void removeItem(MenuObject* pObject);
    std::vector<CMPCThemeMenu*> allocatedMenus;
    struct Dimensions {
        int subMenuPadding;
        int iconSpacing;
        int iconPadding;
        int rowPadding;
        int separatorPadding;
        int separatorHeight;
        int postTextSpacing;
        int accelSpacing;
        int hoverInsetX, hoverInsetY, hoverRadius; //windows 11 style hover pill
        CFont font, symbolFont, bulletFont, checkFont;
    };
    static Dimensions& getDimensions(UINT dpi); //caller must hold resourceLock
    static UINT getOwnerDpi();
    UINT findID(UINT& i, bool byCommand);
    void cleanupItem(UINT nPosition, UINT nFlags);
    bool isOSMenu = false;

    void GetRects(const Dimensions& dim, RECT rcItem, CRect& rectFull, CRect& rectM, CRect& rectIcon, CRect& rectText, CRect& rectArrow);
    static std::map<UINT, std::unique_ptr<Dimensions>> dimensionsByDpi; //popups can be tracked from windows on monitors with different dpi
    static HBRUSH bgBrush, bgMenubarBrush;
    static CCritSec resourceLock;
};

