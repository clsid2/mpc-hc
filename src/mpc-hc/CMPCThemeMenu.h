#pragma once
#include <afxwin.h>

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
    static CMPCThemeMenu* getParentMenu(UINT itemID);
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    void GetStrings(MenuObject* mo, CString& left, CString& right);
    virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    virtual BOOL AppendMenu(UINT nFlags, UINT_PTR nIDNewItem = 0, LPCTSTR lpszNewItem = NULL);
    virtual BOOL DeleteMenu(UINT nPosition, UINT nFlags);
    virtual BOOL RemoveMenu(UINT nPosition, UINT nFlags);
    virtual BOOL SetThemedMenuItemInfo(UINT uItem, LPMENUITEMINFO lpMenuItemInfo, BOOL fByPos = FALSE);
    static BOOL SetThemedMenuItemInfo(CMenu *menu, UINT uItem, LPMENUITEMINFO lpMenuItemInfo, BOOL fByPos = FALSE);
    CMPCThemeMenu* GetSubMenu(int nPos);
    static void updateItem(CCmdUI* pCmdUI);
    static void clearDimensions() { hasDimensions = false; };
    void setOSMenu(bool isOSMenu) { this->isOSMenu = isOSMenu; };
protected:
    static std::map<UINT, CMPCThemeMenu*> subMenuIDs;
    std::vector<MenuObject*> allocatedItems;
    std::vector<CMPCThemeMenu*> allocatedMenus;
    void initDimensions();
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
    static HBRUSH bgBrush, bgMenubarBrush;
    static CFont font, symbolFont, bulletFont, checkFont;
    static CCritSec resourceLock;
    static std::mutex submenuMutex;
};

