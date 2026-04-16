/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015 see Authors.txt
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
#include "PPageFileInfoRes.h"
#include "PathUtils.h"


// CPPageFileInfoRes dialog

IMPLEMENT_DYNAMIC(CPPageFileInfoRes, CMPCThemeResizablePropertyPage)
CPPageFileInfoRes::CPPageFileInfoRes(CString path, IFilterGraph* pFG, IFileSourceFilter* pFSF)
    : CMPCThemeResizablePropertyPage(CPPageFileInfoRes::IDD, CPPageFileInfoRes::IDD)
    , m_hIcon(nullptr)
    , m_fn(path)
    , m_displayFn(path)
{
    if (pFSF) {
        CComHeapPtr<OLECHAR> pFN;
        if (SUCCEEDED(pFSF->GetCurFile(&pFN, nullptr))) {
            m_fn = pFN;
        }
    }

    m_fn.TrimRight('/');
    int i = std::max(m_fn.ReverseFind('\\'), m_fn.ReverseFind('/'));
    if (i >= 0 && i < m_fn.GetLength() - 1) {
        m_fn = m_fn.Mid(i + 1);
    }

    if (PathUtils::IsURL(path)) {
        m_displayFn = UrlDecodeWithUTF8(ShortenURL(m_fn));
    } else {
        m_displayFn = m_fn;
    }

    BeginEnumFilters(pFG, pEF, pBF) {
        if (CComQIPtr<IDSMResourceBag> pRB = pBF)
            if (pRB && pRB->ResGetCount() > 0) {
                for (DWORD j = 0; j < pRB->ResGetCount(); j++) {
                    CComBSTR name, desc, mime;
                    BYTE* pData = nullptr;
                    DWORD len = 0;
                    if (SUCCEEDED(pRB->ResGet(j, &name, &desc, &mime, &pData, &len, nullptr))) {
                        m_res.emplace_back(name, desc, mime, pData, len);
                        CoTaskMemFree(pData);
                    }
                }
            }
    }
    EndEnumFilters;
}

CPPageFileInfoRes::~CPPageFileInfoRes()
{
    if (m_hIcon) {
        DestroyIcon(m_hIcon);
    }
}

void CPPageFileInfoRes::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DEFAULTICON, m_icon);
    DDX_Text(pDX, IDC_EDIT1, m_displayFn);
    DDX_Control(pDX, IDC_LIST1, m_list);
}

BEGIN_MESSAGE_MAP(CPPageFileInfoRes, CMPCThemeResizablePropertyPage)
    ON_BN_CLICKED(IDC_BUTTON1, OnSaveAs)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateSaveAs)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnOpenEmbeddedResInBrowser)
END_MESSAGE_MAP()

// CPPageFileInfoRes message handlers

BOOL CPPageFileInfoRes::OnInitDialog()
{
    __super::OnInitDialog();

    m_hIcon = LoadIcon(m_fn, false);
    if (m_hIcon) {
        m_icon.SetIcon(m_hIcon);
    }

    //m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
    m_list.setAdditionalStyles(LVS_EX_FULLROWSELECT);
    m_list.InsertColumn(0, ResStr(IDS_EMB_RESOURCES_VIEWER_NAME), LVCFMT_LEFT, 187);
    m_list.InsertColumn(1, ResStr(IDS_EMB_RESOURCES_VIEWER_TYPE), LVCFMT_LEFT, 127);
    for (size_t i = 0, count = m_res.size(); i < count; i++) {
        const auto& r = m_res[i];
        int nItem = m_list.InsertItem(m_list.GetItemCount(), r.name);
        m_list.SetItemText(nItem, 1, r.mime);
        m_list.SetItemData(nItem, i);
    }

    AddAnchor(IDC_EDIT1, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_STATIC1, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_LIST1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_DEFAULTICON, TOP_LEFT);
    AddAnchor(IDC_BUTTON1, BOTTOM_LEFT);


    UpdateData(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageFileInfoRes::OnSetActive()
{
    BOOL ret = __super::OnSetActive();

    PostMessage(WM_NEXTDLGCTL, (WPARAM)GetParentSheet()->GetTabControl()->GetSafeHwnd(), TRUE);
    GetDlgItem(IDC_EDIT1)->PostMessage(WM_KEYDOWN, VK_HOME);

    return ret;
}

void CPPageFileInfoRes::OnSaveAs()
{
    int i = m_list.GetSelectionMark();
    if (i >= 0) {
        const auto& r = m_res[m_list.GetItemData(i)];

        CFileDialog fd(FALSE, nullptr, r.name,
                       OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR,
                       _T("All files|*.*||"), this, 0);

        if (fd.DoModal() == IDOK) {
            CFile file;
            if (file.Open(fd.GetPathName(), CFile::modeCreate | CFile::modeWrite)) {
                file.Write(r.data.GetData(), (UINT)r.data.GetCount());
                file.Close();
            }
        }
    }
}

void CPPageFileInfoRes::OnUpdateSaveAs(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_list.GetSelectedCount());
}

void CPPageFileInfoRes::OnOpenEmbeddedResInBrowser(NMHDR* pNMHDR, LRESULT* pResult)
{
    int i = m_list.GetSelectionMark();
    if (i >= 0) {
        const CAppSettings& s = AfxGetAppSettings();

        if (s.fEnableWebServer) {
            const auto& r = m_res[m_list.GetItemData(i)];

            CString url;
            url.Format(_T("http://localhost:%d/viewres.html?id=%Ix"), s.nWebServerPort, reinterpret_cast<uintptr_t>(&r));
            ShellExecute(nullptr, _T("open"), url, nullptr, nullptr, SW_SHOWDEFAULT);
        } else {
            AfxMessageBox(IDS_EMB_RESOURCES_VIEWER_INFO, MB_ICONINFORMATION);
        }
    }
}
