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
#include "CastDevicesDlg.h"
#include "mplayerc.h"
#include "Logger.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>

#define CAST_REFRESH_TIMER 1
#define CAST_REFRESH_MS    1000

// How long the manual "Find" waits for one host to answer. It is a single
// unicast exchange with a machine the user says is there, so this is generous.
#define CAST_PROBE_MS      2500

// The longest name the user may give a device. Long enough for anything worth
// reading in a menu, short enough that nothing here can grow without bound.
#define CAST_NAME_MAX      64

namespace
{
    // "Chromecast" and "DLNA" are the protocols' own names and stay as they are
    // in every language.
    LPCTSTR ProtocolName(CastProtocol protocol)
    {
        return protocol == CastProtocol::Chromecast ? _T("Chromecast") : _T("DLNA");
    }

    // The address a host name stands for, or the text itself when it is already
    // one. Empty when nothing answers to that name.
    CString ResolveHostToIPv4(const CString& host)
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return CString();
        }

        IN_ADDR addr;
        if (inet_pton(AF_INET, CStringA(host), &addr) == 1) {
            WSACleanup();
            return host;
        }

        addrinfo hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        addrinfo* pResult = nullptr;
        CString resolved;
        if (getaddrinfo(CStringA(host), nullptr, &hints, &pResult) == 0) {
            for (const addrinfo* p = pResult; p; p = p->ai_next) {
                if (p->ai_family == AF_INET && p->ai_addr) {
                    char buf[16] = { 0 };
                    if (inet_ntop(AF_INET, &((sockaddr_in*)p->ai_addr)->sin_addr, buf, sizeof(buf))) {
                        resolved = CString(buf);
                        break;
                    }
                }
            }
            freeaddrinfo(pResult);
        }
        WSACleanup();
        return resolved;
    }

    // What the user typed, made fit to keep: no control characters, no
    // surrounding whitespace and no more than the edit box would take anyway.
    CString CleanUserName(const CString& text)
    {
        CString name(text);
        for (int i = 0; i < name.GetLength(); i++) {
            if (name[i] < _T(' ')) {
                name.SetAt(i, _T(' '));
            }
        }
        name.Trim();
        return name.Left(CAST_NAME_MAX);
    }
}

CCastDevicesDlg::CCastDevicesDlg(CCastTarget* pTarget, CWnd* pParent /*= nullptr*/)
    : CMPCThemeResizableDialog(CCastDevicesDlg::IDD, pParent)
    , m_pTarget(pTarget)
{
}

CCastDevicesDlg::~CCastDevicesDlg()
{
    if (m_bDiscovering && m_pTarget) {
        m_pTarget->ReleaseDiscovery(); // OnDestroy normally got here first
        m_bDiscovering = false;
    }
}

void CCastDevicesDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_CASTDEV_LIST, m_list);
    DDX_Control(pDX, IDC_CASTDEV_NAME, m_name);
    DDX_Control(pDX, IDC_CASTDEV_HOST, m_host);
    DDX_Control(pDX, IDC_CASTDEV_PORT, m_port);
    DDX_Control(pDX, IDC_CASTDEV_PROTOCOL, m_protocol);
}

BEGIN_MESSAGE_MAP(CCastDevicesDlg, CMPCThemeResizableDialog)
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_SIZE()
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_CASTDEV_LIST, OnLvnItemChanged)
    ON_BN_CLICKED(IDC_CASTDEV_ADD, OnAdd)
    ON_UPDATE_COMMAND_UI(IDC_CASTDEV_ADD, OnUpdateAdd)
    ON_BN_CLICKED(IDC_CASTDEV_REMOVE, OnRemove)
    ON_UPDATE_COMMAND_UI(IDC_CASTDEV_REMOVE, OnUpdateRemove)
    ON_BN_CLICKED(IDC_CASTDEV_RENAME, OnRename)
    ON_UPDATE_COMMAND_UI(IDC_CASTDEV_RENAME, OnUpdateRename)
    ON_BN_CLICKED(IDC_CASTDEV_RESCAN, OnRescan)
    ON_BN_CLICKED(IDC_CASTDEV_ADDMANUAL, OnFind)
    ON_UPDATE_COMMAND_UI(IDC_CASTDEV_ADDMANUAL, OnUpdateFind)
END_MESSAGE_MAP()

BOOL CCastDevicesDlg::OnInitDialog()
{
    EnableSaveRestoreKey(IDS_R_DLG_CAST_DEVICES);

    __super::OnInitDialog();

    m_list.InsertColumn(0, ResStr(IDS_CAST_DLG_COL_NAME));
    m_list.InsertColumn(1, ResStr(IDS_CAST_DLG_COL_ADVERTISED));
    m_list.InsertColumn(2, ResStr(IDS_CAST_DLG_COL_PROTOCOL));
    m_list.InsertColumn(3, ResStr(IDS_CAST_DLG_COL_ADDRESS));
    m_list.InsertColumn(4, ResStr(IDS_CAST_DLG_COL_STATUS));
    m_list.setAdditionalStyles(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    m_protocol.AddString(ProtocolName(CastProtocol::Chromecast));
    m_protocol.AddString(ProtocolName(CastProtocol::Dlna));
    m_protocol.SetCurSel(1); // the protocol a device is named by hand for
    m_name.SetLimitText(CAST_NAME_MAX);
    m_port.SetLimitText(5);

    m_rows.clear();
    for (const CastSavedDevice& dev : m_devices) {
        Row row;
        row.device = dev;
        row.saved = true;
        m_rows.emplace_back(std::move(row));
    }
    FillList();

    SetupAnchors();

    CRect wr;
    GetWindowRect(wr);
    SetMinTrackSize(wr.Size());

    UpdateColumnWidths();
    fulfillThemeReqs();

    StartDiscovery();
    VERIFY(SetTimer(CAST_REFRESH_TIMER, CAST_REFRESH_MS, nullptr));
    UpdateStatusText();

    return TRUE;
}

void CCastDevicesDlg::SetupAnchors()
{
    AddAnchor(IDC_CASTDEV_LIST, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_CASTDEV_STATUS, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_CASTDEV_ADD, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_REMOVE, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_RESCAN, BOTTOM_RIGHT);
    AddAnchor(IDC_CASTDEV_NAMELABEL, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_NAME, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_CASTDEV_RENAME, BOTTOM_RIGHT);
    AddAnchor(IDC_CASTDEV_MANUAL_GRP, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_CASTDEV_HOSTLABEL, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_HOST, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_PORTLABEL, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_PORT, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_PROTOLABEL, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_PROTOCOL, BOTTOM_LEFT);
    AddAnchor(IDC_CASTDEV_ADDMANUAL, BOTTOM_RIGHT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
    AddAnchor(IDCANCEL, BOTTOM_RIGHT);
}

void CCastDevicesDlg::StartDiscovery()
{
    m_scanStarted = GetTickCount64();
    if (m_pTarget && !m_bDiscovering) {
        m_bDiscovering = m_pTarget->AcquireDiscovery();
    }
}

void CCastDevicesDlg::OnDestroy()
{
    KillTimer(CAST_REFRESH_TIMER);
    // Nothing this window started outlives it. Only what it took is given
    // back: a discovery somebody else is holding -- a /castto search still
    // looking for its device -- goes on running and keeps its device list.
    if (m_bDiscovering && m_pTarget) {
        m_pTarget->ReleaseDiscovery();
    }
    m_bDiscovering = false;

    __super::OnDestroy();
}

BOOL CCastDevicesDlg::PreTranslateMessage(MSG* pMsg)
{
    // Enter in one of the entry fields does what the field is for, instead of
    // closing the dialog.
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) {
        const HWND hFocus = ::GetFocus();
        if (hFocus == m_name.GetSafeHwnd()) {
            OnRename();
            return TRUE;
        }
        if (hFocus == m_host.GetSafeHwnd() || hFocus == m_port.GetSafeHwnd()) {
            OnFind();
            return TRUE;
        }
    }
    return __super::PreTranslateMessage(pMsg);
}

void CCastDevicesDlg::OnOK()
{
    m_devices.clear();
    for (const Row& row : m_rows) {
        if (row.saved) {
            m_devices.emplace_back(row.device);
        }
    }
    __super::OnOK();
}

CCastDevicesDlg::Row* CCastDevicesDlg::FindRow(const CString& id)
{
    for (Row& row : m_rows) {
        if (row.device.id == id) {
            return &row;
        }
    }
    return nullptr;
}

void CCastDevicesDlg::RebuildRows()
{
    for (Row& row : m_rows) {
        row.online = false;
    }

    for (const CastTargetDevice& dev : m_pTarget->GetDevices()) {
        Row* pRow = FindRow(dev.id);
        if (!pRow) {
            Row row;
            static_cast<CastTargetDevice&>(row.device) = dev;
            row.online = true;
            m_rows.emplace_back(std::move(row));
            continue;
        }
        pRow->online = true;
        // What the device says about itself is taken as it comes, except that
        // silence is not an answer: a renderer that has not yet been asked what
        // it accepts must not wipe what a previous session learned, and the
        // name the user gave it is never touched at all.
        const CastSavedDevice previous = pRow->device;
        static_cast<CastTargetDevice&>(pRow->device) = dev;
        pRow->device.userName = previous.userName;
        if (dev.formats.IsEmpty() && !previous.formats.IsEmpty()) {
            pRow->device.formats = previous.formats;
            pRow->device.supportsVideo = previous.supportsVideo;
            pRow->device.supportsAudio = previous.supportsAudio;
        }
        if (dev.model.IsEmpty() && !previous.model.IsEmpty()) {
            pRow->device.model = previous.model;
        }
    }

    // A device that is neither kept nor answering any more is nothing to show:
    // it would sit there claiming to have been found.
    m_rows.erase(std::remove_if(m_rows.begin(), m_rows.end(), [](const Row & row) {
        return !row.saved && !row.online;
    }), m_rows.end());
}

// Everything the list shows, in one string. The refresh runs once a second and
// rebuilding the list for nothing would flicker, throw the selection about and
// wipe a name the user is in the middle of typing.
CString CCastDevicesDlg::RowsSignature() const
{
    CString signature;
    for (const Row& row : m_rows) {
        signature.AppendFormat(_T("%s\1%s\1%s\1%s\1%d\1%d\2"), row.device.id.GetString(),
                               row.device.DisplayName().GetString(), row.device.name.GetString(),
                               row.device.address.GetString(), row.saved ? 1 : 0, row.online ? 1 : 0);
    }
    return signature;
}

void CCastDevicesDlg::FillList()
{
    m_signature = RowsSignature();

    const int selected = SelectedRow();
    const CString selectedId = selected >= 0 ? m_rows[selected].device.id : CString();

    // A device is called what it calls itself until the user says otherwise,
    // so for most of them the advertised name is the name and saying it twice
    // tells nobody anything. It is shown only where it differs, and the column
    // itself only when some row has something to put in it.
    m_bShowAdvertised = false;
    for (const Row& row : m_rows) {
        m_bShowAdvertised |= row.device.name != row.device.DisplayName();
    }

    m_bFilling = true;
    m_list.SetRedraw(FALSE);
    m_list.DeleteAllItems();
    for (size_t i = 0; i < m_rows.size(); i++) {
        const Row& row = m_rows[i];
        const int item = m_list.InsertItem((int)i, row.device.DisplayName());
        m_list.SetItemText(item, 1, row.device.name != row.device.DisplayName()
                           ? row.device.name : CString());
        m_list.SetItemText(item, 2, ProtocolName(row.device.protocol));
        m_list.SetItemText(item, 3, row.device.address);
        m_list.SetItemText(item, 4, ResStr(row.saved ? (row.online ? IDS_CAST_DLG_STATUS_SAVED_FOUND
                                                        : IDS_CAST_DLG_STATUS_SAVED)
                                           : IDS_CAST_DLG_STATUS_FOUND));
    }
    m_list.SetRedraw(TRUE);
    m_list.Invalidate();

    UpdateColumnWidths(); // the advertised column may have come or gone
    SelectById(selectedId);
    m_bFilling = false;
}

int CCastDevicesDlg::SelectedRow() const
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (!pos) {
        return -1;
    }
    const int item = m_list.GetNextSelectedItem(pos);
    return item >= 0 && item < (int)m_rows.size() ? item : -1;
}

void CCastDevicesDlg::SelectById(const CString& id)
{
    if (id.IsEmpty()) {
        return;
    }
    for (size_t i = 0; i < m_rows.size(); i++) {
        if (m_rows[i].device.id == id) {
            m_list.SetItemState((int)i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            m_list.EnsureVisible((int)i, FALSE);
            return;
        }
    }
}

void CCastDevicesDlg::UpdateColumnWidths()
{
    if (!::IsWindow(m_list.GetSafeHwnd())) {
        return;
    }
    CRect r;
    m_list.GetClientRect(r);
    const int width = std::max(r.Width(), 200);
    // name and advertised name take what is left over from the fixed columns,
    // and the name takes all of it when no device has an advertised name that
    // differs from it
    const int fixed = width * 40 / 100;
    const int names = width - fixed;
    const int advertised = m_bShowAdvertised ? names - names * 55 / 100 : 0;
    m_list.SetColumnWidth(0, names - advertised);
    m_list.SetColumnWidth(1, advertised);
    m_list.SetColumnWidth(2, fixed * 25 / 100);
    m_list.SetColumnWidth(3, fixed * 40 / 100);
    m_list.SetColumnWidth(4, fixed - fixed * 25 / 100 - fixed * 40 / 100);
}

void CCastDevicesDlg::UpdateStatusText()
{
    int online = 0;
    for (const Row& row : m_rows) {
        online += row.online ? 1 : 0;
    }
    const int seconds = (int)((GetTickCount64() - m_scanStarted) / 1000);

    CString text;
    text.Format(IDS_CAST_DLG_SEARCHING, online, seconds);
    SetDlgItemText(IDC_CASTDEV_STATUS, text);
}

void CCastDevicesDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == CAST_REFRESH_TIMER) {
        RebuildRows();
        if (RowsSignature() != m_signature) {
            FillList();
        }
        UpdateStatusText();
        return;
    }
    __super::OnTimer(nIDEvent);
}

void CCastDevicesDlg::OnSize(UINT nType, int cx, int cy)
{
    __super::OnSize(nType, cx, cy);
    UpdateColumnWidths();
}

void CCastDevicesDlg::OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    const LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if (!m_bFilling && (pNMLV->uChanged & LVIF_STATE)) {
        const int row = SelectedRow();
        m_name.SetWindowText(row >= 0 ? m_rows[row].device.userName : CString());
    }
    *pResult = 0;
}

void CCastDevicesDlg::OnAdd()
{
    const int row = SelectedRow();
    if (row >= 0) {
        m_rows[row].saved = true;
        // What the device will and will not be offered, recorded the moment it
        // is kept rather than only when something is cast to it.
        CastLogDeviceCapabilities(m_rows[row].device);
        FillList();
    }
}

void CCastDevicesDlg::OnUpdateAdd(CCmdUI* pCmdUI)
{
    const int row = SelectedRow();
    pCmdUI->Enable(row >= 0 && !m_rows[row].saved);
}

void CCastDevicesDlg::OnRemove()
{
    const int row = SelectedRow();
    if (row < 0) {
        return;
    }
    // A device that is still answering stays on the list as one that was
    // found; one that is not is simply gone.
    if (m_rows[row].online) {
        m_rows[row].saved = false;
        m_rows[row].device.userName.Empty();
    } else {
        m_rows.erase(m_rows.begin() + row);
    }
    FillList();
}

void CCastDevicesDlg::OnUpdateRemove(CCmdUI* pCmdUI)
{
    const int row = SelectedRow();
    pCmdUI->Enable(row >= 0 && m_rows[row].saved);
}

void CCastDevicesDlg::OnRename()
{
    const int row = SelectedRow();
    if (row < 0 || !m_rows[row].saved) {
        return;
    }
    CString name;
    m_name.GetWindowText(name);
    // An empty name is not a name: the device goes back to being called what
    // it calls itself.
    m_rows[row].device.userName = CleanUserName(name);
    m_name.SetWindowText(m_rows[row].device.userName);
    FillList();
}

void CCastDevicesDlg::OnUpdateRename(CCmdUI* pCmdUI)
{
    const int row = SelectedRow();
    pCmdUI->Enable(row >= 0 && m_rows[row].saved);
}

void CCastDevicesDlg::OnRescan()
{
    if (m_pTarget) {
        // Letting go and taking it again starts the search over from an empty
        // list, unless somebody else is holding the discovery too -- then it
        // keeps running and this only refreshes what it has found so far.
        if (m_bDiscovering) {
            m_pTarget->ReleaseDiscovery();
            m_bDiscovering = false;
        }
        StartDiscovery();
    }
    RebuildRows();
    FillList();
    UpdateStatusText();
}

void CCastDevicesDlg::OnFind()
{
    CString host;
    m_host.GetWindowText(host);
    host.Trim();
    if (host.IsEmpty() || !m_pTarget) {
        return;
    }

    CString portText;
    m_port.GetWindowText(portText);
    const CastProtocol protocol = m_protocol.GetCurSel() == 0 ? CastProtocol::Chromecast : CastProtocol::Dlna;

    CWaitCursor wait;
    CastTargetDevice device;
    const CString address = ResolveHostToIPv4(host);
    if (address.IsEmpty()
            || !m_pTarget->ProbeAddress(protocol, address, (UINT)_ttoi(portText), CAST_PROBE_MS, device)) {
        CASTING_LOG(_T("discovery: nothing answered a %s query at %s"),
                    protocol == CastProtocol::Chromecast ? _T("Chromecast") : _T("DLNA"),
                    address.IsEmpty() ? host.GetString() : address.GetString());
        AfxMessageBox(IDS_CAST_DLG_NO_ANSWER, MB_ICONINFORMATION | MB_OK);
        return;
    }
    CastLogDeviceCapabilities(device);

    Row* pRow = FindRow(device.id);
    if (!pRow) {
        m_rows.emplace_back(Row());
        pRow = &m_rows.back();
    }
    static_cast<CastTargetDevice&>(pRow->device) = device;
    pRow->saved = true;
    pRow->online = true;

    m_host.SetWindowText(CString());
    FillList();
    SelectById(device.id);
}

void CCastDevicesDlg::OnUpdateFind(CCmdUI* pCmdUI)
{
    CString host;
    m_host.GetWindowText(host);
    pCmdUI->Enable(!host.Trim().IsEmpty());
}
