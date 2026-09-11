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
#include "PPageMkvMetadata.h"

namespace {

void AppendLine(CString& text, const CString& label, const CString& value)
{
    if (!value.IsEmpty()) {
        text.AppendFormat(L"%s:\t%s\r\n", label, value.GetString());
    }
}

} // namespace

IMPLEMENT_DYNAMIC(CPPageMkvMetadata, CMPCThemeResizablePropertyPage)

CPPageMkvMetadata::CPPageMkvMetadata()
    : CMPCThemeResizablePropertyPage(CPPageMkvMetadata::IDD, CPPageMkvMetadata::IDD)
{
}

bool CPPageMkvMetadata::HasMetadata() const
{
    return m_metadata.HasDisplayableData();
}

void CPPageMkvMetadata::SetLoading()
{
    m_text = ResStr(IDS_MKVMETADATA_LOADING);
}

void CPPageMkvMetadata::SetMetadata(MkvMetadata&& metadata)
{
    m_metadata = std::move(metadata);
    m_text.Empty();

    AppendLine(m_text, ResStr(IDS_MKVMETADATA_TITLE), m_metadata.title);
    AppendLine(m_text, ResStr(IDS_MKVMETADATA_RELEASE_DATE), m_metadata.releaseDate);

    if (!m_metadata.creators.empty()) {
        if (!m_text.IsEmpty()) {
            m_text += L"\r\n";
        }
        m_text += ResStr(IDS_MKVMETADATA_CREATORS) + L"\r\n";
        for (const auto& creator : m_metadata.creators) {
            AppendLine(m_text, creator.name, creator.value);
        }
    }

    if (!m_metadata.cast.empty()) {
        if (!m_text.IsEmpty()) {
            m_text += L"\r\n";
        }
        m_text += ResStr(IDS_MKVMETADATA_CAST) + L"\r\n";
        for (const auto& member : m_metadata.cast) {
            if (member.role.IsEmpty()) {
                m_text += member.actor + L"\r\n";
            } else {
                AppendLine(m_text, member.actor, member.role);
            }
        }
    }

    if (GetSafeHwnd()) {
        m_metadataEdit.SetWindowText(m_text);
    }
}

void CPPageMkvMetadata::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MKVMETADATA_EDIT, m_metadataEdit);
    DDX_Text(pDX, IDC_MKVMETADATA_EDIT, m_text);
}

BEGIN_MESSAGE_MAP(CPPageMkvMetadata, CMPCThemeResizablePropertyPage)
END_MESSAGE_MAP()

BOOL CPPageMkvMetadata::OnInitDialog()
{
    __super::OnInitDialog();

    AddAnchor(IDC_MKVMETADATA_EDIT, TOP_LEFT, BOTTOM_RIGHT);
    UpdateData(FALSE);

    return TRUE;
}

BOOL CPPageMkvMetadata::OnSetActive()
{
    BOOL ret = __super::OnSetActive();
    PostMessage(WM_NEXTDLGCTL, (WPARAM)GetParentSheet()->GetTabControl()->GetSafeHwnd(), TRUE);
    return ret;
}
