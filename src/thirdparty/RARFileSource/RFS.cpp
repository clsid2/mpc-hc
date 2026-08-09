/*
 * Copyright (C) 2008-2012, OctaneSnail <os@v12pwr.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include <streams.h>
#ifdef STANDALONE_FILTER
#include <initguid.h>
#endif
#include <strsafe.h>

#include "RFS.h"
#include "Utils.h"
#include "unrar/rar.hpp"
#include "Anchor.h"
#include "resource.h"
#include "Mediatype.h"

// unrar 7.x removed GetVolNumPart() from pathfn.cpp; this is a local copy of
// the old implementation. Returns a pointer to the last digit of the volume
// number part inside ArcName.
static wchar* RfsGetVolNumPart(const wchar *ArcName)
{
  // We do not want to increment any characters in path component.
  for (const wchar *s=ArcName; *s!=0; s++)
    if (*s=='\\' || *s=='/')
      ArcName=s+1;

  if (*ArcName==0)
    return (wchar *)ArcName;

  // Pointing to last name character.
  const wchar *ChPtr=ArcName+wcslen(ArcName)-1;

  // Skipping the archive extension.
  while (!IsDigit(*ChPtr) && ChPtr>ArcName)
    ChPtr--;

  // Skipping the numeric part of name.
  const wchar *NumPtr=ChPtr;
  while (IsDigit(*NumPtr) && NumPtr>ArcName)
    NumPtr--;

  // Searching for first numeric part in names like name.part##of##.rar.
  // Stop search on the first dot.
  while (NumPtr>ArcName && *NumPtr!='.')
  {
    if (IsDigit(*NumPtr))
    {
      // Validate the first numeric part only if it has a dot somewhere
      // before it.
      const wchar *Dot=wcschr(ArcName,'.');
      if (Dot!=NULL && Dot<NumPtr)
        ChPtr=NumPtr;
      break;
    }
    NumPtr--;
  }
  return (wchar *)ChPtr;
}
#include "File.h"


#define RFS_GUID_STRING "{9FFE11D2-29F2-463f-AD5F-C04A5EE2E58D}"


//{1AC0BEBD-4D2B-45AD-BCEB-F2C41C5E3788}
DEFINE_GUID(MEDIASUBTYPE_Matroska,
0x1AC0BEBD, 0x4D2B, 0x45AD, 0xBC, 0xEB, 0xF2, 0xC4, 0x1C, 0x5E, 0x37, 0x88);

#ifdef STANDALONE_FILTER

// Setup information
const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
	&MEDIATYPE_Stream,		// Major type
	&MEDIASUBTYPE_NULL		// Minor type
};

const AMOVIESETUP_PIN sudpPin =
{
	L"Output",		// Pin name
	FALSE,			// Is it rendered
	TRUE,			// Is it an output
	FALSE,			// Zero instances allowed
	FALSE,			// Multiple instances allowed
	&CLSID_NULL,	// Connects to filter
	NULL,			// Connects to pin
	1,				// Number of types
	&sudPinTypes	// Pin types
};

const AMOVIESETUP_FILTER sudRARFileSource =
{
	&__uuidof(CRARFileSource),	// Filter CLSID
	RARFileSourceName,		// Filter name
	MERIT_UNLIKELY,			// Filter merit
	1,						// Number of pins
	&sudpPin,				// Pin information
	CLSID_LegacyAmFilterCategory
};

// List of class IDs and creator functions for the class factory.

CFactoryTemplate g_Templates [] =
{
	{
		RARFileSourceName,
		&__uuidof(CRARFileSource),
		CRARFileSource::CreateInstance,
		NULL,
		&sudRARFileSource
	}
};

int g_cTemplates = sizeof (g_Templates) / sizeof (g_Templates [0]);

#endif

/* static */
const std::vector<std::wstring> CRARFileSource::s_file_types
({
	L"avi",
    L"mkv",
    L"mka",
    L"mp4",
    L"webm",
    L"mp3",
    L"m4a",
    L"flac",
    L"ogm",
    L"ogg",
    L"opus",
    L"mpg",
    L"mpeg",
	L"vob",
	L"mov",
	L"wav",
	L"mpa",
	L"mpv",
	L"dat",
    L"m2ts",
    L"mk3d",
    L"flv",
    L"divx",
});

#ifdef STANDALONE_FILTER

extern "C" BOOL WINAPI DllEntryPoint (HINSTANCE, ULONG, LPVOID);

BOOL WINAPI DllMain (HINSTANCE hDllHandle, DWORD dwReason, LPVOID lpReserved)
{
	return DllEntryPoint (hDllHandle, dwReason, lpReserved);
}

STDAPI DllRegisterServer ()
{
	HKEY key;
	LONG ret;
	wchar_t key_name [] = L"Media Type\\Extensions\\.rar";
	wchar_t byte_key_name [] = L"Media Type\\{E436EB83-524F-11CE-9F53-0020AF0BA770}\\{7F1CD2B6-DFC6-4F4C-982B-0472673920AD}";
	char bytes [] = "0,4,,52617221";
	
	ret = RegCreateKey (HKEY_CLASSES_ROOT, key_name, &key);

	if (ret != ERROR_SUCCESS)
	{
		ret = RegOpenKey (HKEY_CLASSES_ROOT, key_name, &key);

		if (ret != ERROR_SUCCESS)
			return ret;
	}
	
	ret = RegSetValueExA (key, "Source Filter", 0, REG_SZ, (BYTE *) RFS_GUID_STRING, (DWORD) strlen (RFS_GUID_STRING) + 1);

	if (ret != ERROR_SUCCESS)
	{
		RegCloseKey (key);
		return ret;
	}

	RegCloseKey (key);

	ret = RegCreateKey (HKEY_CLASSES_ROOT, byte_key_name, &key);

	if (ret != ERROR_SUCCESS)
	{
		ret = RegOpenKey (HKEY_CLASSES_ROOT, byte_key_name, &key);

		if (ret != ERROR_SUCCESS)
			return ret;
	}

	ret = RegSetValueExA (key, "0", 0, REG_SZ, (BYTE *) bytes, (DWORD) strlen (bytes));
	ret = RegSetValueExA (key, "Source Filter", 0, REG_SZ, (BYTE *) RFS_GUID_STRING, (DWORD) strlen (RFS_GUID_STRING) + 1);

	if (ret != ERROR_SUCCESS)
	{
		RegCloseKey (key);
		return ret;
	}

	RegCloseKey (key);

	return AMovieDllRegisterServer2 (TRUE);
}

STDAPI DllUnregisterServer ()
{
	HKEY key;
	LONG ret;
	char value [40];
	DWORD len = sizeof (value);
	wchar_t key_name [] = L"Media Type\\Extensions\\.rar";
	wchar_t byte_key_name [] = L"Media Type\\{E436EB83-524F-11CE-9F53-0020AF0BA770}\\{7F1CD2B6-DFC6-4F4C-982B-0472673920AD}";

	ret = RegOpenKey (HKEY_CLASSES_ROOT, key_name, &key);
	if (ret == ERROR_SUCCESS)
	{
		ret = RegQueryValueExA (key, "Source Filter", NULL, NULL, (BYTE *) value, &len);
		RegCloseKey (key);

		if (ret == ERROR_SUCCESS)
		{
			if (!_stricmp (value, RFS_GUID_STRING))
				RegDeleteKey (HKEY_CLASSES_ROOT, key_name);
		}
	}

	ret = RegOpenKey (HKEY_CLASSES_ROOT, byte_key_name, &key);
	if (ret == ERROR_SUCCESS)
	{
		ret = RegQueryValueExA (key, "Source Filter", NULL, NULL, (BYTE *) value, &len);
		RegCloseKey (key);

		if (ret == ERROR_SUCCESS)
		{
			if (!_stricmp (value, RFS_GUID_STRING))
				RegDeleteKey (HKEY_CLASSES_ROOT, byte_key_name);
		}
	}

	return AMovieDllRegisterServer2 (FALSE);
}

#endif

CRARFileSource::CRARFileSource (LPUNKNOWN punk, HRESULT *phr) :
	CBaseFilter (RARFileSourceName, punk, &m_lock, __uuidof(CRARFileSource)),
	m_pin (this, &m_lock, phr),
	m_file_name (NULL),
	m_file (NULL)
{
	if (phr)
		*phr = S_OK;
} 

CRARFileSource::~CRARFileSource ()
{
	delete m_file_name;
	delete m_file;
}

CUnknown *CRARFileSource::CreateInstance (LPUNKNOWN punk, HRESULT *phr) // OK
{
	CRARFileSource *pNewObject = new CRARFileSource (punk, phr);

	if (pNewObject == NULL)
		*phr = E_OUTOFMEMORY;

	DbgSetModuleLevel (LOG_TRACE, 2);
	DbgLog((LOG_TRACE, 2, L"CRARFileSource::CreateInstance () succeded."));

	return pNewObject;
}

STDMETHODIMP CRARFileSource::NonDelegatingQueryInterface (REFIID riid, void **ppv)
{
	if (riid == IID_IFileSourceFilter)
		return GetInterface ((IFileSourceFilter *) this, ppv);
	else
		return CBaseFilter::NonDelegatingQueryInterface (riid, ppv);
}

/* static */
void CRARFileSource::UpdateArchiveName (wchar_t *ext, size_t len, int volume, bool new_numbering)
{
	if (new_numbering)
	{
		StringCchPrintf (ext, len + 1, L"%0*d", len, volume + 1);
		ext [len] = L'.';
	}
	else
	{
		ASSERT (len == 2);

		if (volume == 0)
		{
			ext [0] = L'a';
			ext [1] = L'r';
		}
		else
			StringCchPrintf (ext - 1, len + 2, L"%c%02d", 114 + (volume - 1) / 100, (volume - 1) % 100);
	}
}

#define HEADER_SKIP_FILE \
	delete [] rh.fh.filename; \
	SetFilePointerEx (hFile, rh.bytesRemaining, NULL, FILE_CURRENT); \
	continue;

HRESULT CRARFileSource::ScanArchive(wchar_t* archive_name, CRFSList<CRFSFile>* file_list, int* files_found, int* ok_files_found) {
    Archive rarArchive;
    *ok_files_found = 0;
    *files_found = 0;
    bool redirectedToFirstVolume = false;

    do {
        if (!rarArchive.Open(archive_name)) {
            ErrorMsg(GetLastError(), L"Could not open archive.");
            return E_FAIL;
        }

        if (!rarArchive.IsArchive(false)) {
            ErrorMsg(GetLastError(), L"Could not read RAR header.");
            return E_FAIL;
        }

        if (rarArchive.Encrypted) {
            ErrorMsg(0, L"Encrypted RAR volumes are not supported.");
            return RFS_E_ENCRYPTED;
        }

        if (!redirectedToFirstVolume && !rarArchive.FirstVolume) {
            if (rarArchive.NewNumbering && nullptr != wcsstr(archive_name,L".part")) { //verify ".part" exists in case the files have been renamed
                wchar* Num = RfsGetVolNumPart(archive_name); //last digit of vol num

                uint VolNum = 0;
                for (uint K = 1; Num >= archive_name && IsDigit(*Num); K *= 10, Num--) {
                    VolNum += (*Num - '0') * K;
                    *Num = K == 1 ? '1' : '0'; //replacing with 00001 in case not on first volume
                }
                if (VolNum != 1) { //not first volume
                    rarArchive.Close();
                    redirectedToFirstVolume = true;
                }
            } else if (!CmpExt(archive_name, L"rar") && wcslen(archive_name) > 4) { //if it is r00, r01, etc,. make it rar
                wchar* Num = archive_name + wcslen(archive_name) - 1;
                *Num-- = L'r';
                *Num-- = L'a';
                *Num-- = L'r';
                *Num = L'.'; //presumably was a 3 char extension if we are here, but set the '.' anyway
                rarArchive.Close();
                redirectedToFirstVolume = true;
            }
        } else {
            break; //we have redirected once, or we already have the first volume, so exit the loop now
        }
    } while (redirectedToFirstVolume); //only loop if we are redirecting to the first volume

    size_t bytesRead;
    do {
        rarArchive.Seek(rarArchive.NextBlockPos, SEEK_SET); //when switching volumes, we may find ourselves mid-block. works first time as well
        while (bytesRead = rarArchive.SearchBlock(HEAD_FILE)) {
            if (rarArchive.FileHead.Method != 0) {
                DbgLog((LOG_TRACE, 2, L"Compressed files are not supported."));
                (*files_found)++;
                continue;
            }
            if (rarArchive.FileHead.Encrypted != 0) {
                DbgLog((LOG_TRACE, 2, L"Encrypted files are not supported."));
                (*files_found)++;
                continue;
            }

            CRFSFile* file;

            file = new CRFSFile();

            if (!file) {
                ErrorMsg(0, L"Out of memory.");
                return E_OUTOFMEMORY;
            }

            file->media_type.SetType(&MEDIATYPE_Stream);
            file->media_type.SetSubtype(&MEDIASUBTYPE_NULL);
            wchar_t* fname = new wchar_t[rarArchive.FileHead.FileName.size() + 1];
            wcscpy(fname, rarArchive.FileHead.FileName.c_str());
            file->filename = fname;
            file->size = rarArchive.FileHead.UnpSize;
            wchar_t* rfname = new wchar_t[rarArchive.FileName.size() + 1];
            wcscpy(rfname, rarArchive.FileName.c_str());
            file->rarFilename = rfname;
            file->startingBlockPos = rarArchive.CurBlockPos;
            (*files_found)++;

            wchar_t* ext = PathFindExtension(fname);

            if (*ext == L'.') {
                ext++;
                if (std::find(s_file_types.begin(), s_file_types.end(), ext) != s_file_types.end()) {
                    file_list->InsertLast(file);
                    (*ok_files_found)++;
                }
            }
            rarArchive.Seek(rarArchive.NextBlockPos, SEEK_SET); //seek to next block before continuing
        }
    } while (MergeArchive(rarArchive, NULL, false, 'E'));

    return S_OK;
}

/* static */
INT_PTR CALLBACK CRARFileSource::DlgFileList (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int index;

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		int len;
		CRFSList<CRFSFile> *file_list = (CRFSList<CRFSFile> *) lParam;
		CRFSFile *file = file_list->First ();

		while (file)
		{
			index = ListBox_AddString (GetDlgItem (hwndDlg, IDC_FILELIST), file->filename);
			ListBox_SetItemData (GetDlgItem (hwndDlg, IDC_FILELIST), index, file);
			file = file_list->Next (file);
		}
		ListBox_SetCurSel (GetDlgItem (hwndDlg, IDC_FILELIST), 0);
		SetForegroundWindow (hwndDlg);
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
				index = ListBox_GetCurSel (GetDlgItem (hwndDlg, IDC_FILELIST));
				EndDialog (hwndDlg, ListBox_GetItemData (GetDlgItem (hwndDlg, IDC_FILELIST), index));
				return TRUE;

		case IDCANCEL:
				EndDialog (hwndDlg, NULL);
				return TRUE;

		case IDC_FILELIST:
			switch (HIWORD (wParam))
			{
				case LBN_DBLCLK:
					index = ListBox_GetCurSel ((HWND) lParam);
					EndDialog (hwndDlg, ListBox_GetItemData ((HWND) lParam, index));
					return TRUE;
			}
		}
		break;

	case WM_CLOSE:
		EndDialog (hwndDlg, NULL);
		return TRUE;
	}

	return FALSE;
}

void CRARFileSource::SetPreselectedRarFileEntry(std::wstring preselectedRarFileEntry) {
    this->preselectedRarFileEntry = preselectedRarFileEntry;
}
//  IFileSourceFilter methods

STDMETHODIMP CRARFileSource::Load (LPCOLESTR lpwszFileName, const AM_MEDIA_TYPE *pmt)
{
    CRFSList <CRFSFile> file_list;
	int num_files, num_ok_files;
	CAutoLock lck (&m_lock);
	HRESULT hr;

	if (!lpwszFileName)
		return E_POINTER;

	if (m_file)
	{
		DbgLog((LOG_TRACE, 2, L"CRARFileSource::Load called with file already loaded."));
		return E_UNEXPECTED;
	}

	int cch = lstrlen (lpwszFileName) + 1;

	if (m_file_name)
		delete m_file_name;

	m_file_name = new WCHAR [cch];
	if (!m_file_name)
	{
		ErrorMsg (0, L"Out of memory.");
		return E_OUTOFMEMORY;
	}

    wcscpy(m_file_name, lpwszFileName);

    try {
        hr = ScanArchive((wchar_t*)lpwszFileName, &file_list, &num_files, &num_ok_files);
    } catch (...) {
        DbgLog((LOG_TRACE, 2, L"CRARFileSource::Exception was thrown while scanning archive. Could be encrypted file."));
        return E_UNEXPECTED;
    }

	DbgLog((LOG_TRACE, 2, L"Found %d files out of which %d are media files.", num_files, num_ok_files));

	if (!num_ok_files)
	{
		file_list.Clear ();
		return hr; // TODO: Figure out a better error code.
	}

	if (num_ok_files == 1)
	{
		m_file = file_list.First ();
	}
	else
	{
        if (preselectedRarFileEntry.length() > 0) {
            CRFSFile* file = file_list.First();

            while (file) {
                if (preselectedRarFileEntry == file->filename) {
                    m_file = file;
                    break;
                }
                file = file_list.Next(file);
            }


        } else {
#ifdef STANDALONE_FILTER
            m_file = (CRFSFile*)DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILELIST), 0, DlgFileList, (LPARAM)&file_list);
#else
            m_file = (CRFSFile*)DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_FILELIST), 0, DlgFileList, (LPARAM)&file_list);
#endif
        }
		if (!m_file)
		{
			file_list.Clear ();
			return RFS_E_ABORT;
		}
	}

	if (pmt)
		m_file->media_type = *pmt;

	m_file->Unlink ();
	m_pin.SetFile (m_file);

	file_list.Clear ();

	return S_OK;
}

// Behaves like IPersistFile::Load
STDMETHODIMP CRARFileSource::GetCurFile (LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
	if (!ppszFileName)
		return E_POINTER;

	if (m_file)
	{
        DWORD n = sizeof(wchar_t) * (wcslen(m_file->filename) + 1);
        *ppszFileName = (LPOLESTR) CoTaskMemAlloc(n);

		if (*ppszFileName != NULL)
			wcscpy (*ppszFileName, m_file->filename);
		else
			return E_OUTOFMEMORY;
	}
	else
		*ppszFileName = NULL;

	if (pmt != NULL)
		CopyMediaType (pmt, &m_file->media_type);

	return NOERROR;
}

STDMETHODIMP CRARFileSource::QueryFilterInfo (FILTER_INFO* pInfo)
{
	CheckPointer(pInfo, E_POINTER);
	ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));
	wcscpy_s(pInfo->achName, RARFileSourceName);
	pInfo->pGraph = m_pGraph;
	if (m_pGraph) {
		m_pGraph->AddRef();
	}

	return S_OK;
}
