/*
 * (C) 2013-2014 see Authors.txt
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

#include "CMPCThemeModelessResizableDialog.h"

#include "EventDispatcher.h"
#include "PixelShaderCompiler.h"
#include "Shaders.h"
#include "TimerWrappers.h"
#include "CMPCThemeComboBox.h"
#include "CMPCThemeEdit.h"
#include "CMPCThemeToolTipCtrl.h"

//mainly, this class seems to be used to block <enter> closing the dialog, and returning focus to the player when closed
class CModelessDialog : public CMPCThemeModelessResizableDialog
{
public:
    CModelessDialog(UINT nIDTemplate, CWnd* pParent);
    BOOL DestroyWindow();

private:
    virtual void OnCancel();
    virtual void OnOK();
};

class CDebugShadersDlg : public CModelessDialog, public FileChangeNotifier
{
public:
    CDebugShadersDlg();
    BOOL DestroyWindow();

private:
    enum { WM_APP_RECOMPILE_SHADER = WM_APP + 100 };

    enum class TimerOneTimeSubscriber {
        SELECTED_SHADER_CHANGE_COOLDOWN,
    };
    OneTimeTimerPool<TimerOneTimeSubscriber> m_timerOneTime;

    enum { IDD = IDD_DEBUGSHADERS_DLG };
    enum { ps_2_0 = 0, ps_2_b, ps_2_a, ps_3_0 };
    enum {
        TIMER_ONETIME_START = 1,
        TIMER_ONETIME_END = TIMER_ONETIME_START + 16,
    };

    UINT GetDialogTemplateID() const override { return IDD; }
    void SetupAnchors() override;

protected:
    int m_iVersion;
    CMPCThemeComboBox m_Shaders;
    CMPCThemeEdit m_DebugInfo;
    CPixelShaderCompiler m_Compiler;
    ShaderList m_list;

    EventClient m_eventc;
    void EventCallback(MpcEvent ev);

    virtual FileSet GetWatchedList() override;
    virtual void WatchedFilesChanged(const FileSet& changes) override;

    void ShaderFileChangedCooldownCallback();
    void OnListRefresh();
    int AppendShader(int loc, CString filePath);
    CString GetShaderPath(int loc);

    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;

    BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()
    afx_msg BOOL OnToolTipNotify(UINT id, NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnRecompileShader();
    afx_msg void OnSelChange();
    afx_msg void OnVersionClicked();
};
