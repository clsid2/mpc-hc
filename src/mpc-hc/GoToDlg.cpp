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

#include "stdafx.h"
#include "mplayerc.h"
#include "GoToDlg.h"
#include "SettingsDefines.h"
#include <cmath>

// CGoToDlg dialog

IMPLEMENT_DYNAMIC(CGoToDlg, CMPCThemeDialog)
CGoToDlg::CGoToDlg(REFERENCE_TIME time, REFERENCE_TIME maxTime, double fps, bool audioOnly, CWnd* pParent /*=nullptr*/)
    : CMPCThemeDialog(CGoToDlg::IDD, pParent)
    , m_time(time)
    , m_maxTime(maxTime)
    , m_fps(fps)
    , m_bAudioOnly(audioOnly)
{
    if (m_fps == 0 && !m_bAudioOnly) {
        CString str = AfxGetApp()->GetProfileString(IDS_R_SETTINGS, IDS_RS_GOTO_FPS, _T("0"));
        float fps2;
        if (_stscanf_s(str, _T("%f"), &fps2) == 1) {
            m_fps = fps2;
        }
    }
}

CGoToDlg::~CGoToDlg()
{
}

void CGoToDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_EDIT1, m_timestr);
    DDX_Text(pDX, IDC_EDIT2, m_framestr);
    DDX_Control(pDX, IDC_EDIT1, m_timeedit);
    DDX_Control(pDX, IDC_EDIT2, m_frameedit);
    fulfillThemeReqs();
}

BOOL CGoToDlg::OnInitDialog()
{
    __super::OnInitDialog();

    bool showHours = (m_maxTime >= 3600 * 1000 * 10000i64);

    if (showHours) {
        m_timeedit.EnableMask(_T("DD DD DD DDD"), _T("__:__:__.___"), L'0', _T("0123456789"));
    } else {
        m_timeedit.EnableMask(_T("DD DD DDD"), _T("__:__.___"), L'0', _T("0123456789"));
    }
    m_timeedit.EnableGetMaskedCharsOnly(false);
    m_timeedit.EnableSelectByGroup(false);

    if (m_bAudioOnly) {
        GetDlgItem(IDC_EDIT2)->EnableWindow(FALSE);
        GetDlgItem(IDC_OK2)->EnableWindow(FALSE);

        // Crop off the now-unused Frame section (instructions, IDC_EDIT2, IDC_OK2).
        // Shrink by the client-height delta so the non-client metrics stay DPI-correct.
        CRect rcFull(0, 0, 186, 120);
        CRect rcCropped(0, 0, 186, 63);
        MapDialogRect(&rcFull);
        MapDialogRect(&rcCropped);
        CRect rcWindow;
        GetWindowRect(&rcWindow);
        SetWindowPos(nullptr, 0, 0, rcWindow.Width(),
                     rcWindow.Height() - (rcFull.Height() - rcCropped.Height()),
                     SWP_NOMOVE | SWP_NOZORDER);
    }

    int time = (int)(m_time / 10000);
    if (time >= 0) {
        if (showHours) {
            m_timestr.Format(_T("%02d:%02d:%02d.%03d"),
                             (time / (1000 * 60 * 60)) % 60,
                             (time / (1000 * 60)) % 60,
                             (time / 1000) % 60, time % 1000);
        } else {
            m_timestr.Format(_T("%02d:%02d.%03d"),
                             (time / (1000 * 60)) % 60,
                             (time / 1000) % 60,
                             time % 1000);
        }

        if (m_fps > 0 && !m_bAudioOnly) {
            m_framestr.Format(_T("%d, %.3f"), (int)(m_fps * m_time / 10000000 + 1.5), m_fps);
        }

        UpdateData(FALSE);

        if (m_bAudioOnly) {
            m_timeedit.SetFocus();
            m_timeedit.SetSel(0, 0);
        } else {
            switch (AfxGetApp()->GetProfileInt(IDS_R_SETTINGS, IDS_RS_GOTO_LAST_USED, TYPE_TIME)) {
                default:
                case TYPE_TIME:
                    m_timeedit.SetFocus();
                    m_timeedit.SetSel(0, 0);
                    break;
                case TYPE_FRAME:
                    m_frameedit.SetFocus();
                    m_frameedit.SetSel(0, m_framestr.Find(','));
                    break;
            }
        }

    }

    return FALSE;

    //  return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}


BEGIN_MESSAGE_MAP(CGoToDlg, CMPCThemeDialog)
    ON_BN_CLICKED(IDC_OK1, OnParseTimeCode)
    ON_BN_CLICKED(IDC_OK2, OnParseFrameCode)
END_MESSAGE_MAP()


// CGoToDlg message handlers

void CGoToDlg::OnParseTimeCode()
{
    UpdateData();

    AfxGetApp()->WriteProfileInt(IDS_R_SETTINGS, IDS_RS_GOTO_LAST_USED, TYPE_TIME);

    unsigned int hh = 0;
    unsigned int mm = 0;
    float ss = 0.0f;
    WCHAR c; // extra character to ensure the end of string was reached

    if (((swscanf_s(m_timestr, L"%f%c", &ss, &c, 1) == 1) // ss[.ms]
            || (swscanf_s(m_timestr, L"%u:%f%c", &mm, &ss, &c, 1) == 2 && ss < 60.0f) // mm:ss[.ms]
            || (swscanf_s(m_timestr, L"%u:%u:%f%c", &hh, &mm, &ss, &c, 1) == 3 && mm < 60  && ss < 60.0f)) // hh:mm:ss[.ms]
            && ss >= 0.0f) {

        int time = (int)(1000.0f * ((hh * 60 + mm) * 60 + ss) + 0.5f);
        m_time = time * 10000i64;

        OnOK();
    } else {
        AfxMessageBox(IDS_GOTO_ERROR_PARSING_TIME, MB_ICONEXCLAMATION | MB_OK, 0);
    }
}

void CGoToDlg::OnParseFrameCode()
{
    UpdateData();

    AfxGetApp()->WriteProfileInt(IDS_R_SETTINGS, IDS_RS_GOTO_LAST_USED, TYPE_FRAME);

    int frame;
    float fps;
    WCHAR c1; // delimiter character
    WCHAR c2; // extra character to ensure the end of string was reached

    int result = swscanf_s(m_framestr, L"%d%c%f%c", &frame, &c1, 1, &fps, &c2, 1);
    if (result == 1) {
        m_time = frame < 2 ? 0LL : (REFERENCE_TIME)ceil(10000000.0 * (frame - 1) / m_fps);
        OnOK();
    } else if (result == 3 && c1 == L',') {
        m_time = frame < 2 ? 0LL : (REFERENCE_TIME)ceil(10000000.0 * (frame - 1) / fps);
        OnOK();
    } else if (result == 0 || c1 != L',') {
        AfxMessageBox(IDS_GOTO_ERROR_PARSING_TEXT, MB_ICONEXCLAMATION | MB_OK, 0);
    } else {
        AfxMessageBox(IDS_GOTO_ERROR_PARSING_FPS, MB_ICONEXCLAMATION | MB_OK, 0);
    }
}

void CGoToDlg::OnOK()
{
    if (m_time > m_maxTime) {
        AfxMessageBox(IDS_GOTO_ERROR_INVALID_TIME, MB_ICONEXCLAMATION | MB_OK, 0);
    } else {
        __super::OnOK();
    }
}

BOOL CGoToDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
        if (*GetFocus() == m_timeedit) {
            OnParseTimeCode();
        } else if (*GetFocus() == m_frameedit) {
            OnParseFrameCode();
        }

        return TRUE;
    }

    return __super::PreTranslateMessage(pMsg);
}
