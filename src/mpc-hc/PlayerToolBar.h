/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2014, 2016 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "VolumeCtrl.h"
#include "CMPCThemeToolTipCtrl.h"
#include "CMPCThemeUtil.h"
#include <atlimage.h>
#include <optional>

class CMainFrame;

class CPlayerToolBar : public CToolBar, public CMPCThemeUtil
{
    DECLARE_DYNAMIC(CPlayerToolBar)

public:
    enum PositionLock {
        LOCK_NONE = 0,
        LOCK_LEFT,
        LOCK_RIGHT,
    };
private:
    CMainFrame* m_pMainFrame;

    CImage volumeOn, volumeOff;
    bool IsMuted() const;
    void SetMute(bool fMute = true);
    int getHitButtonIdx(CPoint point);
    bool LoadExternalToolBar(CImage& image);
    void LoadToolbarImage(bool tbArtChanged = false);
    void PlaceButtons(bool loadSavedLayout);
    int rightButtonIndex=-1, leftButtonIndex=-1;
    CMPCThemeToolTipCtrl themedToolTip;

    int m_nButtonHeight;
    std::unique_ptr<CImageList> m_pButtonsImages;
    std::unique_ptr<CImageList> m_pDisabledButtonsImages;
    std::unique_ptr<CImageList> m_pCustomizeButtonImages;
    int m_volumeCtrlSize;

    EventClient m_eventc;
    void EventCallback(MpcEvent ev);
    int volumeButtonIndex, dummySeparatorIndex, flexibleSpaceIndex;
    int currentlyDraggingButton;
    CPoint mousePosition;
    bool toolbarAdjustActive;

    struct svgButtonInfo {
        UINT style;
        int svgIndex;
        UINT strID = 0;
        PositionLock positionLocked = LOCK_NONE;
        UINT activeStrID = 0;
        CString text;
    };

    static std::map<WORD, svgButtonInfo> supportedSvgButtons;
public:
    CPlayerToolBar(CMainFrame* pMainFrame);
    virtual ~CPlayerToolBar();

    bool LoadExternalToolBar(CImage& image, float svgscale, CStringW resolutionPostfix);
    void MakeImageList(bool createCustomizeButtons, int buttonSize, std::unique_ptr<CImageList> &imageList);
    LPCWSTR GetStringFromID(int idCommand);
    const std::map<WORD, svgButtonInfo> GetSupportedSvgButtons() {
        return supportedSvgButtons;
    }
    std::unique_ptr<CImageList>& GetCustomizeButtonImages() {
        return m_pCustomizeButtonImages;
    }

    int GetVolume() const;
    int GetMinWidth() const;
    void SetVolume(int volume);
    __declspec(property(get = GetVolume, put = SetVolume)) int Volume;

    TBBUTTON GetStandardButton(int cmdid);
    void LoadButtonStrings();
    void ArrangeControls();
    void ToolbarChange();
    void ToolBarReset();

    CImage& GetVolumeImageOn() { return volumeOn; };
    CImage& GetVolumeImageOff() { return volumeOff; };

    CVolumeCtrl m_volctrl;

    std::optional<bool> lastFullscreen = std::nullopt;
    void SetFullscreen(bool isFS);

    std::optional<bool> lastPlaylist = std::nullopt;
    void SetPlaylist(bool isVisible);

    std::optional<bool> lastShuffle = std::nullopt;
    void SetShuffle(bool isEnabled);

    std::optional<bool> lastRepeat = std::nullopt;
    void SetRepeat(bool isEnabled);
private:
    void ToggleButton(int buttonID, bool isActive, std::optional<bool>& lastBool);

public:

    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CPlayerToolBar)
    virtual BOOL Create(CWnd* pParentWnd);
    //}}AFX_VIRTUAL

    UINT CustomToCMD(UINT nID, int mouseButtonSource);

    // Generated message map functions
protected:
    //{{AFX_MSG(CPlayerToolBar)
    afx_msg BOOL OnMenuButton(UINT nID);
    afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnInitialUpdate();
    afx_msg BOOL OnVolumeMute(UINT nID);
    afx_msg void OnUpdateVolumeMute(CCmdUI* pCmdUI);
    afx_msg void OnUpdateFullscreen(CCmdUI* pCmdUI);
    afx_msg void OnUpdatePlaylist(CCmdUI* pCmdUI);
    afx_msg void OnUpdateShuffle(CCmdUI* pCmdUI);
    afx_msg void OnUpdateRepeat(CCmdUI* pCmdUI);
    afx_msg void OnUpdateCustomAction(CCmdUI* pCmdUI);
    bool CmdIsMenu(int cmd);
    afx_msg BOOL OnCustomAction(UINT nID);
    afx_msg BOOL OnVolumeUp(UINT nID);
    afx_msg BOOL OnVolumeDown(UINT nID);
    afx_msg BOOL OnFullscreenButton(UINT nID);
    afx_msg BOOL OnPlaylistButton(UINT nID);
    afx_msg BOOL OnShuffleButton(UINT nID);
    afx_msg BOOL OnRepeatButton(UINT nID);
    afx_msg void OnNcPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    void DoCustomContextMenu(int itemId, int cmd, int buttonId, int mouseButtonSource);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnTbnQueryDelete(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTbnQueryInsert(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void SaveToolbarState();
    afx_msg void OnTbnToolbarChange(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnTbnGetButtonInfo(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTbnInitCustomize(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTbnBeginAdjust(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTbnEndAdjust(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnTbnReset(NMHDR* pNMHDR, LRESULT* pResult);
};


