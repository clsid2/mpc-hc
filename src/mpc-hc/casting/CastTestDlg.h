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

#include "CMPCThemeDialog.h"
#include "CMPCThemeComboBox.h"
#include "CastTarget.h"
#include "resource.h"

// A short, interactive check of how many audio channels a cast device can
// actually output. The wire never says: a receiver that cannot handle a
// surround layout plays the picture and drops the sound without a word, so the
// only honest test is to play a known-good tone -- one speaker at a time, so a
// downmix is heard -- and ask the user what came out. The tone is fetched from
// the source tree at test time rather than carried in the binary, since almost
// nobody runs this and the file it would add is dead weight for everyone else.
class CCastTestDlg : public CMPCThemeDialog
{
    DECLARE_DYNAMIC(CCastTestDlg)

public:
    // preferredCodec is the loaded file's audio codec when there is one, so the
    // tone is played in the same codec the user's file uses -- a device can
    // output six channels of one codec and downmix six of another, so a test in
    // the wrong codec would answer a question the user did not ask.
    CCastTestDlg(CCastTarget* pTarget, const CastSavedDevice& device,
                 CastMediaInfo::Audio preferredCodec, CWnd* pParent = nullptr);
    virtual ~CCastTestDlg();

    enum { IDD = IDD_CASTTEST_DLG };

    // The channel count the test settled on, or -1 when it learned nothing
    // (could not connect, or the user cancelled). The caller writes it to the
    // device only when it is >= 0.
    int m_result = -1;

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnCancel(); // ignored while a test blocks the thread

    void SetStatus(const CString& text);
    // One rung of the ladder: fetch and play the layout, then decide whether the
    // device truly output it. Returns 1 supported, 0 not, -1 aborted.
    int TestLayout(int channels);
    // Downloads the tone for this layout in the chosen codec to a temp file;
    // empty on failure. The previous temp file, if any, is removed first.
    CString FetchTone(int channels);
    // Pumps messages until the load plays, fails, or times out. Returns 1
    // played, 0 device refused/errored, -1 timed out (network, not the device).
    int WaitForLoad();
    // Asks the user what they heard for a layout that reached playback.
    // Returns 1 supported, 0 not (silent or downmixed), -1 cancelled.
    int AskHeard(int channels);

    CastMediaInfo::Audio SelectedCodec() const;
    CString ToneBaseURL() const;

    CCastTarget* m_pTarget;
    CastSavedDevice m_device;
    CastMediaInfo::Audio m_preferredCodec;
    CMPCThemeComboBox m_codecCombo;
    bool m_bRunning = false;   // a test is in progress; the second click is ignored
    CString m_tempFile;        // the tone on disk now, removed as the test moves on

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnStart();
    afx_msg void OnDestroy();
    afx_msg void OnClose(); // ignored while a test blocks the thread
};
