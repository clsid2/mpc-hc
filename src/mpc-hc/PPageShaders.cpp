/*
 * (C) 2013-2014, 2016 see Authors.txt
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
#include "PPageShaders.h"
#include "MainFrm.h"
#include "PathUtils.h"
#include "mplayerc.h"
#undef SubclassWindow

CShaderListBox::CShaderListBox()
    : CMPCThemeListBox()
{
}

int CShaderListBox::AddShader(const Shader& shader)
{
    int ret = CListBox::AddString(GetTitle(shader));
    if (ret >= 0) {
        m_List.push_back(shader);
    }
    return ret;
}

int CShaderListBox::InsertShader(int nIndex, const Shader& shader)
{
    int ret = CListBox::InsertString(nIndex, GetTitle(shader));
    if (ret >= 0) {
        m_List.insert(m_List.begin() + ret, shader);
    }
    return ret;
}

int CShaderListBox::UpShader(UINT nIndex)
{
    int ret = LB_ERR;
    if ((nIndex != 0) && ((int)nIndex < GetCount())) {
        ASSERT(nIndex < m_List.size());
        Shader shader = m_List.at(nIndex);
        VERIFY(DeleteShader(nIndex) != LB_ERR);
        ret = InsertShader(nIndex - 1, shader);
    }
    return ret;
}

int CShaderListBox::DownShader(UINT nIndex)
{
    int ret = LB_ERR;
    if ((int)nIndex + 1 < GetCount()) {
        ASSERT(nIndex < m_List.size());
        Shader shader = m_List.at(nIndex);
        VERIFY(DeleteShader(nIndex) != LB_ERR);
        ret = InsertShader(nIndex + 1, shader);
    }
    return ret;
}

int CShaderListBox::DeleteShader(UINT nIndex)
{
    int ret = CListBox::DeleteString(nIndex);
    if (ret >= 0) {
        ASSERT(ret + 1 == (int)m_List.size());
        m_List.erase(m_List.begin() + nIndex);
    }
    return ret;
}

void CShaderListBox::ResetContent()
{
    CListBox::ResetContent();
    m_List.clear();
}

void CShaderListBox::SetList(const ShaderList& list)
{
    if (&list != &m_List) {
        ResetContent();
        for (const auto& shader : list) {
            VERIFY(AddShader(shader) >= 0);
        }
    }
}

const ShaderList& CShaderListBox::GetList()
{
    return m_List;
}

bool CShaderListBox::UpCurrentShader()
{
    bool ret = false;
    int sel = GetCurSel();
    if ((sel != LB_ERR) && (sel > 0)) {
        sel = UpShader(sel);
        if (sel >= 0) {
            ret = true;
            VERIFY(SetCurSel(sel) != LB_ERR);
        } else {
            ASSERT(FALSE);
        }
    }
    return ret;
}

bool CShaderListBox::DownCurrentShader()
{
    bool ret = false;
    int sel = GetCurSel();
    if ((sel != LB_ERR) && (sel + 1 < GetCount())) {
        sel = DownShader(sel);
        if (sel >= 0) {
            ret = true;
            VERIFY(SetCurSel(sel) != LB_ERR);
        } else {
            ASSERT(FALSE);
        }
    }
    return ret;
}

bool CShaderListBox::DeleteCurrentShader()
{
    bool ret = false;
    int sel = GetCurSel();
    if (sel != LB_ERR) {
        if (DeleteShader(sel) != LB_ERR) {
            ret = true;
            if (GetCount() == sel) {
                sel--;
            }
            if (sel >= 0) {
                VERIFY(SetCurSel(sel) != LB_ERR);
            }
        } else {
            ASSERT(FALSE);
        }
    }
    return ret;
}

CString CShaderListBox::GetTitle(const Shader& shader)
{
    CString ret = PathUtils::FileName(shader.filePath);
    if (shader.filePath.Find(MULTIPASS_SUFFIX1) != -1) {
        ShaderList sl;
        CString passStr;
        sl.push_back(shader);
        size_t passes = sl.ExpandMultiPassShaderList().size();
        if (passes > 1) {
            passStr.Format(_T(" (%dP)"), passes);
            ret.Replace(MULTIPASS_SUFFIX1, passStr);
        }
    }
    if (!PathUtils::IsFile(shader.filePath)) {
        ret += _T(" <not found!>"); // TODO: externalize this string and merge it with the one in PPageExternalFilters
    }
    return ret;
}

void CShaderListBox::PreSubclassWindow()
{
    CMPCThemeListBox::PreSubclassWindow();
    if (AppIsThemeLoaded()) {
        EnableToolTips(FALSE);
    } else {
        EnableToolTips(TRUE);
    }
}

INT_PTR CShaderListBox::OnToolHitTest(CPoint point, TOOLINFO* pTI) const
{
    BOOL out = FALSE;
    UINT item = ItemFromPoint(point, out);
    if (out) {
        return -1;
    }
    pTI->uFlags |= TTF_ALWAYSTIP;
    pTI->hwnd = m_hWnd;
    pTI->uId = item + 1; //0 -> crash for addtool, add 1 and subtract later
    VERIFY(GetItemRect(item, &pTI->rect) != LB_ERR);
    pTI->lpszText = LPSTR_TEXTCALLBACK;
    return pTI->uId;
}

BOOL CShaderListBox::OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
    static CString text;
    if (pNMHDR->idFrom <= INT_MAX) {
        int item = (int)pNMHDR->idFrom - 1;
        ASSERT(m_List.size() <= INT_MAX);
        if ((item < GetCount()) && (item < (int)m_List.size())) {
            text = m_List.at(item).filePath;
            ((TOOLTIPTEXT*)pNMHDR)->lpszText = (LPTSTR)(LPCTSTR)text;
        } else {
            ASSERT(FALSE);
        }
    } else {
        ASSERT(FALSE);
    }
    return TRUE;
}

BEGIN_MESSAGE_MAP(CShaderListBox, CMPCThemeListBox)
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, OnToolTipNotify)
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(CPPageShaders, CMPCThemePPageBase)

CPPageShaders::CPPageShaders()
    : CMPCThemePPageBase(IDD, IDD)
    , m_bCurrentPresetChanged(false)
{
    EventRouter::EventSelection fires;
    fires.insert(MpcEvent::SHADER_LIST_CHANGED);
    GetEventd().Connect(m_eventc, fires);
}

void CPPageShaders::DoDataExchange(CDataExchange* pDX)
{
    CPPageBase::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_LIST1, m_Shaders);
    DDX_Control(pDX, IDC_LIST2, m_PreResize);
    DDX_Control(pDX, IDC_LIST3, m_PostResize);
    DDX_Control(pDX, IDC_COMBO1, m_PresetsBox);
    DDX_Control(pDX, IDC_STATIC5, mpcvrNote);
}

void CPPageShaders::CheckRenderer()
{
    if (AfxGetAppSettings().iDSVideoRendererType == VIDRNDT_DS_MPCVR) {
        mpcvrNote.ShowWindow(TRUE);
    } else {
        mpcvrNote.ShowWindow(FALSE);
    }
}


BOOL CPPageShaders::OnInitDialog()
{
    __super::OnInitDialog();
    const auto& s = AfxGetAppSettings();

    m_Shaders.SetList(ShaderList::GetDefaultShaders());
    for (const auto& shader : s.m_ShadersExtraList) {
        if (!shader.IsDefault()) {
            VERIFY(m_Shaders.AddShader(shader) >= 0);
        }
    }

    m_PreResize.SetList(s.m_Shaders.GetCurrentPreset().GetPreResize());
    m_PostResize.SetList(s.m_Shaders.GetCurrentPreset().GetPostResize());

    m_PresetsBox.Clear();
    m_Presets = s.m_Shaders.GetPresets();
    for (const auto& pair : m_Presets) {
        VERIFY(m_PresetsBox.AddString(pair.first) >= 0);
    }
    CString preset;
    if (s.m_Shaders.GetCurrentPresetName(preset)) {
        VERIFY(m_PresetsBox.SelectString(-1, preset) != CB_ERR);
    }

    SetMPCThemeButtonIcon(IDC_BUTTON6, { IDB_SHADER_UP });
    SetMPCThemeButtonIcon(IDC_BUTTON7, { IDB_SHADER_DOWN });
    SetMPCThemeButtonIcon(IDC_BUTTON8, { IDB_SHADER_DEL });
    SetMPCThemeButtonIcon(IDC_BUTTON9, { IDB_SHADER_UP });
    SetMPCThemeButtonIcon(IDC_BUTTON10, { IDB_SHADER_DOWN });
    SetMPCThemeButtonIcon(IDC_BUTTON11, { IDB_SHADER_DEL });

    m_bCurrentPresetChanged = false;

    CheckRenderer();
    fulfillThemeReqs();
    return TRUE;
}

BOOL CPPageShaders::OnSetActive() {
    BOOL ret = __super::OnSetActive();
    CheckRenderer();
    return ret;
}


BOOL CPPageShaders::OnApply()
{
    auto& s = AfxGetAppSettings();

    ShaderPreset preset;
    preset.SetLists(m_PreResize.GetList(), m_PostResize.GetList());
    bool bUnnamed = true;
    s.m_Shaders.SetPresets(m_Presets);
    if (m_PresetsBox.GetCurSel() != CB_ERR) {
        CString text;
        m_PresetsBox.GetWindowText(text);
        auto it = m_Presets.find(text);
        if (it != m_Presets.end() && it->second == preset) {
            VERIFY(s.m_Shaders.SetCurrentPreset(text));
            bUnnamed = false;
        }
    }
    if (bUnnamed) {
        s.m_Shaders.SetCurrentPreset(preset);
    }

    s.m_ShadersExtraList.clear();
    auto list = m_Shaders.GetList();
    for (const auto& shader : list) {
        if (!shader.IsDefault()) {
            s.m_ShadersExtraList.push_back(shader);
        }
    }

    m_eventc.FireEvent(MpcEvent::SHADER_LIST_CHANGED);

    return CPPageBase::OnApply();
}

void CPPageShaders::OnLoadShaderPreset()
{
    int sel = m_PresetsBox.GetCurSel();
    if (sel != CB_ERR) {
        CString name;
        m_PresetsBox.GetLBText(sel, name);
        if (!name.IsEmpty()) {
            auto it = m_Presets.find(name);
            if (it != m_Presets.cend()) {
                m_PreResize.SetList(it->second.GetPreResize());
                m_PostResize.SetList(it->second.GetPostResize());
            } else {
                ASSERT(FALSE);
            }
        } else {
            ASSERT(FALSE);
        }
    } else {
        ASSERT(FALSE);
    }
    m_bCurrentPresetChanged = false;
    SetModified();
}

void CPPageShaders::OnSaveShaderPreset()
{
    CString name;
    m_PresetsBox.GetWindowText(name);
    if (!name.IsEmpty()) {
        ShaderPreset preset;
        preset.SetLists(m_PreResize.GetList(), m_PostResize.GetList());
        m_Presets[name] = preset;
        if (m_PresetsBox.GetCurSel() == CB_ERR) {
            VERIFY(m_PresetsBox.SetCurSel(m_PresetsBox.AddString(name)) != CB_ERR);
        }
    } else {
        ASSERT(FALSE);
    }
    m_bCurrentPresetChanged = false;
}

void CPPageShaders::OnDeleteShaderPreset()
{
    int sel = m_PresetsBox.GetCurSel();
    if (sel != CB_ERR) {
        CString name;
        m_PresetsBox.GetLBText(sel, name);
        VERIFY(m_PresetsBox.DeleteString(sel) != CB_ERR);
        VERIFY(m_Presets.erase(name) > 0);
    } else {
        ASSERT(FALSE);
    }
    m_bCurrentPresetChanged = true;
}

void CPPageShaders::OnChangeShaderPresetText()
{
    CString text;
    m_PresetsBox.GetWindowText(text);
    int sel = m_PresetsBox.FindStringExact(-1, text);
    if (sel != CB_ERR) {
        VERIFY(m_PresetsBox.SetCurSel(sel) != CB_ERR);
    }
    m_bCurrentPresetChanged = true;
}

void CPPageShaders::OnAddToPreResize()
{
    int sel = m_Shaders.GetCurSel();
    ASSERT(sel != LB_ERR);
    VERIFY(m_PreResize.AddShader(m_Shaders.GetList().at(sel)) >= 0);
    m_bCurrentPresetChanged = true;
    SetModified();
}

void CPPageShaders::OnAddToPostResize()
{
    int sel = m_Shaders.GetCurSel();
    ASSERT(sel != LB_ERR);
    VERIFY(m_PostResize.AddShader(m_Shaders.GetList().at(sel)) >= 0);
    m_bCurrentPresetChanged = true;
    SetModified();
}

void CPPageShaders::OnUpPreResize()
{
    VERIFY(m_PreResize.UpCurrentShader());
    m_bCurrentPresetChanged = true;
    SetModified();
}

void CPPageShaders::OnDownPreResize()
{
    VERIFY(m_PreResize.DownCurrentShader());
    m_bCurrentPresetChanged = true;
    SetModified();
}

void CPPageShaders::OnRemovePreResize()
{
    VERIFY(m_PreResize.DeleteCurrentShader());
    m_bCurrentPresetChanged = true;
    SetModified();
}

void CPPageShaders::OnUpPostResize()
{
    VERIFY(m_PostResize.UpCurrentShader());
    m_bCurrentPresetChanged = true;
    SetModified();
}

void CPPageShaders::OnDownPostResize()
{
    VERIFY(m_PostResize.DownCurrentShader());
    m_bCurrentPresetChanged = true;
    SetModified();
}

void CPPageShaders::OnRemovePostResize()
{
    VERIFY(m_PostResize.DeleteCurrentShader());
    m_bCurrentPresetChanged = true;
    SetModified();
}

void CPPageShaders::OnAddShaderFile()
{
    ASSERT(CString(SHADERS_EXT).Left(1) == _T('.'));
    CString dlgFilter = CString(_T("Pixel Shader Files (*")) + SHADERS_EXT + _T(")|*") + SHADERS_EXT + _T("|");
    DWORD dlgFlags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    CFileDialog dlg(TRUE, nullptr, nullptr, dlgFlags, dlgFilter, this);
    // default buffer size is 260 chars
    // since we allow multi-select, we want it larger
    CString buff;
    const DWORD bufflen = 4096;
    dlg.GetOFN().lpstrFile = buff.GetBuffer(bufflen);
    dlg.GetOFN().nMaxFile = bufflen;

    if (dlg.DoModal() == IDOK) {
        auto list = m_Shaders.GetList();
        POSITION dlgPos = dlg.GetStartPosition();
        while (dlgPos) {
            Shader shader(dlg.GetNextPathName(dlgPos));
            if (std::find(list.begin(), list.end(), shader) == std::end(list)) {
                m_Shaders.AddShader(shader);
            }
        }

        SetModified();
    }
    buff.ReleaseBuffer(0);
}

void CPPageShaders::OnRemoveShader()
{
    VERIFY(m_Shaders.DeleteCurrentShader());
    SetModified();
}

void CPPageShaders::OnUpdateAddToPreResize(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_Shaders.GetCurSel() != LB_ERR);
}

void CPPageShaders::OnUpdateAddToPostResize(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_Shaders.GetCurSel() != LB_ERR);
}

void CPPageShaders::OnUpdateLoadShaderPreset(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_PresetsBox.GetCurSel() != CB_ERR);
}

void CPPageShaders::OnUpdateSaveShaderPreset(CCmdUI* pCmdUI)
{
    CString text;
    m_PresetsBox.GetWindowText(text);
    pCmdUI->Enable(m_bCurrentPresetChanged && !text.IsEmpty());
}

void CPPageShaders::OnUpdateDeleteShaderPreset(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_PresetsBox.GetCurSel() != CB_ERR);
}

void CPPageShaders::OnUpdateUpPreResize(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_PreResize.GetCurSel() > 0);
}

void CPPageShaders::OnUpdateDownPreResize(CCmdUI* pCmdUI)
{
    const int sel = m_PreResize.GetCurSel();
    pCmdUI->Enable((sel != LB_ERR) && (sel + 1 < m_PreResize.GetCount()));
}

void CPPageShaders::OnUpdateRemovePreResize(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_PreResize.GetCurSel() != LB_ERR);
}

void CPPageShaders::OnUpdateUpPostResize(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_PostResize.GetCurSel() > 0);
}

void CPPageShaders::OnUpdateDownPostResize(CCmdUI* pCmdUI)
{
    const int sel = m_PostResize.GetCurSel();
    pCmdUI->Enable((sel != LB_ERR) && (sel + 1 < m_PostResize.GetCount()));
}

void CPPageShaders::OnUpdateRemovePostResize(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_PostResize.GetCurSel() != LB_ERR);
}

void CPPageShaders::OnUpdateAddShaderFile(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);
}

void CPPageShaders::OnUpdateRemoveShader(CCmdUI* pCmdUI)
{
    const int sel = m_Shaders.GetCurSel();
    BOOL bEnable = (sel != LB_ERR);
    if (bEnable) {
        auto list = m_Shaders.GetList();
        ASSERT(sel < (int)list.size());
        if (list.at(sel).IsDefault()) { // default shaders are not allowed to be removed
            bEnable = FALSE;
        }
    }
    pCmdUI->Enable(bEnable);
}

BEGIN_MESSAGE_MAP(CPPageShaders, CMPCThemePPageBase)
    ON_BN_CLICKED(IDC_BUTTON1, OnAddToPreResize)
    ON_BN_CLICKED(IDC_BUTTON2, OnAddToPostResize)
    ON_BN_CLICKED(IDC_BUTTON3, OnLoadShaderPreset)
    ON_BN_CLICKED(IDC_BUTTON4, OnSaveShaderPreset)
    ON_BN_CLICKED(IDC_BUTTON5, OnDeleteShaderPreset)
    ON_BN_CLICKED(IDC_BUTTON6, OnUpPreResize)
    ON_BN_CLICKED(IDC_BUTTON7, OnDownPreResize)
    ON_BN_CLICKED(IDC_BUTTON8, OnRemovePreResize)
    ON_BN_CLICKED(IDC_BUTTON9, OnUpPostResize)
    ON_BN_CLICKED(IDC_BUTTON10, OnDownPostResize)
    ON_BN_CLICKED(IDC_BUTTON11, OnRemovePostResize)
    ON_BN_CLICKED(IDC_BUTTON12, OnAddShaderFile)
    ON_BN_CLICKED(IDC_BUTTON13, OnRemoveShader)
    ON_CBN_EDITCHANGE(IDC_COMBO1, OnChangeShaderPresetText)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON1, OnUpdateAddToPreResize)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON2, OnUpdateAddToPostResize)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON3, OnUpdateLoadShaderPreset)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON4, OnUpdateSaveShaderPreset)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON5, OnUpdateDeleteShaderPreset)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON6, OnUpdateUpPreResize)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON7, OnUpdateDownPreResize)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON8, OnUpdateRemovePreResize)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON9, OnUpdateUpPostResize)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON10, OnUpdateDownPostResize)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON11, OnUpdateRemovePostResize)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON12, OnUpdateAddShaderFile)
    ON_UPDATE_COMMAND_UI(IDC_BUTTON13, OnUpdateRemoveShader)
    ON_LBN_SELCHANGE(IDC_LIST1, &CPPageShaders::OnLbnSelchangeList1)
END_MESSAGE_MAP()


void CPPageShaders::OnLbnSelchangeList1()
{
    // TODO: Add your control notification handler code here
}
