/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2016 see Authors.txt
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

#include "ResizableLib/ResizableDialog.h"
#include "SubtitlesProviders.h" // Forward declaration doesn't work on VS2013. Remove this once VS2013 support is dropped.
#include <list>
#include "CMPCThemePlayerListCtrl.h"
#include "CMPCThemeModelessResizableDialog.h"
#include "CMPCThemeStatusBar.h"
#include "CMPCThemeProgressCtrl.h"

class CMainFrame;
struct SubtitlesInfo;
using SubtitlesList = std::list<SubtitlesInfo>;
enum SRESULT;

class CSubtitleDlDlgListCtrl final : public CMPCThemePlayerListCtrl
{
    void PreSubclassWindow() override;

    DECLARE_MESSAGE_MAP();
    afx_msg BOOL OnToolNeedText(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
};

class CSubtitleDlDlg : public CMPCThemeModelessResizableDialog
{
public:
    enum {
        COL_PROVIDER,
        COL_FILENAME,
        COL_LANGUAGE,
        //COL_FORMAT,
        COL_FRAMERATE,
        COL_HEARINGIMPAIRED,
        COL_DOWNLOADS,
        COL_TITLES,
        COL_SCORE,
        COL_DISC,
        COL_TOTAL_COLUMNS
    };

private:
    struct PARAMSORT {
        PARAMSORT(HWND hWnd, int nSortColumn, int fSortOrder)
            : m_hWnd(hWnd), m_nSortColumn(nSortColumn), m_fSortOrder(fSortOrder) {}
        HWND m_hWnd;
        int m_nSortColumn;
        int m_fSortOrder;
    };
    typedef PARAMSORT* PPARAMSORT;

    PARAMSORT m_ps;
    bool m_bIsRefreshed;

    CSubtitleDlDlgListCtrl m_list;
    CMPCThemeProgressCtrl m_progress;
    CMPCThemeStatusBar m_status;
    CMainFrame* m_pMainFrame;
    SubtitlesList m_Subtitles;
    CString manualSearch;

    static int CALLBACK SortCompare(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
    void SetStatusText(const CString& status, BOOL bPropagate = TRUE);
    void SetListViewSortColumn();
    void DownloadSelectedSubtitles();

public:
    CSubtitleDlDlg(CMainFrame* pParentWnd);
    virtual ~CSubtitleDlDlg() = default;
    enum { IDD = IDD_SUBTITLEDL_DLG };

    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;

    void UpdateStatusBarLayout();

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual void OnOK();

    DECLARE_MESSAGE_MAP()

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnUpdateOk(CCmdUI* pCmdUI);
    afx_msg void OnUpdateRefresh(CCmdUI* pCmdUI);
    afx_msg void OnAbort();
    afx_msg void OnRefresh();
    afx_msg void OnManualSearch();
    afx_msg void OnOptions();
    afx_msg void OnColumnClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDestroy();
    afx_msg void OnDoubleClickSubtitle(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyPressedSubtitle(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnRightClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnItemChanging(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);

    afx_msg LRESULT OnSearch(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnSearching(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDownloading(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDownloaded(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnCompleted(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFinished(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFailedSearch(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFailedDownload(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnClear(WPARAM wParam, LPARAM lParam);

public:
    void DoSearch(INT _nCount);
    void DoSearching(SubtitlesInfo& _fileInfo);
    void DoDownloading(SubtitlesInfo& _fileInfo);
    void DoDownloaded(SubtitlesInfo& _fileInfo);
    void DoCompleted(SRESULT _result, SubtitlesList& _subtitlesList);
    void DoFinished(BOOL _bAborted, BOOL _bShowDialog);
    void DoSearchFailed();
    void DoDownloadFailed(DWORD statuscode);
    void DoClear();
};
