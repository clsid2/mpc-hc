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
        IDS_CMD_DEVICE, IDS_CMD_OPEN, IDS_CMD_PLAY, IDS_CMD_CLOSE, IDS_CMD_SHUTDOWN,
        IDS_CMD_STANDBY, IDS_CMD_HIBERNATE, IDS_CMD_LOGOFF, IDS_CMD_LOCK, IDS_CMD_MONITOROFF,
        IDS_CMD_PLAYNEXT, IDS_CMD_FULLSCREEN, IDS_CMD_VIEWPRESET, IDS_CMD_MINIMIZED, IDS_CMD_NEW,
        IDS_CMD_ADD, IDS_CMD_RANDOMIZE, IDS_CMD_VOLUME, IDS_CMD_REGVID, IDS_CMD_REGAUD, IDS_CMD_REGPL,
        IDS_CMD_REGALL, IDS_CMD_UNREGALL, IDS_CMD_START, IDS_CMD_STARTPOS, IDS_CMD_AB_START, IDS_CMD_AB_END, IDS_CMD_FIXEDSIZE, IDS_CMD_MONITOR,
        IDS_CMD_AUDIORENDERER, IDS_CMD_SHADERPRESET, IDS_CMD_PNS, IDS_CMD_ICONASSOC,
        IDS_CMD_NOFOCUS, IDS_CMD_WEBPORT, IDS_CMD_DEBUG, IDS_CMD_NOCRASHREPORTER,
        IDS_CMD_SLAVE, IDS_CMD_HWGPU, IDS_CMD_RESET, IDS_CMD_MUTE, IDS_CMD_THUMBNAILS, IDS_CMD_HELP
    };

    for (const auto& cmdArg : cmdArgs) {
        m_text.AppendFormat(_T("\n%s"), ResStr(cmdArg).GetString());
    }
    m_text.Replace(_T("\n"), _T("\r\n"));

    UpdateData(FALSE);

    GetDlgItem(IDOK)->SetFocus(); // Force the focus on the OK button

    SetupAnchors();
    fulfillThemeReqs();

    return FALSE;
}

void CmdLineHelpDlg::SetupAnchors()
{
    AddAnchor(IDC_STATIC1, TOP_LEFT);
    AddAnchor(IDC_EDIT1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
}
