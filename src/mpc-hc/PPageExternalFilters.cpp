/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2017 see Authors.txt
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
#include <atlpath.h>
#include <dmoreg.h>
#include "PathUtils.h"
#include "mplayerc.h"
#include "PPageExternalFilters.h"
#include "ComPropertySheet.h"
#include "RegFilterChooserDlg.h"
#include "SelectMediaType.h"
#include "FGFilter.h"
#include "FGFilterLAV.h"
#include "moreuuids.h"
#include "FakeFilterMapper2.h"


IMPLEMENT_DYNAMIC(CPPageExternalFiltersListBox, CMPCThemePlayerListCtrl)
CPPageExternalFiltersListBox::CPPageExternalFiltersListBox()
{
}

void CPPageExternalFiltersListBox::PreSubclassWindow()
{
    __super::PreSubclassWindow();
    GetToolTips()->Activate(FALSE);
    //EnableToolTips(TRUE);
}

INT_PTR CPPageExternalFiltersListBox::OnToolHitTest(CPoint point, TOOLINFO* pTI) const
{
    int item = HitTest(point);
    if (item < 0) {
        return -1;
    }

    pTI->uFlags |= TTF_ALWAYSTIP;
    pTI->hwnd = m_hWnd;
    pTI->uId = (UINT)item + 1;
    VERIFY(GetItemRect(item, &pTI->rect, LVIR_BOUNDS));
    pTI->lpszText = LPSTR_TEXTCALLBACK;

    return pTI->uId;
}

BEGIN_MESSAGE_MAP(CPPageExternalFiltersListBox, CMPCThemePlayerListCtrl)
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
END_MESSAGE_MAP()

BOOL CPPageExternalFiltersListBox::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    // Forward the tooltip request to the list's parent
    GetParent()->SendMessage(WM_NOTIFY, (WPARAM)m_hWnd, (LPARAM)pNMHDR);

    *pResult = 0;

    return TRUE; // message was handled
}


// CPPageExternalFilters dialog

IMPLEMENT_DYNAMIC(CPPageExternalFilters, CMPCThemePPageBase)
CPPageExternalFilters::CPPageExternalFilters()
    : CMPCThemePPageBase(CPPageExternalFilters::IDD, CPPageExternalFilters::IDD)
    , m_pLastSelFilter(nullptr)
    , m_iLoadType(FilterOverride::PREFERRED)
{
}

CPPageExternalFilters::~CPPageExternalFilters()
{
}

void CPPageExternalFilters::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST1, m_filters);
    DDX_Radio(pDX, IDC_RADIO1, m_iLoadType);
    DDX_Control(pDX, IDC_EDIT1, m_dwMerit);
    DDX_Control(pDX, IDC_TREE1, m_tree);
    m_tree.fulfillThemeReqs();
}

void CPPageExternalFilters::Exchange(CListCtrl& list, int i, int j)
{
    CString text = list.GetItemText(i, 0);
    DWORD_PTR data = list.GetItemData(i);
    int check = list.GetCheck(i);
    bool selected = !!list.GetItemState(i, LVIS_SELECTED);

    list.SetItemText(i, 0, list.GetItemText(j, 0));
    list.SetItemData(i, list.GetItemData(j));
    list.SetCheck(i, list.GetCheck(j));
    list.SetItemState(i, LVIS_SELECTED, list.GetItemState(j, LVIS_SELECTED));

    list.SetItemText(j, 0, text);
    list.SetItemData(j, data);
    list.SetCheck(j, check);
    list.SetItemState(j, LVIS_SELECTED, selected ? LVIS_SELECTED : 0);

    int mark = list.GetSelectionMark();
    if (mark == i) {
        list.SetSelectionMark(j);
    } else if (mark == j) {
        list.SetSelectionMark(i);
    }
}

void CPPageExternalFilters::StepUp(CListCtrl& list)
{
    POSITION pos = list.GetFirstSelectedItemPosition();

    if (!pos) {
        return;
    }

    int i = list.GetNextSelectedItem(pos);

    ASSERT(i > 0);

    Exchange(list, i, i - 1);

    SetModified();
}

void CPPageExternalFilters::StepDown(CListCtrl& list)
{
    POSITION pos = list.GetFirstSelectedItemPosition();

    if (!pos) {
        return;
    }

    int i = list.GetNextSelectedItem(pos);

    ASSERT(i + 1 < list.GetItemCount());

    Exchange(list, i, i + 1);

    SetModified();
}

FilterOverride* CPPageExternalFilters::GetCurFilter()
{
    POSITION pos = m_filters.GetFirstSelectedItemPosition();

    if (!pos) {
        return nullptr;
    }

    return m_pFilters.GetAt((POSITION)m_filters.GetItemData(m_filters.GetNextSelectedItem(pos)));
}

void CPPageExternalFilters::SetupMajorTypes(CAtlArray<GUID>& guids)
{
    guids.RemoveAll();
    guids.Add(MEDIATYPE_NULL);
    guids.Add(MEDIATYPE_Video);
    guids.Add(MEDIATYPE_Audio);
    guids.Add(MEDIATYPE_Text);
    guids.Add(MEDIATYPE_Midi);
    guids.Add(MEDIATYPE_Stream);
    guids.Add(MEDIATYPE_Interleaved);
    guids.Add(MEDIATYPE_File);
    guids.Add(MEDIATYPE_ScriptCommand);
    guids.Add(MEDIATYPE_AUXLine21Data);
    guids.Add(MEDIATYPE_VBI);
    guids.Add(MEDIATYPE_Timecode);
    guids.Add(MEDIATYPE_LMRT);
    guids.Add(MEDIATYPE_URL_STREAM);
    guids.Add(MEDIATYPE_MPEG1SystemStream);
    guids.Add(MEDIATYPE_AnalogVideo);
    guids.Add(MEDIATYPE_AnalogAudio);
    guids.Add(MEDIATYPE_MPEG2_PACK);
    guids.Add(MEDIATYPE_MPEG2_PES);
    guids.Add(MEDIATYPE_MPEG2_SECTIONS);
    guids.Add(MEDIATYPE_DVD_ENCRYPTED_PACK);
    guids.Add(MEDIATYPE_DVD_NAVIGATION);
}

void CPPageExternalFilters::SetupSubTypes(CAtlArray<GUID>& guids)
{
    guids.RemoveAll();
    guids.Add(MEDIASUBTYPE_NULL);
}

BEGIN_MESSAGE_MAP(CPPageExternalFilters, CMPCThemePPageBase)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON2, OnUpdateFilter)
    ON_UPDATE_COMMAND_UI(IDC_RADIO1, OnUpdateFilter)
    ON_UPDATE_COMMAND_UI(IDC_RADIO2, OnUpdateFilter)
    ON_UPDATE_COMMAND_UI(IDC_RADIO3, OnUpdateFilter)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON3, OnUpdateFilterUp)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON4, OnUpdateFilterDown)
    ON_UPDATE_COMMAND_UI(IDC_EDIT1, OnUpdateFilterMerit)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON5, OnUpdateFilter)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON6, OnUpdateSubType)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON7, OnUpdateDeleteType)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON8, OnUpdateFilter)
    ON_BN_CLICKED(IDC_BUTTON1, OnAddRegistered)
    ON_BN_CLICKED(IDC_BUTTON2, OnRemoveFilter)
    ON_BN_CLICKED(IDC_BUTTON3, OnMoveFilterUp)
    ON_BN_CLICKED(IDC_BUTTON4, OnMoveFilterDown)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST1, OnDoubleClickFilter)
    ON_WM_VKEYTOITEM()
    ON_BN_CLICKED(IDC_BUTTON5, OnAddMajorType)
    ON_BN_CLICKED(IDC_BUTTON6, OnAddSubType)
    ON_BN_CLICKED(IDC_BUTTON7, OnDeleteType)
    ON_BN_CLICKED(IDC_BUTTON8, OnResetTypes)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnFilterChanged)
    ON_BN_CLICKED(IDC_RADIO1, OnClickedMeritRadioButton)
    ON_BN_CLICKED(IDC_RADIO2, OnClickedMeritRadioButton)
    ON_BN_CLICKED(IDC_RADIO3, OnClickedMeritRadioButton)
    ON_EN_CHANGE(IDC_EDIT1, OnChangeMerit)
    ON_NOTIFY(NM_DBLCLK, IDC_TREE1, OnDoubleClickType)
    ON_NOTIFY(TVN_KEYDOWN, IDC_TREE1, OnKeyDownType)
    ON_WM_DESTROY()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
END_MESSAGE_MAP()


// CPPageExternalFilters message handlers

BOOL CPPageExternalFilters::OnInitDialog()
{
    __super::OnInitDialog();

    m_filters.InsertColumn(0, _T(""));
    m_filters.SetExtendedStyle(m_filters.GetExtendedStyle() | LVS_EX_CHECKBOXES /*| LVS_EX_DOUBLEBUFFER*/);
    m_filters.setAdditionalStyles(LVS_EX_DOUBLEBUFFER);

    m_dropTarget.Register(this);

    const CAppSettings& s = AfxGetAppSettings();

    m_pFilters.RemoveAll();

    POSITION pos = s.m_filters.GetHeadPosition();
    while (pos) {
        CAutoPtr<FilterOverride> f(DEBUG_NEW FilterOverride(s.m_filters.GetNext(pos)));

        CString name(_T("<unknown>"));

        if (f->type == FilterOverride::REGISTERED) {
            name = CFGFilterRegistry(f->dispname).GetName();
            if (name.IsEmpty()) {
                name = f->name + _T(" <not registered>");
            }
        } else if (f->type == FilterOverride::EXTERNAL) {
            name = f->name;
            if (f->fTemporary) {
                name += _T(" <temporary>");
            }
            if (!PathUtils::Exists(MakeFullPath(f->path))) {
                name += _T(" <not found!>");
            }
        }

        int i = m_filters.InsertItem(m_filters.GetItemCount(), name);
        m_filters.SetCheck(i, f->fDisabled ? 0 : 1);
        m_filters.SetItemData(i, reinterpret_cast<DWORD_PTR>(m_pFilters.AddTail(f)));
    }

    m_filters.SetColumnWidth(0, LVSCW_AUTOSIZE);

    UpdateData(FALSE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CPPageExternalFilters::OnDestroy()
{
    m_dropTarget.Revoke();
    __super::OnDestroy();
}

BOOL CPPageExternalFilters::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.m_filters.RemoveAll();

    for (int i = 0; i < m_filters.GetItemCount(); i++) {
        if (POSITION pos = (POSITION)m_filters.GetItemData(i)) {
            CAutoPtr<FilterOverride> f(DEBUG_NEW FilterOverride(m_pFilters.GetAt(pos)));
            f->fDisabled = !m_filters.GetCheck(i);
            s.m_filters.AddTail(f);
        }
    }

    s.SaveExternalFilters();

    return __super::OnApply();
}

void CPPageExternalFilters::OnUpdateFilter(CCmdUI* pCmdUI)
{
    if (FilterOverride* f = GetCurFilter()) {
        pCmdUI->Enable(!(pCmdUI->m_nID == IDC_RADIO2 && f->type == FilterOverride::EXTERNAL));
    } else {
        pCmdUI->Enable(FALSE);
    }
}

void CPPageExternalFilters::OnUpdateFilterUp(CCmdUI* pCmdUI)
{
    POSITION pos = m_filters.GetFirstSelectedItemPosition();
    pCmdUI->Enable(pos && m_filters.GetNextSelectedItem(pos) > 0);
}

void CPPageExternalFilters::OnUpdateFilterDown(CCmdUI* pCmdUI)
{
    POSITION pos = m_filters.GetFirstSelectedItemPosition();
    pCmdUI->Enable(pos && m_filters.GetNextSelectedItem(pos) < m_filters.GetItemCount() - 1);
}

void CPPageExternalFilters::OnUpdateFilterMerit(CCmdUI* pCmdUI)
{
    UpdateData();
    pCmdUI->Enable(m_iLoadType == FilterOverride::MERIT);
}

void CPPageExternalFilters::OnUpdateSubType(CCmdUI* pCmdUI)
{
    HTREEITEM node = m_tree.GetSelectedItem();
    pCmdUI->Enable(node != nullptr && m_tree.GetItemData(node) == 0);
}

void CPPageExternalFilters::OnUpdateDeleteType(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!!m_tree.GetSelectedItem());
}

bool IsExternalVideoRenderer(CLSID clsid)
{
    return clsid == CLSID_FrankyRenderer || clsid == CLSID_MPCVR || clsid == CLSID_MadVR || clsid == CLSID_DXR || clsid == CLSID_EnhancedVideoRenderer || clsid == CLSID_VideoMixingRenderer9 || clsid == CLSID_VideoMixingRenderer || \
        clsid == CLSID_VideoRenderer || clsid == CLSID_VideoRendererDefault || clsid == CLSID_OverlayMixer || clsid == CLSID_OverlayMixer2 || clsid == CLSID_NullRenderer;
}

bool IgnoreExternalFilter(CLSID clsid)
{
    if (IsExternalVideoRenderer(clsid)) {
        return true;
    } else if (clsid == CLSID_XySubFilter || clsid == CLSID_XySubFilter_AutoLoader || clsid == CLSID_VSFilter || clsid == CLSID_VSFilter2) {
        return true;
    } else if (clsid == GUID_LAVSplitterSource) {
        return true;
    } else if (clsid == CLSID_DVDNavigator || clsid == CLSID_SmartTee || clsid == CLSID_VideoPortManager) {
        return true;
    } else if (clsid == CLSID_WMAsfWriter || clsid == CLSID_AviDest || clsid == CLSID_FileWriter || clsid == CLSID_DVMux || clsid == CLSID_MultFile) {
        return true;
    } else if (clsid == __uuidof(CMPEG2EncoderVideoDS) || clsid == __uuidof(CMPEG2EncoderDS) || clsid == __uuidof(CMPEG2EncoderAudioDS) || clsid == __uuidof(CMSAC3Enc)) {
        return true;
    }
    return false;
}

void CPPageExternalFilters::OnAddRegistered()
{
    CRegFilterChooserDlg dlg(this);
    if (dlg.DoModal() == IDOK) {
        while (!dlg.m_filters.IsEmpty()) {
            if (FilterOverride* f = dlg.m_filters.RemoveHead()) {
                CAutoPtr<FilterOverride> p(f);

                if (f->name.IsEmpty() && !f->dwMerit && f->guids.IsEmpty()) {
                    AfxMessageBox(L"Error: Unsupported filter", MB_OK);
                    continue;
                } else if (IsExternalVideoRenderer(f->clsid)) {
                    AfxMessageBox(L"You can not add video renderers as external filter. You should select your preferred video renderer on the Playback Output settings page.", MB_OK);
                    continue;
                }

                CString name = f->name;
                if (f->type == FilterOverride::EXTERNAL) {
                    if (!PathUtils::Exists(MakeFullPath(f->path))) {
                        name += _T(" <not found!>");
                    }
                }

                int i = m_filters.InsertItem(m_filters.GetItemCount(), name);
                m_filters.SetItemData(i, reinterpret_cast<DWORD_PTR>(m_pFilters.AddTail(p)));
                m_filters.SetCheck(i, 1);

                if (dlg.m_filters.IsEmpty()) {
                    m_filters.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
                    m_filters.SetSelectionMark(i);
                    OnFilterSelectionChange();
                }

                m_filters.SetColumnWidth(0, LVSCW_AUTOSIZE);

                SetModified();
            }
        }
    }
}

void CPPageExternalFilters::OnRemoveFilter()
{
    POSITION pos = m_filters.GetFirstSelectedItemPosition();
    if (pos) {
        int i = m_filters.GetNextSelectedItem(pos);
        POSITION pos2 = (POSITION)m_filters.GetItemData(i);
        if (!m_pFilters.IsEmpty() && pos2) {
            m_pFilters.RemoveAt(pos2);
            m_filters.DeleteItem(i);
            if (i >= m_filters.GetItemCount()) {
                i--;
            }
            if (i >= 0) {
                m_filters.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
                m_filters.SetSelectionMark(i);
            }
            OnFilterSelectionChange();
            SetModified();
        } else {
            ASSERT(false);
        }
    } else {
        ASSERT(false);
    }
}

void CPPageExternalFilters::OnMoveFilterUp()
{
    StepUp(m_filters);
}

void CPPageExternalFilters::OnMoveFilterDown()
{
    StepDown(m_filters);
}

void CPPageExternalFilters::OnDoubleClickFilter(NMHDR* pNMHDR, LRESULT* pResult)
{
    ASSERT(pNMHDR);
    ASSERT(pResult);
    *pResult = 0;

    if (FilterOverride* f = GetCurFilter()) {
        if (f->clsid == CLSID_VapourSynthFilter) {
            CString vsscript_path = L"VSScript.dll";
            wchar_t* env_value = nullptr;
            size_t size = 0;
            errno_t result = _wdupenv_s(&env_value, &size, L"VSSCRIPT_PATH");
            if (result == 0 && env_value != nullptr)
            {
                vsscript_path = env_value;
                free(env_value);
            }
            HMODULE hVS = LoadLibraryExW(vsscript_path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (hVS) {
                FreeLibrary(hVS);
            } else {
                AfxMessageBox(L"Error: Can not open filter properties. The required Vapoursynth runtime is not installed.", MB_OK);
                return;
            }
        }
        if (f->clsid == CLSID_AviSynthFilter) {
            HMODULE hAVS = LoadLibraryExW(L"avisynth.dll", nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (hAVS) {
                FreeLibrary(hAVS);
            } else {
                AfxMessageBox(L"Error: Can not open filter properties. The required Avisynth runtime is not installed.", MB_OK);
                return;
            }
        }

        CComPtr<IBaseFilter> pBF;
        CString name;
        if (f->type == FilterOverride::REGISTERED) {
            CStringW namew;
            if (CreateFilter(f->dispname, &pBF, namew)) {
                name = namew;
            }
        } else if (f->type == FilterOverride::EXTERNAL) {
            if (SUCCEEDED(LoadExternalFilter(f->path, f->clsid, &pBF))) {
                name = f->name;
            }
        }

        if (CComQIPtr<ISpecifyPropertyPages> pSPP = pBF) {
            CComPropertySheet ps(name, this);
            if (ps.AddPages(pSPP) > 0) {
                CComPtr<IFilterGraph> pFG;
                if (SUCCEEDED(pFG.CoCreateInstance(CLSID_FilterGraph))) {
                    pFG->AddFilter(pBF, L"");
                }

                ps.DoModal();
            }
        }
    }
}

int CPPageExternalFilters::OnVKeyToItem(UINT nKey, CListBox* pListBox, UINT nIndex)
{
    if (nKey == VK_DELETE) {
        OnRemoveFilter();
        return -2;
    }

    return __super::OnVKeyToItem(nKey, pListBox, nIndex);
}

void CPPageExternalFilters::OnAddMajorType()
{
    FilterOverride* f = GetCurFilter();
    if (!f) {
        return;
    }

    CAtlArray<GUID> guids;
    SetupMajorTypes(guids);

    CSelectMediaType dlg(guids, MEDIATYPE_NULL, this);
    if (dlg.DoModal() == IDOK) {
        POSITION pos = f->guids.GetHeadPosition();
        while (pos) {
            if (f->guids.GetNext(pos) == dlg.m_guid) {
                AfxMessageBox(IDS_EXTERNAL_FILTERS_ERROR_MT, MB_ICONEXCLAMATION | MB_OK, 0);
                return;
            }
            f->guids.GetNext(pos);
        }

        f->guids.AddTail(dlg.m_guid);
        pos = f->guids.GetTailPosition();
        f->guids.AddTail(GUID_NULL);

        CString major = GetMediaTypeName(dlg.m_guid);
        CString sub = GetMediaTypeName(GUID_NULL);

        HTREEITEM node = m_tree.InsertItem(major);
        m_tree.SetItemData(node, 0);

        node = m_tree.InsertItem(sub, node);
        m_tree.SetItemData(node, (DWORD_PTR)pos);

        SetModified();
    }
}

void CPPageExternalFilters::OnAddSubType()
{
    FilterOverride* f = GetCurFilter();
    if (!f) {
        return;
    }

    HTREEITEM node = m_tree.GetSelectedItem();
    if (!node) {
        return;
    }

    HTREEITEM child = m_tree.GetChildItem(node);
    if (!child) {
        return;
    }

    POSITION pos = (POSITION)m_tree.GetItemData(child);
    GUID major = f->guids.GetAt(pos);

    CAtlArray<GUID> guids;
    SetupSubTypes(guids);

    CSelectMediaType dlg(guids, MEDIASUBTYPE_NULL, this);
    if (dlg.DoModal() == IDOK) {
        for (child = m_tree.GetChildItem(node); child; child = m_tree.GetNextSiblingItem(child)) {
            pos = (POSITION)m_tree.GetItemData(child);
            f->guids.GetNext(pos);
            if (f->guids.GetAt(pos) == dlg.m_guid) {
                AfxMessageBox(IDS_EXTERNAL_FILTERS_ERROR_MT, MB_ICONEXCLAMATION | MB_OK, 0);
                return;
            }
        }

        f->guids.AddTail(major);
        pos = f->guids.GetTailPosition();
        f->guids.AddTail(dlg.m_guid);

        CString sub = GetMediaTypeName(dlg.m_guid);

        node = m_tree.InsertItem(sub, node);
        m_tree.SetItemData(node, (DWORD_PTR)pos);

        SetModified();
    }
}

void CPPageExternalFilters::OnDeleteType()
{
    if (FilterOverride* f = GetCurFilter()) {
        HTREEITEM node = m_tree.GetSelectedItem();
        if (!node) {
            return;
        }

        POSITION pos = (POSITION)m_tree.GetItemData(node);

        if (pos == nullptr) {
            for (HTREEITEM child = m_tree.GetChildItem(node); child; child = m_tree.GetNextSiblingItem(child)) {
                pos = (POSITION)m_tree.GetItemData(child);

                POSITION pos1 = pos;
                f->guids.GetNext(pos);
                POSITION pos2 = pos;
                f->guids.GetNext(pos);

                f->guids.RemoveAt(pos1);
                f->guids.RemoveAt(pos2);
            }

            m_tree.DeleteItem(node);
        } else {
            HTREEITEM parent = m_tree.GetParentItem(node);

            POSITION pos1 = pos;
            f->guids.GetNext(pos);
            POSITION pos2 = pos;
            f->guids.GetNext(pos);

            m_tree.DeleteItem(node);

            if (!m_tree.ItemHasChildren(parent)) {
                f->guids.SetAt(pos2, GUID_NULL);
                node = m_tree.InsertItem(GetMediaTypeName(GUID_NULL), parent);
                m_tree.SetItemData(node, (DWORD_PTR)pos1);
            } else {
                f->guids.RemoveAt(pos1);
                f->guids.RemoveAt(pos2);
            }
        }

        SetModified();
    }
}

void CPPageExternalFilters::OnResetTypes()
{
    if (FilterOverride* f = GetCurFilter()) {
        if (f->type == FilterOverride::REGISTERED) {
            CFGFilterRegistry fgf(f->dispname);
            if (!fgf.GetName().IsEmpty()) {
                f->guids.RemoveAll();
                f->backup.RemoveAll();

                f->guids.AddTailList(&fgf.GetTypes());
                f->backup.AddTailList(&fgf.GetTypes());
            } else {
                f->guids.RemoveAll();
                f->guids.AddTailList(&f->backup);
            }
        } else {
            f->guids.RemoveAll();
            f->guids.AddTailList(&f->backup);
        }

        m_pLastSelFilter = nullptr;
        OnFilterSelectionChange();

        SetModified();
    }
}

void CPPageExternalFilters::OnFilterChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    ASSERT(pNMHDR);
    ASSERT(pResult);

    auto pNMLV = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

    if (pNMLV->lParam && (pNMLV->uChanged & LVIF_STATE)) {
        if (pNMLV->uNewState & LVIS_SELECTED) {
            OnFilterSelectionChange();
        }
        if (pNMLV->uNewState & LVIS_STATEIMAGEMASK) {
            OnFilterCheckChange();
        }
    }

    *pResult = 0;
}

void CPPageExternalFilters::OnFilterSelectionChange()
{
    if (FilterOverride* f = GetCurFilter()) {
        if (m_pLastSelFilter == f) {
            return;
        }
        m_pLastSelFilter = f;

        m_iLoadType = f->iLoadType;
        UpdateData(FALSE);
        m_dwMerit = f->dwMerit;

        HTREEITEM dummy_item = m_tree.InsertItem(_T(""), 0, 0, nullptr, TVI_FIRST);
        if (dummy_item)
            for (HTREEITEM item = m_tree.GetNextVisibleItem(dummy_item); item; item = m_tree.GetNextVisibleItem(dummy_item)) {
                m_tree.DeleteItem(item);
            }

        CMapStringToPtr map;

        POSITION pos = f->guids.GetHeadPosition();
        while (pos) {
            POSITION tmp = pos;
            CString major = GetMediaTypeName(f->guids.GetNext(pos));
            CString sub = GetMediaTypeName(f->guids.GetNext(pos));

            HTREEITEM node = nullptr;

            void* val = nullptr;
            if (map.Lookup(major, val)) {
                node = (HTREEITEM)val;
            } else {
                map[major] = node = m_tree.InsertItem(major);
            }
            m_tree.SetItemData(node, 0);

            node = m_tree.InsertItem(sub, node);
            m_tree.SetItemData(node, (DWORD_PTR)tmp);
        }

        m_tree.DeleteItem(dummy_item);

        for (HTREEITEM item = m_tree.GetFirstVisibleItem(); item; item = m_tree.GetNextVisibleItem(item)) {
            m_tree.Expand(item, TVE_EXPAND);
        }

        m_tree.EnsureVisible(m_tree.GetRootItem());
    } else {
        m_pLastSelFilter = nullptr;

        m_iLoadType = FilterOverride::PREFERRED;
        UpdateData(FALSE);
        m_dwMerit = 0;

        m_tree.DeleteAllItems();
    }
}

void CPPageExternalFilters::OnFilterCheckChange()
{
    SetModified();
}

void CPPageExternalFilters::OnClickedMeritRadioButton()
{
    UpdateData();

    FilterOverride* f = GetCurFilter();
    if (f && f->iLoadType != m_iLoadType) {
        f->iLoadType = m_iLoadType;

        SetModified();
    }
}

void CPPageExternalFilters::OnChangeMerit()
{
    UpdateData();
    if (FilterOverride* f = GetCurFilter()) {
        DWORD dw;
        if (m_dwMerit.GetDWORD(dw) && f->dwMerit != dw) {
            f->dwMerit = dw;

            SetModified();
        }
    }
}

void CPPageExternalFilters::OnDoubleClickType(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;

    if (FilterOverride* f = GetCurFilter()) {
        HTREEITEM node = m_tree.GetSelectedItem();
        if (!node) {
            return;
        }

        POSITION pos = (POSITION)m_tree.GetItemData(node);
        if (!pos) {
            return;
        }

        f->guids.GetNext(pos);
        if (!pos) {
            return;
        }

        CAtlArray<GUID> guids;
        SetupSubTypes(guids);

        CSelectMediaType dlg(guids, f->guids.GetAt(pos), this);
        if (dlg.DoModal() == IDOK) {
            f->guids.SetAt(pos, dlg.m_guid);
            m_tree.SetItemText(node, GetMediaTypeName(dlg.m_guid));

            SetModified();
        }
    }
}

void CPPageExternalFilters::OnKeyDownType(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMTVKEYDOWN pTVKeyDown = reinterpret_cast<LPNMTVKEYDOWN>(pNMHDR);

    if (pTVKeyDown->wVKey == VK_DELETE) {
        OnDeleteType();
    }

    *pResult = 0;
}

DROPEFFECT CPPageExternalFilters::OnDropAccept(COleDataObject*, DWORD, CPoint)
{
    return DROPEFFECT_COPY;
}

void CPPageExternalFilters::OnDropFiles(CAtlList<CString>& slFiles, DROPEFFECT)
{
    SetActiveWindow();

    POSITION pos = slFiles.GetHeadPosition();

    while (pos) {
        CString fn = slFiles.GetNext(pos);

        CFilterMapper2 fm2(false);
        fm2.Register(fn);

        while (!fm2.m_filters.IsEmpty()) {
            if (FilterOverride* f = fm2.m_filters.RemoveHead()) {
                CAutoPtr<FilterOverride> p(f);

                if (IsExternalVideoRenderer(f->clsid)) {
                    AfxMessageBox(L"You can not add video renderers as external filter. You should select your preferred video renderer on the Playback Output settings page.", MB_OK);
                    continue;
                }

                int i = m_filters.InsertItem(m_filters.GetItemCount(), f->name);
                m_filters.SetItemData(i, reinterpret_cast<DWORD_PTR>(m_pFilters.AddTail(p)));
                m_filters.SetCheck(i, 1);

                if (fm2.m_filters.IsEmpty()) {
                    m_filters.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
                    m_filters.SetSelectionMark(i);
                    OnFilterSelectionChange();
                }

                m_filters.SetColumnWidth(0, LVSCW_AUTOSIZE);

                SetModified();
            }
        }
    }
}

BOOL CPPageExternalFilters::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;

    int nIndex = (int)pNMHDR->idFrom - 1;
    if (0 <= nIndex && nIndex < m_filters.GetItemCount()) {
        if (POSITION pos = (POSITION)m_filters.GetItemData(nIndex)) {
            CAutoPtr<FilterOverride>& f = m_pFilters.GetAt(pos);

            pTTT->lpszText = const_cast<LPTSTR>(f->type == FilterOverride::EXTERNAL ? f->path.GetString() : _T("Registered filter"));
        } else {
            ASSERT(FALSE);
        }
    } else {
        ASSERT(FALSE);
    }

    *pResult = 0;

    return TRUE; // message was handled
}
