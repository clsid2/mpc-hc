#include "pch.h"
#include "AudioDeviceManager.h"

#include "AudioDeviceEvent.h"
#include "AudioDevicePush.h"
#include "DspMatrix.h"

namespace SaneAudioRenderer
{
    namespace
    {
        WAVEFORMATEX BuildWaveFormat(WORD formatTag, uint32_t formatBits, uint32_t rate, uint32_t channelCount)
        {
            WAVEFORMATEX format;

            format.wFormatTag      = formatTag;
            format.nChannels       = channelCount;
            format.nSamplesPerSec  = rate;
            format.nAvgBytesPerSec = formatBits / 8 * channelCount * rate;
            format.nBlockAlign     = formatBits / 8 * channelCount;
            format.wBitsPerSample  = formatBits;
            format.cbSize          = (formatTag == WAVE_FORMAT_EXTENSIBLE) ? 22 : 0;

            return format;
        }

        WAVEFORMATEXTENSIBLE BuildWaveFormatExt(GUID formatGuid, uint32_t formatBits, WORD formatExtProps,
                                                uint32_t rate, uint32_t channelCount, DWORD channelMask)
        {
            WAVEFORMATEXTENSIBLE format;

            format.Format        = BuildWaveFormat(WAVE_FORMAT_EXTENSIBLE, formatBits, rate, channelCount);
            format.Samples.wValidBitsPerSample = formatExtProps;
            format.dwChannelMask = channelMask;
            format.SubFormat     = formatGuid;

            return format;
        }

        template <typename T>
        void AppendPcmFormatPack(T& data, uint32_t rate, uint32_t channelCount, DWORD channelMask)
        {
            data.insert(data.cend(), {
                BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32, rate, channelCount, channelMask),
                BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 32, 32, rate, channelCount, channelMask),
                BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 24, 24, rate, channelCount, channelMask),
                BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 32, 24, rate, channelCount, channelMask),
                BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_PCM, 16, 16, rate, channelCount, channelMask),
            });
        }

        UINT32 GetDevicePropertyUint(IPropertyStore* pStore, REFPROPERTYKEY key)
        {
            assert(pStore);

            PROPVARIANT prop;
            PropVariantInit(&prop);
            ThrowIfFailed(pStore->GetValue(key, &prop));
            assert(prop.vt == VT_UI4);
            UINT32 ret = prop.uintVal;
            PropVariantClear(&prop);

            return ret;
        }

        SharedString GetDevicePropertyString(IPropertyStore* pStore, REFPROPERTYKEY key)
        {
            assert(pStore);

            PROPVARIANT prop;
            PropVariantInit(&prop);
            ThrowIfFailed(pStore->GetValue(key, &prop));
            if (prop.vt != VT_LPWSTR) {
                throw HResultException{E_FAIL};
            }
            auto ret = std::make_shared<std::wstring>(prop.pwszVal);
            PropVariantClear(&prop);

            return ret;
        }

        DWORD ShiftBackSide(DWORD mask)
        {
            return mask ^ (SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
                           SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT);
        }

        void CreateAudioClient(IMMDeviceEnumerator* pEnumerator, AudioDeviceBackend& backend)
        {
            assert(pEnumerator);

            IMMDevicePtr device;

            if (!backend.id || backend.id->empty())
            {
                ThrowIfFailed(pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device));

                LPWSTR pDeviceId = nullptr;
                ThrowIfFailed(device->GetId(&pDeviceId));
                std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);
                backend.id = std::make_shared<std::wstring>(pDeviceId);
            }
            else
            {
                IMMDeviceCollectionPtr collection;
                ThrowIfFailed(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection));

                UINT count = 0;
                ThrowIfFailed(collection->GetCount(&count));

                for (UINT i = 0; i < count; i++)
                {
                    ThrowIfFailed(collection->Item(i, &device));

                    LPWSTR pDeviceId = nullptr;
                    ThrowIfFailed(device->GetId(&pDeviceId));
                    std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);

                    if (*backend.id == pDeviceId)
                        break;

                    device = nullptr;
                }
            }

            if (!device)
                return;

            IPropertyStorePtr devicePropertyStore;
            ThrowIfFailed(device->OpenPropertyStore(STGM_READ, &devicePropertyStore));

            backend.adapterName  = GetDevicePropertyString(devicePropertyStore, PKEY_DeviceInterface_FriendlyName);
            backend.endpointName = GetDevicePropertyString(devicePropertyStore, PKEY_Device_DeviceDesc);

            static const PROPERTYKEY formFactorKey = { // PKEY_AudioEndpoint_FormFactor
                {0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}}, 0
            };
            backend.endpointFormFactor = GetDevicePropertyUint(devicePropertyStore, formFactorKey);

            static const PROPERTYKEY supportsEventModeKey = { // PKEY_AudioEndpoint_Supports_EventDriven_Mode
                {0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}}, 7
            };
            backend.supportsSharedEventMode = true;
            backend.supportsExclusiveEventMode = backend.supportsSharedEventMode &&
                                                 GetDevicePropertyUint(devicePropertyStore, supportsEventModeKey);

            ThrowIfFailed(device->Activate(__uuidof(IAudioClient),
                                           CLSCTX_INPROC_SERVER, nullptr, (void**)&backend.audioClient));
        }

        HRESULT CheckBitstreamFormat(IMMDeviceEnumerator* pEnumerator, SharedWaveFormat format, ISettings* pSettings)
        {
            assert(pEnumerator);
            assert(format);
            assert(pSettings);

            try
            {
                AudioDeviceBackend device = {};

                {
                    LPWSTR pDeviceId = nullptr;
                    ThrowIfFailed(pSettings->GetOutputDevice(&pDeviceId, nullptr, nullptr));
                    std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);

                    device.id = std::make_shared<std::wstring>(pDeviceId);
                }

                CreateAudioClient(pEnumerator, device);

                if (!device.audioClient)
                    return E_FAIL;

                return device.audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &(*format), nullptr);
            }
            catch (...)
            {
                return exception_to_hresult();
            }
        }

        HRESULT CreateAudioDeviceBackend(IMMDeviceEnumerator* pEnumerator,
                                         SharedWaveFormat format, bool realtime, ISettings* pSettings,
                                         std::shared_ptr<AudioDeviceBackend>& backend)
        {
            assert(pEnumerator);
            assert(format);
            assert(pSettings);

            try
            {
                backend = std::make_shared<AudioDeviceBackend>();

                LPWSTR pDeviceId = nullptr;
                {
                    BOOL exclusive;
                    UINT32 buffer;
                    ThrowIfFailed(pSettings->GetOutputDevice(&pDeviceId, &exclusive, &buffer));
                    if (pDeviceId) {
                        std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> holder(pDeviceId);
                        backend->id = std::make_shared<std::wstring>(pDeviceId);
                    }
                    backend->bitstream = (DspFormatFromWaveFormat(*format) == DspFormat::Unknown);
                    backend->exclusive = backend->bitstream || !!exclusive;
                    backend->realtime = realtime;
                    backend->bufferDuration = buffer;
                }

                CreateAudioClient(pEnumerator, *backend);

                if (!backend->audioClient) {
                    if (pDeviceId && *pDeviceId!=0) {
                        // failure with a chosen device, now try again with default audio device
                        backend->id = nullptr;
                        backend->bufferDuration = 200;
                        CreateAudioClient(pEnumerator, *backend);
                        if (!backend->audioClient) {
                            return E_FAIL;
                        }
                    } else {
                        return E_FAIL;
                    }
                }

                WAVEFORMATEX* pFormat;
                ThrowIfFailed(backend->audioClient->GetMixFormat(&pFormat));
                SharedWaveFormat mixFormat(pFormat, CoTaskMemFreeDeleter());

                backend->mixFormat = mixFormat;
                backend->ignoredSystemChannelMixer = false;

                const auto inputRate = format->nSamplesPerSec;
                const auto inputChannels = format->nChannels;
                const auto inputMask = DspMatrix::GetChannelMask(*format);
                const auto mixRate = mixFormat->nSamplesPerSec;
                const auto mixChannels = mixFormat->nChannels;
                const auto mixMask = DspMatrix::GetChannelMask(*mixFormat);

                if (backend->bitstream)
                {
                    // Exclusive bitstreaming.
                    backend->dspFormat = DspFormat::Unknown;
                    backend->waveFormat = format;
                }
                else if (backend->exclusive)
                {
                    // Exclusive.
                    std::vector<WAVEFORMATEXTENSIBLE> priorities;

                    if (backend->endpointFormFactor == DigitalAudioDisplayDevice)
                    {
                        AppendPcmFormatPack(priorities, inputRate, inputChannels, inputMask);
                        AppendPcmFormatPack(priorities, mixRate, inputChannels, inputMask);

                        // Shift between 5.1 with side channels and 5.1 with back channels.
                        if (inputMask == KSAUDIO_SPEAKER_5POINT1 ||
                            inputMask == ShiftBackSide(KSAUDIO_SPEAKER_5POINT1))
                        {
                            auto altMask = ShiftBackSide(inputMask);
                            AppendPcmFormatPack(priorities, inputRate, inputChannels, altMask);
                            AppendPcmFormatPack(priorities, mixRate, inputChannels, altMask);
                        }
                    }

                    AppendPcmFormatPack(priorities, inputRate, mixChannels, mixMask);
                    AppendPcmFormatPack(priorities, mixRate, mixChannels, mixMask);

                    priorities.insert(priorities.cend(), {
                        WAVEFORMATEXTENSIBLE{BuildWaveFormat(WAVE_FORMAT_PCM, 16, inputRate, mixChannels)},
                        WAVEFORMATEXTENSIBLE{BuildWaveFormat(WAVE_FORMAT_PCM, 16, mixRate, mixChannels)}
                    });

                    for (const auto& f : priorities)
                    {
                        assert(DspFormatFromWaveFormat(f.Format) != DspFormat::Unknown);

                        if (SUCCEEDED(backend->audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                                              &f.Format, nullptr)))
                        {
                            backend->dspFormat = DspFormatFromWaveFormat(f.Format);
                            backend->waveFormat = CopyWaveFormat(f.Format);
                            break;
                        }
                    }

                    if (backend->dspFormat == DspFormat::Unknown) {
                        // Exclusive mode is not supported, fallback to shared mode
                        backend->exclusive = false;
                    }
                }
                if (!backend->bitstream && !backend->exclusive)
                {
                    // Shared.
                    backend->dspFormat = DspFormat::Float;
                    backend->waveFormat = mixFormat;

                    BOOL crossfeedEnabled = pSettings->GetCrossfeedEnabled();

                    std::deque<WAVEFORMATEXTENSIBLE> priorities = {
                        BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32,
                                           mixRate, inputChannels, inputMask),
                    };

                    // Prefer 7.1 over 6.1 because of widespread audio driver bugs.
                    // No impact on audio quality.
                    if (inputMask == (KSAUDIO_SPEAKER_5POINT1 | SPEAKER_BACK_CENTER))
                    {
                        assert(inputChannels == 7);
                        priorities.push_front(BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32,
                                                                 mixRate, 8, KSAUDIO_SPEAKER_7POINT1_SURROUND));
                    }

                    // Shift between 5.1 with side channels and 5.1 with back channels.
                    if (inputMask == KSAUDIO_SPEAKER_5POINT1 ||
                        inputMask == ShiftBackSide(KSAUDIO_SPEAKER_5POINT1))
                    {
                        priorities.push_back(BuildWaveFormatExt(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 32, 32,
                                                                mixRate, inputChannels, ShiftBackSide(inputMask)));
                    }

                    for (const auto& f : priorities)
                    {
                        assert(DspFormatFromWaveFormat(f.Format) != DspFormat::Unknown);

                        WAVEFORMATEX* pClosest;
                        if (SUCCEEDED(backend->audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                                                              &f.Format, &pClosest)))
                        {
                            if (pClosest)
                            {
                                CoTaskMemFree(pClosest);
                            }
                            else
                            {
                                bool usingSystemChannelMixer = f.Format.nChannels != mixChannels ||
                                                               f.dwChannelMask != mixMask;

                                if (usingSystemChannelMixer && pSettings->GetIgnoreSystemChannelMixer())
                                {
                                    // Ignore system channel mixer if explicitly requested.
                                    backend->ignoredSystemChannelMixer = true;
                                }
                                else if (usingSystemChannelMixer && pSettings->GetCrossfeedEnabled() &&
                                                                    DspMatrix::IsStereoFormat(*mixFormat))
                                {
                                    // Crossfeed takes priority over system channel mixer.
                                    backend->ignoredSystemChannelMixer = true;
                                }
                                else
                                {
                                    backend->dspFormat = DspFormatFromWaveFormat(f.Format);
                                    backend->waveFormat = CopyWaveFormat(f.Format);
                                }
                                break;
                            }
                        }
                    }
                }

                backend->eventMode = (realtime && backend->supportsSharedEventMode) ||
                                     (backend->exclusive && backend->supportsExclusiveEventMode);

                {
                    AUDCLNT_SHAREMODE mode = backend->exclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE :
                                                                  AUDCLNT_SHAREMODE_SHARED;

                    DWORD flags = AUDCLNT_STREAMFLAGS_NOPERSIST;
                    if (backend->eventMode)
                        flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

                    REFERENCE_TIME defaultPeriod;
                    REFERENCE_TIME minimumPeriod;
                    ThrowIfFailed(backend->audioClient->GetDevicePeriod(&defaultPeriod, &minimumPeriod));

                    REFERENCE_TIME bufferDuration = OneMillisecond * backend->bufferDuration;
                    if (backend->eventMode)
                        bufferDuration = realtime ? minimumPeriod : defaultPeriod;

                    REFERENCE_TIME periodicy = 0;
                    if (backend->exclusive && backend->eventMode)
                        periodicy = bufferDuration;

                    // Initialize our audio client.
                    HRESULT result = backend->audioClient->Initialize(mode, flags, bufferDuration,
                                                                      periodicy, &(*backend->waveFormat), nullptr);

                    // Requested periodicity may have not met alignment requirements of the audio device.
                    if (result == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED &&
                        backend->exclusive && backend->eventMode)
                    {
                        // Ask the audio driver for closest supported periodicity.
                        UINT32 bufferFrames;
                        ThrowIfFailed(backend->audioClient->GetBufferSize(&bufferFrames));

                        // Recreate our audio client (MSDN suggests it).
                        backend->audioClient = nullptr;
                        CreateAudioClient(pEnumerator, *backend);
                        if (!backend->audioClient)
                            return E_FAIL;

                        bufferDuration = FramesToTime(bufferFrames, backend->waveFormat->nSamplesPerSec);
                        periodicy = bufferDuration;

                        // Initialize our audio client again with the right periodicity.
                        result = backend->audioClient->Initialize(mode, flags, bufferDuration,
                                                                  periodicy, &(*backend->waveFormat), nullptr);
                    }

                    ThrowIfFailed(result);
                }

                ThrowIfFailed(backend->audioClient->GetService(IID_PPV_ARGS(&backend->audioRenderClient)));
                ThrowIfFailed(backend->audioClient->GetService(IID_PPV_ARGS(&backend->audioClock)));

                ThrowIfFailed(backend->audioClient->GetStreamLatency(&backend->deviceLatency));
                ThrowIfFailed(backend->audioClient->GetBufferSize(&backend->deviceBufferSize));

                return S_OK;
            }
            catch (...)
            {
                backend = nullptr;
                return exception_to_hresult();
            }
        }

        HRESULT RecreateAudioDeviceBackend(IMMDeviceEnumerator* pEnumerator,
                                           std::shared_ptr<AudioDeviceBackend>& backend)
        {
            try
            {
                // Recreate
                backend->audioClient = nullptr;
                CreateAudioClient(pEnumerator, *backend);
                if (!backend->audioClient) {
                    return E_FAIL;
                }

                // Initialize
                {
                    AUDCLNT_SHAREMODE mode = backend->exclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE :
                                                                  AUDCLNT_SHAREMODE_SHARED;

                    DWORD flags = AUDCLNT_STREAMFLAGS_NOPERSIST;
                    if (backend->eventMode)
                        flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

                    REFERENCE_TIME bufferDuration = FramesToTime(backend->deviceBufferSize,
                                                                 backend->waveFormat->nSamplesPerSec);

                    REFERENCE_TIME periodicy = 0;
                    if (backend->exclusive && backend->eventMode)
                        periodicy = bufferDuration;

                    ThrowIfFailed(backend->audioClient->Initialize(mode, flags, bufferDuration,
                                                                   periodicy, &(*backend->waveFormat), nullptr));
                }

                // Get services
                ThrowIfFailed(backend->audioClient->GetService(IID_PPV_ARGS(&backend->audioRenderClient)));
                ThrowIfFailed(backend->audioClient->GetService(IID_PPV_ARGS(&backend->audioClock)));
                ThrowIfFailed(backend->audioClient->GetStreamLatency(&backend->deviceLatency));
                ThrowIfFailed(backend->audioClient->GetBufferSize(&backend->deviceBufferSize));
            }
            catch (...)
            {
                return exception_to_hresult();
            }

            return S_OK;
        }

        HRESULT GetDefaultDeviceIdInternal(IMMDeviceEnumerator* pEnumerator,
                                           std::unique_ptr<WCHAR, CoTaskMemFreeDeleter>& id)
        {
            assert(pEnumerator);

            id = nullptr;

            try
            {
                IMMDevicePtr device;
                ThrowIfFailed(pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device));

                LPWSTR pDeviceId = nullptr;
                ThrowIfFailed(device->GetId(&pDeviceId));
                id = std::unique_ptr<WCHAR, CoTaskMemFreeDeleter>(pDeviceId);
            }
            catch (...)
            {
                return exception_to_hresult();
            }

            return S_OK;
        }
    }

    AudioDeviceNotificationClient::AudioDeviceNotificationClient(std::atomic<uint32_t>& defaultDeviceSerial)
        : CUnknown("SaneAudioRenderer::AudioDeviceNotificationClient", nullptr)
        , m_defaultDeviceSerial(defaultDeviceSerial)
    {
    }

    STDMETHODIMP AudioDeviceNotificationClient::NonDelegatingQueryInterface(REFIID riid, void** ppv)
    {
        if (riid == __uuidof(IMMNotificationClient))
            return GetInterface(static_cast<IMMNotificationClient*>(this), ppv);

        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }

    STDMETHODIMP AudioDeviceNotificationClient::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR)
    {
        if (flow == eRender && role == eMultimedia)
            m_defaultDeviceSerial++;

        return S_OK;
    }

    AudioDeviceManager::AudioDeviceManager(HRESULT& result)
    {
        if (FAILED(result))
            return;

        try
        {
            if (static_cast<HANDLE>(m_wake) == NULL ||
                static_cast<HANDLE>(m_done) == NULL)
            {
                throw HResultException{ E_OUTOFMEMORY };
            }

            m_thread = std::thread(
                [this]
                {
                    CoInitializeHelper coInitializeHelper(COINIT_MULTITHREADED);

                    while (!m_exit)
                    {
                        m_wake.Wait();

                        if (m_function)
                        {
                            m_result = m_function();
                            m_function = nullptr;
                            m_done.Set();
                        }
                    }
                }
            );

            {
                m_function = [&] { return CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                                           CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_enumerator)); };
                m_wake.Set();
                m_done.Wait();
                ThrowIfFailed(m_result);
                assert(m_enumerator);
            }

            {
                auto pNotificationClient = new AudioDeviceNotificationClient(m_defaultDeviceSerial);

                pNotificationClient->NonDelegatingAddRef();

                ThrowIfFailed(pNotificationClient->NonDelegatingQueryInterface(IID_PPV_ARGS(&m_notificationClient)));

                pNotificationClient->NonDelegatingRelease();

                ThrowIfFailed(m_enumerator->RegisterEndpointNotificationCallback(m_notificationClient));
            }
        }
        catch (...)
        {
            result = exception_to_hresult();
        }
    }

    AudioDeviceManager::~AudioDeviceManager()
    {
        if (m_enumerator && m_notificationClient)
            m_enumerator->UnregisterEndpointNotificationCallback(m_notificationClient);

        m_enumerator = nullptr;

        m_exit = true;
        m_wake.Set();

        if (m_thread.joinable())
            m_thread.join();
    }

    bool AudioDeviceManager::BitstreamFormatSupported(SharedWaveFormat format, ISettings* pSettings)
    {
        assert(format);
        assert(pSettings);

        m_function = [&] { return CheckBitstreamFormat(m_enumerator, format, pSettings); };
        m_wake.Set();
        m_done.Wait();

        return SUCCEEDED(m_result);
    }

    std::unique_ptr<AudioDevice> AudioDeviceManager::CreateDevice(SharedWaveFormat format, bool realtime,
                                                                  ISettings* pSettings)
    {
        assert(format);
        assert(pSettings);

        std::shared_ptr<AudioDeviceBackend> backend;

        m_function = [&] { return CreateAudioDeviceBackend(m_enumerator, format, realtime, pSettings, backend); };
        m_wake.Set();
        m_done.Wait();

        if (FAILED(m_result))
            return nullptr;

        try
        {
            if (backend->eventMode)
                return std::unique_ptr<AudioDevice>(new AudioDeviceEvent(backend));

            return std::unique_ptr<AudioDevice>(new AudioDevicePush(backend));
        }
        catch (...)
        {
            return nullptr;
        }
    }

    bool AudioDeviceManager::RenewInactiveDevice(AudioDevice& device, int64_t& position)
    {
        auto renewFunction = [this](std::shared_ptr<AudioDeviceBackend>& backend) -> bool
        {
            m_function = [&] { return RecreateAudioDeviceBackend(m_enumerator, backend); };
            m_wake.Set();
            m_done.Wait();

            return SUCCEEDED(m_result);
        };

        try
        {
            return device.RenewInactive(renewFunction, position);
        }
        catch (...)
        {
            return false;
        }
    }

    std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> AudioDeviceManager::GetDefaultDeviceId()
    {
        assert(m_enumerator);

        std::unique_ptr<WCHAR, CoTaskMemFreeDeleter> id;

        m_function = [&] { return GetDefaultDeviceIdInternal(m_enumerator, id); };
        m_wake.Set();
        m_done.Wait();

        return id;
    }
}
