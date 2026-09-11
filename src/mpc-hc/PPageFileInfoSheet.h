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

#pragma once

#include "PPageFileInfoClip.h"
#include "PPageFileInfoDetails.h"
#include "PPageFileInfoRes.h"
#include "PPageFileMediaInfo.h"
#include "PPageMkvMetadata.h"
#include <afxdlgs.h>
#include "CMPCThemePropertySheet.h"
#include "CMPCThemeButton.h"
#include "CMPCThemeResizablePropertySheet.h"

#include <memory>

class CMainFrame;
struct MkvMetadataTaskState;

// CPPageFileInfoSheet

class CPPageFileInfoSheet : public CMPCThemeResizablePropertySheet
{
    DECLARE_DYNAMIC(CPPageFileInfoSheet)

private:
    CPPageFileInfoClip m_clip;
    CPPageFileInfoDetails m_details;
    CPPageFileInfoRes m_res;
    CPPageFileMediaInfo m_mi;
    CPPageMkvMetadata m_mkvMetadata;
    CMPCThemeButton m_Button_MI;
    bool m_mkvMetadataAdded = false;
    std::shared_ptr<MkvMetadataTaskState> m_mkvMetadataTask;

public:
    CPPageFileInfoSheet(CString path, CString ydlsrc, CMainFrame* pMainFrame, CWnd* pParentWnd);
    virtual ~CPPageFileInfoSheet();

    afx_msg void OnSaveAs();
    CString m_path;
protected:
    virtual BOOL OnInitDialog();

    afx_msg void OnTimer(UINT_PTR nIDEvent);
    void CompleteMkvMetadataTask();

    DECLARE_MESSAGE_MAP()
};
