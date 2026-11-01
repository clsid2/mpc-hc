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

#pragma once

#include "EventDispatcher.h"
#include "PPageBase.h"
#include "Shaders.h"
#include "CMPCThemePPageBase.h"
#include "CMPCThemeListBox.h"
#include "CMPCThemeComboBox.h"
#include "CMPCThemeStatic.h"

class CShaderListBox : public CMPCThemeListBox
{
public:
    CShaderListBox();
    int AddShader(const Shader& shader);
    int InsertShader(int nIndex, const Shader& shader);
    int UpShader(UINT nIndex);
    int DownShader(UINT nIndex);
    int DeleteShader(UINT nIndex);
    virtual void ResetContent();
    void SetList(const ShaderList& list);
    const ShaderList& GetList();

    bool UpCurrentShader();
    bool DownCurrentShader();
    bool DeleteCurrentShader();

private:
    virtual int AddString(LPCTSTR) { ASSERT(FALSE); return LB_ERR; };         // YOU
    virtual int DeleteString(UINT) { ASSERT(FALSE); return LB_ERR; };         // SHALL
    virtual int InsertString(int, LPCTSTR) { ASSERT(FALSE); return LB_ERR; }; // NOT
    virtual int Dir(UINT, LPCTSTR) { ASSERT(FALSE); return LB_ERR; };         // USE

protected:
    ShaderList m_List;
    CString GetTitle(const Shader& shader);
    virtual void PreSubclassWindow() override;

    virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO* pTI) const override;
    BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP();
};

class CPPageShaders : public CMPCThemePPageBase
{
    DECLARE_DYNAMIC(CPPageShaders)
public:
    CPPageShaders();

private:
    enum { IDD = IDD_PPAGESHADERS };

    EventClient m_eventc;
    bool m_bCurrentPresetChanged;

protected:
    CShaderListBox m_Shaders, m_PreResize, m_PostResize;
    CMPCThemeComboBox m_PresetsBox;
    ShaderSelection::ShaderPresetMap m_Presets;
    CMPCThemeStatic mpcvrNote;
    CString m_mpcvrNoteText;

    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual BOOL OnSetActive();
    virtual BOOL OnApply() override;

    void CheckRenderer();
    void OnLoadShaderPreset();
    void OnSaveShaderPreset();
    void OnDeleteShaderPreset();
    void OnChangeShaderPresetText();

    void OnAddToPreResize();
    void OnAddToPostResize();
    void OnUpPreResize();
    void OnDownPreResize();
    void OnRemovePreResize();
    void OnUpPostResize();
    void OnDownPostResize();
    void OnRemovePostResize();
    void OnAddShaderFile();
    void OnRemoveShader();

    void OnUpdateLoadShaderPreset(CCmdUI* pCmdUI);
    void OnUpdateSaveShaderPreset(CCmdUI* pCmdUI);
    void OnUpdateDeleteShaderPreset(CCmdUI* pCmdUI);
    void OnUpdateAddToPreResize(CCmdUI* pCmdUI);
    void OnUpdateAddToPostResize(CCmdUI* pCmdUI);
    void OnUpdateUpPreResize(CCmdUI* pCmdUI);
    void OnUpdateDownPreResize(CCmdUI* pCmdUI);
    void OnUpdateRemovePreResize(CCmdUI* pCmdUI);
    void OnUpdateUpPostResize(CCmdUI* pCmdUI);
    void OnUpdateDownPostResize(CCmdUI* pCmdUI);
    void OnUpdateRemovePostResize(CCmdUI* pCmdUI);
    void OnUpdateAddShaderFile(CCmdUI* pCmdUI);
    void OnUpdateRemoveShader(CCmdUI* pCmdUI);

    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnLbnSelchangeList1();
};
