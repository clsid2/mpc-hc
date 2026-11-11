/*
 * (C) 2026 see Authors.txt
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

#include "FavoriteOrganizeDlg.h"
#include <memory>

class CMainFrame;
struct CPlayerGraph;
class CHistoryDlg;
class RecentFileEntry;

struct FileFavorite {
    CString Name;
    REFERENCE_TIME Start;
    REFERENCE_TIME MarkA;
    REFERENCE_TIME MarkB;
    BOOL RelativeDrive;

    FileFavorite() {
        Start = MarkA = MarkB = 0;
        RelativeDrive = FALSE;
    }

    static bool TryParse(const CString& fav, FileFavorite& ff);
    static bool TryParse(const CString& fav, FileFavorite& ff, CAtlList<CString>& parts);

    CString ToString() const;
};

// File, DVD and device favorites, the recent files list and the history
// dialog, and the two submenus that show them. Composed into CMainFrame,
// which routes its commands here from OnCmdMsg. The menu objects stay in
// CMainFrame with the other dynamic menus, which are created and destroyed
// together.
class CFavoritesRecent : public CCmdTarget
{
    CMainFrame& m_frame;
    CPlayerGraph& m_graph;

public:
    explicit CFavoritesRecent(CMainFrame& frame);

    int recentFilesMenuFromMRUSequence;
    CFavoriteOrganizeDlg m_wndFavoriteOrganizeDialog;
    std::unique_ptr<CHistoryDlg> m_pHistoryDlg;

    void AddFavorite(bool fDisplayMessage = false, bool fShowDialog = true);
    void PlayFavoriteFile(const CString& fav);
    void PlayFavoriteDVD(CString fav);
    FileFavorite ParseFavoriteFile(const CString& fav, CAtlList<CString>& args, REFERENCE_TIME* prtStart = nullptr);
    void OpenRecentFileEntry(RecentFileEntry& r);
    LPCTSTR GetRecentFile() const;

    void SetupFavoritesSubMenu();
    void SetupRecentFilesSubMenu();

    afx_msg void OnFavoritesAdd();
    afx_msg void OnUpdateFavoritesAdd(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesQuickAddFavorite();
    afx_msg void OnFavoritesOrganize();
    afx_msg void OnUpdateFavoritesOrganize(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesFile(UINT nID);
    afx_msg void OnUpdateFavoritesFile(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesDVD(UINT nID);
    afx_msg void OnUpdateFavoritesDVD(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesDevice(UINT nID);
    afx_msg void OnUpdateFavoritesDevice(CCmdUI* pCmdUI);
    afx_msg void OnRecentFileClear();
    afx_msg void OnUpdateRecentFileClear(CCmdUI* pCmdUI);
    afx_msg void OnRecentFile(UINT nID);
    afx_msg void OnUpdateRecentFile(CCmdUI* pCmdUI);
    afx_msg void OnShowHistory();

    DECLARE_MESSAGE_MAP()
};
