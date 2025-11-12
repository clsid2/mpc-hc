/*
* (C) 2002-2021 see Authors.txt
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
#include "MainFrm.h"
#include "MediaTransControls.h"
#include "shcore.h"
#include "PathUtils.h"
#include "SysVersion.h"

#pragma comment(lib, "RuntimeObject.lib")
#pragma comment(lib, "ShCore.lib")

/// #include <SystemMediaTransportControlsInterop.h>
#include <wrl.h>
#include <string>

/// The file content of SystemMediaTransportControlsInterop.h
#ifndef ISystemMediaTransportControlsInterop
EXTERN_C const IID IID_ISystemMediaTransportControlsInterop;
MIDL_INTERFACE("ddb0472d-c911-4a1f-86d9-dc3d71a95f5a")
ISystemMediaTransportControlsInterop : public IInspectable{
 public:
  virtual HRESULT STDMETHODCALLTYPE GetForWindow(
      /* [in] */ __RPC__in HWND appWindow,
      /* [in] */ __RPC__in REFIID riid,
      /* [iid_is][retval][out] */
      __RPC__deref_out_opt void** mediaTransportControl) = 0;
};
#  endif

using namespace Windows::Foundation;
using namespace ABI::Windows::Media;
using namespace ABI::Windows::Storage;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

bool MediaTransControls::Init(CMainFrame* main) {
    /// Windows 8.1 or later is required
    if (!SysVersion::IsWin81orLater()) {
        return false;
    }

    CComPtr<ISystemMediaTransportControlsInterop> op;
    HRESULT ret;
    if ((ret = GetActivationFactory(HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls).Get(), &op)) != S_OK) {
        TRACE(_T("MediaTransControls: GetActivationFactory error %ld\n"), ret);
        return false;
    }
    if ((ret = op->GetForWindow(main->GetSafeHwnd(), IID_PPV_ARGS(&smtc_controls))) != S_OK) {
        smtc_controls = nullptr;
        TRACE(_T("MediaTransControls: GetForWindow error %ld\n"), ret);
        return false;
    }
    ret = smtc_controls->get_DisplayUpdater(&smtc_updater);
    if (ret != S_OK) {
        smtc_controls = nullptr;
        TRACE(_T("MediaTransControls: get_DisplayUpdater error %ld\n"), ret);
        return false;
    }
    ret = smtc_controls->put_IsEnabled(false);
    if (ret != S_OK) {
        smtc_controls = nullptr;
        smtc_updater = nullptr;
        TRACE(_T("MediaTransControls: put_IsEnabled error %ld\n"), ret);
        return false;
    }
    ret = smtc_controls->put_PlaybackStatus(MediaPlaybackStatus::MediaPlaybackStatus_Closed);
    if (ret != S_OK) {
        smtc_controls = nullptr;
        smtc_updater = nullptr;
        TRACE(_T("MediaTransControls: put_PlaybackStatus error %ld\n"), ret);
        return false;
    }

    auto callbackButtonPressed = Callback<ABI::Windows::Foundation::ITypedEventHandler<SystemMediaTransportControls*, SystemMediaTransportControlsButtonPressedEventArgs*>>(
        [this](ISystemMediaTransportControls*, ISystemMediaTransportControlsButtonPressedEventArgs* pArgs) {
            HRESULT ret;
            SystemMediaTransportControlsButton button;
            if ((ret = pArgs->get_Button(&button)) != S_OK) {
                return ret;
            }
            OnButtonPressed(button);
            return S_OK;
        });
    ret = smtc_controls->add_ButtonPressed(callbackButtonPressed.Get(), &m_EventRegistrationToken);
    if (ret != S_OK) {
        smtc_controls = nullptr;
        smtc_updater = nullptr;
        TRACE(_T("MediaTransControls: add_ButtonPressed error %ld\n"), ret);
        return false;
    }

    smtc_controls->put_IsPlayEnabled(true);
    smtc_controls->put_IsPauseEnabled(true);
    smtc_controls->put_IsStopEnabled(true);
    smtc_controls->put_IsPreviousEnabled(true);
    smtc_controls->put_IsNextEnabled(true);

    m_pMainFrame = main;

    return true;
}

void MediaTransControls::close() {
    if (smtc_controls != nullptr) {
        TRACE(_T("MediaTransControls::close()"));
        smtc_controls->put_PlaybackStatus(MediaPlaybackStatus::MediaPlaybackStatus_Closed);
        smtc_controls->put_IsEnabled(false);
        if (smtc_updater) smtc_updater->ClearAll();
    }
}

template <typename T>
bool AwaitForIAsyncOperation(CComPtr<ABI::Windows::Foundation::IAsyncOperation<T>> io) {
    CComPtr<ABI::Windows::Foundation::IAsyncInfo> info;
    ABI::Windows::Foundation::AsyncStatus status;
    HRESULT ret;
    info = io;
    while (true) {
        if ((ret = info->get_Status(&status)) != S_OK) {
            return false;
        }
        if (status != ABI::Windows::Foundation::AsyncStatus::Started) {
            if (status == ABI::Windows::Foundation::AsyncStatus::Completed) return true;
            return false;
        }
        Sleep(10);
    }
}

void MediaTransControls::loadThumbnail(CString fn) {
    if (fn.IsEmpty() || !smtc_updater) {
        return;
    }
    if (PathUtils::IsURL(fn)) {
        return loadThumbnailFromUrl(fn);
    }

    HRESULT ret;
    CComPtr<IStorageFileStatics> sfs;
    if ((ret = GetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_StorageFile).Get(), &sfs)) != S_OK) {
        return;
    }
    CComPtr<ABI::Windows::Foundation::IAsyncOperation<StorageFile*>> af;
    if ((ret = sfs->GetFileFromPathAsync(HStringReference(fn.GetString()).Get(), &af)) != S_OK) {
        return;
    }
    /// Present file
    CComPtr<IStorageFile> f;
    if (!AwaitForIAsyncOperation(af)) {
        return;
    }
    if ((ret = af->GetResults(&f)) != S_OK) {
        return;
    }
    CComPtr<Streams::IRandomAccessStreamReferenceStatics> rasrs;
    if ((ret = GetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(), &rasrs)) != S_OK) {
        return;
    }
    CComPtr<Streams::IRandomAccessStreamReference> stream;
    if ((ret = rasrs->CreateFromFile(f, &stream)) != S_OK) {
        return;
    }
    smtc_updater->put_Thumbnail(stream);
}

void MediaTransControls::loadThumbnail(BYTE* content, size_t size) {
    if (!content || !size || !smtc_updater) return;

    ComPtr<Streams::IRandomAccessStream> s;
    HRESULT ret;
    if ((ret = ActivateInstance(HStringReference(RuntimeClass_Windows_Storage_Streams_InMemoryRandomAccessStream).Get(), s.GetAddressOf())) != S_OK) {
        return;
    }
    ComPtr<IStream> writer;
    CreateStreamOverRandomAccessStream(s.Get(), IID_PPV_ARGS(writer.GetAddressOf()));
    writer->Write(content, (ULONG)size, nullptr);
    CComPtr<Streams::IRandomAccessStreamReferenceStatics> rasrs;
    if ((ret = GetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(), &rasrs)) != S_OK) {
        return;
    }
    CComPtr<Streams::IRandomAccessStreamReference> stream;
    if ((ret = rasrs->CreateFromStream(s.Get(), &stream)) != S_OK) {
        return;
    }
    smtc_updater->put_Thumbnail(stream);
}

void MediaTransControls::loadThumbnailFromUrl(CString url) {
    if (url.IsEmpty() || !smtc_updater) return;

    HRESULT ret;
    CComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> u;
    if ((ret = Windows::Foundation::GetActivationFactory(HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(), &u)) != S_OK) {
        return;
    }
    CComPtr<ABI::Windows::Foundation::IUriRuntimeClass> uri;
    if ((ret = u->CreateUri(HStringReference(url).Get(), &uri)) != S_OK) {
        return;
    }
    CComPtr<Streams::IRandomAccessStreamReferenceStatics> rasrs;
    if ((ret = GetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(), &rasrs)) != S_OK) {
        return;
    }
    CComPtr<Streams::IRandomAccessStreamReference> stream;
    if ((ret = rasrs->CreateFromUri(uri, &stream)) != S_OK) {
        return;
    }
    ret = smtc_updater->put_Thumbnail(stream);
    ASSERT(ret == S_OK);
}

void MediaTransControls::OnButtonPressed(SystemMediaTransportControlsButton button) {
    if (!m_pMainFrame) return;

    switch (button) {
        case SystemMediaTransportControlsButton_Play:
            m_pMainFrame->PostMessageW(WM_COMMAND, ID_PLAY_PLAY);
            break;
        case SystemMediaTransportControlsButton_Pause:
            m_pMainFrame->PostMessageW(WM_COMMAND, ID_PLAY_PAUSE);
            break;
        case SystemMediaTransportControlsButton_Stop:
            m_pMainFrame->PostMessageW(WM_COMMAND, ID_PLAY_STOP);
            break;
        case SystemMediaTransportControlsButton_Previous:
            m_pMainFrame->PostMessageW(WM_COMMAND, ID_NAVIGATE_SKIPBACK);
            break;
        case SystemMediaTransportControlsButton_Next:
            m_pMainFrame->PostMessageW(WM_COMMAND, ID_NAVIGATE_SKIPFORWARD);
            break;
    }
}

bool MediaTransControls::IsActive() {
    if (smtc_controls) {
        boolean enabled;
        HRESULT hr;
        if ((hr = smtc_controls->get_IsEnabled(&enabled)) == S_OK) {
            return enabled;
        }
    }
    return false;
}
