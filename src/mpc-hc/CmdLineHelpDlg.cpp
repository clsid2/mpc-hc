/*
 * (C) 2014, 2016-2017 see Authors.txt
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
#include "CmdLineHelpDlg.h"
#include "SettingsDefines.h"

CmdLineHelpDlg::CmdLineHelpDlg(const CString& cmdLine /*= _T("")*/)
    : CMPCThemeResizableDialog(CmdLineHelpDlg::IDD)
    , m_cmdLine(cmdLine)
{
}

CmdLineHelpDlg::~CmdLineHelpDlg()
{
}

void CmdLineHelpDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STATIC1, m_icon);
    DDX_Text(pDX, IDC_EDIT1, m_text);
}


BEGIN_MESSAGE_MAP(CmdLineHelpDlg, CMPCThemeResizableDialog)
    ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
END_MESSAGE_MAP()

BOOL CmdLineHelpDlg::OnInitDialog()
{
    EnableSaveRestoreKey(IDS_R_DLG_CMD_LINE_HELP);

    __super::OnInitDialog();

    LoadStaticIcon(IDC_STATIC1, IDI_INFORMATION, true);

    if (!m_cmdLine.IsEmpty()) {
        m_text.LoadString(IDS_UNKNOWN_SWITCH);
        m_text.AppendFormat(_T("%s\n\n"), m_cmdLine.GetString());
    }
    m_text.AppendFormat(_T("%s\n"), ResStr(IDS_USAGE).GetString());

    constexpr int cmdArgs[] = {
        IDS_CMD_PATHNAME, IDS_CMD_DUB, IDS_CMD_DUBDELAY, IDS_CMD_D3DFS, IDS_CMD_SUB,
        IDS_CMD_FILTER, IDS_CMD_DVD, IDS_CMD_DVDPOS_TC, IDS_CMD_DVDPOS_TIME, IDS_CMD_CD,
        IDS_CMD_DEVICE, IDS_CMD_DVBSCAN, IDS_CMD_DVBSCANOUT, IDS_CMD_DVBBANDWIDTH, IDS_CMD_DVBSYMBOLRATE, IDS_CMD_DVBSCANSAVE,
        IDS_CMD_OPEN, IDS_CMD_PLAY, IDS_CMD_CLOSE, IDS_CMD_SHUTDOWN,
        IDS_CMD_STANDBY, IDS_CMD_HIBERNATE, IDS_CMD_LOGOFF, IDS_CMD_LOCK, IDS_CMD_MONITOROFF,
        IDS_CMD_PLAYNEXT, IDS_CMD_FULLSCREEN, IDS_CMD_VIEWPRESET, IDS_CMD_MINIMIZED, IDS_CMD_NEW,
        IDS_CMD_ADD, IDS_CMD_RANDOMIZE, IDS_CMD_VOLUME, IDS_CMD_REGVID, IDS_CMD_REGAUD, IDS_CMD_REGPL,
        IDS_CMD_REGALL, IDS_CMD_UNREGALL, IDS_CMD_START, IDS_CMD_STARTPOS, IDS_CMD_AB_START, IDS_CMD_AB_END, IDS_CMD_FIXEDSIZE, IDS_CMD_MONITOR,
        IDS_CMD_AUDIORENDERER, IDS_CMD_SHADERPRESET, IDS_CMD_PNS, IDS_CMD_PNS_VALUES, IDS_CMD_ICONASSOC,
        IDS_CMD_NOFOCUS, IDS_CMD_WEBPORT, IDS_CMD_DEBUG, IDS_CMD_NOCRASHREPORTER,
        IDS_CMD_SLAVE, IDS_CMD_HWGPU, IDS_CMD_RESET, IDS_CMD_MUTE, IDS_CMD_THUMBNAILS, IDS_CMD_HELP
    };

    m_switchNames.reserve(_countof(cmdArgs));

    for (const auto& cmdArg : cmdArgs) {
        CString entry;
        if (cmdArg == IDS_CMD_PNS) {
            // Get the translated preset name from IDS_SCALE_16_9
            CString presetStr = ResStr(IDS_SCALE_16_9);
            int commaPos = presetStr.Find(',');
            CString presetName = (commaPos != -1) ? presetStr.Left(commaPos) : presetStr;

            entry.Format(ResStr(IDS_CMD_PNS), presetName.GetString());
        } else {
            entry = ResStr(cmdArg);
        }
        // Some strings use several tabs to line up their description on the default
        // tab stops. Collapse every tab run to a single tab so that all of them,
        // continuation lines included, land on the one stop we set below.
        while (entry.Replace(_T("\t\t"), _T("\t")) > 0) {}
        m_text.AppendFormat(_T("\n%s"), entry.GetString());

        // Remember the switch names, so that the column can be measured again when
        // the control's font changes.
        int tabPos = entry.Find(_T('\t'));
        if (tabPos != -1) {
            m_switchNames.push_back(entry.Left(tabPos));
        }
    }
    m_text.Replace(_T("\n"), _T("\r\n"));

    ApplySwitchColumnTabStop();

    UpdateData(FALSE);

    GetDlgItem(IDOK)->SetFocus(); // Force the focus on the OK button

    SetupAnchors();
    fulfillThemeReqs();

    return FALSE;
}

void CmdLineHelpDlg::ApplySwitchColumnTabStop()
{
    if (m_switchNames.empty()) {
        return;
    }

    CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT1);
    if (!pEdit || !pEdit->GetFont()) {
        return;
    }

    // Measure with the font the control actually uses, which the base class has
    // already scaled for the current DPI.
    CClientDC dc(pEdit);
    CFont* pOldFont = dc.SelectObject(pEdit->GetFont());

    int maxWidth = 0;
    for (const auto& switchName : m_switchNames) {
        maxWidth = std::max(maxWidth, (int)dc.GetTextExtent(switchName).cx);
    }

    TEXTMETRIC tm;
    dc.GetTextMetrics(&tm);
    int aveCharWidth = (int)tm.tmAveCharWidth;

    dc.SelectObject(pOldFont);

    if (maxWidth > 0 && aveCharWidth > 0) {
        // EM_SETTABSTOPS wants dialog units, which the control converts to pixels once,
        // against the font it has when the stop is set, so the stop has to be applied
        // again whenever that font changes. A single stop is repeated at every multiple,
        // which means an unexpectedly wide entry falls to the next one instead of
        // pushing the column for every other line.
        int gapWidth = 2 * aveCharWidth;
        pEdit->SetTabStops(MulDiv(maxWidth + gapWidth, 4, aveCharWidth));
    }
}

LRESULT CmdLineHelpDlg::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    // Let the base class install the new font first, then measure the column against it
    LRESULT result = __super::OnDpiChanged(wParam, lParam);

    ApplySwitchColumnTabStop();

    return result;
}

void CmdLineHelpDlg::SetupAnchors()
{
    AddAnchor(IDC_STATIC1, TOP_LEFT);
    AddAnchor(IDC_EDIT1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
}
