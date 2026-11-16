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

#include "resource.h"
#include "CMPCThemeDialog.h"
#include "CMPCThemeEdit.h"
#include "CMPCThemeProgressCtrl.h"
#include "TrimExport.h"
#include <memory>
#include <mutex>

// Save Clip: the output file, then progress and outcome of the export.
class CTrimExportDlg : public CMPCThemeDialog
{
    DECLARE_DYNAMIC(CTrimExportDlg)

public:
    CTrimExportDlg(const std::wstring& source, REFERENCE_TIME start, REFERENCE_TIME end, CWnd* pParent);
    virtual ~CTrimExportDlg();

    enum { IDD = IDD_TRIM_SAVE_DLG };

    bool IsExporting() const { return m_pJob != nullptr; }
    void Abort();

private:
    enum {
        WM_TRIM_PROBED = WM_APP + 1,
        WM_TRIM_PROGRESS,
    };

    const std::wstring m_source;
    const REFERENCE_TIME m_start;
    const REFERENCE_TIME m_end;

    struct ProbeState {
        std::mutex mutex;
        HWND window = nullptr;
        TrimExport::SourceInfo info;
        bool complete = false;
    };
    std::shared_ptr<ProbeState> m_probe;
    TrimExport::SourceInfo m_info;
    bool m_bProbed = false;
    TrimExport::Mode m_mode = TrimExport::Mode::Fast;
    std::unique_ptr<TrimExport::CJob> m_pJob;
    bool m_bCloseWhenDone = false;
    bool m_bSaved = false;
    CString m_confirmedPath;    // a destination the user agreed to replace
    CString m_savedPath;

    CMPCThemeEdit m_fileName;
    CMPCThemeEdit m_folder;
    CMPCThemeProgressCtrl m_progress;
    bool m_bExporting = false;
    CString m_status;

    CString DefaultPath() const;
    CString ValidatedPath();
    void SetPhase(bool bExporting);
    void SetStatus(const CString& text);
    void UpdateLayout();
    static CString StatusText(TrimExport::Status status);

    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;
    virtual void OnCancel() override;

    afx_msg void OnBrowse();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnProbed(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnProgress(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()
};
