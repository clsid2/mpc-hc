/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
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
#include "SelectMediaType.h"
#include "DSUtil.h"


// CSelectMediaType dialog

IMPLEMENT_DYNAMIC(CSelectMediaType, CDialog)
CSelectMediaType::CSelectMediaType(CAtlArray<GUID>& guids, GUID guid, CWnd* pParent /*=nullptr*/)
    : CMPCThemeResizableDialog(CSelectMediaType::IDD, pParent)
    , m_guids(guids)
    , m_guid(guid)
{
    m_guidstr = CStringFromGUID(guid);
}

CSelectMediaType::~CSelectMediaType()
{
}

void CSelectMediaType::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_CBString(pDX, IDC_COMBO1, m_guidstr);
    DDX_Control(pDX, IDC_COMBO1, m_guidsctrl);
    fulfillThemeReqs();
}


BEGIN_MESSAGE_MAP(CSelectMediaType, CMPCThemeResizableDialog)
    ON_CBN_EDITCHANGE(IDC_COMBO1, OnCbnEditchangeCombo1)
    ON_UPDATE_COMMAND_UI(IDOK, OnUpdateOK)
END_MESSAGE_MAP()


// CSelectMediaType message handlers

BOOL CSelectMediaType::OnInitDialog()
{
    CMPCThemeResizableDialog::OnInitDialog();

    for (size_t i = 0; i < m_guids.GetCount(); i++) {
        m_guidsctrl.AddString(GetMediaTypeName(m_guids[i]));
    }

    AddAnchor(IDC_COMBO1, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDOK, BOTTOM_RIGHT);
    AddAnchor(IDCANCEL, BOTTOM_RIGHT);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CSelectMediaType::OnCbnEditchangeCombo1()
{
    UpdateData();
    int i = m_guidsctrl.FindStringExact(0, m_guidstr);
    if (i >= 0) {
        DWORD sel = m_guidsctrl.GetEditSel();
        m_guidsctrl.SetCurSel(i);
        m_guidsctrl.SetEditSel(sel, sel);
    }
}

// The list entry named by the edit text, which is what the box shows. The list's own selection can
// disagree with it: closing the dropped list without a pick puts the selection back on the entry
// picked before it dropped, while the edit keeps the typed text, and no CBN_EDITCHANGE is sent.
int CSelectMediaType::FindTypedType() const
{
    return m_guidstr.IsEmpty() ? CB_ERR : m_guidsctrl.FindStringExact(-1, m_guidstr);
}

void CSelectMediaType::OnUpdateOK(CCmdUI* pCmdUI)
{
    UpdateData();

    pCmdUI->Enable(!m_guidstr.IsEmpty() && (FindTypedType() >= 0 || GUIDFromCString(m_guidstr) != GUID_NULL));
}

void CSelectMediaType::OnOK()
{
    UpdateData();

    int i = FindTypedType();
    m_guid = i >= 0 ? m_guids[i] : GUIDFromCString(m_guidstr);

    CMPCThemeResizableDialog::OnOK();
}
