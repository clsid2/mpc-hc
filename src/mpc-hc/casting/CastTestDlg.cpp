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
#include "CastTestDlg.h"
#include "mplayerc.h"
#include "Logger.h"
#include "SettingsDefines.h"
#include <afxinet.h>

// Where the test tones live, in the source tree rather than the binary. The
// default is the upstream develop branch, which is where they sit once a
// release is cut; a build made from a branch before the merge can point this at
// its own branch with the CastTestToneBaseURL setting.
#define CAST_TONE_BASE_URL _T("https://raw.githubusercontent.com/clsid2/mpc-hc/develop/src/mpc-hc/res/cast/")

// The two layouts the ladder tries, high to low. Stereo is not tested: every
// cast device outputs it, so it is the floor the ladder falls back to when
// neither of these is heard.
namespace
{
    LPCTSTR ToneStem(int channels) { return channels == 8 ? _T("tone_71") : _T("tone_51"); }
    UINT LayoutStringId(int channels)
    {
        return channels == 8 ? IDS_CAST_DLG_CHAN_71
               : channels == 6 ? IDS_CAST_DLG_CHAN_51 : IDS_CAST_DLG_CHAN_STEREO;
    }
}

IMPLEMENT_DYNAMIC(CCastTestDlg, CMPCThemeDialog)

CCastTestDlg::CCastTestDlg(CCastTarget* pTarget, const CastSavedDevice& device,
                           CastMediaInfo::Audio preferredCodec, CWnd* pParent /*= nullptr*/)
    : CMPCThemeDialog(CCastTestDlg::IDD, pParent)
    , m_pTarget(pTarget)
    , m_device(device)
    , m_preferredCodec(preferredCodec)
{
}

CCastTestDlg::~CCastTestDlg()
{
}

void CCastTestDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CASTTEST_CODEC, m_codecCombo);
}

BEGIN_MESSAGE_MAP(CCastTestDlg, CMPCThemeDialog)
    ON_WM_DESTROY()
    ON_WM_CLOSE()
    ON_BN_CLICKED(IDC_CASTTEST_START, OnStart)
END_MESSAGE_MAP()

void CCastTestDlg::OnCancel()
{
    if (m_bRunning) {
        return; // the session is being torn down; do not pull the window out from under it
    }
    __super::OnCancel();
}

void CCastTestDlg::OnClose()
{
    if (m_bRunning) {
        return;
    }
    __super::OnClose();
}

BOOL CCastTestDlg::OnInitDialog()
{
    __super::OnInitDialog();

    // AAC first: it is the codec most files that reach a cast device use, and
    // the one whose channel behaviour differs most between devices. FLAC is the
    // other bundled codec. The item data is the codec the tone is fetched in.
    m_codecCombo.AddString(_T("AAC"));
    m_codecCombo.SetItemData(0, (DWORD_PTR)CastMediaInfo::Audio::AAC);
    m_codecCombo.AddString(_T("FLAC"));
    m_codecCombo.SetItemData(1, (DWORD_PTR)CastMediaInfo::Audio::FLAC);
    m_codecCombo.SetCurSel(m_preferredCodec == CastMediaInfo::Audio::FLAC ? 1 : 0);

    fulfillThemeReqs();
    return TRUE;
}

CastMediaInfo::Audio CCastTestDlg::SelectedCodec() const
{
    const int sel = m_codecCombo.GetCurSel();
    return sel >= 0 ? (CastMediaInfo::Audio)m_codecCombo.GetItemData(sel) : CastMediaInfo::Audio::AAC;
}

CString CCastTestDlg::ToneBaseURL() const
{
    CString url = AfxGetApp()->GetProfileString(IDS_R_SETTINGS, _T("CastTestToneBaseURL"), CAST_TONE_BASE_URL);
    if (!url.IsEmpty() && url[url.GetLength() - 1] != _T('/')) {
        url += _T('/');
    }
    return url;
}

void CCastTestDlg::SetStatus(const CString& text)
{
    SetDlgItemText(IDC_CASTTEST_STATUS, text);
    // The test blocks the thread, so paint the new line before the next step.
    GetDlgItem(IDC_CASTTEST_STATUS)->UpdateWindow();
}

CString CCastTestDlg::FetchTone(int channels)
{
    if (!m_tempFile.IsEmpty()) {
        DeleteFile(m_tempFile);
        m_tempFile.Empty();
    }

    const CastMediaInfo::Audio codec = SelectedCodec();
    const CString ext = codec == CastMediaInfo::Audio::FLAC ? _T("flac") : _T("m4a");
    const CString url = ToneBaseURL() + ToneStem(channels) + _T(".") + ext;

    TCHAR tempDir[MAX_PATH] = { 0 };
    GetTempPath(MAX_PATH, tempDir);
    CString temp;
    temp.Format(_T("%smpc-casttone-%u.%s"), tempDir, GetCurrentProcessId(), ext.GetString());

    bool ok = false;
    try {
        CInternetSession internet;
        internet.SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, 8000);
        internet.SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, 8000);
        CHttpFile* file = (CHttpFile*)internet.OpenURL(url, 1,
                          INTERNET_FLAG_TRANSFER_BINARY | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_RELOAD,
                          nullptr, DWORD(-1));
        if (file) {
            DWORD status = 0;
            if (file->QueryInfoStatusCode(status) && status == HTTP_STATUS_OK) {
                CFile out;
                if (out.Open(temp, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary)) {
                    BYTE buf[16384];
                    UINT br;
                    while ((br = file->Read(buf, sizeof(buf))) > 0) {
                        out.Write(buf, br);
                    }
                    out.Close();
                    ok = true;
                }
            } else {
                CASTING_LOG(_T("cast test: %s returned HTTP %lu"), url.GetString(), status);
            }
            file->Close();
            delete file;
        }
    } catch (CInternetException* pEx) {
        pEx->Delete();
        CASTING_LOG(_T("cast test: could not download %s"), url.GetString());
    }

    if (ok) {
        m_tempFile = temp;
        return temp;
    }
    DeleteFile(temp);
    return CString();
}

int CCastTestDlg::WaitForLoad()
{
    // A receiver that cannot output a layout does not refuse the load: it reports
    // Playing for an instant and then errors (measured at ~60 ms on a real Google
    // receiver). So Playing is not trusted until it has held for a stabilization
    // window; a real play holds it for the length of the tone.
    const ULONGLONG deadline = GetTickCount64() + 15000;
    const ULONGLONG stableFor = 2000;
    ULONGLONG playingSince = 0;
    for (;;) {
        switch (m_pTarget->GetState()) {
            case CastTargetState::Playing:
                if (playingSince == 0) {
                    playingSince = GetTickCount64();
                }
                if (GetTickCount64() - playingSince >= stableFor) {
                    return 1;
                }
                break;
            case CastTargetState::Failed:
            case CastTargetState::Ended:
                return 0; // errored, whether or not it flashed Playing first
            case CastTargetState::TakenOver:
                return -1;
            default:
                playingSince = 0; // dropped back to buffering; the window restarts
                break;
        }
        if (GetTickCount64() > deadline) {
            // Never settled. Having seen Playing at all points at a flaky output
            // rather than a connection that never arrived.
            return playingSince ? 0 : -1;
        }
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        Sleep(50);
    }
}

int CCastTestDlg::AskHeard(int channels)
{
    const CString layout = ResStr(LayoutStringId(channels));

    CString q;
    q.Format(ResStr(IDS_CASTTEST_ASK_HEAR), layout.GetString());
    int r = AfxMessageBox(q, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) {
        return -1;
    }
    if (r == IDNO) {
        return 0; // nothing came out: the device did not play this layout
    }

    r = AfxMessageBox(ResStr(IDS_CASTTEST_ASK_SURROUND), MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) {
        return -1;
    }
    // Heard, but only from the front pair, is the device folding the surround
    // down -- which is exactly the outcome this test exists to catch.
    return r == IDYES ? 1 : 0;
}

int CCastTestDlg::TestLayout(int channels)
{
    // Each layout is its own small session. A receiver that errors on a layout
    // leaves the media session dead -- it will not take a second load until it
    // is reconnected -- so the ladder cannot share one connection across rungs;
    // it connects, plays, and tears down for every rung it tries.
    CString text;
    text.Format(ResStr(IDS_CASTTEST_CONNECTING), m_device.DisplayName().GetString());
    SetStatus(text);

    bool connected;
    {
        CWaitCursor wait;
        connected = m_pTarget->ConnectSaved(m_device, 1500, 4000);
    }
    if (!connected) {
        SetStatus(ResStr(IDS_CASTTEST_NOCONNECT));
        return -1;
    }

    text.Format(ResStr(IDS_CASTTEST_FETCHING), ResStr(LayoutStringId(channels)).GetString());
    SetStatus(text);

    const CString tone = FetchTone(channels);
    if (tone.IsEmpty()) {
        m_pTarget->StopCasting();
        SetStatus(ResStr(IDS_CASTTEST_NOTONE));
        return -1;
    }

    CastMediaInfo info;
    info.audio = SelectedCodec();
    info.channels = channels;
    info.durationSec = 8.0;
    m_pTarget->LoadMedia(tone, ResStr(LayoutStringId(channels)), info.durationSec, 0.0, info);

    const int loaded = WaitForLoad();
    int result;
    if (loaded < 0) {
        result = -1; // the network or the connection gave out, not the device
    } else if (loaded == 0) {
        result = 0;  // the device refused or errored on this layout
    } else {
        text.Format(ResStr(IDS_CASTTEST_PLAYING), ResStr(LayoutStringId(channels)).GetString());
        SetStatus(text);
        result = AskHeard(channels); // asked while the tone is still playing
    }

    m_pTarget->StopCasting();
    if (result < 0) {
        // Aborted here rather than answered: the connection dropped mid-play or
        // the user cancelled. This path owns the final line so OnStart does not
        // paint a vaguer one over it.
        SetStatus(ResStr(IDS_CASTTEST_STOPPED));
    }
    return result;
}

void CCastTestDlg::OnStart()
{
    if (m_bRunning || !m_pTarget) {
        return;
    }
    if (m_pTarget->IsCasting()) {
        AfxMessageBox(IDS_CASTTEST_BUSY, MB_ICONINFORMATION | MB_OK);
        return;
    }

    m_bRunning = true;
    m_codecCombo.EnableWindow(FALSE);
    GetDlgItem(IDC_CASTTEST_START)->EnableWindow(FALSE);
    GetDlgItem(IDOK)->EnableWindow(FALSE); // no closing mid-test; the session must be torn down first

    CString text;
    int result = 0; // 0 so far means nothing above stereo was heard
    bool aborted = false;
    for (int channels : {8, 6}) {
        const int r = TestLayout(channels);
        if (r < 0) {
            aborted = true;
            break;
        }
        if (r > 0) {
            result = channels;
            break;
        }
    }

    if (aborted) {
        // TestLayout already set the specific reason (could not connect, could
        // not fetch the tone, or the test was stopped); leave it.
    } else if (result > 0) {
        m_result = result;
        text.Format(ResStr(IDS_CASTTEST_RESULT), ResStr(LayoutStringId(result)).GetString());
        SetStatus(text);
    } else {
        m_result = 2; // heard neither surround layout: stereo is the floor
        SetStatus(ResStr(IDS_CASTTEST_RESULT_STEREO));
    }

    if (!m_tempFile.IsEmpty()) {
        DeleteFile(m_tempFile);
        m_tempFile.Empty();
    }

    m_bRunning = false;
    m_codecCombo.EnableWindow(TRUE);
    GetDlgItem(IDC_CASTTEST_START)->EnableWindow(TRUE);
    GetDlgItem(IDOK)->EnableWindow(TRUE);
}

void CCastTestDlg::OnDestroy()
{
    if (!m_tempFile.IsEmpty()) {
        DeleteFile(m_tempFile);
        m_tempFile.Empty();
    }
    __super::OnDestroy();
}
