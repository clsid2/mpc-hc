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
#include "ShaderController.h"
#include "MainFrm.h"
#include "mplayerc.h"

BEGIN_MESSAGE_MAP(CShaderController, CCmdTarget)
    ON_COMMAND(ID_VIEW_DEBUGSHADERS, OnViewDebugShaders)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DEBUGSHADERS, OnUpdateViewDebugShaders)
	ON_UPDATE_COMMAND_UI(ID_PRESIZE_SHADERS_TOGGLE, OnUpdateShaderToggle1)
	ON_COMMAND(ID_PRESIZE_SHADERS_TOGGLE, OnShaderToggle1)
	ON_UPDATE_COMMAND_UI(ID_POSTSIZE_SHADERS_TOGGLE, OnUpdateShaderToggle2)
	ON_COMMAND(ID_POSTSIZE_SHADERS_TOGGLE, OnShaderToggle2)
    ON_COMMAND(ID_SHADERS_SELECT, OnPlayShadersSelect)
    ON_COMMAND(ID_SHADERS_PRESET_NEXT, OnPlayShadersPresetNext)
    ON_COMMAND(ID_SHADERS_PRESET_PREV, OnPlayShadersPresetPrev)
    ON_COMMAND_RANGE(ID_SHADERS_PRESETS_START, ID_SHADERS_PRESETS_END, OnPlayShadersPresets)
END_MESSAGE_MAP()

CShaderController::CShaderController(CMainFrame& frame)
    : m_frame(frame)
    , m_graph(frame.Graph())
    , m_bToggleShader(false)
    , m_bToggleShaderScreenSpace(false)
{
}

void CShaderController::OnUpdateShaderToggle1(CCmdUI* pCmdUI)
{
	if (AfxGetAppSettings().m_Shaders.GetCurrentPreset().GetPreResize().empty()) {
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck (0);
	} else {
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck (m_bToggleShader);
	}
}

void CShaderController::OnUpdateShaderToggle2(CCmdUI* pCmdUI)
{
	CAppSettings& s = AfxGetAppSettings();

	if (s.m_Shaders.GetCurrentPreset().GetPostResize().empty()) {
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
	} else {
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(m_bToggleShaderScreenSpace);
	}
}

void CShaderController::OnShaderToggle1()
{
	m_bToggleShader = !m_bToggleShader;
    SetShaders(m_bToggleShader, m_bToggleShaderScreenSpace);
    if (m_bToggleShader) {
		m_frame.m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_PRESIZE_SHADERS_ENABLED));
	} else {
		m_frame.m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_PRESIZE_SHADERS_DISABLED));
	}

	if (m_graph.m_pCAP) {
		m_frame.RepaintVideo();
	}
}

void CShaderController::OnShaderToggle2()
{
	m_bToggleShaderScreenSpace = !m_bToggleShaderScreenSpace;
    SetShaders(m_bToggleShader, m_bToggleShaderScreenSpace);
    if (m_bToggleShaderScreenSpace) {
        m_frame.m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_POSTSIZE_SHADERS_ENABLED));
	} else {
        m_frame.m_OSD.DisplayMessage(OSD_TOPRIGHT, ResStr(IDS_POSTSIZE_SHADERS_DISABLED));
	}

	if (m_graph.m_pCAP) {
		m_frame.RepaintVideo();
	}
}

void CShaderController::OnViewDebugShaders()
{
    auto& dlg = m_pDebugShaders;
    if (dlg && !dlg->m_hWnd) {
        // something has destroyed the dialog and we didn't know about it
        dlg = nullptr;
    }
    if (!dlg) {
        // dialog doesn't exist - create and show it
        dlg = std::make_unique<CDebugShadersDlg>();
        dlg->ShowWindow(SW_SHOW);
    } else if (dlg->IsWindowVisible()) {
        if (dlg->IsIconic()) {
            // dialog is visible but iconic - restore it
            VERIFY(dlg->ShowWindow(SW_RESTORE));
        } else {
            // dialog is visible and not iconic - destroy it
            VERIFY(dlg->DestroyWindow());
            ASSERT(!dlg->m_hWnd);
            dlg = nullptr;
        }
    } else {
        // dialog is not visible - show it
        VERIFY(!dlg->ShowWindow(SW_SHOW));
    }
}

void CShaderController::OnUpdateViewDebugShaders(CCmdUI* pCmdUI)
{
    const auto& dlg = m_pDebugShaders;
    pCmdUI->SetCheck(dlg && dlg->m_hWnd && dlg->IsWindowVisible());
}

void CShaderController::OnPlayShadersSelect()
{
    m_frame.ShowOptions(IDD_PPAGESHADERS);
}

void CShaderController::OnPlayShadersPresetNext()
{
    auto& s = AfxGetAppSettings();
    if (s.m_Shaders.NextPreset()) {
        CString name;
        if (s.m_Shaders.GetCurrentPresetName(name)) {
            CString msg;
            msg.Format(IDS_OSD_SHADERS_PRESET, name.GetString());
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, msg);
        }
    }
}

void CShaderController::OnPlayShadersPresetPrev()
{
    auto& s = AfxGetAppSettings();
    if (s.m_Shaders.PrevPreset()) {
        CString name;
        if (s.m_Shaders.GetCurrentPresetName(name)) {
            CString msg;
            msg.Format(IDS_OSD_SHADERS_PRESET, name.GetString());
            m_frame.m_OSD.DisplayMessage(OSD_TOPLEFT, msg);
        }
    }
}

void CShaderController::OnPlayShadersPresets(UINT nID)
{
    ASSERT((nID >= ID_SHADERS_PRESETS_START) && (nID <= ID_SHADERS_PRESETS_END));
    auto& s = AfxGetAppSettings();
    int num = (int)nID - ID_SHADERS_PRESETS_START;
    auto presets = s.m_Shaders.GetPresets();
    ASSERT(num < (int)presets.size());
    for (const auto& pair : presets) {
        if (num-- == 0) {
            s.m_Shaders.SetCurrentPreset(pair.first);
            break;
        }
    }
}

ShaderC* CShaderController::GetShader(CString path, bool bD3D11)
{
	ShaderC* pShader = nullptr;

    CString shadersDir = ShaderList::GetShadersDir();
    CString shadersDir11 = ShaderList::GetShadersDir11();
    CString tPath = path;
    tPath.Replace(shadersDir, shadersDir11);

    //if the shader exists in the Shaders11 folder, use that one
    if (bD3D11 && ::PathFileExistsW(tPath)) {
        path = tPath;
    }

	for (auto& shader : m_ShaderCache) {
		if (shader.Match(path, bD3D11)) {
			pShader = &shader;
			break;
		}
	}

	if (!pShader) {
		if (::PathFileExistsW(path)) {
			CStdioFile file;
			if (file.Open(path, CFile::modeRead | CFile::shareDenyWrite | CFile::typeText)) {
				ShaderC shader;
				shader.label = path;

				CString str;
				file.ReadString(str); // read first string
				if (str.Left(25) == L"// $MinimumShaderProfile:") {
					shader.profile = str.Mid(25).Trim(); // shader version property
				} else {
					file.SeekToBegin();
				}

                if (shader.profile == L"ps_4_0" && !bD3D11 || shader.profile == L"ps_5_0") {
                    ASSERT(false);
                    return nullptr;
                } else if (bD3D11) {
                    shader.profile = L"ps_4_0";
                } else if (shader.profile == L"ps_3_sw") {
					shader.profile = L"ps_3_0";
				} else if (shader.profile != L"ps_2_0"
						&& shader.profile != L"ps_2_a"
						&& shader.profile != L"ps_2_b"
						&& shader.profile != L"ps_3_0") {
                    shader.profile = L"ps_3_0";
				}

				while (file.ReadString(str)) {
					shader.srcdata += str + L"\n";
				}

				shader.length = file.GetLength();

				FILETIME ftCreate, ftAccess, ftWrite;
				if (GetFileTime(file.m_hFile, &ftCreate, &ftAccess, &ftWrite)) {
					shader.ftwrite = ftWrite;
				}

				file.Close();

				m_ShaderCache.push_back(shader);
				pShader = &m_ShaderCache.back();
			}
		}
	}

	return pShader;
}

bool CShaderController::SaveShaderFile(ShaderC* shader)
{
	CString path;
	if (AfxGetMyApp()->GetAppSavePath(path)) {
		path.AppendFormat(L"Shaders\\%s.hlsl", static_cast<LPCWSTR>(shader->label));

		CStdioFile file;
		if (file.Open(path, CFile::modeWrite  | CFile::shareExclusive | CFile::typeText)) {
			file.SetLength(0);

			CString str;
			str.Format(L"// $MinimumShaderProfile: %s\n", static_cast<LPCWSTR>(shader->profile));
			file.WriteString(static_cast<LPCWSTR>(str));

			file.WriteString(static_cast<LPCWSTR>(shader->srcdata));
			file.Close();

			// delete out-of-date data from the cache
			for (auto it = m_ShaderCache.begin(), end = m_ShaderCache.end(); it != end; ++it) {
				if (it->Match(shader->label, false)) {
					m_ShaderCache.erase(it);
					break;
				}
			}

			return true;
		}
	}
	return false;
}

bool CShaderController::DeleteShaderFile(LPCWSTR label)
{
	CString path;
	if (AfxGetMyApp()->GetAppSavePath(path)) {
		path.AppendFormat(L"Shaders\\%s.hlsl", label);

		if (!::PathFileExistsW(path) || ::DeleteFileW(path)) {
			// if the file is missing or deleted successfully, then remove it from the cache
			for (auto it = m_ShaderCache.begin(), end = m_ShaderCache.end(); it != end; ++it) {
				if (it->Match(label, false)) {
					m_ShaderCache.erase(it);
					return true;
				}
			}
		}
	}

	return false;
}

void CShaderController::TidyShaderCache()
{
	CString appsavepath;
	if (!AfxGetMyApp()->GetAppSavePath(appsavepath)) {
		return;
	}

	for (auto it = m_ShaderCache.cbegin(); it != m_ShaderCache.cend(); ) {
		CString path(appsavepath);
		path += L"Shaders\\";
		path += (*it).label + L".hlsl";

		CFile file;
		if (file.Open(path, CFile::modeRead | CFile::modeCreate | CFile::shareDenyNone)) {
			ULONGLONG length = file.GetLength();
			FILETIME ftCreate = {}, ftAccess = {}, ftWrite = {};
			GetFileTime(file.m_hFile, &ftCreate, &ftAccess, &ftWrite);

			file.Close();

			if ((*it).length == length && CompareFileTime(&(*it).ftwrite, &ftWrite) == 0) {
				it++;
				continue; // actual shader
			}
		}

		m_ShaderCache.erase(it++); // outdated shader
	}
}

void CShaderController::SetShaders(bool bSetPreResize/* = true*/, bool bSetPostResize/* = true*/)
{
    if (m_frame.GetLoadState() != MLS::LOADED) {
        return;
    }

    const auto& s = AfxGetAppSettings();
    bool preFailed = false, postFailed = false;

    if (m_graph.m_pCAP3) { //interfaces for madVR and MPC-VR
        TidyShaderCache();
        const int PShaderMode = m_graph.m_pCAP3->GetPixelShaderMode();
        if (PShaderMode != 9 && PShaderMode != 11) {
            return;
        }

        m_graph.m_pCAP3->ClearPixelShaders(TARGET_FRAME);
        m_graph.m_pCAP3->ClearPixelShaders(TARGET_SCREEN);
        int shadercount = 0;
        if (bSetPreResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPreResize().ExpandMultiPassShaderList()) {
                ShaderC* pShader = GetShader(shader.filePath, PShaderMode == 11);
                if (pShader) {
                    shadercount++;
                    CStringW label;
                    label.Format(L"Shader%d", shadercount);
                    CStringA profile = pShader->profile;
                    CStringA srcdata = pShader->srcdata;
                    if (FAILED(m_graph.m_pCAP3->AddPixelShader(TARGET_FRAME, label, profile, srcdata))) {
                        preFailed = true;
                        m_graph.m_pCAP3->ClearPixelShaders(TARGET_FRAME);
                        break;
                    }
                }
            }
        }
        if (bSetPostResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPostResize().ExpandMultiPassShaderList()) {
                ShaderC* pShader = GetShader(shader.filePath, PShaderMode == 11);
                if (pShader) {
                    shadercount++;
                    CStringW label;
                    label.Format(L"Shader%d", shadercount);
                    CStringA profile = pShader->profile;
                    CStringA srcdata = pShader->srcdata;
                    if (FAILED(m_graph.m_pCAP3->AddPixelShader(TARGET_SCREEN, label, profile, srcdata))) {
                        postFailed = true;
                        m_graph.m_pCAP3->ClearPixelShaders(TARGET_SCREEN);
                        break;
                    }
                }
            }
        }
    } else if (m_graph.m_pCAP2) {
        // When pTarget parameter of ISubPicAllocatorPresenter2::SetPixelShader2() is nullptr,
        // internal video renderers select maximum available profile and madVR (the only external renderer that
        // supports shader part of ISubPicAllocatorPresenter2 interface) seems to ignore it altogether.
        m_graph.m_pCAP2->SetPixelShader2(nullptr, nullptr, false);
        if (bSetPreResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPreResize().ExpandMultiPassShaderList()) {
                if (FAILED(m_graph.m_pCAP2->SetPixelShader2(shader.GetCode(), nullptr, false))) {
                    preFailed = true;
                    m_graph.m_pCAP2->SetPixelShader2(nullptr, nullptr, false);
                    break;
                }
            }
        }
        m_graph.m_pCAP2->SetPixelShader2(nullptr, nullptr, true);
        if (bSetPostResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPostResize().ExpandMultiPassShaderList()) {
                if (FAILED(m_graph.m_pCAP2->SetPixelShader2(shader.GetCode(), nullptr, true))) {
                    postFailed = true;
                    m_graph.m_pCAP2->SetPixelShader2(nullptr, nullptr, true);
                    break;
                }
            }
        }
    } else if (m_graph.m_pCAP) {
        // shouldn't happen, all known renderers that support ISubPicAllocatorPresenter interface
        // support ISubPicAllocatorPresenter2 as well, and it takes priority
        ASSERT(FALSE);
        m_graph.m_pCAP->SetPixelShader(nullptr, nullptr);
        if (bSetPreResize) {
            for (const auto& shader : s.m_Shaders.GetCurrentPreset().GetPreResize().ExpandMultiPassShaderList()) {
                if (FAILED(m_graph.m_pCAP->SetPixelShader(shader.GetCode(), nullptr))) {
                    preFailed = true;
                    m_graph.m_pCAP->SetPixelShader(nullptr, nullptr);
                    break;
                }
            }
        }
        postFailed = !s.m_Shaders.GetCurrentPreset().GetPostResize().empty();
    }

    CString errMsg;
    if (preFailed && postFailed) {
        VERIFY(errMsg.LoadString(IDS_MAINFRM_BOTH_SHADERS_FAILED));
    } else if (preFailed) {
        VERIFY(errMsg.LoadString(IDS_MAINFRM_PRE_SHADERS_FAILED));
    } else if (postFailed) {
        VERIFY(errMsg.LoadString(IDS_MAINFRM_POST_SHADERS_FAILED));
    } else {
        return;
    }
    m_frame.SendStatusMessage(errMsg, 3000, true);
}

bool CShaderController::SetupShadersSubMenu()
{
    const auto& s = AfxGetAppSettings();

    CMPCThemeMenu& subMenu = m_frame.m_shadersMenu;
    // Empty the menu
    while (subMenu.RemoveMenu(0, MF_BYPOSITION));

    if (!(s.iDSVideoRendererType == VIDRNDT_DS_EVR_CUSTOM || s.iDSVideoRendererType == VIDRNDT_DS_SYNC
        || s.iDSVideoRendererType == VIDRNDT_DS_VMR9RENDERLESS || s.iDSVideoRendererType == VIDRNDT_DS_MADVR || s.iDSVideoRendererType == VIDRNDT_DS_MPCVR)) {
        return false;
    }        

    subMenu.AppendMenu(MF_BYCOMMAND | MF_STRING | MF_ENABLED, ID_PRESIZE_SHADERS_TOGGLE, ResStr(IDS_PRESIZE_SHADERS_TOGGLE));
    subMenu.AppendMenu(MF_BYCOMMAND | MF_STRING | MF_ENABLED, ID_POSTSIZE_SHADERS_TOGGLE, ResStr(IDS_POSTSIZE_SHADERS_TOGGLE));
    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_SHADERS_SELECT, ResStr(IDS_SHADERS_SELECT)));
    VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, ID_VIEW_DEBUGSHADERS, ResStr(IDS_SHADERS_DEBUG)));

    auto presets = s.m_Shaders.GetPresets();
    if (!presets.empty()) {
        CString current;
        bool selected = s.m_Shaders.GetCurrentPresetName(current);
        VERIFY(subMenu.AppendMenu(MF_SEPARATOR));
        UINT nID = ID_SHADERS_PRESETS_START;
        for (const auto& pair : presets) {
            if (nID > ID_SHADERS_PRESETS_END) {
                // too many presets
                ASSERT(FALSE);
                break;
            }
            VERIFY(subMenu.AppendMenu(MF_STRING | MF_ENABLED, nID, SanitizeMenuLabel(pair.first)));
            if (selected && pair.first == current) {
                VERIFY(subMenu.CheckMenuRadioItem(nID, nID, nID, MF_BYCOMMAND));
                selected = false;
            }
            nID++;
        }
    }
    return true;
}
