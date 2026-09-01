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
#include "CastTargetMulti.h"
#include "Logger.h"
#include <algorithm>
#include <vector>

// The prefix a device id carries, so that the protocol it belongs to travels
// with it and two protocols can never hand out the same id.
#define CHROMECAST_PREFIX _T("cc:")
#define DLNA_PREFIX       _T("dlna:")

namespace
{
    // A Chromecast is never asked what it plays, so what goes in the log is
    // our own table talking, and it says so. Every verdict comes out of the
    // function that makes the real decision, so the log cannot describe a rule
    // the player does not follow.
    void LogChromecastCapabilities(const CastTargetDevice& device)
    {
        CString offered, withheld;
        for (const CCastMediaServer::FileType& type : CCastMediaServer::KnownFileTypes()) {
            CString probe;
            probe.Format(_T("media.%hs"), type.ext); // nothing is opened; the name is only judged
            const bool needsScreen = CStringA(type.mime).Left(6).CompareNoCase("video/") == 0;
            const bool ok = CChromecastTarget::ReceiverCanPlay(probe, CastMediaInfo(), device.model)
                            && (!needsScreen || device.supportsVideo);
            (ok ? offered : withheld).AppendFormat(_T(" %hs"), type.ext);
        }

        // The codecs, each judged inside an MP4 so that only the codec itself
        // decides, and the audio ones with no video so they do the same.
        CString videos;
        for (CastMediaInfo::Video codec : { CastMediaInfo::Video::H264, CastMediaInfo::Video::HEVC,
                                            CastMediaInfo::Video::VP8, CastMediaInfo::Video::VP9,
                                            CastMediaInfo::Video::AV1, CastMediaInfo::Video::MPEG2 }) {
            CastMediaInfo info;
            info.video = codec;
            videos.AppendFormat(_T(" %s=%s"), CastVideoCodecName(codec),
                                CChromecastTarget::ReceiverCanPlay(_T("media.mp4"), info, device.model)
                                ? _T("yes") : _T("no"));
        }

        CString audios;
        for (CastMediaInfo::Audio codec : { CastMediaInfo::Audio::AAC, CastMediaInfo::Audio::MP3,
                                            CastMediaInfo::Audio::FLAC, CastMediaInfo::Audio::Opus,
                                            CastMediaInfo::Audio::Vorbis, CastMediaInfo::Audio::LPCM,
                                            CastMediaInfo::Audio::AC3, CastMediaInfo::Audio::EAC3,
                                            CastMediaInfo::Audio::DTS, CastMediaInfo::Audio::TrueHD,
                                            CastMediaInfo::Audio::WMA }) {
            CastMediaInfo info;
            info.audio = codec;
            info.channels = 2;
            audios.AppendFormat(_T(" %s=%s"), CastAudioCodecName(codec),
                                CChromecastTarget::ReceiverCanPlay(_T("media.mp4"), info, device.model)
                                ? _T("yes") : _T("no"));
            if (codec == CastMediaInfo::Audio::AAC) {
                info.channels = 6;
                audios.AppendFormat(_T(" AAC-6ch=%s"),
                                    CChromecastTarget::ReceiverCanPlay(_T("media.mp4"), info, device.model)
                                    ? _T("yes") : _T("no"));
            }
        }

        CASTING_LOG(_T("device: Chromecast \"%s\" (md=\"%s\"), video out %s, audio out %s"),
                    device.name.GetString(), device.model.GetString(),
                    device.supportsVideo ? _T("yes") : _T("no"),
                    device.supportsAudio ? _T("yes") : _T("no"));
        CASTING_LOG(_T("device:   containers offered:%s"),
                    offered.IsEmpty() ? _T(" none") : offered.GetString());
        CASTING_LOG(_T("device:   containers withheld:%s"),
                    withheld.IsEmpty() ? _T(" none") : withheld.GetString());
        CASTING_LOG(_T("device:   video:%s"), videos.GetString());
        CASTING_LOG(_T("device:   audio:%s"), audios.GetString());
        CASTING_LOG(_T("device:   the above is what our own model table concludes; a Chromecast is ")
                    _T("never asked what it can play, so a wrong guess here shows up as LOAD_FAILED"));
    }

    // A renderer, unlike a Chromecast, answers for itself. Its answer is
    // thousands of characters of protocol info, which nobody can read, so what
    // goes in the log is that answer applied to the file types casting knows.
    void LogDlnaCapabilities(const CastTargetDevice& device)
    {
        const CStringA sink(device.formats);
        CString accepted, refused;
        std::vector<CStringA> seen; // several extensions share one media type

        for (const CCastMediaServer::FileType& type : CCastMediaServer::KnownFileTypes()) {
            const CStringA mime(type.mime);
            if (std::find(seen.cbegin(), seen.cend(), mime) != seen.cend()) {
                continue;
            }
            seen.emplace_back(mime);
            (CDlnaTarget::AcceptsMime(sink, mime) ? accepted : refused).AppendFormat(_T(" %hs"), type.mime);
        }

        CASTING_LOG(_T("device: DLNA \"%s\" at %s, %s"), device.name.GetString(), device.address.GetString(),
                    sink.IsEmpty() ? _T("it answered no protocol info, so the common containers are assumed")
                    : _T("from its own GetProtocolInfo answer"));
        CASTING_LOG(_T("device:   accepts:%s"), accepted.IsEmpty() ? _T(" nothing") : accepted.GetString());
        CASTING_LOG(_T("device:   refuses:%s"), refused.IsEmpty() ? _T(" nothing") : refused.GetString());
    }
}

void CastLogDeviceCapabilities(const CastTargetDevice& device)
{
    if (!CASTING_LOGGING()) {
        return; // nothing above gets built while the casting log is off
    }
    if (device.protocol == CastProtocol::Chromecast) {
        LogChromecastCapabilities(device);
    } else {
        LogDlnaCapabilities(device);
    }
    if (CCastTarget::ignoreFormatSupport) {
        CASTING_LOG(_T("device:   none of the above is enforced: CastIgnoreFormatSupport is on, so every ")
                    _T("file is offered to this device and the device is what refuses it"));
    }
}

bool CCastTargetMulti::StartDiscovery()
{
    // both are started; one protocol failing must not hide the other
    const bool chromecast = m_chromecast.StartDiscovery();
    const bool dlna = m_dlna.StartDiscovery();
    return chromecast || dlna;
}

void CCastTargetMulti::StopDiscovery()
{
    m_chromecast.StopDiscovery();
    m_dlna.StopDiscovery();
}

void CCastTargetMulti::AppendDevices(CCastTarget& target, LPCTSTR prefix, std::vector<CastTargetDevice>& devices)
{
    for (CastTargetDevice& dev : target.GetDevices()) {
        dev.id = prefix + dev.id;
        devices.emplace_back(std::move(dev));
    }
}

std::vector<CastTargetDevice> CCastTargetMulti::GetDevices()
{
    // The names stay exactly as the devices advertise them: a device that
    // speaks both protocols is announced twice under one name, and telling
    // those two apart is the job of whatever displays them.
    std::vector<CastTargetDevice> devices;
    AppendDevices(m_chromecast, CHROMECAST_PREFIX, devices);
    AppendDevices(m_dlna, DLNA_PREFIX, devices);
    return devices;
}

bool CCastTargetMulti::ProbeAddress(CastProtocol protocol, const CString& address, UINT port,
                                    DWORD timeoutMs, CastTargetDevice& device)
{
    CCastTarget& target = protocol == CastProtocol::Chromecast
                          ? static_cast<CCastTarget&>(m_chromecast) : m_dlna;
    if (!target.ProbeAddress(protocol, address, port, timeoutMs, device)) {
        return false;
    }
    device.id = PrefixFor(protocol) + device.id;
    return true;
}

void CCastTargetMulti::SetNotifyWindow(HWND hNotifyWnd, UINT stateMsg)
{
    m_chromecast.SetNotifyWindow(hNotifyWnd, stateMsg);
    m_dlna.SetNotifyWindow(hNotifyWnd, stateMsg);
}

LPCTSTR CCastTargetMulti::PrefixFor(CastProtocol protocol)
{
    return protocol == CastProtocol::Chromecast ? CHROMECAST_PREFIX : DLNA_PREFIX;
}

CCastTarget* CCastTargetMulti::TargetFor(CastProtocol protocol, const CString& deviceId, CString& targetDeviceId)
{
    LPCTSTR prefix = PrefixFor(protocol);
    const int prefixLen = (int)_tcslen(prefix);
    targetDeviceId = deviceId.Left(prefixLen) == prefix ? deviceId.Mid(prefixLen) : deviceId;
    return protocol == CastProtocol::Chromecast ? static_cast<CCastTarget*>(&m_chromecast) : &m_dlna;
}

CCastTarget* CCastTargetMulti::TargetFor(const CString& deviceId, CString& targetDeviceId)
{
    const int chromecastPrefix = (int)_tcslen(CHROMECAST_PREFIX);
    if (deviceId.Left(chromecastPrefix) == CHROMECAST_PREFIX) {
        targetDeviceId = deviceId.Mid(chromecastPrefix);
        return &m_chromecast;
    }
    const int dlnaPrefix = (int)_tcslen(DLNA_PREFIX);
    if (deviceId.Left(dlnaPrefix) == DLNA_PREFIX) {
        targetDeviceId = deviceId.Mid(dlnaPrefix);
        return &m_dlna;
    }
    targetDeviceId.Empty();
    return nullptr;
}

bool CCastTargetMulti::Connect(const CString& deviceId)
{
    if (IsCasting()) {
        return false;
    }

    CString targetDeviceId;
    CCastTarget* pTarget = TargetFor(deviceId, targetDeviceId);
    if (!pTarget || !pTarget->Connect(targetDeviceId)) {
        return false;
    }
    m_pActive = pTarget;
    m_activeId = deviceId;
    return true;
}

bool CCastTargetMulti::ConnectSaved(CastSavedDevice& saved, DWORD directMs, DWORD searchMs)
{
    if (IsCasting()) {
        return false;
    }

    CastSavedDevice device(saved);
    CCastTarget* pTarget = TargetFor(saved.protocol, saved.id, device.id);
    if (!pTarget->ConnectSaved(device, directMs, searchMs)) {
        return false;
    }
    device.id = saved.id; // the caller keeps the id it knows the device by
    saved = device;       // ...and everything the device just said about itself

    m_pActive = pTarget;
    m_activeId = saved.id;
    return true;
}

bool CCastTargetMulti::CanCastFile(const CString& deviceId, const CString& path, const CastMediaInfo& info)
{
    CString targetDeviceId;
    CCastTarget* pTarget = TargetFor(deviceId, targetDeviceId);
    return pTarget && pTarget->CanCastFile(targetDeviceId, path, info);
}

bool CCastTargetMulti::CanCastFileSaved(const CastSavedDevice& saved, const CString& path,
                                        const CastMediaInfo& info)
{
    // Neither target needs the id here: what a saved device can play is
    // decided by what was recorded about the device, not by looking it up.
    return saved.protocol == CastProtocol::Chromecast ? m_chromecast.CanCastFileSaved(saved, path, info)
           : m_dlna.CanCastFileSaved(saved, path, info);
}

void CCastTargetMulti::LoadMedia(const CString& filePath, const CString& title, double durationSec, double startSec,
                                 const CastMediaInfo& info)
{
    if (m_pActive) {
        m_pActive->LoadMedia(filePath, title, durationSec, startSec, info);
    }
}

void CCastTargetMulti::Play()
{
    if (m_pActive) {
        m_pActive->Play();
    }
}

void CCastTargetMulti::Pause()
{
    if (m_pActive) {
        m_pActive->Pause();
    }
}

void CCastTargetMulti::Seek(double seconds)
{
    if (m_pActive) {
        m_pActive->Seek(seconds);
    }
}

void CCastTargetMulti::SetVolume(double level, bool muted)
{
    if (m_pActive) {
        m_pActive->SetVolume(level, muted);
    }
}

void CCastTargetMulti::StopCasting()
{
    if (m_pActive) {
        m_pActive->StopCasting();
        m_pActive = nullptr;
    }
    m_activeId.Empty();
}

CastTargetState CCastTargetMulti::GetState() const
{
    return m_pActive ? m_pActive->GetState() : CastTargetState::Idle;
}

CString CCastTargetMulti::GetFailureReason() const
{
    return m_pActive ? m_pActive->GetFailureReason() : CString();
}
