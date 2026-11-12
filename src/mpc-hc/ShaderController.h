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

#pragma once

#include "AppSettings.h"
#include "DebugShadersDlg.h"
#include <list>
#include <memory>

class CMainFrame;
struct CPlayerGraph;

// The pixel shaders: applying the current preset to the renderer, the
// pre- and post-resize toggles, the preset commands and the Shaders
// submenu, the debug shaders dialog and the cache of compiled shader files.
// Composed into CMainFrame, which routes its commands here from OnCmdMsg.
// The menu object itself stays with the other dynamic menus in CMainFrame.
class CShaderController : public CCmdTarget
{
    CMainFrame& m_frame;
    CPlayerGraph& m_graph;

public:
    explicit CShaderController(CMainFrame& frame);

    void SetShaders(bool bSetPreResize = true, bool bSetPostResize = true);
	
	bool m_bToggleShader;
	bool m_bToggleShaderScreenSpace;
	std::list<ShaderC> m_ShaderCache;
	ShaderC* GetShader(CString path, bool bD3D11);
	bool SaveShaderFile(ShaderC* shader);
	bool DeleteShaderFile(LPCWSTR label);
	void TidyShaderCache();

    bool SetupShadersSubMenu();

    std::unique_ptr<CDebugShadersDlg> m_pDebugShaders;

    afx_msg void OnViewDebugShaders();
    afx_msg void OnUpdateViewDebugShaders(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShaderToggle1(CCmdUI* pCmdUI);
	afx_msg void OnUpdateShaderToggle2(CCmdUI* pCmdUI);
	afx_msg void OnShaderToggle1();
	afx_msg void OnShaderToggle2();
    afx_msg void OnPlayShadersSelect();
    afx_msg void OnPlayShadersPresetNext();
    afx_msg void OnPlayShadersPresetPrev();
    afx_msg void OnPlayShadersPresets(UINT nID);

    DECLARE_MESSAGE_MAP()
};
