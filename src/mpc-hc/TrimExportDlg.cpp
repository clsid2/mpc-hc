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

#include "stdafx.h"
#include "TrimExportDlg.h"
#include "TrimSelection.h"
#include "mplayerc.h"
#include "PathUtils.h"

namespace
{
    CString FormatLength(REFERENCE_TIME length)
    {
        const auto seconds = static_cast<unsigned long long>(std::max<REFERENCE_TIME>(length, 0) / 10000000);
        CString text;
        text.Format(_T("%02llu:%02llu"), seconds / 60, seconds % 60);
        return text;
    }
}

IMPLEMENT_DYNAMIC(CTrimExportDlg, CMPCThemeDialog)

BEGIN_MESSAGE_MAP(CTrimExportDlg, CMPCThemeDialog)
    ON_BN_CLICKED(IDC_TRIM_BROWSE, OnBrowse)
    ON_WM_DESTROY()
    ON_MESSAGE(WM_TRIM_PROBED, OnProbed)
    ON_MESSAGE(WM_TRIM_PROGRESS, OnProgress)
END_MESSAGE_MAP()

CTrimExportDlg::CTrimExportDlg(const std::wstring& source, REFERENCE_TIME start, REFERENCE_TIME end, CWnd* pParent)
    : CMPCThemeDialog(IDD, pParent)
    , m_source(source)
    , m_start(start)
    , m_end(end)
{
}

CTrimExportDlg::~CTrimExportDlg()
{
    m_pJob.reset();
}

void CTrimExportDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TRIM_FILENAME, m_fileName);
    DDX_Control(pDX, IDC_TRIM_PATH, m_folder);
    DDX_Control(pDX, IDC_TRIM_PROGRESS, m_progress);
    fulfillThemeReqs();
}

BOOL CTrimExportDlg::OnInitDialog()
{
    __super::OnInitDialog();

    SetDlgItemText(IDC_TRIM_DURATION, FormatLength(m_end - m_start));
    CString path = DefaultPath();
    m_fileName.SetWindowText(PathFindFileName(path));
    PathRemoveFileSpec(path.GetBuffer());
    path.ReleaseBuffer();
    m_folder.SetWindowText(path);
    m_progress.SetRange(0, 1000);
    CMPCThemeUtil::fulfillThemeReqs(&m_progress);
    SetStatus(ResStr(IDS_TRIM_CHECKING));
    GetDlgItem(IDOK)->EnableWindow(FALSE);

    // Inspecting the file opens it through Media Foundation, which may take a moment on
    // slow storage; the dialog stays responsive meanwhile.
    m_probe = std::make_shared<ProbeState>();
    m_probe->window = m_hWnd;
    // The worker owns its input and result so closing the dialog never waits for file I/O.
    std::thread([probe = m_probe, source = m_source, start = m_start] {
        TrimExport::SourceInfo info;
        try {
            info = TrimExport::Probe(source, start);
        } catch (const std::bad_alloc&) {
            info.hr = E_OUTOFMEMORY;
        }
        std::lock_guard<std::mutex> lock(probe->mutex);
        probe->info = info;
        probe->complete = true;
        if (probe->window) {
            ::PostMessage(probe->window, WM_TRIM_PROBED, 0, 0);
        }
    }).detach();
    return TRUE;
}

CString CTrimExportDlg::DefaultPath() const
{
    CString base(m_source.c_str());
    PathRemoveExtension(base.GetBuffer());
    base.ReleaseBuffer();
    CString path = base + _T("_trimmed.mp4");
    for (int i = 2; PathFileExists(path) && i < 1000; ++i) {
        path.Format(_T("%s_trimmed (%d).mp4"), base.GetString(), i);
    }
    return path;
}

LRESULT CTrimExportDlg::OnProbed(WPARAM, LPARAM)
{
    if (!m_probe) {
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(m_probe->mutex);
        if (!m_probe->complete) {
            return 0;
        }
        m_info = m_probe->info;
    }
    m_bProbed = true;
    using TrimExport::Status;

    if (m_info.status != Status::Ok) {
        SetStatus(StatusText(m_info.status));
        return 0;
    }

    // A stream copy keeps the original quality and, through an edit list, still starts and
    // ends exactly at the selection. Only files that cannot be copied are converted.
    CString note;
    if (m_info.fast == Status::Ok) {
        m_mode = TrimExport::Mode::Fast;
    } else if (m_info.precise == Status::Ok) {
        m_mode = TrimExport::Mode::Precise;
        note.LoadString(IDS_TRIM_INFO_CONVERT);
    } else {
        SetStatus(StatusText(m_info.precise));
        return 0;
    }
    if (m_info.audioStreams > 1) {
        note += (note.IsEmpty() ? CString() : CString(_T(" "))) + ResStr(IDS_TRIM_FIRST_AUDIO_ONLY);
    }
    SetStatus(note);
    GetDlgItem(IDOK)->EnableWindow(TRUE);
    return 0;
}

void CTrimExportDlg::OnBrowse()
{
    CString folder;
    m_folder.GetWindowText(folder);
    folder.Trim();
    CFolderPickerDialog fd(ForceTrailingSlash(folder), FOS_PATHMUSTEXIST, this);
    if (fd.DoModal() == IDOK) {
        m_folder.SetWindowText(fd.GetPathName());
    }
}

CString CTrimExportDlg::ValidatedPath()
{
    CString fileName, directory;
    m_fileName.GetWindowText(fileName);
    m_folder.GetWindowText(directory);
    fileName.Trim();
    directory.Trim();
    bool invalidName = fileName.IsEmpty() || fileName.FindOneOf(_T("<>:\"/\\|?*")) >= 0 || fileName.Right(1) == _T(".");
    for (int i = 0; i < fileName.GetLength(); ++i) {
        invalidName |= fileName[i] < _T(' ');
    }
    if (invalidName || directory.IsEmpty() || PathIsRelative(directory) || !PathIsDirectory(directory)) {
        AfxMessageBox(IDS_TRIM_INVALID_PATH, MB_ICONWARNING | MB_OK);
        return CString();
    }
    if (PathFindExtension(fileName)[0] == _T('\0')) {
        fileName += _T(".mp4");
    }
    const CString path = PathUtils::CombinePaths(directory, fileName);
    if (PathIsDirectory(path)) {
        AfxMessageBox(IDS_TRIM_INVALID_PATH, MB_ICONWARNING | MB_OK);
        return CString();
    }
    if (TrimExport::IsSameFile(path.GetString(), m_source)) {
        AfxMessageBox(StatusText(TrimExport::Status::DestinationIsSource), MB_ICONWARNING | MB_OK);
        return CString();
    }
    if (PathFileExists(path) && path.CompareNoCase(m_confirmedPath) != 0) {
        CString question;
        question.Format(IDS_TRIM_REPLACE, path.GetString());
        if (AfxMessageBox(question, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
            return CString();
        }
        m_confirmedPath = path;
    }
    m_fileName.SetWindowText(fileName);
    m_folder.SetWindowText(directory);
    return path;
}

void CTrimExportDlg::SetPhase(bool bExporting)
{
    m_bExporting = bExporting;
    for (const int id : { IDC_TRIM_FILENAME, IDC_TRIM_PATH, IDC_TRIM_BROWSE }) {
        GetDlgItem(id)->EnableWindow(!bExporting);
    }
    GetDlgItem(IDOK)->EnableWindow(!bExporting);
    m_progress.SetPos(0);
    UpdateLayout();
}

void CTrimExportDlg::SetStatus(const CString& text)
{
    m_status = text;
    SetDlgItemText(IDC_TRIM_STATUS, text);
    UpdateLayout();
}

void CTrimExportDlg::UpdateLayout()
{
    CRect progressRect(9, 72, 277, 80), spacing(0, 0, 9, 6);
    MapDialogRect(progressRect);
    MapDialogRect(spacing);
    int y = progressRect.top;
    m_progress.ShowWindow(m_bExporting ? SW_SHOW : SW_HIDE);
    if (m_bExporting) {
        m_progress.MoveWindow(progressRect);
        y = progressRect.bottom + spacing.bottom;
    }
    CWnd* status = GetDlgItem(IDC_TRIM_STATUS);
    status->ShowWindow(m_status.IsEmpty() ? SW_HIDE : SW_SHOW);
    if (!m_status.IsEmpty()) {
        CClientDC dc(this);
        CFont* oldFont = dc.SelectObject(GetFont());
        CRect textRect(progressRect.left, y, progressRect.right, y);
        dc.DrawText(m_status, textRect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        dc.SelectObject(oldFont);
        status->MoveWindow(progressRect.left, y, progressRect.Width(), textRect.Height());
        y += textRect.Height() + spacing.bottom;
    }
    int buttonHeight = 0;
    for (const int id : { IDOK, IDCANCEL }) {
        CRect rect;
        GetDlgItem(id)->GetWindowRect(rect);
        ScreenToClient(rect);
        buttonHeight = rect.Height();
        GetDlgItem(id)->MoveWindow(rect.left, y, rect.Width(), buttonHeight);
    }
    CRect windowRect, clientRect;
    GetWindowRect(windowRect);
    GetClientRect(clientRect);
    CRect padding(0, 0, 0, 9);
    MapDialogRect(padding);
    const int height = windowRect.Height() - clientRect.Height() + y + buttonHeight + padding.bottom;
    if (height != windowRect.Height()) {
        SetWindowPos(nullptr, 0, 0, windowRect.Width(), height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CTrimExportDlg::OnOK()
{
    if (m_bSaved) {
        // After a successful save the button shows the clip in Explorer.
        ShellExecute(m_hWnd, nullptr, _T("explorer.exe"), _T("/select,\"") + m_savedPath + _T("\""), nullptr, SW_SHOWNORMAL);
        return;
    }
    if (!m_bProbed || m_pJob) {
        return;
    }
    const CString path = ValidatedPath();
    if (path.IsEmpty()) {
        return;
    }

    TrimExport::Request request;
    request.source = m_source;
    request.destination = path.GetString();
    request.start = m_start;
    request.end = m_end;
    request.mode = m_mode;
    request.replaceExisting = path.CompareNoCase(m_confirmedPath) == 0;

    SetPhase(true);
    CString status;
    status.Format(IDS_TRIM_SAVING, 0);
    SetStatus(status);
    m_pJob = std::make_unique<TrimExport::CJob>(request, m_hWnd, WM_TRIM_PROGRESS);
}

LRESULT CTrimExportDlg::OnProgress(WPARAM wParam, LPARAM)
{
    if (!m_pJob) {
        return 0;
    }
    if (wParam != TrimExport::CJob::kDone) {
        m_progress.SetPos(int(wParam));
        if (!m_bCloseWhenDone) {
            CString status;
            status.Format(IDS_TRIM_SAVING, int(wParam) / 10);
            SetStatus(status);
        }
        return 0;
    }

    m_pJob->Wait();
    const TrimExport::Result result = m_pJob->GetResult();
    const CString destination = m_pJob->GetRequest().destination.c_str();
    m_pJob.reset();
    if (m_bCloseWhenDone) {
        EndDialog(IDCANCEL);
        return 0;
    }

    SetPhase(false);
    if (result.status == TrimExport::Status::Ok) {
        m_bSaved = true;
        m_savedPath = destination;
        SetStatus(ResStr(IDS_TRIM_SAVED));
        for (const int id : { IDC_TRIM_FILENAME, IDC_TRIM_PATH, IDC_TRIM_BROWSE }) {
            GetDlgItem(id)->EnableWindow(FALSE);
        }
        SetDlgItemText(IDOK, ResStr(IDS_TRIM_SHOW_IN_FOLDER));
        SetDlgItemText(IDCANCEL, ResStr(IDS_TRIM_CLOSE));
        GetDlgItem(IDCANCEL)->SetFocus();
    } else if (result.status != TrimExport::Status::Cancelled) {
        CString text = StatusText(result.status);
        if (FAILED(result.hr)) {
            text.AppendFormat(_T(" (0x%08lX)"), result.hr);
        }
        SetStatus(text);
    } else {
        SetStatus(CString());
    }
    return 0;
}

void CTrimExportDlg::OnCancel()
{
    if (m_pJob) {
        // Closing waits for the worker to stop and remove its temporary file.
        m_bCloseWhenDone = true;
        m_pJob->Cancel();
        GetDlgItem(IDCANCEL)->EnableWindow(FALSE);
        SetStatus(ResStr(IDS_TRIM_CANCELLING));
        return;
    }
    __super::OnCancel();
}

void CTrimExportDlg::Abort()
{
    m_pJob.reset();
    EndDialog(IDCANCEL);
}

void CTrimExportDlg::OnDestroy()
{
    m_pJob.reset();
    if (m_probe) {
        std::lock_guard<std::mutex> lock(m_probe->mutex);
        m_probe->window = nullptr;
    }
    __super::OnDestroy();
}

CString CTrimExportDlg::StatusText(TrimExport::Status status)
{
    return ResStr(IDS_TRIM_STATUS_OK + UINT(status));
}
