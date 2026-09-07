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
#include "CastTargetChromecast.h"
#include "CastTranscoder.h"
#include "Logger.h"
#include <algorithm>

#define CAST_MSGWND_CLASS   _T("MPCHCCastTarget")
#define WM_CAST_SESSION     (WM_APP + 0)
// what the streaming transcode (CCastHlsStream) reports its progress on:
// wParam is one of CCastHlsStream::Notify
#define WM_CAST_HLS         (WM_APP + 1)
// the whole-file transcode worker's completion: wParam is 1 on success, 0 on
// failure (the file is then served as it is)
#define WM_CAST_DOWNMIX     (WM_APP + 2)

// how long StopCasting() lets the polite media STOP reach the device before
// the connection is torn down regardless
#define STOP_MEDIA_TIMEOUT_MS 1000

CChromecastTarget::~CChromecastTarget()
{
    m_session.Stop();
    // The server stops before the transcode is abandoned, in the same order as
    // StopCasting(): its file-range parts read the raw fragmented MP4, and
    // only once the server is gone is its handle of it closed, so the
    // transcode's temp file can actually be deleted.
    m_server.EndHls();
    m_server.ClearFile();
    m_server.ClearAllowedPeer();
    m_server.Stop();
    AbandonTranscode();
    m_discovery.Stop();
    if (m_hMsgWnd) {
        DestroyWindow(m_hMsgWnd);
        m_hMsgWnd = nullptr;
    }
}

LRESULT CALLBACK CChromecastTarget::MsgWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* pThis = reinterpret_cast<CChromecastTarget*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (pThis && uMsg == WM_CAST_SESSION) {
        pThis->OnSessionStateChanged();
        return 0;
    }
    if (pThis && uMsg == WM_CAST_HLS) {
        pThis->OnHlsEvent(static_cast<int>(wParam));
        return 0;
    }
    if (pThis && uMsg == WM_CAST_DOWNMIX) {
        pThis->OnDownmixDone(wParam != 0);
        return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

bool CChromecastTarget::EnsureMessageWindow()
{
    if (m_hMsgWnd) {
        return true;
    }

    const HINSTANCE hInstance = AfxGetInstanceHandle();
    WNDCLASS wc = {};
    if (!GetClassInfo(hInstance, CAST_MSGWND_CLASS, &wc)) {
        wc.lpfnWndProc = MsgWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = CAST_MSGWND_CLASS;
        if (!RegisterClass(&wc)) {
            return false;
        }
    }

    m_hMsgWnd = CreateWindowEx(0, CAST_MSGWND_CLASS, nullptr, 0, 0, 0, 0, 0,
                               HWND_MESSAGE, nullptr, hInstance, nullptr);
    if (m_hMsgWnd) {
        SetWindowLongPtr(m_hMsgWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
    return m_hMsgWnd != nullptr;
}

CString CChromecastTarget::DeviceKey(const CastDevice& dev)
{
    if (!dev.id.IsEmpty()) {
        return dev.id;
    }
    CString key;
    key.Format(_T("%s:%u"), dev.ipAddress.GetString(), dev.port);
    return key;
}

CString CChromecastTarget::DeviceDisplayName(const CastDevice& dev)
{
    if (!dev.friendlyName.IsEmpty()) {
        return dev.friendlyName;
    }
    return !dev.model.IsEmpty() ? dev.model : dev.ipAddress;
}

bool CChromecastTarget::StartDiscovery()
{
    return m_discovery.Start();
}

void CChromecastTarget::StopDiscovery()
{
    m_discovery.Stop();
}

CastTargetDevice CChromecastTarget::ToTargetDevice(const CastDevice& dev)
{
    CastTargetDevice d;
    d.protocol = CastProtocol::Chromecast;
    d.name = DeviceDisplayName(dev);
    d.model = dev.model;
    d.id = DeviceKey(dev);
    d.address = dev.ipAddress;
    d.port = dev.port;
    d.supportsVideo = dev.SupportsVideo();
    d.supportsAudio = dev.SupportsAudio();
    return d;
}

std::vector<CastTargetDevice> CChromecastTarget::GetDevices()
{
    std::vector<CastTargetDevice> devices;
    for (const CastDevice& dev : m_discovery.GetDevices()) {
        devices.emplace_back(ToTargetDevice(dev));
    }
    return devices;
}

bool CChromecastTarget::ProbeAddress(CastProtocol protocol, const CString& address, UINT /*port*/,
                                     DWORD timeoutMs, CastTargetDevice& device)
{
    CastDevice dev;
    if (protocol != CastProtocol::Chromecast || address.IsEmpty()
            || !CCastDiscovery::ProbeAddress(address, timeoutMs, dev)) {
        return false;
    }
    device = ToTargetDevice(dev);
    return true;
}

bool CChromecastTarget::SearchById(const CString& id, DWORD timeoutMs, CastDevice& device)
{
    const bool wasRunning = m_discovery.IsRunning();
    if (!wasRunning && !m_discovery.Start()) {
        return false;
    }

    bool found = false;
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        for (const CastDevice& dev : m_discovery.GetDevices()) {
            if (!dev.id.IsEmpty() && dev.id == id) {
                device = dev;
                found = true;
                break;
            }
        }
        if (found || GetTickCount64() >= deadline) {
            break;
        }
        Sleep(150);
    }

    if (!wasRunning) {
        m_discovery.Stop(); // the copy above survives it
    }
    return found;
}

void CChromecastTarget::SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg)
{
    m_hNotifyWnd = hNotifyWnd;
    m_stateMsg = stateMsg;
}

void CChromecastTarget::NotifyState(CastTargetState state)
{
    m_lastNotifiedState = state;
    if (m_hNotifyWnd && m_stateMsg) {
        PostMessage(m_hNotifyWnd, m_stateMsg, static_cast<WPARAM>(state), (LPARAM)m_generation);
    }
}

bool CChromecastTarget::Connect(const CString& deviceId)
{
    if (m_casting || !EnsureMessageWindow()) {
        return false;
    }

    m_maxAudioChannels = 0; // a device picked out of a live scan carries no saved cap
    for (const CastDevice& dev : m_discovery.GetDevices()) {
        if (DeviceKey(dev) == deviceId) {
            return StartSession(dev, deviceId, DeviceDisplayName(dev));
        }
    }
    return false; // the device is gone from the discovery snapshot
}

bool CChromecastTarget::StartSession(const CastDevice& dev, const CString& deviceId, const CString& deviceName)
{
    CASTING_LOG(_T("cast: connecting to Chromecast \"%s\" at %s:%u (md=\"%s\")"),
                deviceName.GetString(), dev.ipAddress.GetString(), dev.port, dev.model.GetString());
    m_session.SetNotifyWindow(m_hMsgWnd, WM_CAST_SESSION);
    if (!m_session.Start(dev)) {
        CASTING_LOG(_T("cast: the connection to \"%s\" could not be opened"), deviceName.GetString());
        return false;
    }
    if (!m_server.IsRunning() && !m_server.Start()) {
        CASTING_LOG(_T("cast: the local media server would not start on port %u"),
                    CCastMediaServer::preferredPort);
        m_session.Stop();
        return false;
    }
    m_server.SetAllowedPeer(CStringA(dev.ipAddress));
    CASTING_LOG(_T("server: listening on port %u, only %s may fetch from it"),
                m_server.GetPort(), dev.ipAddress.GetString());
    m_deviceId = deviceId;
    m_deviceName = deviceName;
    m_model = dev.model; // drives the receiver rules the load is judged against
    m_casting = true;
    m_failed = false;
    m_failReason.Empty();
    // notifications of the previous session no longer apply
    m_generation = CastNextSessionGeneration();
    m_lastNotifiedState = CastTargetState::Connecting;
    return true;
}

bool CChromecastTarget::ConnectSaved(CastSavedDevice& saved, DWORD directMs, DWORD searchMs)
{
    if (m_casting || !EnsureMessageWindow()) {
        return false;
    }
    m_maxAudioChannels = saved.maxAudioChannels; // the user's cap for this device

    // The address the device was last seen at is asked first; only when it
    // does not answer, or answers as somebody else, is a full search worth
    // the wait. Either way what the device says about itself is written back,
    // so the saved entry follows the device instead of rotting.
    CastDevice dev;
    bool found = !saved.address.IsEmpty() && CCastDiscovery::ProbeAddress(saved.address, directMs, dev)
                 && (saved.id.IsEmpty() || dev.id.IsEmpty() || dev.id == saved.id);
    if (!found && !saved.address.IsEmpty() && saved.port != 0) {
        // The probe went unanswered, but that does not mean the device is gone.
        // A Chromecast answers a TCP connect far more reliably than an on-demand
        // mDNS probe, and some -- an Android TV with Cast built in -- answer the
        // probe only rarely even while casting works fine (issue #4133, where a
        // TCL set connected about one attempt in twenty, every failure being the
        // probe going unanswered rather than anything wrong with the cast). So
        // connect straight to the endpoint it was last seen on before spending a
        // full search on it. The saved details are used as they stand; if the
        // device has really moved, the connection fails quickly and the search
        // below still finds it by id.
        CastDevice direct;
        direct.ipAddress = saved.address;
        direct.port = saved.port;
        direct.id = saved.id;
        direct.friendlyName = saved.name;
        direct.model = saved.model;
        direct.capabilities = (saved.supportsVideo ? 0x01u : 0u) | (saved.supportsAudio ? 0x04u : 0u);
        CASTING_LOG(_T("cast: \"%s\" did not answer a probe; connecting directly to %s:%u"),
                    saved.DisplayName().GetString(), saved.address.GetString(), saved.port);
        if (StartSession(direct, saved.id, saved.DisplayName())) {
            return true;
        }
    }
    if (!found && !saved.id.IsEmpty()) {
        found = SearchById(saved.id, searchMs, dev);
    }
    if (!found) {
        return false;
    }

    saved.address = dev.ipAddress;
    saved.port = dev.port;
    if (!dev.friendlyName.IsEmpty()) {
        saved.name = dev.friendlyName;
    }
    if (!dev.model.IsEmpty()) {
        // an entry saved before the model was recorded learns it here
        saved.model = dev.model;
    }
    if (dev.capabilities) {
        saved.supportsVideo = dev.SupportsVideo();
        saved.supportsAudio = dev.SupportsAudio();
    }
    return StartSession(dev, saved.id, saved.DisplayName());
}

// Google's own receivers, and what each decodes beyond the codecs all of them
// do. Most specific name first: the plain "Chromecast" of the first three
// generations is a prefix of every later dongle's name.
static const struct {
    LPCTSTR model;
    bool hevc;
    bool av1;
} googleReceivers[] = {
    { _T("Google TV Streamer"), true,  true  },
    { _T("Chromecast Ultra"),   true,  false },
    { _T("Google TV"),          true,  false }, // Chromecast with Google TV, 4K and HD
    { _T("Chromecast HD"),      true,  false },
    { _T("Nest Hub"),           false, false }, // and the Google Home Hub it was named after
    { _T("Home Hub"),           false, false },
    { _T("Chromecast"),         false, false }, // 1st to 3rd generation
    // The speakers decode nothing with a picture in it, and are here only so
    // that they are recognized as Google's and held to the receiver's rules.
    { _T("Google Home"),        false, false },
    { _T("Google Nest"),        false, false },
};

// Whether the device is one of those, and so runs the default media receiver
// with the format list below. Anything else calling itself a cast device -- an
// Android TV box, a television with Chromecast built in -- runs the platform's
// own player, which plays a good deal more. A device that said nothing about
// itself is treated as one of Google's, that being the stricter of the two.
static bool IsGoogleReceiver(const CString& model)
{
    if (model.IsEmpty()) {
        return true;
    }
    for (const auto& entry : googleReceivers) {
        if (model.Find(entry.model) >= 0) {
            return true;
        }
    }
    return false;
}

// What the default media receiver plays, after
// https://developers.google.com/cast/docs/media. Video is the one thing that
// differs between Google's own devices, and only over HEVC and AV1 -- H.264,
// VP8 and VP9 are on every device that has a screen at all. The container list
// and the audio rules are the receiver's own, so a device that is not running
// it is not held to either: a "4K Android TV Box" plays HEVC and 10-bit H.264
// in Matroska and 5.1 AAC in MP4, all of which the receiver refuses.
//
// The table is deliberately small and deliberately optimistic: a model it
// does not recognize is allowed everything. A receiver that refuses the file
// answers LOAD_FAILED, which the session reports, so guessing wrong that way
// costs one visible failed attempt; guessing wrong the other way silently
// refuses content a device released after this table was written plays
// perfectly well, and nobody would ever find out why. Resolution and frame
// rate limits are not modelled for the same reason.
// The channels a downmix would deliver for this file on this device: the file's
// own count, pulled down by whatever the device cannot exceed. The downmix
// output is AAC, so a Google receiver takes only stereo of it, and Media
// Foundation's AAC encoder stops at 5.1; the user's per-device cap applies on
// top. A value below the file's own count means a downmix is called for.
static int DownmixTargetChannels(const CastMediaInfo& info, const CString& model, int maxAudioChannels)
{
    int channels = info.channels;
    if (maxAudioChannels > 0) {
        channels = std::min(channels, maxAudioChannels);
    }
    if (IsGoogleReceiver(model)) {
        channels = std::min(channels, 2); // the default media receiver decodes AAC in stereo
    }
    // The AAC encoder tops out at 5.1, so a reduction that stops higher than that
    // still lands at 5.1. This clamp only bites once something else has already
    // pulled the count below the file's own -- it never triggers a downmix by itself.
    if (channels < info.channels) {
        channels = std::min(channels, 6);
    }
    return channels;
}

bool CChromecastTarget::ReceiverCanPlay(const CString& path, const CastMediaInfo& info, const CString& model,
                                        CString* pRefusal, int maxAudioChannels)
{
    // Written into whatever the caller offered, so that every refusal below
    // says what it refused over rather than only that it did.
    CString ignored;
    CString& refusal = pRefusal ? *pRefusal : ignored;

    const bool googleReceiver = IsGoogleReceiver(model);
    const CStringA mime = CCastMediaServer::MimeForFile(path);
    // Matroska is the one container that separates the two lists in practice:
    // the receiver will not take it, every Android TV device plays it. AVI is
    // not offered either way, having been tried and failed on such a device.
    // A container the receiver will not take is still fine when we can remux it
    // to MP4 on the way out -- that is exactly what the transcode does for
    // Matroska and WebM (CastCanDownmix, which also demands the video be copyable
    // and the audio decodable, so a container we cannot actually remux still
    // fails here). The audio and video checks below still apply: the remux copies
    // the video untouched, so the device must play the codec regardless.
    const bool takesContainer = CCastMediaServer::IsCastableFile(path)
                                || (!googleReceiver && mime == "video/x-matroska")
                                || CastCanDownmix(path, info);
    if (!takesContainer) {
        if (mime == "application/octet-stream") {
            refusal = _T("casting knows no media type for a file of this kind");
        } else {
            refusal.Format(_T("the receiver does not play %hs containers"), mime.GetString());
        }
        return false;
    }

    // A user-set channel cap for this device. It is checked before the codec,
    // because the failure it guards against is the worst kind: the device takes
    // the file, plays the picture and drops the sound with no error. Until the
    // audio can be downmixed to fit, the honest answer is to refuse it here.
    if (maxAudioChannels > 0 && info.channels > maxAudioChannels) {
        if (!CastCanDownmix(path, info)) {
            refusal.Format(_T("the device is set to output at most %d audio channels; this file has %d"),
                           maxAudioChannels, info.channels);
            return false;
        }
        // else: playable by downmixing the audio at load time. Fall through to
        // the codec and video checks, which still refuse for the reasons a
        // downmix cannot fix (a codec the receiver will not take, MPEG-2 video).
    }

    switch (info.audio) {
        case CastMediaInfo::Audio::AC3:
        case CastMediaInfo::Audio::EAC3:
        case CastMediaInfo::Audio::DTS:
        case CastMediaInfo::Audio::TrueHD:
            // The receiver takes none of these directly -- AC-3/E-AC-3 only as
            // passthrough, which our sender does not do, DTS and TrueHD not even
            // that way. But they can be decoded here and re-encoded as AAC, so the
            // file still plays when it can be downmixed; only when it cannot (a
            // container we cannot remux, chiefly) is it refused.
            if (!CastCanDownmix(path, info)) {
                refusal.Format(_T("the receiver does not decode %s audio"), CastAudioCodecName(info.audio));
                return false;
            }
            break;
        case CastMediaInfo::Audio::WMA:
            refusal.Format(_T("the receiver does not decode %s audio"), CastAudioCodecName(info.audio));
            return false;
        case CastMediaInfo::Audio::AAC:
            // the receiver decodes stereo AAC and stops there; a device
            // running its own player is not bound by that, and one such was
            // seen playing 5.1 AAC in MP4
            if (googleReceiver && info.channels > 2 && !CastCanDownmix(path, info)) {
                refusal.Format(_T("the receiver decodes AAC in stereo only, this file has %d channels"),
                               info.channels);
                return false;
            }
            break;
        default:
            // FLAC, MP3, Opus, Vorbis, LPCM and anything unrecognized
            break;
    }

    if (info.video == CastMediaInfo::Video::MPEG2) {
        // MP2T is a container the receiver takes, MPEG-2 video is not
        refusal = _T("the receiver does not decode MPEG-2 video");
        return false;
    }
    if (info.video != CastMediaInfo::Video::HEVC && info.video != CastMediaInfo::Video::AV1) {
        return true;
    }

    for (const auto& entry : googleReceivers) {
        if (model.Find(entry.model) >= 0) {
            if (info.video == CastMediaInfo::Video::HEVC ? entry.hevc : entry.av1) {
                return true;
            }
            refusal.Format(_T("our model table says a \"%s\" does not decode %s"),
                           entry.model, CastVideoCodecName(info.video));
            return false;
        }
    }
    return true; // an unknown model is given the benefit of the doubt
}

bool CChromecastTarget::CanCastFileSaved(const CastSavedDevice& saved, const CString& path,
                                         const CastMediaInfo& info)
{
    CString refusal;
    const CStringA mime = CCastMediaServer::MimeForFile(path);
    bool ok = ignoreFormatSupport || ReceiverCanPlay(path, info, saved.model, &refusal, saved.maxAudioChannels);
    // A device without video, a speaker or a display-less Nest, only takes audio.
    if (ok && mime.Left(6).CompareNoCase("video/") == 0 && !saved.supportsVideo) {
        ok = false;
        refusal = _T("the device advertises no video output");
    }
    LogVerdict(saved.DisplayName(), saved.model, mime, ok, refusal);
    return ok;
}

// Both verdicts read the same way in the log, whether the device came out of a
// live discovery or out of the saved list. The file is named by whoever asked,
// so that only its name is ever written down and never the path to it.
void CChromecastTarget::LogVerdict(const CString& name, const CString& model, const CStringA& mime,
                                   bool ok, const CString& refusal)
{
    if (ok && ignoreFormatSupport) {
        CASTING_LOG(_T("cast: Chromecast \"%s\" (md=\"%s\") is handed this file unchecked, sent as %hs: ")
                    _T("CastIgnoreFormatSupport is on, so nothing was judged and the receiver decides"),
                    name.GetString(), model.GetString(), mime.GetString());
    } else if (ok) {
        CASTING_LOG(_T("cast: Chromecast \"%s\" (md=\"%s\") takes this file, sent as %hs"),
                    name.GetString(), model.GetString(), mime.GetString());
    } else {
        CASTING_LOG(_T("cast: Chromecast \"%s\" (md=\"%s\") refuses this file: %s"),
                    name.GetString(), model.GetString(), refusal.GetString());
    }
}

bool CChromecastTarget::CanCastFile(const CString& deviceId, const CString& path, const CastMediaInfo& info)
{
    for (const CastDevice& dev : m_discovery.GetDevices()) {
        if (DeviceKey(dev) == deviceId) {
            CString refusal;
            const CStringA mime = CCastMediaServer::MimeForFile(path);
            bool ok = ignoreFormatSupport || ReceiverCanPlay(path, info, dev.model, &refusal);
            // A device without video, a speaker or a display-less Nest, only
            // takes audio.
            if (ok && mime.Left(6).CompareNoCase("video/") == 0 && !dev.SupportsVideo()) {
                ok = false;
                refusal = _T("the device advertises no video output");
            }
            LogVerdict(DeviceDisplayName(dev), dev.model, mime, ok, refusal);
            return ok;
        }
    }
    CASTING_LOG(_T("cast: the Chromecast the file was meant for is no longer being announced"));
    return false; // the device is gone from the discovery snapshot
}

void CChromecastTarget::LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                                  const CastMediaInfo& info)
{
    if (!m_casting) {
        return;
    }

    // A device that cannot output the file's channel layout drops the sound
    // silently while it plays the picture. When that is what we would otherwise
    // hand it, and the audio can be reduced to fit, serve a downmixed copy so
    // there is sound: as an HLS stream the device starts playing while the
    // transcode still runs, or, when the stream cannot take the file, as a
    // complete copy the transcode finishes before the file is handed over.
    AbandonTranscode();
    m_failed = false; // a new load answers for itself
    m_failReason.Empty();
    CString servePath = filePath;
    const int target = DownmixTargetChannels(info, m_model, m_maxAudioChannels);
    // The file needs transcoding when it has more channels than the device will
    // output, or its audio is a codec the receiver does not decode at all
    // (AC-3/E-AC-3/DTS/TrueHD) -- re-encoding to AAC fixes both.
    const bool rejectedCodec = info.audio == CastMediaInfo::Audio::AC3
                               || info.audio == CastMediaInfo::Audio::EAC3
                               || info.audio == CastMediaInfo::Audio::DTS
                               || info.audio == CastMediaInfo::Audio::TrueHD;
    // The streaming (HLS) transcode writes a fragmented MP4 through Media
    // Foundation's sink, which needs MF to demux the source -- so it takes
    // MP4-family containers only. Matroska and WebM are demuxed by the LAV
    // splitter instead and go the complete-file route (CastLavRemuxToMp4).
    CString ext;
    const int dot = filePath.ReverseFind(_T('.'));
    if (dot >= 0) {
        ext = filePath.Mid(dot);
        ext.MakeLower();
    }
    const bool matroskaSource = ext == _T(".mkv") || ext == _T(".webm");
    if ((info.channels > target || rejectedCodec) && CastCanDownmix(filePath, info)) {
        if (info.video == CastMediaInfo::Video::H264 && !matroskaSource) {
            // The streaming transcode. Only H.264: the fragmented-MP4 sink it
            // writes through refuses HEVC, and a file with no picture has
            // nothing to show progressively -- both fall through to the
            // complete-file transcode below.
            m_hls = std::make_unique<CCastHlsStream>();
            if (m_hls->Start(filePath, info, target, durationSec, m_server, m_hMsgWnd, WM_CAST_HLS)) {
                m_mime = "application/vnd.apple.mpegurl";
                m_hlsPending = true;
                m_hlsReady = false;
                m_pendingTitle = title;
                m_pendingDuration = durationSec;
                m_pendingSeek = startSec >= 1.0 ? startSec : -1.0;
                // The LOAD goes out when the stream has published its init
                // segment and first segments -- not before, because the device
                // would ask for bytes that do not exist yet. Until then the
                // most that can be truthfully said is that media is being
                // made ready.
                m_loadPending = true;
                CASTING_LOG(_T("cast: the file's %d-channel audio is being downmixed to %d as it is ")
                            _T("streamed; the device is handed the file while it is still transcoded"),
                            info.channels, target);
                NotifyState(CastTargetState::Loading);
                return;
            }
            m_hls.reset();
            CASTING_LOG(_T("cast: the streaming transcode did not start; transcoding the whole file first instead"));
        } else if (matroskaSource) {
            CASTING_LOG(_T("cast: the streaming transcode needs an MP4-family container, and this is ")
                        _T("Matroska/WebM; the LAV splitter remuxes the whole file first instead"));
        } else if (info.video == CastMediaInfo::Video::HEVC) {
            CASTING_LOG(_T("cast: the streaming transcode takes H.264 only, and this file's video is ")
                        _T("HEVC; transcoding the whole file first instead"));
        } else {
            CASTING_LOG(_T("cast: the streaming transcode takes H.264 video, which this file does not ")
                        _T("have; transcoding the whole file first instead"));
        }
        // The whole-file transcode is minutes of work on a large HEVC file, so
        // it runs on a worker rather than freezing the UI thread this is called
        // on. The device is handed nothing until it finishes: the file is not
        // fragmented, so there is nothing to serve progressively. The load waits
        // (m_downmixPending, reported as Loading) for WM_CAST_DOWNMIX.
        TCHAR tempDir[MAX_PATH] = { 0 };
        GetTempPath(MAX_PATH, tempDir);
        CString temp;
        temp.Format(_T("%smpc-castdownmix-%u-%u.mp4"), tempDir, GetCurrentProcessId(), m_generation);
        m_downmixCancel = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        m_downmixSrc = filePath;
        m_downmixTemp = temp;
        m_downmixPending = true;
        m_loadPending = true;
        m_pendingTitle = title;
        m_pendingDuration = durationSec;
        m_pendingSeek = startSec >= 1.0 ? startSec : -1.0;
        const HWND hWnd = m_hMsgWnd;
        const HANDLE hCancel = m_downmixCancel;
        const int chans = info.channels;
        m_downmixThread = std::thread([this, filePath, info, target, temp, hCancel, hWnd, chans] {
            // The transcode's engines (Media Foundation, and LAV's DirectShow
            // graph) are COM; this thread is their apartment.
            HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            CString err;
            const bool ok = CastDownmixToMp4(filePath, info, target, temp, &err, hCancel);
            if (ok) {
                CASTING_LOG(_T("cast: the file's %d-channel audio was downmixed to %d for this device"),
                            chans, target);
            } else {
                m_downmixError = err;
            }
            if (SUCCEEDED(hrCo)) {
                CoUninitialize();
            }
            // Posted last: the handler on the UI thread reads m_downmixError,
            // written above, only after this post is observed.
            PostMessage(hWnd, WM_CAST_DOWNMIX, ok ? 1 : 0, 0);
        });
        CASTING_LOG(_T("cast: transcoding the whole file's %d-channel audio to %d before handing it over"),
                    info.channels, target);
        NotifyState(CastTargetState::Loading);
        return;
    }

    m_pendingTitle = title;
    m_pendingDuration = durationSec;
    m_pendingSeek = startSec >= 1.0 ? startSec : -1.0;
    ServeFileAndLoad(servePath);
}

// A Chromecast is told what it is playing over the cast protocol and never asks
// for DLNA content features, so the server is given none.
void CChromecastTarget::ServeFileAndLoad(const CString& servePath)
{
    m_mime = CCastMediaServer::MimeForFile(servePath);
    m_server.SetFile(servePath, m_mime);

    switch (m_session.GetState()) {
        case CastSessionState::Ready:
        case CastSessionState::Loading:
        case CastSessionState::Buffering:
        case CastSessionState::Playing:
        case CastSessionState::Paused:
        case CastSessionState::Stopped:
            SendLoad();
            break;
        default:
            // the session is still connecting; the LOAD is sent once it
            // reaches Ready
            m_loadPending = true;
            break;
    }
}

void CChromecastTarget::SendLoad()
{
    m_loadPending = false;
    // The streaming transcode serves its own manifest; everything else is the
    // single registered file.
    const CStringA url = m_hls
                         ? m_server.GetHlsURLForHost(CStringA(m_session.GetLocalAddress()))
                         : m_server.GetURLForHost(CStringA(m_session.GetLocalAddress()));
    if (url.IsEmpty()) {
        // no file registered or no local address: the session would sit there
        // casting nothing, so report the failure and let the UI tear it down
        TRACE(_T("ChromecastTarget: no media URL to load\n"));
        CASTING_LOG(_T("cast: nothing to hand the device -- no file is registered, or we have no ")
                    _T("address the device could reach us at"));
        m_failed = true;
        NotifyState(CastTargetState::Failed);
        return;
    }
    CASTING_LOG(_T("cast: handing the device %hs as %hs, %.1f s"),
                CCastMediaServer::MaskURLToken(url).GetString(), m_mime.GetString(), m_pendingDuration);
    m_session.Load(CString(url), CString(m_mime), m_pendingDuration, m_pendingTitle);
}

void CChromecastTarget::Play()
{
    m_session.Play();
}

void CChromecastTarget::Pause()
{
    m_session.Pause();
}

void CChromecastTarget::Seek(double seconds)
{
    m_session.Seek(ClampHlsSeek(seconds));
}

// A seek ahead of what the streaming transcode has produced parks the segment
// request until the transcode reaches it, and the receiver stops waiting long
// before a far-ahead segment lands. Keep seeks just behind what exists
// instead; once the transcode finishes the clamp stops biting.
double CChromecastTarget::ClampHlsSeek(double seconds) const
{
    if (m_hls && m_hls->IsRunning()) {
        const double behind = std::max(0.0, m_hls->ProducedSeconds() - 2.0);
        if (seconds > behind) {
            CASTING_LOG(_T("cast: a seek to %.1f s is held to %.1f s, as far as the transcode has got"),
                        seconds, behind);
            seconds = behind;
        }
    }
    return seconds;
}

void CChromecastTarget::SetVolume(double level, bool muted)
{
    m_session.SetVolume(level, muted);
}

// Cancels whatever transcode is behind the file being served -- the streaming
// one, whose raw file it deletes once its worker has joined, and the
// complete-file one, whose copy goes with it -- and leaves nothing behind for
// the next load to trip over.
void CChromecastTarget::AbandonTranscode()
{
    if (m_hls) {
        m_hls->Abort();
        m_hls.reset();
        // The worker posted its last notification before it exited, so
        // anything of its still queued is discarded here: it would otherwise
        // be handled as if the next stream had sent it.
        if (m_hMsgWnd) {
            MSG msg;
            while (PeekMessage(&msg, m_hMsgWnd, WM_CAST_HLS, WM_CAST_HLS, PM_REMOVE)) {
                // nothing to do with it
            }
        }
    }
    m_hlsPending = false;
    m_hlsReady = false;
    // The whole-file transcode worker: signal it, wait for it to notice and
    // return, then close the event and discard any completion it posted before
    // it saw the cancel (it would otherwise be handled as the next load's).
    if (m_downmixCancel) {
        SetEvent(m_downmixCancel);
    }
    if (m_downmixThread.joinable()) {
        m_downmixThread.join();
    }
    if (m_downmixCancel) {
        CloseHandle(m_downmixCancel);
        m_downmixCancel = nullptr;
    }
    if (m_hMsgWnd) {
        MSG msg;
        while (PeekMessage(&msg, m_hMsgWnd, WM_CAST_DOWNMIX, WM_CAST_DOWNMIX, PM_REMOVE)) {
            // nothing to do with it
        }
    }
    m_downmixPending = false;
    m_downmixSrc.Empty();
    m_downmixError.Empty();
    if (!m_downmixTemp.IsEmpty()) {
        DeleteFile(m_downmixTemp);
        m_downmixTemp.Empty();
    }
}

void CChromecastTarget::StopCasting()
{
    if (m_casting) {
        CASTING_LOG(_T("cast: stopping the session on \"%s\""), m_deviceName.GetString());
    }
    // Stop() only closes the virtual connections, which leaves our media on
    // the receiver, so the media STOP is sent first and given a short window
    // to go out - the queued command would never run if we joined right away.
    m_session.StopMediaAndWait(STOP_MEDIA_TIMEOUT_MS);
    m_session.Stop();
    // The server stops before the transcode is abandoned: its file-range parts
    // read the raw fragmented MP4, and only once the server is gone is its
    // handle of it closed, so the transcode's temp file can actually be
    // deleted.
    m_server.EndHls();
    m_server.ClearFile();
    m_server.ClearAllowedPeer();
    m_server.Stop();
    AbandonTranscode();
    m_casting = false;
    m_failed = false;
    m_failReason.Empty();
    m_loadPending = false;
    m_pendingSeek = -1.0;
    m_deviceId.Empty();
    m_deviceName.Empty();
    m_lastNotifiedState = CastTargetState::Idle;
}

CastTargetState CChromecastTarget::SimplifyState(CastSessionState state)
{
    switch (state) {
        case CastSessionState::Disconnected:
            return CastTargetState::Idle;
        case CastSessionState::Authenticating:
        case CastSessionState::Connecting:
        case CastSessionState::Connected:
        case CastSessionState::Launching:
        case CastSessionState::Ready:
            return CastTargetState::Connecting;
        case CastSessionState::Loading:
            return CastTargetState::Loading;
        case CastSessionState::Buffering:
            return CastTargetState::Buffering;
        case CastSessionState::Playing:
            return CastTargetState::Playing;
        case CastSessionState::Paused:
            return CastTargetState::Paused;
        case CastSessionState::Stopping:
        case CastSessionState::Stopped:
            return CastTargetState::Ended;
        case CastSessionState::TakenOver:
            return CastTargetState::TakenOver;
        case CastSessionState::LoadFailed:
        case CastSessionState::Dead:
        default:
            return CastTargetState::Failed;
    }
}

CastTargetState CChromecastTarget::GetState() const
{
    if (!m_casting) {
        return CastTargetState::Idle;
    }
    if (m_failed) {
        return CastTargetState::Failed;
    }
    // While the load waits on a transcode -- the streaming one's first
    // segments, or the whole-file one finishing -- the session itself still
    // sits in Ready, which would read as Connecting; the honest state is that
    // media is being made ready for it.
    if (m_hlsPending || m_downmixPending) {
        return CastTargetState::Loading;
    }
    return SimplifyState(m_session.GetState());
}

CString CChromecastTarget::GetFailureReason() const
{
    return m_failReason;
}

void CChromecastTarget::OnSessionStateChanged()
{
    if (!m_casting) {
        return;
    }

    const CastSessionState state = m_session.GetState();

    // A streaming transcode holds the LOAD back until its first segments
    // exist, so the session reaching Ready is not by itself enough.
    if (state == CastSessionState::Ready && m_loadPending && (!m_hlsPending || m_hlsReady)) {
        SendLoad();
    } else if (m_pendingSeek >= 0.0
               && (state == CastSessionState::Playing || state == CastSessionState::Paused
                   || state == CastSessionState::Buffering)) {
        // the first non-IDLE media status has arrived, so the session knows
        // the mediaSessionId and the initial seek can go out
        m_session.Seek(ClampHlsSeek(m_pendingSeek));
        m_pendingSeek = -1.0;
    }

    const CastTargetState simplified = SimplifyState(state);
    if (simplified != m_lastNotifiedState) {
        NotifyState(simplified);
    }
}

// The streaming transcode reports its milestones as messages, because they
// decide when the load can happen at all: the LOAD goes out once the first
// segments exist, a stream that gave up fails the session, and a completed
// stream changes nothing -- by then the device is playing it.
void CChromecastTarget::OnHlsEvent(int notify)
{
    if (!m_hls) {
        return; // left over from a stream that was already abandoned
    }
    switch (notify) {
        case CCastHlsStream::NotifyReady:
            m_hlsReady = true;
            m_hlsPending = false;
            switch (m_session.GetState()) {
                case CastSessionState::Ready:
                case CastSessionState::Loading:
                case CastSessionState::Buffering:
                case CastSessionState::Playing:
                case CastSessionState::Paused:
                case CastSessionState::Stopped:
                    SendLoad();
                    break;
                default:
                    m_loadPending = true; // sent when the session reaches Ready
                    break;
            }
            break;
        case CCastHlsStream::NotifyFailed:
            m_hlsPending = false;
            m_failReason = m_hls->FailureReason();
            m_failed = true;
            CASTING_LOG(_T("cast: the streaming transcode failed: %s"), m_failReason.GetString());
            NotifyState(CastTargetState::Failed);
            break;
        case CCastHlsStream::NotifyComplete:
            CASTING_LOG(_T("cast: the whole file has been transcoded; every segment is served"));
            break;
        default:
            break;
    }
}

// The whole-file transcode worker has finished. On success the device is handed
// the downmixed copy; on failure it is handed the original (it plays the
// picture and drops the surround, which still beats casting nothing). Either
// way the LOAD, held back while the transcode ran, now goes out.
void CChromecastTarget::OnDownmixDone(bool ok)
{
    if (!m_downmixPending) {
        return; // the worker was abandoned; its output is cleaned up elsewhere
    }
    m_downmixPending = false;
    if (m_downmixThread.joinable()) {
        m_downmixThread.join();
    }
    if (!m_casting) {
        return;
    }

    CString servePath;
    if (ok) {
        servePath = m_downmixTemp;
    } else {
        CASTING_LOG(_T("cast: could not downmix (%s); handing the device the file as it is"),
                    m_downmixError.GetString());
        if (!m_downmixTemp.IsEmpty()) {
            DeleteFile(m_downmixTemp);
            m_downmixTemp.Empty();
        }
        servePath = m_downmixSrc;
    }
    ServeFileAndLoad(servePath);
}
