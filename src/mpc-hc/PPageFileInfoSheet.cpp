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

#include "stdafx.h"
#include "mplayerc.h"
#include "MainFrm.h"
#include "PPageFileInfoSheet.h"
#include "PPageFileMediaInfo.h"
#include "CMPCTheme.h"

#include <mutex>

namespace {

constexpr UINT_PTR TIMER_MKV_METADATA = 0x4D4B;

} // namespace

struct MkvMetadataTaskState {
    CString path;
    ULONGLONG startTime = GetTickCount64();
    std::atomic_bool cancelled = false;
    HANDLE workerThread = nullptr;
    std::mutex mutex;
    MkvMetadata metadata;
    bool completed = false;

    ~MkvMetadataTaskState()
    {
        if (workerThread) {
            CloseHandle(workerThread);
        }
    }
};

namespace {

void CancelMkvMetadataTask(const std::shared_ptr<MkvMetadataTaskState>& task)
{
    task->cancelled = true;

    // Overlapped reads are cancelled by the reader. This additionally aborts
    // synchronous CreateFile/GetFileSizeEx calls on the dedicated worker.
    if (task->workerThread) {
        CancelSynchronousIo(task->workerThread);
    }
}

void AbortSuspendedMkvMetadataWorker(CWinThread* worker, std::shared_ptr<MkvMetadataTaskState>* parameter)
{
    // A failed ResumeThread leaves the native thread suspended before the
    // entry point can take ownership of parameter. Terminating it at this
    // point is safe: ReadMkvMetadata has not run and holds no resources.
    if (TerminateThread(worker->m_hThread, ERROR_OPERATION_ABORTED)) {
        WaitForSingleObject(worker->m_hThread, INFINITE);
        delete parameter;
        worker->Delete();
        return;
    }

    // Keep the argument alive if Windows refuses to terminate the thread; a
    // successful retry lets the cancelled worker release it normally.
    if (worker->ResumeThread() == static_cast<DWORD>(-1)) {
        ASSERT(FALSE);
    }
}

UINT AFX_CDECL ReadMkvMetadata(LPVOID parameter)
{
    std::unique_ptr<std::shared_ptr<MkvMetadataTaskState>> parameterOwner(static_cast<std::shared_ptr<MkvMetadataTaskState>*>(parameter));
    const std::shared_ptr<MkvMetadataTaskState>& task = *parameterOwner;
    MkvMetadata metadata;
    const bool succeeded = CMkvMetadataReader::Read(task->path, metadata, &task->cancelled, task->startTime);

    if (!task->cancelled.load()) {
        std::lock_guard<std::mutex> lock(task->mutex);
        if (!task->cancelled.load()) {
            if (succeeded) {
                task->metadata = std::move(metadata);
            }
            task->completed = true;
        }
    }

    return 0;
}

} // namespace


// CPPageFileInfoSheet

IMPLEMENT_DYNAMIC(CPPageFileInfoSheet, CMPCThemeResizablePropertySheet)
CPPageFileInfoSheet::CPPageFileInfoSheet(CString path, CString ydlsrc, CMainFrame* pMainFrame, CWnd* pParentWnd)
    : CMPCThemeResizablePropertySheet(IDS_PROPSHEET_PROPERTIES, pParentWnd, 0)
    , m_clip(path, ydlsrc, pMainFrame->m_pGB, pMainFrame->m_pFSF, pMainFrame->m_pDVDI)
    , m_details(path, ydlsrc, pMainFrame->m_pGB, pMainFrame->m_pCAP, pMainFrame->m_pFSF, pMainFrame->m_pDVDI)
    , m_res(path, pMainFrame->m_pGB, pMainFrame->m_pFSF)
    , m_mi(path, pMainFrame->m_pFSF, pMainFrame->m_pDVDI, pMainFrame)
    , m_path(path)
{
    AddPage(&m_details);
    AddPage(&m_clip);

    if (m_res.HasResources()) {
        AddPage(&m_res);
    }

    if (CPPageFileMediaInfo::HasMediaInfo()) {
        AddPage(&m_mi);
    }

    if (path.Right(4).CompareNoCase(L".mkv") == 0) {
        m_mkvMetadata.SetLoading();
        AddPage(&m_mkvMetadata);
        m_mkvMetadataAdded = true;
    }
}

CPPageFileInfoSheet::~CPPageFileInfoSheet()
{
    if (GetSafeHwnd()) {
        KillTimer(TIMER_MKV_METADATA);
    }
    if (m_mkvMetadataTask) {
        CancelMkvMetadataTask(m_mkvMetadataTask);
        m_mkvMetadataTask.reset();
    }
}


BEGIN_MESSAGE_MAP(CPPageFileInfoSheet, CMPCThemeResizablePropertySheet)
    ON_BN_CLICKED(IDC_BUTTON_MI, OnSaveAs)
    ON_WM_TIMER()
END_MESSAGE_MAP()

// CPPageFileInfoSheet message handlers

BOOL CPPageFileInfoSheet::OnInitDialog()
{
    __super::OnInitDialog();

    GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
    GetDlgItem(ID_APPLY_NOW)->ShowWindow(SW_HIDE);
    GetDlgItem(IDOK)->SetWindowText(ResStr(IDS_AG_CLOSE));

    // align the buttons with the page above: Save As flush left, Close flush right (also keeps Close clear of the size grip)
    CRect pageRect;
    GetActivePage()->GetWindowRect(&pageRect);
    ScreenToClient(&pageRect);

    CRect r;
    GetDlgItem(ID_APPLY_NOW)->GetWindowRect(&r);
    ScreenToClient(r);
    r.MoveToX(pageRect.right - r.Width());
    RemoveAnchor(IDOK); //otherwise it crashes when we add it later
    GetDlgItem(IDOK)->MoveWindow(r);
    AddAnchor(IDOK, BOTTOM_RIGHT); // must be added after the move, since AddAnchor bases its margin on the button's current position

    r.MoveToX(pageRect.left);
    r.right += 24;
    m_Button_MI.Create(ResStr(IDS_AG_SAVE_AS), WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE, r, this, IDC_BUTTON_MI);
    m_Button_MI.SetFont(GetFont());
    m_Button_MI.ShowWindow(SW_HIDE);

    GetTabControl()->SetFocus();

    CMPCThemeUtil::enableWindows10DarkFrame(this);

    AddAnchor(IDC_BUTTON_MI, BOTTOM_LEFT);

    if (m_mkvMetadataAdded) {
        auto task = std::make_shared<MkvMetadataTaskState>();
        task->path = m_path;
        m_mkvMetadataTask = task;

        // The worker owns this shared_ptr copy and never touches the dialog.
        // The UI timer safely collects the completed result on this thread.
        auto parameter = new std::shared_ptr<MkvMetadataTaskState>(task);
        CWinThread* worker = AfxBeginThread(ReadMkvMetadata, parameter, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
        if (!worker) {
            delete parameter;
            m_mkvMetadataTask.reset();
            RemovePage(&m_mkvMetadata);
            m_mkvMetadataAdded = false;
        } else if (!DuplicateHandle(GetCurrentProcess(), worker->m_hThread, GetCurrentProcess(), &task->workerThread,
                                    0, FALSE, DUPLICATE_SAME_ACCESS)) {
            CancelMkvMetadataTask(task);
            if (worker->ResumeThread() == static_cast<DWORD>(-1)) {
                AbortSuspendedMkvMetadataWorker(worker, parameter);
            }
            m_mkvMetadataTask.reset();
            RemovePage(&m_mkvMetadata);
            m_mkvMetadataAdded = false;
        } else if (worker->ResumeThread() == static_cast<DWORD>(-1)) {
            CancelMkvMetadataTask(task);
            AbortSuspendedMkvMetadataWorker(worker, parameter);
            m_mkvMetadataTask.reset();
            RemovePage(&m_mkvMetadata);
            m_mkvMetadataAdded = false;
        } else if (!SetTimer(TIMER_MKV_METADATA, 100, nullptr)) {
            CancelMkvMetadataTask(m_mkvMetadataTask);
            m_mkvMetadataTask.reset();
            RemovePage(&m_mkvMetadata);
            m_mkvMetadataAdded = false;
        }
    }

    return FALSE;  // return TRUE unless you set the focus to a control
}

void CPPageFileInfoSheet::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_MKV_METADATA) {
        CompleteMkvMetadataTask();
        return;
    }

    __super::OnTimer(nIDEvent);
}

void CPPageFileInfoSheet::CompleteMkvMetadataTask()
{
    const std::shared_ptr<MkvMetadataTaskState> task = m_mkvMetadataTask;
    if (!task) {
        KillTimer(TIMER_MKV_METADATA);
        return;
    }

    if (GetTickCount64() - task->startTime >= CMkvMetadataReader::READ_TIMEOUT_MS) {
        // CreateFile and GetFileSizeEx may block in the network redirector.
        // The reader cannot inspect its deadline while either call is stuck,
        // so cancel the dedicated worker from the responsive UI thread.
        CancelMkvMetadataTask(task);
        KillTimer(TIMER_MKV_METADATA);
        m_mkvMetadataTask.reset();
        if (m_mkvMetadataAdded) {
            RemovePage(&m_mkvMetadata);
            m_mkvMetadataAdded = false;
        }
        return;
    }

    MkvMetadata metadata;
    {
        std::lock_guard<std::mutex> lock(task->mutex);
        if (!task->completed) {
            return;
        }
        metadata = std::move(task->metadata);
    }
    KillTimer(TIMER_MKV_METADATA);
    m_mkvMetadataTask.reset();

    if (metadata.HasDisplayableData()) {
        m_mkvMetadata.SetMetadata(std::move(metadata));
    } else if (m_mkvMetadataAdded) {
        RemovePage(&m_mkvMetadata);
        m_mkvMetadataAdded = false;
    }

}

void CPPageFileInfoSheet::OnSaveAs()
{
    m_mi.OnSaveAs();
}

