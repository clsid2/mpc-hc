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

#include "CMPCThemeResizablePropertyPage.h"
#include "CMPCThemeEdit.h"
#include "MkvMetadata.h"

class CPPageMkvMetadata : public CMPCThemeResizablePropertyPage
{
    DECLARE_DYNAMIC(CPPageMkvMetadata)

private:
    MkvMetadata m_metadata;
    CString m_text;
    CMPCThemeEdit m_metadataEdit;

public:
    CPPageMkvMetadata();

    enum { IDD = IDD_MKVMETADATA };

    bool HasMetadata() const;
    void SetLoading();
    void SetMetadata(MkvMetadata&& metadata);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual BOOL OnSetActive();

    DECLARE_MESSAGE_MAP()
};
