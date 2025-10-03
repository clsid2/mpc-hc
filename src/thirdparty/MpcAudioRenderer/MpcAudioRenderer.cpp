/*
 * (C) 2009-2025 see Authors.txt
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
#include <map>
#include <moreuuids.h>
#include "../../DSUtil/DSUtil.h"
#include "../../DSUtil/AudioParser.h"
#include "../../DSUtil/AudioTools.h"
#include "../../DSUtil/SysVersion.h"
#include "../../DSUtil/std_helper.h"
#include "AudioTools/AudioHelper.h"
#include "AudioDevice.h"
#include "MpcAudioRenderer.h"

// option names
#define OPT_REGKEY_AudRend          L"Software\\MPC-HC Filters\\MPC Audio Renderer"
#define OPT_SECTION_AudRend         L"Filters\\MPC Audio Renderer"
#define OPT_DeviceMode              L"DeviceMode"
#define OPT_WasapiMethod            L"WasapiMethod"
#define OPT_BufferDuration          L"BufferDuration"
#define OPT_AudioDeviceId           L"SoundDeviceId"
#define OPT_AudioDeviceName         L"SoundDeviceName"
#define OPT_UseBitExactOutput       L"UseBitExactOutput"
#define OPT_UseSystemLayoutChannels L"UseSystemLayoutChannels"
#define OPT_AltCheckFormat          L"AltCheckFormat"
#define OPT_ReleaseDeviceIdle       L"ReleaseDeviceIdle"
#define OPT_UseCrossFeed            L"CrossFeed"
#define OPT_DummyChannels           L"DummyChannels"
// TODO: rename option values

// set to 1(or more) to enable more detail debug log
#define DBGLOG_LEVEL 1

#ifdef REGISTER_FILTER

const AMOVIESETUP_MEDIATYPE sudPinTypesIn[] = {
	{&MEDIATYPE_Audio, &MEDIASUBTYPE_PCM},
	{&MEDIATYPE_Audio, &MEDIASUBTYPE_IEEE_FLOAT}
};

const AMOVIESETUP_PIN sudpPins[] = {
	{(LPWSTR)L"Input", FALSE, FALSE, FALSE, FALSE, &CLSID_NULL, nullptr, std::size(sudPinTypesIn), sudPinTypesIn},
};

const AMOVIESETUP_FILTER sudFilter[] = {
	{&__uuidof(CMpcAudioRenderer), MpcAudioRendererName, /*0x40000001*/MERIT_UNLIKELY + 1, std::size(sudpPins), sudpPins, CLSID_AudioRendererCategory},
};

CFactoryTemplate g_Templates[] = {
	{sudFilter[0].strName, &__uuidof(CMpcAudioRenderer), CreateInstance<CMpcAudioRenderer>, nullptr, &sudFilter[0]},
	{L"CMpcAudioRendererPropertyPage", &__uuidof(CMpcAudioRendererSettingsWnd), CreateInstance<CInternalPropertyPageTempl<CMpcAudioRendererSettingsWnd> >},
	{L"CMpcAudioRendererPropertyPageStatus", &__uuidof(CMpcAudioRendererStatusWnd), CreateInstance<CInternalPropertyPageTempl<CMpcAudioRendererStatusWnd> >},
};

int g_cTemplates = std::size(g_Templates);

STDAPI DllRegisterServer()
{
	return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
	return AMovieDllRegisterServer2(FALSE);
}

#include "../../filters/Filters.h"

CFilterApp theApp;

#endif

struct channel_layout_t {
	WORD channels;
	DWORD layout;
};

static void DumpWaveFormatEx(const WAVEFORMATEX* pwfx)
{
	bool bFloat = false;
	if (IsWaveFormatExtensible(pwfx)) {
		const auto pwfex = (WAVEFORMATEXTENSIBLE*)pwfx;
		bFloat = !!(pwfex->SubFormat == MEDIASUBTYPE_IEEE_FLOAT);
	} else {
		bFloat = !!(pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
	}

	CString format;
	switch (pwfx->wBitsPerSample) {
		case 8:
			format = L"8 bit PCM";
			break;
		case 16:
			format = L"16 bit PCM";
			break;
		case 24:
			format = L"24 bit PCM";
			break;
		case 32:
			format.Format(L"32 bit %s", bFloat ? L"Float" : L"PCM");
			break;
		case 64:
			if (bFloat) {
				format = L"64 bit Double";
			}
			break;
	}

	TRACE(L"        => %s\n", format.GetString());
	TRACE(L"        => wFormatTag      = 0x%04x\n", pwfx->wFormatTag);
	TRACE(L"        => nChannels       = %d\n", pwfx->nChannels);
	TRACE(L"        => nSamplesPerSec  = %d\n", pwfx->nSamplesPerSec);
	TRACE(L"        => nAvgBytesPerSec = %d\n", pwfx->nAvgBytesPerSec);
	TRACE(L"        => nBlockAlign     = %d\n", pwfx->nBlockAlign);
	TRACE(L"        => wBitsPerSample  = %d\n", pwfx->wBitsPerSample);
	TRACE(L"        => cbSize          = %d\n", pwfx->cbSize);
	if (IsWaveFormatExtensible(pwfx)) {
		const auto pwfex = (WAVEFORMATEXTENSIBLE*)pwfx;
		TRACE(L"        WAVEFORMATEXTENSIBLE:\n");
		TRACE(L"            => wValidBitsPerSample = %d\n", pwfex->Samples.wValidBitsPerSample);
		TRACE(L"            => dwChannelMask       = 0x%x\n", pwfex->dwChannelMask);
		TRACE(L"            => SubFormat           = %s\n", CStringFromGUID(pwfex->SubFormat));
	}
}

#define SamplesToTime(samples, wfex)    (RescaleI64x32(samples, UNITS, wfex->nSamplesPerSec))
#define TimeToSamples(time, wfex)       (RescaleI64x32(time, wfex->nSamplesPerSec, UNITS))

#define GetChannelMask(wfex, nChannels) (IsWaveFormatExtensible(wfex) ? (reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfex))->dwChannelMask : GetDefChannelMask(nChannels))

CMpcAudioRenderer::CMpcAudioRenderer(LPUNKNOWN punk, HRESULT *phr)
	: CBaseRenderer(__uuidof(this), L"CMpcAudioRenderer", punk, phr)
	, m_dRate(1.0)
	, m_pSyncClock(nullptr)
	, m_pWaveFormatExInput(nullptr)
	, m_pWaveFormatExOutput(nullptr)
	, m_DeviceMode(MODE_WASAPI_SHARED)
	, m_DeviceModeCurrent(MODE_WASAPI_SHARED)
	, m_WasapiMethod(WASAPI_METHOD::EVENT)
	, m_pMMDevice(nullptr)
	, m_pAudioClient(nullptr)
	, m_pRenderClient(nullptr)
	, m_pAudioClock(nullptr)
	, m_BufferDuration(50)
	, m_hnsBufferDuration(0)
	, m_nFramesInBuffer(0)
	, m_nMaxWasapiQueueSize(0)
	, m_bIsAudioClientStarted(false)
	, m_lVolume(DSBVOLUME_MAX)
	, m_lBalance(DSBPAN_CENTER)
	, m_dVolumeFactor(1.0)
	, m_dBalanceFactor(1.0)
	, m_dwBalanceMask(0)
	, m_bUpdateBalanceMask(true)
	, m_nThreadId(0)
	, m_hRenderThread(nullptr)
	, m_bThreadPaused(FALSE)
	, m_hDataEvent(nullptr)
	, m_hPauseEvent(nullptr)
	, m_hWaitPauseEvent(nullptr)
	, m_hResumeEvent(nullptr)
	, m_hWaitResumeEvent(nullptr)
	, m_hStopRenderThreadEvent(nullptr)
	, m_bIsBitstream(FALSE)
	, m_BitstreamMode(BITSTREAM_NONE)
	, m_hAvrtLib(nullptr)
	, pfAvSetMmThreadCharacteristicsW(nullptr)
	, pfAvRevertMmThreadCharacteristics(nullptr)
	, m_bUseBitExactOutput(TRUE)
	, m_bUseSystemLayoutChannels(TRUE)
	, m_bAltCheckFormat(FALSE)
	, m_bReleaseDeviceIdle(TRUE)
	, m_filterState(State_Stopped)
	, m_hRendererNeedMoreData(nullptr)
	, m_CurrentPacket(nullptr)
	, m_rtStartTime(0)
	, m_rtNextRenderedSampleTime(0)
	, m_rtLastReceivedSampleTimeEnd(0)
	, m_rtLastQueuedSampleTimeEnd(0)
	, m_rtEstimateSlavingJitter(0)
	, m_rtCurrentRenderedTime(0)
	, m_bUseDefaultDevice(FALSE)
	, m_nSampleOffset(0)
	, m_bUseCrossFeed(FALSE)
	, m_bDummyChannels(FALSE)
	, m_bNeedReinitialize(FALSE)
	, m_bNeedReinitializeFull(FALSE)
	, m_FlushEvent(TRUE)
	, m_bFlushing(FALSE)
	, m_bReal32bitSupport(FALSE)
	, m_bs2b_active(false)
	, m_bDVDPlayback(FALSE)
{
	TRACE(L"CMpcAudioRenderer::CMpcAudioRenderer()\n");

#ifdef REGISTER_FILTER
	CRegKey key;
	WCHAR buff[256];
	ULONG len;

	if (ERROR_SUCCESS == key.Open(HKEY_CURRENT_USER, OPT_REGKEY_AudRend, KEY_READ)) {
		DWORD dw;
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_DeviceMode, dw)) {
			m_DeviceMode = (DEVICE_MODE)dw;
		}
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_WasapiMethod, dw)) {
			m_WasapiMethod = (WASAPI_METHOD)dw;
		}
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_BufferDuration, dw)) {
			m_BufferDuration = dw;
		}
		len = std::size(buff);
		memset(buff, 0, sizeof(buff));
		if (ERROR_SUCCESS == key.QueryStringValue(OPT_AudioDeviceId, buff, &len)) {
			m_DeviceId = CString(buff);
		}
		len = std::size(buff);
		memset(buff, 0, sizeof(buff));
		if (ERROR_SUCCESS == key.QueryStringValue(OPT_AudioDeviceName, buff, &len)) {
			m_DeviceName = CString(buff);
		}
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_UseBitExactOutput, dw)) {
			m_bUseBitExactOutput = !!dw;
		}
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_UseSystemLayoutChannels, dw)) {
			m_bUseSystemLayoutChannels = !!dw;
		}
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_AltCheckFormat, dw)) {
			m_bAltCheckFormat = !!dw;
		}
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_ReleaseDeviceIdle, dw)) {
			m_bReleaseDeviceIdle = !!dw;
		}
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_UseCrossFeed, dw)) {
			m_bUseCrossFeed = !!dw;
		}
		if (ERROR_SUCCESS == key.QueryDWORDValue(OPT_DummyChannels, dw)) {
			m_bDummyChannels = !!dw;
		}
	}
#else
    CWinApp* pApp = AfxGetApp();
    m_DeviceMode = (DEVICE_MODE)pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_DeviceMode, *(int*)&m_DeviceMode);
    m_WasapiMethod = (WASAPI_METHOD)pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_WasapiMethod, *(int*)&m_WasapiMethod);
    m_BufferDuration = pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_BufferDuration, m_BufferDuration);
    m_DeviceId = pApp->GetProfileString(OPT_SECTION_AudRend, OPT_AudioDeviceId, m_DeviceId);
    m_DeviceName = pApp->GetProfileString(OPT_SECTION_AudRend, OPT_AudioDeviceName, m_DeviceName);
    m_bUseBitExactOutput = pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_UseBitExactOutput, m_bUseBitExactOutput);
    m_bUseSystemLayoutChannels = pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_UseSystemLayoutChannels, m_bUseSystemLayoutChannels);
    m_bAltCheckFormat = pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_AltCheckFormat, m_bAltCheckFormat);
    m_bReleaseDeviceIdle = pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_ReleaseDeviceIdle, m_bReleaseDeviceIdle);
    m_bUseCrossFeed = pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_UseCrossFeed, m_bUseCrossFeed);
    m_bDummyChannels = pApp->GetProfileInt(OPT_SECTION_AudRend, OPT_DummyChannels, m_bDummyChannels);

#endif

	if (m_DeviceMode != MODE_WASAPI_EXCLUSIVE) {
		m_DeviceMode = MODE_WASAPI_SHARED;
	}
	m_DeviceModeCurrent = m_DeviceMode;

	if (m_WasapiMethod != WASAPI_METHOD::PUSH) {
		m_WasapiMethod = WASAPI_METHOD::EVENT;
	}

	m_BufferDuration = discard(m_BufferDuration, 50, { 0, 50, 100 });

	if (phr) {
		*phr = E_FAIL;
	}

	m_hAvrtLib = LoadLibraryW(L"avrt.dll");
	if (m_hAvrtLib) {
		pfAvSetMmThreadCharacteristicsW   = (PTR_AvSetMmThreadCharacteristicsW)GetProcAddress(m_hAvrtLib, "AvSetMmThreadCharacteristicsW");
		pfAvRevertMmThreadCharacteristics = (PTR_AvRevertMmThreadCharacteristics)GetProcAddress(m_hAvrtLib, "AvRevertMmThreadCharacteristics");
	} else {
		return;
	}

	m_hDataEvent             = CreateEventW(nullptr, FALSE, FALSE, nullptr);

	m_hPauseEvent            = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	m_hWaitPauseEvent        = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	m_hResumeEvent           = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	m_hWaitResumeEvent       = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	m_hStopRenderThreadEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

	m_hRendererNeedMoreData  = CreateEventW(nullptr, TRUE, FALSE, nullptr);

	HRESULT hr = S_OK;
	m_pInputPin = new(std::nothrow) CMpcAudioRendererInputPin(this, &hr);
	if (!m_pInputPin) {
		hr = E_OUTOFMEMORY;
	}
	if (FAILED(hr)) {
		if (phr) {
			*phr = hr;
		}
		return;
	}

	// CBaseRenderer is using a lazy initialization for the CRendererPosPassThru - we need it always
	m_pPosition = new(std::nothrow) CRendererPosPassThru(L"CRendererPosPassThru", GetOwner(), &hr, m_pInputPin);
	if (m_pPosition == nullptr) {
		hr = E_OUTOFMEMORY;
	} else if (FAILED(hr)) {
		delete m_pPosition;
		m_pPosition = nullptr;
		hr = E_NOINTERFACE;
	}
	if (FAILED(hr)) {
		if (phr) {
			*phr = hr;
		}
		return;
	}

	m_pSyncClock = DNew CAudioSyncClock(GetOwner(), &hr);
	if (phr) {
		*phr = hr;
	}

	if (SUCCEEDED(hr)) {
		HRESULT hr = m_pMMDeviceEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator));
		if (FAILED(hr)) {
			TRACE(L"CMpcAudioRenderer::CMpcAudioRenderer() - IMMDeviceEnumerator::CoCreateInstance() failed: (0x%08x)\n", hr);
			if (phr) {
				*phr = hr;
			}
		}

		if (SUCCEEDED(hr)) {
			m_pMMDeviceEnumerator->RegisterEndpointNotificationCallback(this);
		}
	}
}

CMpcAudioRenderer::~CMpcAudioRenderer()
{
	TRACE(L"CMpcAudioRenderer::~CMpcAudioRenderer()\n");

	m_pMMDeviceEnumerator->UnregisterEndpointNotificationCallback(this);

	EndReleaseTimer();

	m_pSyncClock->UnSlave();
	SAFE_DELETE(m_pSyncClock);

	StopRendererThread();
	if (m_bIsAudioClientStarted && m_pAudioClient) {
		m_pAudioClient->Stop();
	}

	WasapiFlush();

	SAFE_CLOSE_HANDLE(m_hStopRenderThreadEvent);
	SAFE_CLOSE_HANDLE(m_hDataEvent);
	SAFE_CLOSE_HANDLE(m_hPauseEvent);
	SAFE_CLOSE_HANDLE(m_hWaitPauseEvent);
	SAFE_CLOSE_HANDLE(m_hResumeEvent);
	SAFE_CLOSE_HANDLE(m_hWaitResumeEvent);

	SAFE_CLOSE_HANDLE(m_hRendererNeedMoreData);

	SAFE_RELEASE(m_pRenderClient);
	SAFE_RELEASE(m_pAudioClock);
	SAFE_RELEASE(m_pAudioClient);
	SAFE_RELEASE(m_pMMDevice);

	SAFE_DELETE_ARRAY(m_pWaveFormatExInput);
	SAFE_DELETE_ARRAY(m_pWaveFormatExOutput);

	if (m_hAvrtLib) {
		FreeLibrary(m_hAvrtLib);
	}
}

HRESULT CMpcAudioRenderer::CheckInputType(const CMediaType *pmt)
{
	return CheckMediaType(pmt);
}

HRESULT	CMpcAudioRenderer::CheckMediaType(const CMediaType *pmt)
{
	CAutoLock cAutoLock(&m_csCheck);

	CheckPointer(pmt, E_POINTER);
	CheckPointer(pmt->pbFormat, VFW_E_TYPE_NOT_ACCEPTED);

	if ((pmt->majortype != MEDIATYPE_Audio) || (pmt->formattype != FORMAT_WaveFormatEx)) {
		TRACE(L"CMpcAudioRenderer::CheckMediaType() - Not supported\n");
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	if (pmt->subtype != MEDIASUBTYPE_PCM && pmt->subtype != MEDIASUBTYPE_IEEE_FLOAT) {
		TRACE(L"CMpcAudioRenderer::CheckMediaType() - allow only PCM/IEEE_FLOAT input\n");
		return VFW_E_TYPE_NOT_ACCEPTED;
	}

	HRESULT hr = S_OK;
	if (pmt->subtype == MEDIASUBTYPE_PCM) {
		// Check S/PDIF & HDMI Bitstream
		const WAVEFORMATEX *pWaveFormatEx = (WAVEFORMATEX*)pmt->pbFormat;
		const BOOL bIsBitstreamInput = IsBitstream(pWaveFormatEx);
		if (bIsBitstreamInput) {
			hr = CreateAudioClient();
			if (FAILED(hr)) {
				TRACE(L"CMpcAudioRenderer::CheckMediaType() - audio client initialization error\n");
				return VFW_E_CANNOT_CONNECT;
			}

			hr = m_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, pWaveFormatEx, nullptr);
		}
	}

	return hr;
}

HRESULT CMpcAudioRenderer::Receive(IMediaSample* pSample)
{
	if (m_FlushEvent.Check()) {
		return S_OK;
	}

	if (m_bFlushing && m_bDVDPlayback && m_pAudioClock && !m_pSyncClock->IsSlave()) {
		CRefTime rtTime;
		if (SUCCEEDED(StreamTime(rtTime))) {
			m_rtLastQueuedSampleTimeEnd = m_rtNextRenderedSampleTime = m_rtCurrentRenderedTime = m_rtLastReceivedSampleTimeEnd = rtTime;
		}

		CAutoLock cCheckLock(&m_csCheck);
		CAutoLock cAutoLock(&m_csAudioClock);
		m_pSyncClock->Slave(m_pAudioClock, m_rtStartTime + m_rtCurrentRenderedTime);
	}

	m_bFlushing = FALSE;

	ASSERT(pSample);

	// It may return VFW_E_SAMPLE_REJECTED code to say don't bother
	HRESULT hr = PrepareReceive(pSample);
	ASSERT(m_bInReceive == SUCCEEDED(hr));
	if (FAILED(hr)) {
		if (hr == VFW_E_SAMPLE_REJECTED) {
			hr = S_OK;
		}
		return hr;
	}

	if (m_State == State_Paused) {
		{
			CAutoLock cRendererLock(&m_InterfaceLock);
			if (m_State == State_Stopped) {
				m_bInReceive = FALSE;
				return S_OK;
			}
		}
		Ready();
	}

	Transform(pSample);

	m_bInReceive = FALSE;

	CAutoLock cRendererLock(&m_InterfaceLock);
	if (m_State == State_Stopped) {
		return S_OK;
	}

	CAutoLock cSampleLock(&m_RendererLock);

	ClearPendingSample();
	SendEndOfStream();
	CancelNotification();

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	if (riid == IID_IReferenceClock) {
		return m_pSyncClock->NonDelegatingQueryInterface(riid, ppv);
	} else if (riid == IID_IDispatch) {
		return GetInterface(static_cast<IDispatch*>(this), ppv);
	} else if (riid == IID_IBasicAudio) {
		return GetInterface(static_cast<IBasicAudio*>(this), ppv);
	} else if (riid == IID_IMediaSeeking) {
		return GetInterface(static_cast<IMediaSeeking*>(this), ppv);
	} else if (riid == __uuidof(IMMNotificationClient)) {
		return GetInterface(static_cast<IMMNotificationClient*>(this), ppv);
	} else if (riid == __uuidof(IAMStreamSelect)) {
		return GetInterface(static_cast<IAMStreamSelect*>(this), ppv);
	} else if (riid == __uuidof(ISpecifyPropertyPages)) {
		return GetInterface(static_cast<ISpecifyPropertyPages*>(this), ppv);
	} else if (riid == __uuidof(ISpecifyPropertyPages2)) {
		return GetInterface(static_cast<ISpecifyPropertyPages2*>(this), ppv);
	} else if (riid == __uuidof(IMpcAudioRendererFilter)) {
		return GetInterface(static_cast<IMpcAudioRendererFilter*>(this), ppv);
	}

	return CBaseRenderer::NonDelegatingQueryInterface (riid, ppv);
}

HRESULT CMpcAudioRenderer::SetMediaType(const CMediaType *pmt)
{
	CheckPointer(pmt, E_POINTER);

	WAVEFORMATEX *pwf = (WAVEFORMATEX *)pmt->Format();
	CheckPointer(pwf, VFW_E_TYPE_NOT_ACCEPTED);

	TRACE(L"CMpcAudioRenderer::SetMediaType()\n");

	HRESULT hr = S_OK;

	if (!m_pAudioClient) {
		hr = CreateAudioClient();
		if (FAILED(hr)) {
			TRACE(L"CMpcAudioRenderer::SetMediaType() - audio client initialization error\n");
			return VFW_E_CANNOT_CONNECT;
		}
	}

	if (m_pRenderClient) {
		TRACE(L"CMpcAudioRenderer::SetMediaType() : Render client already initialized. Reinitialization...\n");
	} else {
		TRACE(L"CMpcAudioRenderer::SetMediaType() : Render client initialization\n");
	}

	hr = CheckAudioClient(pwf);

	EXIT_ON_ERROR(hr);

	CopyWaveFormat(pwf, &m_pWaveFormatExInput);

	m_BitstreamMode = BITSTREAM_NONE;

	return CBaseRenderer::SetMediaType(pmt);
}

HRESULT CMpcAudioRenderer::CompleteConnect(IPin *pReceivePin)
{
	TRACE(L"CMpcAudioRenderer::CompleteConnect()\n");

	m_bDVDPlayback = FindFilter(CLSID_DVDNavigator, m_pGraph) != nullptr;

	return CBaseRenderer::CompleteConnect(pReceivePin);
}

DWORD WINAPI CMpcAudioRenderer::RenderThreadEntryPoint(LPVOID lpParameter)
{
	return ((CMpcAudioRenderer*)lpParameter)->RenderThread();
}

HRESULT CMpcAudioRenderer::StopRendererThread()
{
	if (m_hRenderThread) {
		SetEvent(m_hStopRenderThreadEvent);
		if (WaitForSingleObject(m_hRenderThread, 1000) == WAIT_TIMEOUT) {
			TerminateThread(m_hRenderThread, 0xDEAD);
		}

		CloseHandle(m_hRenderThread);
		m_hRenderThread = nullptr;
	}

	return S_OK;
}

HRESULT CMpcAudioRenderer::StartRendererThread()
{
	if (!m_hRenderThread) {
		m_hRenderThread = ::CreateThread(nullptr, 0, RenderThreadEntryPoint, (LPVOID)this, 0, &m_nThreadId);
		return m_hRenderThread ? S_OK : E_FAIL;
	} else if (m_bThreadPaused) {
		SetEvent(m_hResumeEvent);
		WaitForSingleObject(m_hWaitResumeEvent, INFINITE);
	}

	return S_OK;
}

HRESULT CMpcAudioRenderer::PauseRendererThread()
{
	if (m_bThreadPaused) {
		return S_OK;
	}

	// Pause the render thread
	if (m_hRenderThread) {
		SetEvent(m_hPauseEvent);
		WaitForSingleObject(m_hWaitPauseEvent, INFINITE);
		return S_OK;
	}

	return E_FAIL;
}

DWORD CMpcAudioRenderer::RenderThread()
{
	TRACE(L"CMpcAudioRenderer::RenderThread() - start, thread ID = 0x%x\n", m_nThreadId);

	HRESULT hr = S_OK;

	// These are wait handles for the thread stopping, pausing redering and new sample arrival
	HANDLE renderHandles[3] = {
		m_hStopRenderThreadEvent,
		m_hPauseEvent,
		m_hDataEvent
	};

	// These are wait handles for resuming or exiting the thread
	HANDLE resumeHandles[2] = {
		m_hStopRenderThreadEvent,
		m_hResumeEvent
	};

	HANDLE hTaskHandle = nullptr;
	if (pfAvSetMmThreadCharacteristicsW && pfAvRevertMmThreadCharacteristics) {
		// Ask MMCSS to temporarily boost the thread priority
		// to reduce glitches while the low-latency stream plays.
		DWORD taskIndex = 0;

		hTaskHandle = pfAvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
		if (hTaskHandle != nullptr) {
			TRACE(L"CMpcAudioRenderer::EnableMMCSS() - Putting thread 0x%x in higher priority for Wasapi mode (lowest latency)\n", ::GetCurrentThreadId());
		}
	}
	if (hTaskHandle == nullptr) {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	}

	bool bExit = false;
	while (!bExit) {
		CheckBufferStatus();

		DWORD result = -1;
		if (m_WasapiMethod == WASAPI_METHOD::PUSH) {
			HRESULT hr = RenderWasapiBuffer();
			if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
				SetReinitializeAudioDevice(TRUE);
			}

			result = WaitForMultipleObjects(std::size(renderHandles) - 1, renderHandles, FALSE, m_hnsBufferDuration / 10000 / 2);
		} else {
			result = WaitForMultipleObjects(std::size(renderHandles), renderHandles, FALSE, 1000);
		}

		switch (result) {
			case WAIT_TIMEOUT: // timed out after a 1 second wait
				if (m_WasapiMethod == WASAPI_METHOD::EVENT && m_pRenderClient) {
					SetReinitializeAudioDevice(TRUE);
				}
				break;
			case WAIT_OBJECT_0: // exit event
				TRACE(L"CMpcAudioRenderer::RenderThread() - exit events\n");
				bExit = true;
				break;
			case WAIT_OBJECT_0 + 1: // pause event
				{
					TRACE(L"CMpcAudioRenderer::RenderThread() - pause events\n");

					m_bThreadPaused = TRUE;
					ResetEvent(m_hResumeEvent);
					SetEvent(m_hWaitPauseEvent);

					const DWORD resultResume = WaitForMultipleObjects(std::size(resumeHandles), resumeHandles, FALSE, INFINITE);
					switch (resultResume) {
						case WAIT_OBJECT_0: // exit event
							TRACE(L"CMpcAudioRenderer::RenderThread() - exit events\n");
							bExit = true;
							break;
						case WAIT_OBJECT_0 + 1: // resume event
							TRACE(L"CMpcAudioRenderer::RenderThread() - resume events\n");
							m_bThreadPaused = FALSE;
							SetEvent(m_hWaitResumeEvent);
					}
				}
				break;
			case WAIT_OBJECT_0 + 2: // render event
				{
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 1
					TRACE(L"CMpcAudioRenderer::RenderThread() - Data Event, Audio client state = %s", m_bIsAudioClientStarted ? L"Started" : L"Stoped\n");
#endif
					HRESULT hr = RenderWasapiBuffer();
					if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
						SetReinitializeAudioDevice(TRUE);
					}
				}
				break;
			default:
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 1
				TRACE(L"CMpcAudioRenderer::RenderThread() - WaitForMultipleObjects() failed: %u\n", GetLastError());
#endif
				break;
		}
	}

	if (hTaskHandle != nullptr) {
		pfAvRevertMmThreadCharacteristics(hTaskHandle);
	}

	TRACE(L"CMpcAudioRenderer::RenderThread() - close, thread ID = 0x%x\n", m_nThreadId);

	return 0;
}

STDMETHODIMP CMpcAudioRenderer::Run(REFERENCE_TIME rtStart)
{
	TRACE(L"CMpcAudioRenderer::Run()\n");

	if (m_State == State_Running) {
		return NOERROR;
	}

	m_filterState = State_Running;
	m_rtStartTime = rtStart;

	if (m_bEOS) {
		NotifyEvent(EC_COMPLETE, S_OK, 0);
	}

	EndReleaseTimer();

	if (m_hRendererNeedMoreData) {
		SetEvent(m_hRendererNeedMoreData);
	}

	if (m_bFlushing) {
		NewSegment();
	}

	{
		CAutoLock cCheckLock(&m_csCheck);
		if (m_pAudioClock && !m_pSyncClock->IsSlave()) {
			CAutoLock cAutoLock(&m_csAudioClock);
			m_pSyncClock->Slave(m_pAudioClock, m_rtStartTime + m_rtCurrentRenderedTime);
		}
	}

	return CBaseRenderer::Run(rtStart);
}

STDMETHODIMP CMpcAudioRenderer::Stop()
{
	TRACE(L"CMpcAudioRenderer::Stop()\n");

	m_pSyncClock->UnSlave();

	const auto bNeedStartReleaseTimer = m_filterState == State_Running;

	m_filterState = State_Stopped;
	if (m_hRendererNeedMoreData) {
		SetEvent(m_hRendererNeedMoreData);
	}

	if (bNeedStartReleaseTimer) {
		StartReleaseTimer();
	}

	return CBaseRenderer::Stop();
};

STDMETHODIMP CMpcAudioRenderer::Pause()
{
	TRACE(L"CMpcAudioRenderer::Pause()\n");

	m_pSyncClock->UnSlave();

	const auto bNeedStartReleaseTimer = m_filterState == State_Running;

	m_filterState = State_Paused;
	if (m_hRendererNeedMoreData) {
		SetEvent(m_hRendererNeedMoreData);
	}

	if (bNeedStartReleaseTimer) {
		StartReleaseTimer();
	}

	return CBaseRenderer::Pause();
};

// === IBasicAudio
STDMETHODIMP CMpcAudioRenderer::put_Volume(long lVolume)
{
	if (lVolume <= DSBVOLUME_MIN) {
		m_lVolume = DSBVOLUME_MIN;
		m_dVolumeFactor = 0.0;
	}
	else if (lVolume >= DSBVOLUME_MAX) {
		m_lVolume = DSBVOLUME_MAX;
		m_dVolumeFactor = 1.0;
	}
	else {
		m_lVolume = lVolume;
		m_dVolumeFactor = pow(10.0, m_lVolume / 2000.0);
	}

	if (m_lBalance > DSBPAN_LEFT && m_lBalance < DSBPAN_RIGHT) {
		m_dBalanceFactor = m_dVolumeFactor * pow(10.0, -labs(m_lBalance) / 2000.0);
	}

	m_bUpdateBalanceMask = true;

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::get_Volume(long *plVolume)
{
	*plVolume = m_lVolume;

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::put_Balance(long lBalance)
{
	if (lBalance <= DSBPAN_LEFT) {
		m_lBalance = DSBPAN_LEFT;
		m_dBalanceFactor = 0.0;
	}
	else if (lBalance >= DSBPAN_RIGHT) {
		m_lBalance = DSBPAN_RIGHT;
		m_dBalanceFactor = 0.0;
	}
	else {
		m_lBalance = lBalance;
		m_dBalanceFactor = m_dVolumeFactor * pow(10.0, -labs(m_lBalance) / 2000.0);
	}

	m_bUpdateBalanceMask = true;

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::get_Balance(long *plBalance)
{
	*plBalance = m_lBalance;

	return S_OK;
}

// === IMediaSeeking
STDMETHODIMP CMpcAudioRenderer::IsFormatSupported(const GUID* pFormat)
{
	return m_pPosition->IsFormatSupported(pFormat);
}

STDMETHODIMP CMpcAudioRenderer::QueryPreferredFormat(GUID* pFormat)
{
	return m_pPosition->QueryPreferredFormat(pFormat);
}

STDMETHODIMP CMpcAudioRenderer::SetTimeFormat(const GUID* pFormat)
{
	return m_pPosition->SetTimeFormat(pFormat);
}

STDMETHODIMP CMpcAudioRenderer::IsUsingTimeFormat(const GUID* pFormat)
{
	return m_pPosition->IsUsingTimeFormat(pFormat);
}

STDMETHODIMP CMpcAudioRenderer::GetTimeFormat(GUID* pFormat)
{
	return m_pPosition->GetTimeFormat(pFormat);
}

STDMETHODIMP CMpcAudioRenderer::GetDuration(LONGLONG* pDuration)
{
	return m_pPosition->GetDuration(pDuration);
}

STDMETHODIMP CMpcAudioRenderer::GetStopPosition(LONGLONG* pStop)
{
	return m_pPosition->GetStopPosition(pStop);
}

STDMETHODIMP CMpcAudioRenderer::GetCurrentPosition(LONGLONG* pCurrent)
{
	return m_pPosition->GetCurrentPosition(pCurrent);
}

STDMETHODIMP CMpcAudioRenderer::GetCapabilities(DWORD* pCapabilities)
{
	return m_pPosition->GetCapabilities(pCapabilities);
}

STDMETHODIMP CMpcAudioRenderer::CheckCapabilities(DWORD* pCapabilities)
{
	return m_pPosition->CheckCapabilities(pCapabilities);
}

STDMETHODIMP CMpcAudioRenderer::ConvertTimeFormat(LONGLONG* pTarget, const GUID* pTargetFormat, LONGLONG Source, const GUID* pSourceFormat)
{
	return m_pPosition->ConvertTimeFormat(pTarget, pTargetFormat, Source, pSourceFormat);
}

STDMETHODIMP CMpcAudioRenderer::SetPositions(LONGLONG* pCurrent, DWORD CurrentFlags, LONGLONG* pStop, DWORD StopFlags)
{
	return m_pPosition->SetPositions(pCurrent, CurrentFlags, pStop, StopFlags);
}

STDMETHODIMP CMpcAudioRenderer::GetPositions(LONGLONG* pCurrent, LONGLONG* pStop)
{
	return m_pPosition->GetPositions(pCurrent, pStop);
}

STDMETHODIMP CMpcAudioRenderer::GetAvailable(LONGLONG* pEarliest, LONGLONG* pLatest)
{
	return m_pPosition->GetAvailable(pEarliest, pLatest);
}

STDMETHODIMP CMpcAudioRenderer::SetRate(double dRate)
{
	CAutoLock cInterfaceLock(&m_InterfaceLock);

	if (dRate < 0.25 || dRate > 16.0 || m_bIsBitstream) {
		return VFW_E_UNSUPPORTED_AUDIO;
	}

	m_dRate = dRate;
	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::GetRate(double* pdRate)
{
	return m_pPosition->GetRate(pdRate);
}

STDMETHODIMP CMpcAudioRenderer::GetPreroll(LONGLONG* pPreroll)
{
	return m_pPosition->GetPreroll(pPreroll);
}

// === IMMNotificationClient
STDMETHODIMP CMpcAudioRenderer::OnDeviceStateChanged(__in LPCWSTR pwstrDeviceId, __in DWORD dwNewState)
{
	CheckPointer(m_hRenderThread, S_OK);

	if (m_strCurrentDeviceId == pwstrDeviceId) {
		SetReinitializeAudioDevice(TRUE);
	}

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::OnDefaultDeviceChanged(__in EDataFlow flow, __in ERole role, __in LPCWSTR pwstrDefaultDeviceId)
{
	CheckPointer(m_hRenderThread, S_OK);

	if (m_bUseDefaultDevice
			&& flow == eRender
			&& role == eConsole) {
		SetReinitializeAudioDevice(TRUE);
	}

	return S_OK;
}

// === IAMStreamSelect
STDMETHODIMP CMpcAudioRenderer::Count(DWORD* pcStreams)
{
	CheckPointer(pcStreams, E_POINTER);

	return AudioDevices::GetActiveAudioDevices(m_pMMDeviceEnumerator, nullptr, (UINT*)pcStreams, FALSE);
}

STDMETHODIMP CMpcAudioRenderer::Info(long lIndex, AM_MEDIA_TYPE** ppmt, DWORD* pdwFlags, LCID* plcid, DWORD* pdwGroup, WCHAR** ppszName, IUnknown** ppObject, IUnknown** ppUnk)
{
	AudioDevices::deviceList_t deviceList;
	if (FAILED(AudioDevices::GetActiveAudioDevices(m_pMMDeviceEnumerator, &deviceList, nullptr, FALSE))) {
		return E_FAIL;
	}

	if (lIndex >= (long)deviceList.size()) {
		return S_FALSE;
	}

	const auto& [deviceName, deviceId] = deviceList[lIndex];

	if (ppmt) {
		*ppmt = nullptr;
	}
	if (plcid) {
		*plcid = 0;
	}
	if (pdwGroup) {
		*pdwGroup = 0xFF;
	}
	if (ppObject) {
		*ppObject = nullptr;
	}
	if (ppUnk) {
		*ppUnk = nullptr;
	}

	if (pdwFlags) {
		if (deviceId == GetCurrentDeviceId()) {
			*pdwFlags = AMSTREAMSELECTINFO_ENABLED;
		} else {
			*pdwFlags = 0;
		}
	}

	if (ppszName) {
		*ppszName = (WCHAR*)CoTaskMemAlloc((deviceName.GetLength() + 1) * sizeof(WCHAR));
		if (*ppszName) {
			wcscpy_s(*ppszName, deviceName.GetLength() + 1, deviceName);
		}
	}

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::Enable(long lIndex, DWORD dwFlags)
{
	if (dwFlags != AMSTREAMSELECTENABLE_ENABLE) {
		return E_NOTIMPL;
	}

	AudioDevices::deviceList_t deviceList;
	if (FAILED(AudioDevices::GetActiveAudioDevices(m_pMMDeviceEnumerator, &deviceList, nullptr, FALSE))) {
		return E_FAIL;
	}

	if (lIndex >= (long)deviceList.size()) {
		return S_FALSE;
	}

	return SetDeviceId(deviceList[lIndex].deviceId, deviceList[lIndex].deviceName);
}

// === ISpecifyPropertyPages2
STDMETHODIMP CMpcAudioRenderer::GetPages(CAUUID* pPages)
{
	CheckPointer(pPages, E_POINTER);

	BOOL bShowStatusPage = m_pInputPin && m_pInputPin->IsConnected();

	pPages->cElems = bShowStatusPage ? 2 : 1;
	pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
	CheckPointer(pPages->pElems, E_OUTOFMEMORY);

	pPages->pElems[0] = __uuidof(CMpcAudioRendererSettingsWnd);
	if (bShowStatusPage) {
		pPages->pElems[1] = __uuidof(CMpcAudioRendererStatusWnd);
	}

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::CreatePage(const GUID& guid, IPropertyPage** ppPage)
{
	CheckPointer(ppPage, E_POINTER);

	if (*ppPage != nullptr) {
		return E_INVALIDARG;
	}

	HRESULT hr;

	if (guid == __uuidof(CMpcAudioRendererSettingsWnd)) {
		(*ppPage = DNew CInternalPropertyPageTempl<CMpcAudioRendererSettingsWnd>(nullptr, &hr))->AddRef();
	} else if (guid == __uuidof(CMpcAudioRendererStatusWnd)) {
		(*ppPage = DNew CInternalPropertyPageTempl<CMpcAudioRendererStatusWnd>(nullptr, &hr))->AddRef();
	}

	return *ppPage ? S_OK : E_FAIL;
}

// === IMpcAudioRendererFilter
STDMETHODIMP CMpcAudioRenderer::Apply()
{
#ifdef REGISTER_FILTER
	CRegKey key;
	if (ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, OPT_REGKEY_AudRend)) {
		key.SetDWORDValue(OPT_DeviceMode, (DWORD)m_DeviceMode);
		key.SetDWORDValue(OPT_WasapiMethod, (DWORD)m_WasapiMethod);
		key.SetDWORDValue(OPT_BufferDuration, (DWORD)m_BufferDuration);
		key.SetStringValue(OPT_AudioDeviceId, m_DeviceId);
		key.SetStringValue(OPT_AudioDeviceName, m_DeviceName);
		key.SetDWORDValue(OPT_UseBitExactOutput, m_bUseBitExactOutput);
		key.SetDWORDValue(OPT_UseSystemLayoutChannels, m_bUseSystemLayoutChannels);
		key.SetDWORDValue(OPT_AltCheckFormat, m_bAltCheckFormat);
		key.SetDWORDValue(OPT_ReleaseDeviceIdle, m_bReleaseDeviceIdle);
		key.SetDWORDValue(OPT_UseCrossFeed, m_bUseCrossFeed);
		key.SetDWORDValue(OPT_DummyChannels, m_bDummyChannels);
	}
#else
    CWinApp* pApp = AfxGetApp();
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_DeviceMode, (int)m_DeviceMode);
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_WasapiMethod, (int)m_WasapiMethod);
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_BufferDuration, m_BufferDuration);
    pApp->WriteProfileString(OPT_SECTION_AudRend, OPT_AudioDeviceId, m_DeviceId);
    pApp->WriteProfileString(OPT_SECTION_AudRend, OPT_AudioDeviceName, m_DeviceName);
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_UseBitExactOutput, m_bUseBitExactOutput);
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_UseSystemLayoutChannels, m_bUseSystemLayoutChannels);
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_AltCheckFormat, m_bAltCheckFormat);
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_ReleaseDeviceIdle, m_bReleaseDeviceIdle);
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_UseCrossFeed, m_bUseCrossFeed);
    pApp->WriteProfileInt(OPT_SECTION_AudRend, OPT_DummyChannels, m_bDummyChannels);

#endif

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::SetWasapiMode(INT nValue)
{
	CAutoLock cAutoLock(&m_csProps);

	if (m_DeviceMode != nValue) {
		SetReinitializeAudioDevice();
	}

	m_DeviceMode = m_DeviceModeCurrent = (DEVICE_MODE)nValue;
	return S_OK;
}

STDMETHODIMP_(INT) CMpcAudioRenderer::GetWasapiMode()
{
	CAutoLock cAutoLock(&m_csProps);
	return (INT)m_DeviceMode;
}

STDMETHODIMP CMpcAudioRenderer::SetWasapiMethod(INT nValue)
{
	CAutoLock cAutoLock(&m_csProps);

	if (m_WasapiMethod != nValue) {
		SetReinitializeAudioDevice();
	}

	m_WasapiMethod = (WASAPI_METHOD)nValue;
	return S_OK;
}

STDMETHODIMP_(INT) CMpcAudioRenderer::GetWasapiMethod()
{
	CAutoLock cAutoLock(&m_csProps);
	return (INT)m_WasapiMethod;
}

STDMETHODIMP CMpcAudioRenderer::SetDevicePeriod(INT nValue)
{
	if (discard(nValue, -1, { 0, 50, 100 }) < 0) {
		return E_INVALIDARG;
	}

	CAutoLock cAutoLock(&m_csProps);

	if (m_BufferDuration != nValue) {
		m_BufferDuration = nValue;
		if (m_pAudioClient) {
			SetReinitializeAudioDevice();
		}
	}

	return S_OK;
}

STDMETHODIMP_(INT) CMpcAudioRenderer::GetDevicePeriod()
{
	CAutoLock cAutoLock(&m_csProps);
	return (INT)m_BufferDuration;
}

STDMETHODIMP CMpcAudioRenderer::SetDeviceId(const CString& deviceId, const CString& deviceName)
{
	CAutoLock cAutoLock(&m_csProps);

	auto deviceIdSrc = m_strCurrentDeviceId;
	auto deviceIdDst = deviceId;

	if (deviceIdSrc != deviceIdDst
			&& (deviceIdSrc.IsEmpty() || deviceIdDst.IsEmpty())) {
		AudioDevices::device_t device;
		AudioDevices::GetDefaultAudioDevice(m_pMMDeviceEnumerator, device);

		if (deviceIdSrc.IsEmpty()) {
			deviceIdSrc = device.deviceId;
		}
		if (deviceIdDst.IsEmpty()) {
			deviceIdDst = device.deviceId;
		}
	}

	if (deviceIdSrc != deviceIdDst) {
		m_DeviceModeCurrent = m_DeviceMode;
		SetReinitializeAudioDevice(TRUE);
	}

	m_DeviceId = deviceId;
	m_DeviceName = deviceName;
	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::GetDeviceId(CString& deviceId, CString& deviceName)
{
	CAutoLock cAutoLock(&m_csProps);

	deviceId = m_DeviceId;
	deviceName = m_DeviceName;
	return S_OK;
}

STDMETHODIMP_(UINT) CMpcAudioRenderer::GetMode()
{
	CAutoLock cAutoLock(&m_csProps);

	CheckPointer(m_pGraph, MODE_WASAPI_NONE)

	if (m_bIsBitstream) {
		return MODE_WASAPI_EXCLUSIVE_BITSTREAM;
	}

	return (UINT)m_DeviceModeCurrent;
}

STDMETHODIMP CMpcAudioRenderer::GetStatus(WAVEFORMATEX** ppWfxIn, WAVEFORMATEX** ppWfxOut)
{
	CAutoLock cAutoLock(&m_csProps);

	*ppWfxIn  = m_pWaveFormatExInput;
	*ppWfxOut = m_pWaveFormatExOutput;

	return S_OK;
}

STDMETHODIMP CMpcAudioRenderer::SetBitExactOutput(BOOL bValue)
{
	CAutoLock cAutoLock(&m_csProps);

	if (m_bUseBitExactOutput != bValue && IsExclusiveMode()) {
		SetReinitializeAudioDevice();
	}

	m_bUseBitExactOutput = bValue;
	return S_OK;
}

STDMETHODIMP_(BOOL) CMpcAudioRenderer::GetBitExactOutput()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_bUseBitExactOutput;
}

STDMETHODIMP CMpcAudioRenderer::SetSystemLayoutChannels(BOOL bValue)
{
	CAutoLock cAutoLock(&m_csProps);

	if (m_bUseSystemLayoutChannels != bValue && IsExclusiveMode()) {
		SetReinitializeAudioDevice();
	}

	m_bUseSystemLayoutChannels = bValue;
	return S_OK;
}

STDMETHODIMP_(BOOL) CMpcAudioRenderer::GetSystemLayoutChannels()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_bUseSystemLayoutChannels;
}

STDMETHODIMP CMpcAudioRenderer::SetAltCheckFormat(BOOL bValue)
{
	CAutoLock cAutoLock(&m_csProps);

	m_bAltCheckFormat = bValue;
	return S_OK;
}

STDMETHODIMP_(BOOL) CMpcAudioRenderer::GetAltCheckFormat()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_bAltCheckFormat;
}

STDMETHODIMP CMpcAudioRenderer::SetReleaseDeviceIdle(BOOL bValue)
{
	CAutoLock cAutoLock(&m_csProps);

	m_bReleaseDeviceIdle = bValue;
	return S_OK;
}

STDMETHODIMP_(BOOL) CMpcAudioRenderer::GetReleaseDeviceIdle()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_bReleaseDeviceIdle;
}

STDMETHODIMP_(BITSTREAM_MODE) CMpcAudioRenderer::GetBitstreamMode()
{
	CAutoLock cAutoLock(&m_csProps);
	return (m_pGraph && m_bIsBitstream) ? m_BitstreamMode : BITSTREAM_NONE;
}

STDMETHODIMP_(CString) CMpcAudioRenderer::GetCurrentDeviceName()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_strCurrentDeviceName;
}

STDMETHODIMP_(CString) CMpcAudioRenderer::GetCurrentDeviceId()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_strCurrentDeviceId;
}

STDMETHODIMP CMpcAudioRenderer::SetCrossFeed(BOOL bValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_bUseCrossFeed = bValue;

	if (m_bUseCrossFeed && m_pAudioClient && !m_bs2b_active) {
		if (m_output_params.channels == 2
				&& m_output_params.samplerate >= BS2B_MINSRATE
				&& m_output_params.samplerate <= BS2B_MAXSRATE) {
			m_bs2b.clear();
			m_bs2b.set_srate(m_output_params.samplerate);
			m_bs2b.set_level_fcut(700); // 700 Hz
			m_bs2b.set_level_feed(60);  // 6 dB
			m_bs2b_active = true;
			return S_OK;
		}
	}

	m_bs2b_active = false;
	return S_OK;
}

STDMETHODIMP_(BOOL) CMpcAudioRenderer::GetCrossFeed()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_bUseCrossFeed;
}

STDMETHODIMP CMpcAudioRenderer::SetDummyChannels(BOOL bValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_bDummyChannels = bValue;
	return S_OK;
}

STDMETHODIMP_(BOOL) CMpcAudioRenderer::GetDummyChannels()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_bDummyChannels;
}

void CMpcAudioRenderer::SetBalanceMask(const DWORD output_layout)
{
	m_dwBalanceMask = 0;
	DWORD quiet_layout = 0;

	if (m_lBalance < DSBPAN_CENTER) { // left louder, right quieter
		quiet_layout = SPEAKER_FRONT_RIGHT|SPEAKER_BACK_RIGHT|SPEAKER_FRONT_RIGHT_OF_CENTER|SPEAKER_SIDE_RIGHT|SPEAKER_TOP_FRONT_RIGHT|SPEAKER_TOP_BACK_RIGHT;
	}
	else if (m_lBalance > DSBPAN_CENTER) { // right louder, left quieter
		quiet_layout = SPEAKER_FRONT_LEFT|SPEAKER_BACK_LEFT|SPEAKER_FRONT_LEFT_OF_CENTER|SPEAKER_SIDE_LEFT|SPEAKER_TOP_FRONT_LEFT|SPEAKER_TOP_BACK_LEFT;
	}

	unsigned channel_num = 0;
	for (unsigned i = 0; i < 32; i++) {
		DWORD ch = 1u << i;

		if (ch & output_layout) {
			if (ch & quiet_layout) {
				m_dwBalanceMask |= 1u << channel_num;
			}
			channel_num++;
		}
	}
}

#pragma region
// ==== WASAPI

static const WCHAR* GetSampleFormatString(const SampleFormat value)
{
#define UNPACK_VALUE(VALUE) case VALUE: return L#VALUE;
	switch (value) {
		UNPACK_VALUE(SAMPLE_FMT_U8);
		UNPACK_VALUE(SAMPLE_FMT_S16);
		UNPACK_VALUE(SAMPLE_FMT_S24);
		UNPACK_VALUE(SAMPLE_FMT_S32);
		UNPACK_VALUE(SAMPLE_FMT_FLT);
		UNPACK_VALUE(SAMPLE_FMT_DBL);
	};
#undef UNPACK_VALUE
	return L"Error value";
};

HRESULT CMpcAudioRenderer::Transform(IMediaSample *pMediaSample)
{
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 1
	TRACE(L"CMpcAudioRenderer::Transform()\n");
#endif

	if (m_filterState == State_Stopped
			|| !m_hRenderThread) {
		return S_FALSE;
	}

	if (m_bNeedReinitializeFull) {
		ReinitializeAudioDevice(TRUE);
	}
	if (m_bNeedReinitialize) {
		ReinitializeAudioDevice();
	}

	CheckBufferStatus();

	HANDLE handles[3] = {
		m_hStopRenderThreadEvent,
		m_FlushEvent,
		m_hRendererNeedMoreData,
	};

	const DWORD result = WaitForMultipleObjects(3, handles, FALSE, INFINITE);
	if (result == WAIT_OBJECT_0 || result == WAIT_OBJECT_0 + 1) {
		// m_hStopRenderThreadEvent, m_FlushEvent
		return S_FALSE;
	}

	long lSize = pMediaSample->GetActualDataLength();
	if (!lSize) {
		return S_FALSE;
	}

	REFERENCE_TIME rtStart, rtStop;
	if (FAILED(pMediaSample->GetTime(&rtStart, &rtStop))) {
		rtStart = rtStop = INVALID_TIME;
	}

	if (rtStart < 0 && rtStart != INVALID_TIME) {
		return S_FALSE;
	}

	CheckPointer(m_pWaveFormatExInput, S_FALSE);

	m_rtLastReceivedSampleTimeEnd = rtStart + SamplesToTime(lSize / m_pWaveFormatExInput->nBlockAlign, m_pWaveFormatExInput) / m_dRate;

	if (!m_pRenderClient) {
		if (!m_pAudioClient) {
			InitAudioClient();
		}
		if (!m_pRenderClient && m_pAudioClient) {
			CheckAudioClient(m_pWaveFormatExInput);
		}

		if (!m_pRenderClient) {
			m_bReleased = false;

			m_rtNextRenderedSampleTime = rtStart + SamplesToTime(lSize / m_pWaveFormatExInput->nBlockAlign, m_pWaveFormatExInput) / m_dRate;

			const auto duration = m_rtStartTime + rtStart - m_pSyncClock->GetPrivateTime() - m_hnsBufferDuration;
			if (duration >= OneMillisecond) {
				const DWORD dwMilliseconds = duration / OneMillisecond;
				TRACE(L"CMpcAudioRenderer::Transform() : sleep %u ms\n", dwMilliseconds);
				WaitForMultipleObjects(2, handles, FALSE, dwMilliseconds);
			}

			return S_FALSE;
		}
	}

	CheckPointer(m_pWaveFormatExOutput, S_FALSE);

	if (m_input_params.sf == SAMPLE_FMT_NONE || m_output_params.sf == SAMPLE_FMT_NONE) {
		return S_FALSE;
	}

	if (m_rtEstimateSlavingJitter > 0) {
		const REFERENCE_TIME rtSilence = m_rtEstimateSlavingJitter / m_dRate;
		const UINT32 nSilenceFrames    = TimeToSamples(rtSilence, m_pWaveFormatExOutput);
		const UINT32 nSilenceBytes     = nSilenceFrames * m_pWaveFormatExOutput->nBlockAlign;
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL
		TRACE(L"CMpcAudioRenderer::Transform() - Pad silence %.2f ms to minimize slaving jitter\n", rtSilence / 10000.0f);
#endif
		std::unique_ptr<CPacket> pSilence(DNew CPacket());
		pSilence->rtStart = rtStart - rtSilence;
		pSilence->rtStop  = rtStart;
		pSilence->SetCount(nSilenceBytes);

		m_rtNextRenderedSampleTime = pSilence->rtStart;
		m_rtEstimateSlavingJitter = 0;

		WasapiQueueAdd(pSilence);

		StartAudioClient();
	}

	BYTE *pMediaBuffer = nullptr;
	HRESULT hr = pMediaSample->GetPointer(&pMediaBuffer);
	if (FAILED(hr)) {
		return hr;
	}

	auto pInputBufferPointer = pMediaBuffer;

	std::unique_ptr<BYTE> pChangedBuffer;

	if (m_bUpdateBalanceMask) {
		SetBalanceMask(m_output_params.layout);
		m_bUpdateBalanceMask = false;
	}

	int in_samples = lSize / m_pWaveFormatExInput->nBlockAlign;
	int out_samples = in_samples;
	UNREFERENCED_PARAMETER(out_samples);

	const bool bFormatChanged = !m_bIsBitstream && (m_input_params.layout != m_output_params.layout || m_input_params.samplerate != m_output_params.samplerate || m_input_params.sf != m_output_params.sf);
	if (bFormatChanged) {
		if (m_input_params.layout != m_output_params.layout || m_input_params.samplerate != m_output_params.samplerate) {
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 2
			CString dbgLog = L"CMpcAudioRenderer::Transform() - use Mixer:\n";
			dbgLog.Append      (L"    input:\n");
			dbgLog.AppendFormat(L"        layout     = 0x%x\n", m_input_params.layout);
			dbgLog.AppendFormat(L"        channels   = %d\n",   m_input_params.channels);
			dbgLog.AppendFormat(L"        samplerate = %d\n",   m_input_params.samplerate);
			dbgLog.AppendFormat(L"        format     = %s\n",   GetSampleFormatString(m_input_params.sf));
			dbgLog.Append      (L"    output:\n");
			dbgLog.AppendFormat(L"        layout     = 0x%x\n", m_output_params.layout);
			dbgLog.AppendFormat(L"        channels   = %d\n",   m_output_params.channels);
			dbgLog.AppendFormat(L"        samplerate = %d\n",   m_output_params.samplerate);
			dbgLog.AppendFormat(L"        format     = %s",     GetSampleFormatString(m_output_params.sf));
			TRACE(dbgLog);
#endif

			CAutoLock cResamplerLock(&m_csResampler);

			m_Resampler.SetOptions(1.0, 1.0, false, !!m_bDummyChannels);
			m_Resampler.UpdateInput(m_input_params.sf, m_input_params.layout, m_input_params.samplerate);
			m_Resampler.UpdateOutput(m_output_params.sf, m_output_params.layout, m_output_params.samplerate);
			out_samples = m_Resampler.CalcOutSamples(in_samples);
			if (!out_samples) {
				TRACE(L"CMpcAudioRenderer::Transform() - resampling failure\n");
				return E_INVALIDARG;
			}
			REFERENCE_TIME delay = m_Resampler.GetDelay();
			pChangedBuffer.reset(DNew BYTE[out_samples * m_output_params.channels * get_bytes_per_sample(m_output_params.sf)]);
			out_samples = m_Resampler.Mixing(pChangedBuffer.get(), out_samples, pMediaBuffer, in_samples);
			if (!out_samples) {
				return E_INVALIDARG;
			}
			lSize = out_samples * m_output_params.channels * get_bytes_per_sample(m_output_params.sf);

			if (delay && rtStart != INVALID_TIME) {
				rtStart -= delay;
				rtStop  -= delay;
			}
		} else {
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 2
			TRACE(L"CMpcAudioRenderer::Transform() - convert: '%s' -> '%s'\n", GetSampleFormatString(m_input_params.sf), GetSampleFormatString(m_output_params.sf));
#endif

			lSize = out_samples * m_output_params.channels * get_bytes_per_sample(m_output_params.sf);
			pChangedBuffer.reset(DNew BYTE[lSize]);

			HRESULT hr = E_INVALIDARG;
			switch (m_output_params.sf) {
				case SAMPLE_FMT_S16:
					m_DitherInt16.UpdateInput(m_input_params.sf, m_output_params.channels);
					m_DitherInt16.Process(reinterpret_cast<int16_t*>(pChangedBuffer.get()), pMediaBuffer, out_samples);
					hr = S_OK;
					break;
				case SAMPLE_FMT_S24:
					hr = convert_to_int24(m_input_params.sf, m_output_params.channels, out_samples, pMediaBuffer, pChangedBuffer.get());
					break;
				case SAMPLE_FMT_S32:
					hr = convert_to_int32(m_input_params.sf, m_output_params.channels, out_samples, pMediaBuffer, reinterpret_cast<int32_t*>(pChangedBuffer.get()));
					break;
				case SAMPLE_FMT_FLT:
					hr = convert_to_float(m_input_params.sf, m_output_params.channels, out_samples, pMediaBuffer, reinterpret_cast<float*>(pChangedBuffer.get()));
					break;
			}

			if (FAILED(hr)) {
				return hr;
			}
		}

		pInputBufferPointer	= pChangedBuffer.get();
	}

	if (m_bs2b_active) {
		switch (m_output_params.sf) {
		case SAMPLE_FMT_S16:
			m_bs2b.cross_feed(reinterpret_cast<int16_t*>(pInputBufferPointer), out_samples);
			break;
		case SAMPLE_FMT_S24:
			m_bs2b.cross_feed(reinterpret_cast<bs2b_int24_t*>(pInputBufferPointer), out_samples);
			break;
		case SAMPLE_FMT_S32:
			m_bs2b.cross_feed(reinterpret_cast<int32_t*>(pInputBufferPointer), out_samples);
			break;
		case SAMPLE_FMT_FLT:
			m_bs2b.cross_feed(reinterpret_cast<float*>(pInputBufferPointer), out_samples);
			break;
		}
	}

	std::unique_ptr<CPacket> p(DNew CPacket());
	p->rtStart = rtStart;
	p->rtStop  = rtStop;
	p->bDiscontinuity = (S_OK == pMediaSample->IsDiscontinuity());
	p->SetData(pInputBufferPointer, lSize);

	if (m_bIsBitstream && m_BitstreamMode == BITSTREAM_NONE) {
		if (m_pWaveFormatExOutput->wFormatTag == WAVE_FORMAT_DOLBY_AC3_SPDIF) {
			m_BitstreamMode = BITSTREAM_DTS;
			// AC3/DTS
			if (lSize > 8) {
				const BYTE IEC61937_type = pInputBufferPointer[4];
				if (IEC61937_type == IEC61937_AC3) {
					m_BitstreamMode = BITSTREAM_AC3;
				}
			}
		} else if (IsWaveFormatExtensible(m_pWaveFormatExOutput)) {
			const WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE*)m_pWaveFormatExOutput;
			if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS) {
				m_BitstreamMode = BITSTREAM_EAC3;
			} else if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD) {
				m_BitstreamMode = BITSTREAM_DTSHD;
			} else if (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP) {
				m_BitstreamMode = BITSTREAM_TRUEHD;
			}
		}
	}

	if (m_dRate != 1.0 && !m_bIsBitstream) {
		hr = SetupAudioFilter();

		if (SUCCEEDED(hr)) {
			hr = m_AudioFilter.Push(p);
			if (SUCCEEDED(hr)) {
				while (SUCCEEDED(m_AudioFilter.Pull(p))) {
					PushToQueue(p);
				}
			}
		}
	} else {
		PushToQueue(p);
	}

	if (!m_bIsAudioClientStarted) {
		StartRendererThread();
		StartAudioClient();
	}

	return S_OK;
}

HRESULT CMpcAudioRenderer::PushToQueue(std::unique_ptr<CPacket>& p)
{
    if (!m_pWaveFormatExOutput) {
        ASSERT(false);
        return E_FAIL;
    }

	if (p && (!m_rtLastQueuedSampleTimeEnd || p->bDiscontinuity)) {
		if (p->bDiscontinuity && (p->rtStop <= m_rtLastQueuedSampleTimeEnd)) {
			TRACE(L"CMpcAudioRenderer::PushToQueue() - drop [%I64d]\n", p->rtStart);
			return S_OK;
		} else if (p->rtStart > m_rtLastQueuedSampleTimeEnd) {
			const auto rtLastTimeEnd = std::max(m_rtLastQueuedSampleTimeEnd, m_rtNextRenderedSampleTime);
			const auto rtLastStop = (!rtLastTimeEnd && m_pSyncClock->IsSlave()) ? m_pSyncClock->GetPrivateTime() - m_rtStartTime : rtLastTimeEnd;
			const auto rtSilence = p->rtStart - rtLastStop;
			if (rtSilence > 0 && rtSilence <= 60 * UNITS) {
				const UINT32 nSilenceFrames = TimeToSamples(rtSilence, m_pWaveFormatExOutput);
				if (nSilenceFrames > 0) {
					const UINT32 nSilenceBytes = nSilenceFrames * m_pWaveFormatExOutput->nBlockAlign;
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL
					TRACE(L"CMpcAudioRenderer::PushToQueue() - Pad silence %.2f ms [%I64d -> %I64d, %s -> %s]\n",
						 rtSilence / 10000.0f, rtLastStop, p->rtStart, ReftimeToString(rtLastStop), ReftimeToString(p->rtStart));
#endif
					std::unique_ptr<CPacket> pSilence(DNew CPacket());
					pSilence->rtStart = p->rtStart - rtSilence;
					pSilence->rtStop = p->rtStart;
					pSilence->SetCount(nSilenceBytes);

					WasapiQueueAdd(pSilence);
				}
			}
		}
	}

	WasapiQueueAdd(p);

	return S_OK;
}
#pragma endregion

HRESULT CMpcAudioRenderer::SetupAudioFilter()
{
	if (m_AudioFilter.IsInitialized()) {
		return S_FALSE;
	}

	if (m_dRate == 1.0 || m_dRate < 0.25 || m_dRate > 100.0) {
		m_AudioFilter.Flush();
		return E_INVALIDARG;
	}

	HRESULT hr;
	CStringA flt_args;
	if (m_dRate < 0.5) {
		flt_args.Format("tempo=%f", sqrt(m_dRate));
		hr = m_AudioFilter.Initialize(
			m_output_params.sf, m_output_params.layout, m_output_params.samplerate,
			m_output_params.sf, m_output_params.layout, m_output_params.samplerate,
			false,
			{ {"atempo", flt_args}, {"atempo", flt_args} }
		);
	}
	else {
		flt_args.Format("tempo=%f", m_dRate);
		hr = m_AudioFilter.Initialize(
			m_output_params.sf, m_output_params.layout, m_output_params.samplerate,
			m_output_params.sf, m_output_params.layout, m_output_params.samplerate,
			false, { {"atempo", flt_args} }
		);
	}

	return hr;
}

HRESULT CMpcAudioRenderer::GetAudioDevice(const BOOL bForceUseDefaultDevice)
{
	TRACE(L"CMpcAudioRenderer::GetAudioDevice() - Target device: '%s'/'%s'\n", m_DeviceId, m_DeviceName);

	m_bUseDefaultDevice = FALSE;
	m_strCurrentDeviceId.Empty();
	m_strCurrentDeviceName.Empty();

	if (!bForceUseDefaultDevice && !m_DeviceId.IsEmpty()) {
		AudioDevices::deviceList_t deviceList;
		if (SUCCEEDED(AudioDevices::GetActiveAudioDevices(m_pMMDeviceEnumerator, &deviceList, nullptr, FALSE))) {
			TRACE(L"CMpcAudioRenderer::GetAudioDevice() - trying by device id\n");
			for (const auto& device : deviceList) {
				if (m_DeviceId == device.deviceId) {
					TRACE(L"CMpcAudioRenderer::GetAudioDevice() - Using the device: '%s'/'%s'\n", device.deviceId, device.deviceName);

					m_strCurrentDeviceId = device.deviceId;
					m_strCurrentDeviceName = device.deviceName;
					m_pMMDeviceEnumerator->GetDevice(m_strCurrentDeviceId, &m_pMMDevice);

					return S_OK;
				}
			}

			TRACE(L"CMpcAudioRenderer::GetAudioDevice() - trying by device name\n");
			for (const auto& device : deviceList) {
				if (m_DeviceName == device.deviceName) {
					TRACE(L"CMpcAudioRenderer::GetAudioDevice() - Using the device: '%s'/'%s'\n", device.deviceId, device.deviceName);

					m_strCurrentDeviceId = device.deviceId;
					m_strCurrentDeviceName = device.deviceName;
					m_pMMDeviceEnumerator->GetDevice(m_strCurrentDeviceId, &m_pMMDevice);

					return S_OK;
				}
			}
		}
	}

	m_bUseDefaultDevice = TRUE;
	AudioDevices::device_t device;
	if (SUCCEEDED(AudioDevices::GetDefaultAudioDevice(m_pMMDeviceEnumerator, device))) {
		TRACE(L"CMpcAudioRenderer::GetAudioDevice() - Using the default device: '%s'/'%s'\n", device.deviceId, device.deviceName);

		m_strCurrentDeviceId = device.deviceId;
		m_strCurrentDeviceName = device.deviceName;
		m_pMMDeviceEnumerator->GetDevice(m_strCurrentDeviceId, &m_pMMDevice);

		return S_OK;
	}

	return E_FAIL;
}

HRESULT CMpcAudioRenderer::InitAudioClient()
{
	TRACE(L"CMpcAudioRenderer::InitAudioClient()\n");

	HRESULT hr = S_OK;
	m_hnsBufferDuration = 0;

	if (m_pMMDevice == nullptr) {
		TRACE(L"CMpcAudioRenderer::InitAudioClient() - failed, device not loaded\n");
		return E_FAIL;
	}

	if (m_pAudioClient) {
		PauseRendererThread();
		m_bIsAudioClientStarted = false;

		SAFE_RELEASE(m_pAudioClient);
	}

	hr = m_pMMDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&m_pAudioClient));
	if (FAILED(hr)) {
		TRACE(L"CMpcAudioRenderer::InitAudioClient() - IMMDevice::Activate() failed: (0x%08x)\n", hr);
	} else {
		TRACE(L"CMpcAudioRenderer::InitAudioClient() - success\n");
		if (m_wBitsPerSampleList.empty()) {
			// get list of supported output formats - wBitsPerSample, nChannels(dwChannelMask), nSamplesPerSec
			const WORD  wBitsPerSampleValues[] = {16, 24, 32};
			const DWORD nSamplesPerSecValues[] = {44100, 48000, 88200, 96000, 176400, 192000, 384000};
			const channel_layout_t ChannelLayoutValues[] = {
				{2, KSAUDIO_SPEAKER_STEREO},
				{4, KSAUDIO_SPEAKER_QUAD},
				{4, KSAUDIO_SPEAKER_SURROUND},
				{6, KSAUDIO_SPEAKER_5POINT1_SURROUND},
				{6, KSAUDIO_SPEAKER_5POINT1},
				{8, KSAUDIO_SPEAKER_7POINT1_SURROUND},
				{8, KSAUDIO_SPEAKER_7POINT1},
			};

			auto RemoveAll = [&]() {
				m_wBitsPerSampleList.clear();
				m_nChannelsList.clear();
				m_dwChannelMaskList.clear();
				m_AudioParamsList.clear();
				m_bReal32bitSupport = FALSE;
			};

			WAVEFORMATEXTENSIBLE wfex;

			// 1 - wBitsPerSample
			for (const auto& _bitdepth : wBitsPerSampleValues) {
				for (const auto& _samplerate : nSamplesPerSecValues) {
					CreateFormat(wfex, _bitdepth, 2, KSAUDIO_SPEAKER_STEREO, _samplerate);
					if (S_OK == m_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wfex, nullptr)) {
						if (Contains(m_wBitsPerSampleList, _bitdepth) == false) {
							m_wBitsPerSampleList.push_back(_bitdepth);
						}

						if (_bitdepth == 32) {
							CreateFormat(wfex, _bitdepth, 2, KSAUDIO_SPEAKER_STEREO, _samplerate, 32);
							m_bReal32bitSupport = (S_OK == m_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wfex, nullptr));
						}
					}
				}
			}
			if (m_wBitsPerSampleList.empty()) {
				RemoveAll();
				return hr;
			}

			// 2 - m_nSamplesPerSec
			for (const auto& _bitdepth : m_wBitsPerSampleList) {
				for (const auto& _samplerate : nSamplesPerSecValues) {
					CreateFormat(wfex, _bitdepth, ChannelLayoutValues[0].channels, ChannelLayoutValues[0].layout, _samplerate);
					if (S_OK == m_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wfex, nullptr)) {
						AudioParams ap(_bitdepth, _samplerate);
						m_AudioParamsList.push_back(ap);
					}
				}
			}
			if (m_AudioParamsList.empty()) {
				RemoveAll();
				return hr;
			}

			// 3 - nChannels(dwChannelMask)
			AudioParams ap = m_AudioParamsList[0];
			for (const auto item : ChannelLayoutValues) {
				CreateFormat(wfex, ap.wBitsPerSample, item.channels, item.layout, ap.nSamplesPerSec);
				if (S_OK == m_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wfex, nullptr)) {
					m_nChannelsList.push_back(item.channels);
					m_dwChannelMaskList.push_back(item.layout);
				}
			}
			if (m_nChannelsList.empty()) {
				RemoveAll();
				return hr;
			}

#ifdef DEBUG_OR_LOG
			TRACE(L"    List of supported output formats:\n");
			TRACE(L"        BitsPerSample:\n");
			for (const auto& _bitdepth : m_wBitsPerSampleList) {
				if (_bitdepth == 32 && !m_bReal32bitSupport) {
					TRACE(L"            24 padded to 32\n");
				} else {
					TRACE(L"            %d\n", _bitdepth);
				}
				TRACE(L"            SamplesPerSec:\n");
				for (const auto& audioparams : m_AudioParamsList) {
					if (audioparams.wBitsPerSample == _bitdepth) {
						TRACE(L"                %d\n", audioparams.nSamplesPerSec);
					}
				}
			}

			#define ADDENTRY(mode) ChannelMaskStr[mode] = L#mode
			std::map<DWORD, CString> ChannelMaskStr;
			ADDENTRY(KSAUDIO_SPEAKER_STEREO);
			ADDENTRY(KSAUDIO_SPEAKER_QUAD);
			ADDENTRY(KSAUDIO_SPEAKER_SURROUND);
			ADDENTRY(KSAUDIO_SPEAKER_5POINT1_SURROUND);
			ADDENTRY(KSAUDIO_SPEAKER_5POINT1);
			ADDENTRY(KSAUDIO_SPEAKER_7POINT1_SURROUND);
			ADDENTRY(KSAUDIO_SPEAKER_7POINT1);
			#undef ADDENTRY

			TRACE(L"        Channels:\n");
			for (size_t i = 0; i < m_nChannelsList.size(); i++) {
				TRACE(L"            %d/0x%x  [%s]\n", m_nChannelsList[i], m_dwChannelMaskList[i], ChannelMaskStr[m_dwChannelMaskList[i]]);
			}
#endif
		}
	}
	return hr;
}

HRESULT CMpcAudioRenderer::CreateAudioClient(const BOOL bForceUseDefaultDevice/* = FALSE*/)
{
	HRESULT hr = S_OK;
	if (!m_pMMDevice) {
		hr = GetAudioDevice(bForceUseDefaultDevice);
		if (FAILED(hr)) {
			return hr;
		}
	}

	if (!m_pAudioClient) {
		hr = InitAudioClient();
	}

	return hr;
}

HRESULT CMpcAudioRenderer::CheckAudioClient(const WAVEFORMATEX *pWaveFormatEx)
{
	CAutoLock cAutoLock(&m_csCheck);
	TRACE(L"CMpcAudioRenderer::CheckAudioClient()\n");

	BOOL bForceUseDefaultDevice = FALSE;

again:
	HRESULT hr = CreateAudioClient(bForceUseDefaultDevice);
	if (FAILED(hr)) {
		TRACE(L"CMpcAudioRenderer::CheckAudioClient() - audio client initialization error\n");
		return hr;
	}

	auto CheckFormatChanged = [&](const WAVEFORMATEX *pWaveFormatEx, WAVEFORMATEX **ppNewWaveFormatEx) {
		if (m_pWaveFormatExInput == nullptr || IsFormatChanged(pWaveFormatEx, m_pWaveFormatExInput)) {
			return CopyWaveFormat(pWaveFormatEx, ppNewWaveFormatEx);
		}

		return false;
	};

	auto ReleaseAudio = [this](const bool bFull = false) {
		m_csRender.Unlock();

		m_pSyncClock->UnSlave();

		PauseRendererThread();
		m_bIsAudioClientStarted = false;

		SAFE_RELEASE(m_pRenderClient);
		SAFE_RELEASE(m_pAudioClock);
		SAFE_RELEASE(m_pAudioClient);

		if (bFull) {
			SAFE_RELEASE(m_pMMDevice);
		}
	};

	CAutoLock cRenderLock(&m_csRender);

	BOOL bInitNeed = TRUE;
	// Compare the existing WAVEFORMATEX with the one provided
	if (CheckFormatChanged(pWaveFormatEx, &m_pWaveFormatExInput) || !m_pWaveFormatExOutput) {

		// Format has changed, audio client has to be reinitialized
		TRACE(L"CMpcAudioRenderer::CheckAudioClient() - Format changed, re-initialize the audio client\n");

		CopyWaveFormat(m_pWaveFormatExInput, &m_pWaveFormatExOutput);

		if (IsExclusive(pWaveFormatEx)) { // EXCLUSIVE/BITSTREAM
			WAVEFORMATEX *pFormat = nullptr;

			if (IsBitstream(pWaveFormatEx)) {
				pFormat = (WAVEFORMATEX*)pWaveFormatEx;
			} else {
				if (m_bUseBitExactOutput) {
					WAVEFORMATEXTENSIBLE wfexAsIs = { 0 };

					if (m_bAltCheckFormat) {
						WORD nChannels = pWaveFormatEx->nChannels;
						DWORD dwChannelMask = GetChannelMask(pWaveFormatEx, nChannels);

						if (m_bUseSystemLayoutChannels) {
							WAVEFORMATEX* pDeviceFormat = nullptr;
							hr = m_pAudioClient->GetMixFormat(&pDeviceFormat);
							if (SUCCEEDED(hr) && pDeviceFormat) {
								nChannels = pDeviceFormat->nChannels;
								dwChannelMask = GetChannelMask(pDeviceFormat, nChannels);
								CoTaskMemFree(pDeviceFormat);
							}
						}

						CreateFormat(wfexAsIs, pWaveFormatEx->wBitsPerSample, nChannels, dwChannelMask, pWaveFormatEx->nSamplesPerSec);

						ReleaseAudio();
						hr = InitAudioClient();
						if (SUCCEEDED(hr)) {
#ifdef DEBUG_OR_LOG
							TRACE(L"    Trying format:\n");
							DumpWaveFormatEx((WAVEFORMATEX*)&wfexAsIs);
#endif
							hr = CreateRenderClient((WAVEFORMATEX*)&wfexAsIs, FALSE);
							if (S_OK == hr) {
								CopyWaveFormat((WAVEFORMATEX*)&wfexAsIs, &m_pWaveFormatExOutput);
								bInitNeed = FALSE;
							} else {
								const WORD wBitsPerSampleValues[][2] = {
									{ 32, 32 },
									{ 32, 24 },
									{ 24, 24 },
									{ 16, 16 }
								};
								for (const auto& wBitsPerSample : wBitsPerSampleValues) {
									CreateFormat(wfexAsIs, wBitsPerSample[0], nChannels, dwChannelMask, pWaveFormatEx->nSamplesPerSec, wBitsPerSample[1]);
#ifdef DEBUG_OR_LOG
									TRACE(L"    Trying format:\n");
									DumpWaveFormatEx((WAVEFORMATEX*)&wfexAsIs);
#endif
									hr = CreateRenderClient((WAVEFORMATEX*)&wfexAsIs, FALSE);
									if (S_OK == hr) {
										CopyWaveFormat((WAVEFORMATEX*)&wfexAsIs, &m_pWaveFormatExOutput);
										bInitNeed = FALSE;
										break;
									}
								}
							}
						} else {
							if (!m_bUseDefaultDevice) {
								TRACE(L"CMpcAudioRenderer::CheckAudioClient() - InitAudioClient() failed: (0x%08x), trying default\n", hr);

								ReleaseAudio(true);
								bForceUseDefaultDevice = TRUE;

								goto again;
							}

							TRACE(L"CMpcAudioRenderer::CheckAudioClient() - InitAudioClient() failed: (0x%08x)\n", hr);
							return hr;
						}
					}

					if (bInitNeed
							&& SUCCEEDED(SelectFormat(pWaveFormatEx, wfexAsIs))) {
						hr = m_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, (WAVEFORMATEX*)&wfexAsIs, nullptr);
						if (S_OK == hr) {
							CopyWaveFormat((WAVEFORMATEX*)&wfexAsIs, &pFormat);
						}
					}
				}

				if (!pFormat && bInitNeed) {
					IPropertyStore *pProps = nullptr;
					hr = m_pMMDevice->OpenPropertyStore(STGM_READ, &pProps);
					if (SUCCEEDED(hr)) {
						PROPVARIANT varConfig;
						PropVariantInit(&varConfig);
						hr = pProps->GetValue(PKEY_AudioEngine_DeviceFormat, &varConfig);
						if (SUCCEEDED(hr)) {
							if (varConfig.vt == VT_BLOB && varConfig.blob.pBlobData != nullptr) {
								CopyWaveFormat((WAVEFORMATEX*)varConfig.blob.pBlobData, &pFormat);
							}
							PropVariantClear(&varConfig);
						}
						pProps->Release();
					}
				}
			}

			if (bInitNeed && pFormat) {
				hr = m_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, pFormat, nullptr);
				if (S_OK == hr) {
					CopyWaveFormat(pFormat, &m_pWaveFormatExOutput);
				}
			}

			if (!IsBitstream(pWaveFormatEx) && pFormat) {
				delete pFormat;
			}
		} else if (m_DeviceModeCurrent == MODE_WASAPI_SHARED) { // SHARED
			if (SysVersion::IsWin10orLater()) {
				WAVEFORMATEX* pDeviceFormat = nullptr;
				if (SUCCEEDED(m_pAudioClient->GetMixFormat(&pDeviceFormat)) && pDeviceFormat) {
					if (m_pWaveFormatExOutput->wBitsPerSample != pDeviceFormat->wBitsPerSample) {
						m_pWaveFormatExOutput->wBitsPerSample = pDeviceFormat->wBitsPerSample;
						m_pWaveFormatExOutput->nBlockAlign = m_pWaveFormatExOutput->nChannels * m_pWaveFormatExOutput->wBitsPerSample / 8;

						if (IsWaveFormatExtensible(m_pWaveFormatExOutput)) {
							auto pwfex = (WAVEFORMATEXTENSIBLE*)m_pWaveFormatExOutput;
							pwfex->Samples.wValidBitsPerSample = m_pWaveFormatExOutput->wBitsPerSample;
						}
					}

					if (m_pWaveFormatExOutput->nSamplesPerSec != pDeviceFormat->nSamplesPerSec) {
						m_pWaveFormatExOutput->nSamplesPerSec = pDeviceFormat->nSamplesPerSec;
						m_pWaveFormatExOutput->nAvgBytesPerSec = m_pWaveFormatExOutput->nSamplesPerSec * m_pWaveFormatExOutput->nBlockAlign;
					}

					CoTaskMemFree(pDeviceFormat);
				}
			}

			WAVEFORMATEX *pClosestMatch = nullptr;
			hr = m_pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, m_pWaveFormatExOutput, &pClosestMatch);
			if (SUCCEEDED(hr)) {
				if (pClosestMatch) {
					CopyWaveFormat(pClosestMatch, &m_pWaveFormatExOutput);
					CoTaskMemFree(pClosestMatch);
				}
			} else {
				WAVEFORMATEX* pDeviceFormat = nullptr;
				hr = m_pAudioClient->GetMixFormat(&pDeviceFormat);
				if (SUCCEEDED(hr) && pDeviceFormat) {
					CopyWaveFormat(pDeviceFormat, &m_pWaveFormatExOutput);
					CoTaskMemFree(pDeviceFormat);
				}
			}
		}

#ifdef DEBUG_OR_LOG
		TRACE(L"    Input format:\n");
		DumpWaveFormatEx(pWaveFormatEx);

		TRACE(L"    Output format:\n");
		DumpWaveFormatEx(m_pWaveFormatExOutput);
#endif

		if (SUCCEEDED(hr)) {
			TRACE(L"CMpcAudioRenderer::CheckAudioClient() - WASAPI client accepted the format\n");
		} else if (AUDCLNT_E_UNSUPPORTED_FORMAT == hr) {
			TRACE(L"CMpcAudioRenderer::CheckAudioClient() - WASAPI client refused the format\n");
			SAFE_DELETE_ARRAY(m_pWaveFormatExOutput);
			return hr;
		} else if (AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED == hr && (m_DeviceModeCurrent == MODE_WASAPI_EXCLUSIVE && !IsBitstream(pWaveFormatEx))) {
			TRACE(L"CMpcAudioRenderer::CheckAudioClient() - WASAPI exclusive mode not allowed, trying shared\n");

			ReleaseAudio();

			SAFE_DELETE_ARRAY(m_pWaveFormatExOutput);
			m_DeviceModeCurrent = MODE_WASAPI_SHARED;
			goto again;
		} else {
			TRACE(L"CMpcAudioRenderer::CheckAudioClient() - WASAPI failed: (0x%08x)\n", hr);
			SAFE_DELETE_ARRAY(m_pWaveFormatExOutput);
			return hr;
		}
	} else if (m_pRenderClient == nullptr) {
		TRACE(L"CMpcAudioRenderer::CheckAudioClient() - initialization/initialization of the audio renderer\n");
	} else {
		return hr;
	}

	if (bInitNeed) {
		ReleaseAudio();

		hr = InitAudioClient();
		if (SUCCEEDED(hr)) {
			hr = CreateRenderClient(m_pWaveFormatExOutput, m_bAltCheckFormat);
		}

		if (AUDCLNT_E_DEVICE_IN_USE == hr && !m_bUseDefaultDevice) {
			TRACE(L"CMpcAudioRenderer::CheckAudioClient() - device is used, trying default\n");

			ReleaseAudio(true);

			SAFE_DELETE_ARRAY(m_pWaveFormatExOutput);
			bForceUseDefaultDevice = TRUE;
			goto again;
		} else if (AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED == hr && (m_DeviceModeCurrent == MODE_WASAPI_EXCLUSIVE && !IsBitstream(pWaveFormatEx))) {
			TRACE(L"CMpcAudioRenderer::CheckAudioClient() - WASAPI exclusive mode not allowed, trying shared\n");

			ReleaseAudio();

			SAFE_DELETE_ARRAY(m_pWaveFormatExOutput);
			m_DeviceModeCurrent = MODE_WASAPI_SHARED;
			goto again;
		}
	}

	if (SUCCEEDED(hr)) {
		m_AudioFilter.Flush();
	}

	m_bs2b_active = false;

	if (SUCCEEDED(hr)) {
		auto FillFormats = [](const WAVEFORMATEX* pwfe, AudioFormats& format, bool bInput) {
			ZeroMemory(&format, sizeof(format));
			format.sf = SAMPLE_FMT_NONE;

			format.channels   = pwfe->nChannels;
			format.samplerate = pwfe->nSamplesPerSec;

			bool bFloat = false;
			if (IsWaveFormatExtensible(pwfe)) {
				const WAVEFORMATEXTENSIBLE* pwfex = (WAVEFORMATEXTENSIBLE*)pwfe;
				format.layout = pwfex->dwChannelMask;
				bFloat        = !!(pwfex->SubFormat == MEDIASUBTYPE_IEEE_FLOAT);
			} else {
				format.layout = GetDefChannelMask(pwfe->nChannels);
				bFloat        = !!(pwfe->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
			}

			switch (pwfe->wBitsPerSample) {
				case 8:
					if (bInput) {
						format.sf = SAMPLE_FMT_U8;
					}
					break;
				case 16:
					format.sf = SAMPLE_FMT_S16;
					break;
				case 24:
					format.sf = SAMPLE_FMT_S24;
					break;
				case 32:
					format.sf = bFloat ? SAMPLE_FMT_FLT : SAMPLE_FMT_S32;
					break;
				case 64:
					if (bInput && bFloat) {
						format.sf = SAMPLE_FMT_DBL;
					}
					break;
			}
		};

		FillFormats(m_pWaveFormatExInput, m_input_params, true);
		FillFormats(m_pWaveFormatExOutput, m_output_params, false);

		if (m_bUseCrossFeed) {
			if (!m_bIsBitstream
					&& m_output_params.channels == 2
					&& m_output_params.samplerate >= BS2B_MINSRATE
					&& m_output_params.samplerate <= BS2B_MAXSRATE) {
				m_bs2b.clear();
				m_bs2b.set_srate(m_output_params.samplerate);
				m_bs2b.set_level_fcut(700); // 700 Hz
				m_bs2b.set_level_feed(60);  // 6 dB
				m_bs2b_active = true;
			}
		}
	}

	return hr;
}

static HRESULT CalcBitstreamBufferPeriod(WAVEFORMATEX *pWaveFormatEx, REFERENCE_TIME *pHnsBufferPeriod)
{
	CheckPointer(pWaveFormatEx, E_FAIL);

	UINT32 nBufferSize = 0;

	if (pWaveFormatEx->wFormatTag == WAVE_FORMAT_DOLBY_AC3_SPDIF) {
		nBufferSize = 6144;
	} else if (IsWaveFormatExtensible(pWaveFormatEx)) {
		const WAVEFORMATEXTENSIBLE *wfext = (WAVEFORMATEXTENSIBLE*)pWaveFormatEx;

		if (wfext->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP) {
			nBufferSize = 61440;
		} else if (wfext->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD) {
			nBufferSize = pWaveFormatEx->nChannels == 2 ? 8192 : 32768;
		} else if (wfext->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS) {
			nBufferSize = 24576;
		}
	}

	if (nBufferSize) {
		*pHnsBufferPeriod = llMulDiv(nBufferSize << 3, UNITS, pWaveFormatEx->nChannels * pWaveFormatEx->wBitsPerSample * pWaveFormatEx->nSamplesPerSec, 0);

		TRACE(L"CalcBitstreamBufferPeriod() - set period of %I64d hundred-nanoseconds for a %u buffer size\n", *pHnsBufferPeriod, nBufferSize);
	}

	return S_OK;
}

HRESULT CMpcAudioRenderer::CreateRenderClient(WAVEFORMATEX *pWaveFormatEx, const BOOL bCheckFormat/* = TRUE*/)
{
	TRACE(L"CMpcAudioRenderer::CreateRenderClient()\n");
	HRESULT hr = S_OK;

	m_hnsBufferDuration = 0;
	m_BitstreamMode = BITSTREAM_NONE;

	REFERENCE_TIME hnsDefaultDevicePeriod = 0;
	REFERENCE_TIME hnsMinimumDevicePeriod = 0;
	hr = m_pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, &hnsMinimumDevicePeriod);
	if (SUCCEEDED(hr)) {
		TRACE(L"CMpcAudioRenderer::CreateRenderClient() - DefaultDevicePeriod = %.2f ms, MinimumDevicePeriod = %.2f ms\n", hnsDefaultDevicePeriod / 10000.0f, hnsMinimumDevicePeriod / 10000.0f);
	}

	if (m_BufferDuration == 0) {
		m_hnsBufferDuration = hnsDefaultDevicePeriod > 0 ? hnsDefaultDevicePeriod : 500000LL;
	} else {
		m_hnsBufferDuration = m_BufferDuration * 10000LL;
	}
	TRACE(L"CMpcAudioRenderer::CreateRenderClient() - using period = %.2f ms\n", m_hnsBufferDuration / 10000.0f);

	const auto ShareMode = IsExclusive(pWaveFormatEx) ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
	TRACE(L"CMpcAudioRenderer::CreateRenderClient() - using %s mode\n", ShareMode == AUDCLNT_SHAREMODE_SHARED ? L"Shared" : L"Exclusive");

	if (bCheckFormat) {
		WAVEFORMATEX *pClosestMatch = nullptr;
		hr = m_pAudioClient->IsFormatSupported(ShareMode,
											   pWaveFormatEx,
											   &pClosestMatch);
		if (pClosestMatch) {
			CoTaskMemFree(pClosestMatch);
		}

		if (S_OK == hr) {
			TRACE(L"CMpcAudioRenderer::CreateRenderClient() - WASAPI client accepted the format\n");
		} else if (S_FALSE == hr) {
			TRACE(L"CMpcAudioRenderer::CreateRenderClient() - WASAPI client refused the format with a closest match\n");
			return hr;
		} else if (AUDCLNT_E_UNSUPPORTED_FORMAT == hr) {
			TRACE(L"CMpcAudioRenderer::CreateRenderClient() - WASAPI client refused the format\n");
			return hr;
		} else {
			TRACE(L"CMpcAudioRenderer::CreateRenderClient() - IAudioClient::IsFormatSupported() failed: (0x%08x)\n", hr);
			return hr;
		}
	}

	if (m_WasapiMethod == WASAPI_METHOD::EVENT) {
		CalcBitstreamBufferPeriod(pWaveFormatEx, &m_hnsBufferDuration);
	}

	DWORD StreamFlags = AUDCLNT_STREAMFLAGS_NOPERSIST;
	if (m_WasapiMethod == WASAPI_METHOD::EVENT) {
		StreamFlags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	}

	if (SUCCEEDED(hr)) {
		const REFERENCE_TIME hnsPeriodicity = (IsExclusive(pWaveFormatEx) && m_WasapiMethod == WASAPI_METHOD::EVENT) ? m_hnsBufferDuration : 0;
		hr = m_pAudioClient->Initialize(ShareMode,
										StreamFlags,
										m_hnsBufferDuration,
										hnsPeriodicity,
										pWaveFormatEx, nullptr);
	}

	if (AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED == hr && IsExclusive(pWaveFormatEx)) {
		// if the buffer size was not aligned, need to do the alignment dance
		TRACE(L"CMpcAudioRenderer::CreateRenderClient() - Buffer size not aligned. Realigning\n");

		// get the buffer size, which will be aligned
		hr = m_pAudioClient->GetBufferSize(&m_nFramesInBuffer);

		// throw away this IAudioClient
		PauseRendererThread();
		m_bIsAudioClientStarted = false;

		SAFE_RELEASE(m_pRenderClient);
		m_pSyncClock->UnSlave();
		SAFE_RELEASE(m_pAudioClock);
		SAFE_RELEASE(m_pAudioClient);
		if (SUCCEEDED(hr)) {
			hr = InitAudioClient();
		}

		// calculate the new aligned buffer duration and set the same periodicity
		m_hnsBufferDuration = SamplesToTime(m_nFramesInBuffer, pWaveFormatEx);

		TRACE(L"CMpcAudioRenderer::CreateRenderClient() - Trying again with periodicity of %I64d hundred-nanoseconds, or %u frames.\n", m_hnsBufferDuration, m_nFramesInBuffer);
		if (SUCCEEDED(hr)) {
			const REFERENCE_TIME hnsPeriodicity = m_WasapiMethod == WASAPI_METHOD::EVENT ? m_hnsBufferDuration : 0;
			hr = m_pAudioClient->Initialize(ShareMode,
											StreamFlags,
											m_hnsBufferDuration,
											hnsPeriodicity,
											pWaveFormatEx, nullptr);
		}
	}

	if (FAILED(hr)) {
		if (AUDCLNT_E_DEVICE_IN_USE == hr) {
			TRACE(L"CMpcAudioRenderer::CreateRenderClient() - IAudioClient::Initialize() failed: Device in use\n");
		} else if (AUDCLNT_E_UNSUPPORTED_FORMAT == hr) {
			TRACE(L"CMpcAudioRenderer::CreateRenderClient() - IAudioClient::Initialize() failed: Device does not support the specified format\n");
		} else {
			TRACE(L"CMpcAudioRenderer::CreateRenderClient() - IAudioClient::Initialize() failed: (0x%08x)\n", hr);
		}
		return hr;
	}

	// get the buffer size, which is aligned
	hr = m_pAudioClient->GetBufferSize(&m_nFramesInBuffer);
	EXIT_ON_ERROR(hr);
	TRACE(L"CMpcAudioRenderer::CreateRenderClient() - NumBufferFrames = %u (%.2f ms)\n", m_nFramesInBuffer, m_nFramesInBuffer * 1000.0 / pWaveFormatEx->nSamplesPerSec);

	// enables a client to write output data to a rendering endpoint buffer
	hr = m_pAudioClient->GetService(IID_PPV_ARGS(&m_pRenderClient));

	if (FAILED(hr)) {
		TRACE(L"CMpcAudioRenderer::CreateRenderClient() - IAudioClient::GetService(IAudioRenderClient) failed: (0x%08x)\n", hr);
	} else {
		TRACE(L"CMpcAudioRenderer::CreateRenderClient() - service initialization success, with periodicity of %I64d hundred-nanoseconds = %u frames\n", m_hnsBufferDuration, m_nFramesInBuffer);
		m_bIsBitstream = IsBitstream(pWaveFormatEx);
	}
	EXIT_ON_ERROR(hr);

	hr = m_pAudioClient->GetService(IID_PPV_ARGS(&m_pAudioClock));
	if (FAILED(hr)) {
		TRACE(L"CMpcAudioRenderer::CreateRenderClient() - IAudioClient::GetService(IAudioClock) failed: (0x%08x)\n", hr);
	}

	if (!m_bReleased) {
		WasapiFlush();
	}

	m_rtEstimateSlavingJitter = 0;
	if (m_filterState == State_Running && m_pAudioClock) {
		auto GetAudioPosition = [&] {
			UINT64 deviceClockFrequency, deviceClockPosition;
			if (SUCCEEDED(m_pAudioClock->GetFrequency(&deviceClockFrequency))
				&& SUCCEEDED(m_pAudioClock->GetPosition(&deviceClockPosition, nullptr))) {
				return llMulDiv(deviceClockPosition, UNITS, deviceClockFrequency, 0);
			}

			return 0LL;
		};

		if (!m_bReleased) {
			m_rtEstimateSlavingJitter = m_rtLastReceivedSampleTimeEnd - (m_pSyncClock->GetPrivateTime() - m_rtStartTime) + GetAudioPosition();
			if (m_rtEstimateSlavingJitter < 0 || m_rtEstimateSlavingJitter > UNITS) {
				m_rtEstimateSlavingJitter = 0;
			}
			m_pSyncClock->Slave(m_pAudioClock, m_rtStartTime + m_rtLastReceivedSampleTimeEnd - m_rtEstimateSlavingJitter);
		} else {
			const REFERENCE_TIME rtEstimateSlavingJitter = m_rtCurrentRenderedTime - (m_pSyncClock->GetPrivateTime() - m_rtStartTime) + GetAudioPosition();
			if (rtEstimateSlavingJitter >= OneMillisecond
					&& rtEstimateSlavingJitter <= UNITS) {
				const DWORD dwMilliseconds = rtEstimateSlavingJitter / OneMillisecond;
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL
				TRACE(L"CMpcAudioRenderer::CreateRenderClient() - Sleep %u ms to minimize slaving jitter\n", dwMilliseconds);
				Sleep(dwMilliseconds);
#endif
			}
			m_pSyncClock->Slave(m_pAudioClock, m_rtStartTime + m_rtCurrentRenderedTime);
		}
	}

	m_bReleased = false;

	const auto buffer_duration = m_hnsBufferDuration < 1000000LL ? 2000000LL : 5000000LL;
	m_nMaxWasapiQueueSize = TimeToSamples(buffer_duration, m_pWaveFormatExOutput) * m_pWaveFormatExOutput->nBlockAlign;
	TRACE(L"CMpcAudioRenderer::CreateRenderClient() - internal buffer duration = %.2f ms, size = %u\n", buffer_duration / 10000.0f, m_nMaxWasapiQueueSize);

#ifdef DEBUG_OR_LOG
	REFERENCE_TIME hnsLatency = 0;
	if (SUCCEEDED(m_pAudioClient->GetStreamLatency(&hnsLatency))) {
		TRACE(L"CMpcAudioRenderer::CreateRenderClient() - stream latency = %.2f ms\n", hnsLatency / 10000.0f);
	}
#endif

	if (m_WasapiMethod == WASAPI_METHOD::EVENT) {
		hr = m_pAudioClient->SetEventHandle(m_hDataEvent);
		EXIT_ON_ERROR(hr);
	}

	hr = StartRendererThread();

	return hr;
}

bool CMpcAudioRenderer::IsFormatChanged(const WAVEFORMATEX *pWaveFormatEx, const WAVEFORMATEX *pNewWaveFormatEx)
{
	if (!pWaveFormatEx || !pNewWaveFormatEx) {
		return true;
	}

	if (pWaveFormatEx->wFormatTag != pNewWaveFormatEx->wFormatTag
			|| pWaveFormatEx->nChannels != pNewWaveFormatEx->nChannels
			|| pWaveFormatEx->wBitsPerSample != pNewWaveFormatEx->wBitsPerSample
			|| pWaveFormatEx->nSamplesPerSec != pNewWaveFormatEx->nSamplesPerSec) {
		return true;
	}

	WAVEFORMATEXTENSIBLE* wfex    = nullptr;
	WAVEFORMATEXTENSIBLE* wfexNew = nullptr;
	if (IsWaveFormatExtensible(pWaveFormatEx)) {
		wfex = (WAVEFORMATEXTENSIBLE*)pWaveFormatEx;
	}
	if (IsWaveFormatExtensible(pNewWaveFormatEx)) {
		wfexNew	= (WAVEFORMATEXTENSIBLE*)pNewWaveFormatEx;
	}

	if (wfex && wfexNew
			&& (wfex->SubFormat != wfexNew->SubFormat || wfex->dwChannelMask != wfexNew->dwChannelMask)) {
		if (wfex->SubFormat == wfexNew->SubFormat
				&& (wfex->dwChannelMask == KSAUDIO_SPEAKER_5POINT1 && wfexNew->dwChannelMask == KSAUDIO_SPEAKER_5POINT1_SURROUND)) {
			return false;
		}
		return true;
	}

	return false;
}

bool CMpcAudioRenderer::CopyWaveFormat(const WAVEFORMATEX *pSrcWaveFormatEx, WAVEFORMATEX **ppDestWaveFormatEx)
{
	CheckPointer(pSrcWaveFormatEx, false);

	SAFE_DELETE_ARRAY(*ppDestWaveFormatEx);

	size_t size = sizeof(WAVEFORMATEX) + pSrcWaveFormatEx->cbSize;
	*ppDestWaveFormatEx = (WAVEFORMATEX *)DNew BYTE[size];
	if (!(*ppDestWaveFormatEx)) {
		return false;
	}
	memcpy(*ppDestWaveFormatEx, pSrcWaveFormatEx, size);

	return true;
}

bool CMpcAudioRenderer::IsBitstream(const WAVEFORMATEX *pWaveFormatEx) const
{
	CheckPointer(pWaveFormatEx, false);

	if (pWaveFormatEx->wFormatTag == WAVE_FORMAT_DOLBY_AC3_SPDIF) {
		return true;
	}

	if (IsWaveFormatExtensible(pWaveFormatEx)) {
		WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE*)pWaveFormatEx;
		return (wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_DIGITAL_PLUS
				|| wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DTS_HD
				|| wfex->SubFormat == KSDATAFORMAT_SUBTYPE_IEC61937_DOLBY_MLP);
	}

	return false;
}

static DWORD FindClosestInArray(std::vector<DWORD>& array, DWORD val)
{
	LONG diff = abs(LONG(val - array[0]));
	DWORD Num = array[0];
	for (const auto& item : array) {
		LONG diff1 = abs(LONG(val - item));
		if (diff > diff1) {
			diff = diff1;
			Num = item;
		}
	}

	return Num;
}

HRESULT CMpcAudioRenderer::SelectFormat(const WAVEFORMATEX* pwfx, WAVEFORMATEXTENSIBLE& wfex)
{
	// first - check variables ...
	if (m_wBitsPerSampleList.empty()
			|| m_AudioParamsList.empty()
			|| m_nChannelsList.empty()) {
		return E_FAIL;
	}

	// trying to create an output media type similar to input media type ...

	DWORD nSamplesPerSec = pwfx->nSamplesPerSec;
	BOOL bExists = FALSE;
	for (const auto& audioparams : m_AudioParamsList) {
		if (audioparams.nSamplesPerSec == nSamplesPerSec) {
			bExists = TRUE;
			break;
		}
	}

	if (!bExists) {
		std::vector<DWORD> array;
		for (const auto& audioparams : m_AudioParamsList) {
			if (Contains(array, audioparams.nSamplesPerSec) == false) {
				array.push_back(audioparams.nSamplesPerSec);
			}
		}
		nSamplesPerSec = FindClosestInArray(array, nSamplesPerSec);
	}

	WORD wBitsPerSample = pwfx->wBitsPerSample;
	AudioParams ap(wBitsPerSample, nSamplesPerSec);
	if (Contains(m_AudioParamsList, ap) == false) {
		wBitsPerSample = 0;
		for (int i = m_AudioParamsList.size() - 1; i >= 0; i--) {
			if (m_AudioParamsList[i].nSamplesPerSec == nSamplesPerSec) {
				wBitsPerSample = m_AudioParamsList[i].wBitsPerSample;
				break;
			}
		}
	}

	WORD nChannels      = 0;
	DWORD dwChannelMask = 0;

	if (m_bUseSystemLayoutChannels) {
		// to get the number of channels and channel mask quite simple call IAudioClient::GetMixFormat()
		WAVEFORMATEX *pDeviceFormat = nullptr;
		if (SUCCEEDED(m_pAudioClient->GetMixFormat(&pDeviceFormat)) && pDeviceFormat) {
			nChannels = pDeviceFormat->nChannels;
			dwChannelMask = GetChannelMask(pDeviceFormat, nChannels);

			CoTaskMemFree(pDeviceFormat);
		}
	}

	if (!nChannels) {
		nChannels = pwfx->nChannels;
		switch (nChannels) {
			case 1:
			case 3:
				nChannels = 2;
				break;
			case 5:
				nChannels = 6;
				break;
			case 7:
				nChannels = 8;
				break;
		}

		dwChannelMask = GetChannelMask(pwfx, nChannels);

		if (Contains(m_nChannelsList, nChannels) == false) {
			nChannels     = m_nChannelsList[m_nChannelsList.size() - 1];
			auto idx = std::distance(m_nChannelsList.begin(), std::find(m_nChannelsList.begin(), m_nChannelsList.end(), nChannels));
			dwChannelMask = m_dwChannelMaskList[idx];
		}

		if (Contains(m_dwChannelMaskList, dwChannelMask) == false) {
			auto idx = std::distance(m_nChannelsList.begin(), std::find(m_nChannelsList.begin(), m_nChannelsList.end(), nChannels));
			dwChannelMask = m_dwChannelMaskList[idx];
		}
	}

	CreateFormat(wfex, wBitsPerSample, nChannels, dwChannelMask, nSamplesPerSec);

	return S_OK;
}

void CMpcAudioRenderer::CreateFormat(WAVEFORMATEXTENSIBLE& wfex, WORD wBitsPerSample, WORD nChannels, DWORD dwChannelMask, DWORD nSamplesPerSec, WORD wValidBitsPerSample/* = 0*/)
{
	ZeroMemory(&wfex, sizeof(wfex));

	WAVEFORMATEX& wfe   = wfex.Format;
	wfe.nChannels       = nChannels;
	wfe.nSamplesPerSec  = nSamplesPerSec;
	wfe.wBitsPerSample  = wBitsPerSample;
	wfe.nBlockAlign     = nChannels * wfe.wBitsPerSample / 8;
	wfe.nAvgBytesPerSec = nSamplesPerSec * wfe.nBlockAlign;

	wfex.Format.wFormatTag               = WAVE_FORMAT_EXTENSIBLE;
	wfex.Format.cbSize                   = sizeof(wfex) - sizeof(wfex.Format);
	wfex.SubFormat                       = MEDIASUBTYPE_PCM;
	wfex.dwChannelMask                   = dwChannelMask;
	wfex.Samples.wValidBitsPerSample     = wValidBitsPerSample ? wValidBitsPerSample : wBitsPerSample;
	if (wfex.Samples.wValidBitsPerSample == 32
			&& !wValidBitsPerSample
			&& !m_bReal32bitSupport) {
		wfex.Samples.wValidBitsPerSample = 24;
	}
}

HRESULT CMpcAudioRenderer::StartAudioClient()
{
	HRESULT hr = S_OK;

	if (m_pAudioClient) {
		if (!m_bIsAudioClientStarted) {
			TRACE(L"CMpcAudioRenderer::StartAudioClient()\n");

			// To reduce latency, load the first buffer with data
			// from the audio source before starting the stream.
			RenderWasapiBuffer();

			CAutoLock cAutoLock(&m_csAudioClock);
			if (FAILED(hr = m_pAudioClient->Start())) {
				TRACE(L"CMpcAudioRenderer::StartAudioClient() - start audio client failed (0x%08x)\n", hr);
				return hr;
			}
			m_bIsAudioClientStarted = true;
		} else {
			TRACE(L"CMpcAudioRenderer::StartAudioClient() - already started\n");
		}
	}

	return hr;
}

void CMpcAudioRenderer::SetReinitializeAudioDevice(BOOL bFullInitialization/* = FALSE*/)
{
	if (bFullInitialization) {
		m_bNeedReinitializeFull = TRUE;
	} else {
		m_bNeedReinitialize = TRUE;
	}

	if (m_filterState == State_Running) {
		SetEvent(m_hRendererNeedMoreData);
	}
}

HRESULT CMpcAudioRenderer::ReinitializeAudioDevice(BOOL bFullInitialization/* = FALSE*/)
{
	TRACE(L"CMpcAudioRenderer::ReinitializeAudioDevice()\n");

	PauseRendererThread();

	CAutoLock cRenderLock(&m_csRender);

	if (m_bIsAudioClientStarted && m_pAudioClient) {
		m_pAudioClient->Stop();
	}
	m_bIsAudioClientStarted = false;
	m_bReleased = false;

	if (bFullInitialization) {
		SAFE_RELEASE(m_pRenderClient);
		SAFE_RELEASE(m_pAudioClient);
		SAFE_RELEASE(m_pMMDevice);

		m_wBitsPerSampleList.clear();
		m_nChannelsList.clear();
		m_dwChannelMaskList.clear();
		m_AudioParamsList.clear();

		m_bReal32bitSupport = FALSE;
	}
	SAFE_DELETE_ARRAY(m_pWaveFormatExOutput);

	HRESULT hr = S_OK;

	if (bFullInitialization) {
		hr = CreateAudioClient();
		if (SUCCEEDED(hr)) {
			hr = CheckAudioClient(m_pWaveFormatExInput);
		}
	} else {
		hr = CheckAudioClient(m_pWaveFormatExInput);
	}

	if (FAILED(hr)) {
		TRACE(L"CMpcAudioRenderer::ReinitializeAudioDevice() failed: (0x%08x)\n", hr);
	}

	m_bNeedReinitialize = m_bNeedReinitializeFull = FALSE;

	return hr;
}

HRESULT CMpcAudioRenderer::RenderWasapiBuffer()
{
	CheckPointer(m_pRenderClient, S_OK);
	CheckPointer(m_pWaveFormatExOutput, S_OK);

	CAutoLock cRenderLock(&m_csRender);

	HRESULT hr = S_OK;

	UINT32 numFramesPadding = 0;
	if ((m_DeviceModeCurrent == MODE_WASAPI_SHARED && !m_bIsBitstream) || m_WasapiMethod == WASAPI_METHOD::PUSH) {
		hr = m_pAudioClient->GetCurrentPadding(&numFramesPadding);
		if (FAILED(hr)) {
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 1
			TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - GetCurrentPadding() failed (0x%08x)\n", hr);
#endif
			return hr;
		}
	}

	const UINT32 numFramesAvailable = m_nFramesInBuffer - numFramesPadding;
	const UINT32 nAvailableBytes = numFramesAvailable * m_pWaveFormatExOutput->nBlockAlign;

	BYTE* pData = nullptr;
	hr = m_pRenderClient->GetBuffer(numFramesAvailable, &pData);
	if (FAILED(hr)) {
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 1
		TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - GetBuffer() failed with size %u (0x%08x)\n", numFramesAvailable, hr);
#endif
		return hr;
	}

	const size_t nWasapiQueueSize = WasapiQueueSize();
	DWORD dwFlags = 0;

	const BOOL bFlushing = m_FlushEvent.Check();
	if (!pData || !nWasapiQueueSize || m_filterState != State_Running || bFlushing) {
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 1
		if (m_filterState != State_Running) {
			TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - not running\n");
		} else if (!nWasapiQueueSize) {
			TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - data queue is empty\n");
		} else if (bFlushing) {
			TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - flushing\n");
		}
#endif
		dwFlags = AUDCLNT_BUFFERFLAGS_SILENT;

		if (m_bIsAudioClientStarted && !nWasapiQueueSize && m_filterState == State_Running && !bFlushing) {
			const auto duration = SamplesToTime(numFramesAvailable, m_pWaveFormatExOutput);
			m_rtNextRenderedSampleTime = m_rtLastReceivedSampleTimeEnd = std::max(m_rtNextRenderedSampleTime, m_pSyncClock->GetPrivateTime() - m_rtStartTime + duration);
			TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - internal buffer is empty, render silence  %.2f ms to %I64d\n", duration / 10000.0f, m_rtNextRenderedSampleTime);
		}
	} else {
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 1
		TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - requested: %u[%u], available: %Iu\n", nAvailableBytes, numFramesAvailable, nWasapiQueueSize);
#endif
		UINT32 nWritenBytes = 0;

		do {
			if (!m_CurrentPacket) {
				m_nSampleOffset = 0;

				size_t count;
				m_WasapiQueue.RemoveSafe(m_CurrentPacket, count);
				if (!m_CurrentPacket) {
					break;
				}
			}

			if (!m_nSampleOffset) {
				const REFERENCE_TIME rtTimeDelta = m_CurrentPacket->rtStart - m_rtNextRenderedSampleTime;
				if (std::abs(rtTimeDelta) > 200) {
					m_pSyncClock->OffsetAudioClock(rtTimeDelta);
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL
					TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - Discontinuity detected, Correct reference clock by %.2f ms\n", rtTimeDelta / 10000.0f);
#endif
				}
				m_rtNextRenderedSampleTime = m_CurrentPacket->rtStart + SamplesToTime(m_CurrentPacket->size() / m_pWaveFormatExOutput->nBlockAlign, m_pWaveFormatExOutput);
				m_rtCurrentRenderedTime = m_CurrentPacket->rtStart;
			}

			const UINT32 nFilledBytes = std::min((UINT32)m_CurrentPacket->size(), nAvailableBytes - nWritenBytes);
			memcpy(&pData[nWritenBytes], m_CurrentPacket->data(), nFilledBytes);
			if (nFilledBytes == m_CurrentPacket->size()) {
				m_CurrentPacket.reset();
			} else {
				m_CurrentPacket->RemoveHead(nFilledBytes);
			}
			nWritenBytes += nFilledBytes;
			m_nSampleOffset += nFilledBytes;

			m_rtCurrentRenderedTime += SamplesToTime(nFilledBytes / m_pWaveFormatExOutput->nBlockAlign, m_pWaveFormatExOutput);
		} while (nWritenBytes < nAvailableBytes);

		if (!nWritenBytes) {
			dwFlags = AUDCLNT_BUFFERFLAGS_SILENT;
		} else if (nWritenBytes < nAvailableBytes) {
			memset(&pData[nWritenBytes], 0, nAvailableBytes - nWritenBytes);
		}

		if (m_lVolume <= DSBVOLUME_MIN) {
			dwFlags = AUDCLNT_BUFFERFLAGS_SILENT;
		}
		else if (!m_bIsBitstream && (m_lVolume < DSBVOLUME_MAX || m_lBalance != DSBPAN_CENTER)) {
			ApplyVolumeBalance(pData, nAvailableBytes);
		}
	}

	hr = m_pRenderClient->ReleaseBuffer(numFramesAvailable, dwFlags);
#if defined(DEBUG_OR_LOG) && DBGLOG_LEVEL > 1
	if (FAILED(hr)) {
		TRACE(L"CMpcAudioRenderer::RenderWasapiBuffer() - ReleaseBuffer() failed with size %u (0x%08x)\n", numFramesAvailable, hr);
	}
#endif

	return hr;
}

void CMpcAudioRenderer::CheckBufferStatus()
{
	if (!m_pWaveFormatExOutput) {
		return;
	}

	if (!m_pRenderClient || !m_bIsAudioClientStarted || WasapiQueueSize() < m_nMaxWasapiQueueSize) {
		SetEvent(m_hRendererNeedMoreData);
	} else {
		ResetEvent(m_hRendererNeedMoreData);
	}
}

void CMpcAudioRenderer::WasapiFlush()
{
	TRACE(L"CMpcAudioRenderer::WasapiFlush()\n");

	m_WasapiQueue.RemoveAll();
	m_CurrentPacket.reset();

	m_rtLastQueuedSampleTimeEnd = 0;
	m_nSampleOffset = 0;

	CAutoLock cResamplerLock(&m_csResampler);
	m_Resampler.FlushBuffers();
}

HRESULT CMpcAudioRenderer::EndFlush()
{
	CAutoLock cRenderLock(&m_csRender);

	WasapiFlush();
	m_AudioFilter.Flush();

	HRESULT hr = CBaseRenderer::EndFlush();

	m_FlushEvent.Reset();

	return hr;
}

void CMpcAudioRenderer::NewSegment()
{
	TRACE(L"CMpcAudioRenderer::NewSegment()\n");

	m_rtNextRenderedSampleTime = 0;
	m_rtLastReceivedSampleTimeEnd = 0;
	m_rtEstimateSlavingJitter = 0;
	m_rtCurrentRenderedTime = 0;

	if (m_filterState != State_Running) {
		StartReleaseTimer();
	}

	m_bFlushing = FALSE;
}

void CMpcAudioRenderer::Flush()
{
	if (m_bDVDPlayback) {
		m_pSyncClock->UnSlave();
	}

	m_FlushEvent.Set();
	m_bFlushing = TRUE;
}

size_t CMpcAudioRenderer::WasapiQueueSize()
{
	return m_WasapiQueue.GetSize() + (m_CurrentPacket ? m_CurrentPacket->size() : 0);
}

void CMpcAudioRenderer::WaitFinish()
{
	if (!m_bIsBitstream && m_input_params.samplerate != m_output_params.samplerate) {
		int out_samples = m_Resampler.CalcOutSamples(0);
		if (out_samples) {
			REFERENCE_TIME rtStart = m_rtNextRenderedSampleTime;
			for (;;) {
				BYTE* buff = DNew BYTE[out_samples * m_output_params.channels * get_bytes_per_sample(m_output_params.sf)];
				out_samples = m_Resampler.Receive(buff, out_samples);
				if (out_samples) {
					std::unique_ptr<CPacket> p(DNew CPacket());
					p->rtStart = rtStart;
					p->rtStop  = rtStart + SamplesToTime(out_samples, m_pWaveFormatExOutput);
					p->SetData(buff, out_samples * m_output_params.channels * get_bytes_per_sample(m_output_params.sf));

					rtStart = p->rtStop;

					if (m_dRate != 1.0) {
						HRESULT hr = SetupAudioFilter();

						if (SUCCEEDED(hr)) {
							hr = m_AudioFilter.Push(p);
							if (SUCCEEDED(hr)) {
								while (SUCCEEDED(m_AudioFilter.Pull(p))) {
									PushToQueue(p);
								}
							}
						}
					} else {
						PushToQueue(p);
					}
				}

				delete [] buff;

				if (!out_samples) {
					break;
				}

				out_samples = m_Resampler.CalcOutSamples(0);
			}
		}
	}

	if (!m_bIsBitstream && m_dRate != 1.0) {
		HRESULT hr = SetupAudioFilter();

		if (SUCCEEDED(hr)) {
			m_AudioFilter.PushEnd();
			std::unique_ptr<CPacket> p;
			while (SUCCEEDED(m_AudioFilter.Pull(p))) {
				PushToQueue(p);
			}
		}
	}

	for (;;) {
		if (m_bNeedReinitialize || m_bNeedReinitializeFull
				|| m_filterState == State_Stopped || !m_hRenderThread
				|| m_bFlushing
				|| !WasapiQueueSize()) {
			break;
		}

		Sleep(m_hnsBufferDuration / 10000);
	}
}

static VOID CALLBACK TimerCallbackFunc(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	if (auto pRenderer = (CMpcAudioRenderer*)lpParameter) {
		pRenderer->ReleaseDevice();
	}
}

void CMpcAudioRenderer::StartReleaseTimer()
{
	if (m_bReleaseDeviceIdle && !m_hReleaseTimerHandle) {
		std::ignore = CreateTimerQueueTimer(&m_hReleaseTimerHandle,
											nullptr,
											TimerCallbackFunc,
											this,
											3000,
											0,
											WT_EXECUTEINTIMERTHREAD);
	}
}

void CMpcAudioRenderer::EndReleaseTimer()
{
	if (m_hReleaseTimerHandle) {
		std::ignore = DeleteTimerQueueTimer(nullptr, m_hReleaseTimerHandle, INVALID_HANDLE_VALUE);
		m_hReleaseTimerHandle = nullptr;
	}
}

void CMpcAudioRenderer::ReleaseDevice()
{
	TRACE(L"CMpcAudioRenderer::ReleaseDevice()\n");

    CAutoLock cRenderLock(&m_csRender);

	m_bReleased = true;

	PauseRendererThread();
	m_bIsAudioClientStarted = false;

	m_pSyncClock->UnSlave();

	if (IsExclusiveMode()) {
		SAFE_RELEASE(m_pRenderClient);
		SAFE_RELEASE(m_pAudioClock);
		SAFE_RELEASE(m_pAudioClient);
	} else if (m_pAudioClient) {
		m_pAudioClient->Stop();
	}

	std::ignore = DeleteTimerQueueTimer(nullptr, m_hReleaseTimerHandle, nullptr);
	m_hReleaseTimerHandle = nullptr;
}

void CMpcAudioRenderer::ApplyVolumeBalance(BYTE* pData, UINT32 size)
{
	const SampleFormat sf   = m_output_params.sf;
	const size_t channels   = m_output_params.channels;
	const size_t allsamples = size / (m_pWaveFormatExOutput->wBitsPerSample / 8);
	const size_t samples    = allsamples / channels;

	if (m_lBalance == DSBPAN_CENTER) {
		switch (sf) {
		case SAMPLE_FMT_U8:
			gain_uint8(m_dVolumeFactor, allsamples, (uint8_t*)pData);
			break;
		case SAMPLE_FMT_S16:
			gain_int16(m_dVolumeFactor, allsamples, (int16_t*)pData);
			break;
		case SAMPLE_FMT_S24:
			gain_int24(m_dVolumeFactor, allsamples, pData);
			break;
		case SAMPLE_FMT_S32:
			gain_int32(m_dVolumeFactor, allsamples, (int32_t*)pData);
			break;
		case SAMPLE_FMT_FLT:
			gain_float(m_dVolumeFactor, allsamples, (float*)pData);
			break;
		case SAMPLE_FMT_DBL:
			gain_double(m_dVolumeFactor, allsamples, (double*)pData);
			break;
		}
	} else {
		// volume and balance
		// do not use limiter, because  m_dBalanceFactor and m_dVolumeFactor are always less than or equal to 1.0
		switch (sf) {
		case SAMPLE_FMT_U8: {
			uint8_t* p = (uint8_t*)pData;
			for (size_t i = 0; i < samples; i++) {
				for (size_t ch = 0; ch < channels; ch++) {
					int8_t sample = (int8_t)(*p ^ 0x80);
					sample = (int8_t)((m_dwBalanceMask & (1 << ch) ? m_dBalanceFactor : m_dVolumeFactor) * sample);
					*p++ = (uint8_t)sample ^ 0x80;
				}
			}
			break; }
		case SAMPLE_FMT_S16: {
			int16_t* p = (int16_t*)pData;
			for (size_t i = 0; i < samples; i++) {
				for (size_t ch = 0; ch < channels; ch++) {
					*p++ = (int16_t)((m_dwBalanceMask & (1 << ch) ? m_dBalanceFactor : m_dVolumeFactor) * *p);
				}
			}
			break; }
		case SAMPLE_FMT_S24: {
			for (size_t i = 0; i < samples; i++) {
				for (size_t ch = 0; ch < channels; ch++) {
					int32_t sample = 0;
					((BYTE*)(&sample))[1] = *(pData);
					((BYTE*)(&sample))[2] = *(pData + 1);
					((BYTE*)(&sample))[3] = *(pData + 2);
					sample = (int32_t)((m_dwBalanceMask & (1 << ch) ? m_dBalanceFactor : m_dVolumeFactor) * sample);
					*pData++ = ((BYTE*)(&sample))[1];
					*pData++ = ((BYTE*)(&sample))[2];
					*pData++ = ((BYTE*)(&sample))[3];
				}
			}
			break; }
		case SAMPLE_FMT_S32: {
			int32_t* p = (int32_t*)pData;
			for (size_t i = 0; i < samples; i++) {
				for (size_t ch = 0; ch < channels; ch++) {
					*p++ = (int32_t)((m_dwBalanceMask & (1 << ch) ? m_dBalanceFactor : m_dVolumeFactor) * *p);
				}
			}
			break; }
		case SAMPLE_FMT_FLT: {
			float* p = (float*)pData;
			for (size_t i = 0; i < samples; i++) {
				for (size_t ch = 0; ch < channels; ch++) {
					*p++ = (float)((m_dwBalanceMask & (1 << ch) ? m_dBalanceFactor : m_dVolumeFactor) * *p);
				}
			}
			break; }
		case SAMPLE_FMT_DBL: {
			double* p = (double*)pData;
			for (size_t i = 0; i < samples; i++) {
				for (size_t ch = 0; ch < channels; ch++) {
					*p++ = (m_dwBalanceMask & (1 << ch) ? m_dBalanceFactor : m_dVolumeFactor) * *p;
				}
			}
			break; }
		}
	}
}

void CMpcAudioRenderer::WasapiQueueAdd(std::unique_ptr<CPacket>& p)
{
	m_rtLastQueuedSampleTimeEnd = p->rtStart + SamplesToTime(p->size() / m_pWaveFormatExOutput->nBlockAlign, m_pWaveFormatExOutput);
	m_WasapiQueue.Add(p);
}

CMpcAudioRendererInputPin::CMpcAudioRendererInputPin(CBaseRenderer* pRenderer, HRESULT* phr)
	: CRendererInputPin(pRenderer, phr, L"In")
	, m_pRenderer(static_cast<CMpcAudioRenderer*>(pRenderer))
{
}

STDMETHODIMP CMpcAudioRendererInputPin::NewSegment(REFERENCE_TIME startTime, REFERENCE_TIME stopTime, double rate)
{
	TRACE(L"CMpcAudioRendererInputPin::NewSegment()\n");

	CAutoLock cReceiveLock(&m_csReceive);

	m_pRenderer->NewSegment();
	return CRendererInputPin::NewSegment(startTime, stopTime, rate);
}

STDMETHODIMP CMpcAudioRendererInputPin::Receive(IMediaSample* pSample)
{
	CAutoLock cReceiveLock(&m_csReceive);
	return CRendererInputPin::Receive(pSample);
}

STDMETHODIMP CMpcAudioRendererInputPin::EndOfStream()
{
	TRACE(L"CMpcAudioRendererInputPin::EndOfStream()\n");

	CAutoLock cReceiveLock(&m_csReceive);

	m_pRenderer->WaitFinish();
	m_pRenderer->StartReleaseTimer();
	m_pRenderer->EndFlush();

	return CRendererInputPin::EndOfStream();
}

STDMETHODIMP CMpcAudioRendererInputPin::BeginFlush()
{
	TRACE(L"CMpcAudioRendererInputPin::BeginFlush()\n");

	m_pRenderer->Flush();

	CAutoLock cReceiveLock(&m_csReceive);
	return CRendererInputPin::BeginFlush();
}

STDMETHODIMP CMpcAudioRendererInputPin::EndFlush()
{
	TRACE(L"CMpcAudioRendererInputPin::EndFlush()\n");

	return CRendererInputPin::EndFlush();
}
