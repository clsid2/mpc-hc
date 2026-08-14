/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013, 2015, 2017 see Authors.txt
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

#define MERIT64(merit)      (((UINT64)(merit)) << 16)
#define MERIT64_DO_NOT_USE  MERIT64(MERIT_DO_NOT_USE)
#define MERIT64_DO_USE      MERIT64(MERIT_DO_NOT_USE + 1)
#define MERIT64_LOWEST      MERIT64(MERIT_DO_NOT_USE + 2)
#define MERIT64_UNLIKELY    (MERIT64(MERIT_UNLIKELY))
#define MERIT64_NORMAL      (MERIT64(MERIT_NORMAL))
#define MERIT64_PREFERRED   (MERIT64(MERIT_PREFERRED))
#define MERIT64_ABOVE_DSHOW (MERIT64(1) << 32)

#define LowMeritSuffix L" (low merit)"
#define LowMerit(x)    (CStringW(x) + LowMeritSuffix)


class CFGFilter
{
protected:
    CLSID m_clsid;
    CStringW m_name;
    struct {
        union {
            UINT64 val;
            struct {
                UINT64 low: 16, mid: 32, high: 16;
            };
        };
    } m_merit;
    CAtlList<GUID> m_types;

public:
    CFGFilter(const CLSID& clsid, CStringW name = L"", UINT64 merit = MERIT64_DO_USE);
    virtual ~CFGFilter();

    CLSID GetCLSID() const { return m_clsid; }
    CStringW GetName() const { return m_name; }
    UINT64 GetMerit() const { return m_merit.val; }
    DWORD GetMeritForDirectShow() const { return m_merit.mid; }
    const CAtlList<GUID>& GetTypes() const;
    void SetTypes(const CAtlList<GUID>& types);
    void AddType(const GUID& majortype, const GUID& subtype);
    bool CheckTypes(const CAtlArray<GUID>& types, bool fExactMatch);

    CAtlList<CString> m_protocols, m_extensions, m_chkbytes; // TODO: subtype?

    virtual HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks) = 0;
};

class CFGFilterRegistry : public CFGFilter
{
protected:
    CStringW m_DisplayName;
    CComPtr<IMoniker> m_pMoniker;

    void ExtractFilterData(BYTE* p, UINT len);

public:
    CFGFilterRegistry(IMoniker* pMoniker, UINT64 merit = MERIT64_DO_USE);
    CFGFilterRegistry(CStringW DisplayName, UINT64 merit = MERIT64_DO_USE);
    CFGFilterRegistry(const CLSID& clsid, UINT64 merit = MERIT64_DO_USE);

    CStringW GetDisplayName() { return m_DisplayName; }
    IMoniker* GetMoniker() { return m_pMoniker; }

    HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);
private:
    void QueryProperties();
};

template<class T>
class CFGFilterInternal : public CFGFilter
{
public:
    CFGFilterInternal(CStringW name = L"", UINT64 merit = MERIT64_DO_USE) : CFGFilter(__uuidof(T), name, merit) {}

    HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks) {
        CheckPointer(ppBF, E_POINTER);

        HRESULT hr = S_OK;
        CComPtr<IBaseFilter> pBF = DEBUG_NEW T(nullptr, &hr);
        if (FAILED(hr)) {
            return hr;
        }

        *ppBF = pBF.Detach();

        return hr;
    }
};

class CFGFilterFile : public CFGFilter
{
protected:
    CString m_path;
    HINSTANCE m_hInst;

public:
    CFGFilterFile(const CLSID& clsid, CString path, CStringW name = L"", UINT64 merit = MERIT64_DO_USE);

    HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);

    CString GetPath() { return m_path; };
};

class CFGFilterVideoRenderer : public CFGFilter
{
protected:
    HWND m_hWnd;
    bool m_bHasHookReceiveConnection;
    bool m_bIsPreview;

public:
    CFGFilterVideoRenderer(HWND hWnd, const CLSID& clsid, CStringW name = L"", UINT64 merit = MERIT64_DO_USE, bool preview = false);
    virtual ~CFGFilterVideoRenderer();

    HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);
};

class CFGFilterList
{
    struct filter_t {
        int index;
        CFGFilter* pFGF;
        int group;
        bool exactmatch, autodelete;

        bool operator <(const filter_t& rhs) const {
            if (group != rhs.group) {
                return group < rhs.group;
            }

            if (pFGF->GetMerit() != rhs.pFGF->GetMerit()) {
                return pFGF->GetMerit() > rhs.pFGF->GetMerit();
            }

            if (pFGF->GetCLSID() == rhs.pFGF->GetCLSID()) {
                CFGFilterFile* fgfa = dynamic_cast<CFGFilterFile*>(pFGF);
                CFGFilterFile* fgfb = dynamic_cast<CFGFilterFile*>(rhs.pFGF);

                if (fgfa && !fgfb) {
                    return true;
                }
                if (!fgfa && fgfb) {
                    return false;
                }
            }

            if (exactmatch && !rhs.exactmatch) {
                return true;
            }
            if (!exactmatch && rhs.exactmatch) {
                return false;
            }

            if (index != rhs.index) {
                return index < rhs.index;
            }

            return false;
        }
    };
    CAtlList<filter_t> m_filters;
    CAtlList<CFGFilter*> m_sortedfilters;

public:
    CFGFilterList();
    virtual ~CFGFilterList();

    bool IsEmpty() { return m_filters.IsEmpty(); }
    void RemoveAll();
    void Insert(CFGFilter* pFGF, int group, bool exactmatch = false, bool autodelete = true);

    POSITION GetHeadPosition();
    CFGFilter* GetNext(POSITION& pos);
};
