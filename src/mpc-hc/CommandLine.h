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

#include <deque>

class CMainFrame;

// Command lines that reach the running player: the ones redirected from
// another instance by WM_COPYDATA and the Explorer drop target, queued and
// acted on later, and the yt-dlp handling of URLs from every open path.
// Composed into CMainFrame, which forwards WM_MPC_CMDLINE and the command
// line part of WM_COPYDATA here.
class CCommandLine
{
    CMainFrame& m_frame;

public:
    explicit CCommandLine(CMainFrame& frame);

    BOOL ReceiveCommandLine(const COPYDATASTRUCT* pCDS);
    static void UnpackCommandLine(const COPYDATASTRUCT* pCDS, CAtlList<CString>& cmdln);
    void QueueCommandLine(const CAtlList<CString>& cmdln);
    LRESULT OnCommandLineReceived(WPARAM wParam, LPARAM lParam);
    void ProcessCommandLine(CAtlList<CString>& cmdln, ULONGLONG tArrived);

    static bool IsOnYDLWhitelist(const CString url);
    bool CanSendToYoutubeDL(const CString url);
    bool ProcessYoutubeDLURL(CString url, bool append, bool replace = false);
    bool DownloadWithYoutubeDL(CString url, CString filename);

    CString m_sydlLastProcessURL;

private:
    ULONGLONG m_dwLastRun;
    int m_nLastAppendSelectionIndex; // playlist index where the current batch of redirected opens started

    // A command line redirected from another instance, waiting to be acted on. OnCopyData only
    // queues it, so that handler returns immediately instead of probing the filesystem while
    // the sending instances wait on it.
    struct PendingCommandLine {
        CAtlList<CString> cmdln;
        ULONGLONG tArrived = 0;
    };
    std::deque<PendingCommandLine> m_pendingCommandLines;
    bool m_bProcessingCommandLine = false;
};
