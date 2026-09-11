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
#include "MkvMetadata.h"

#include <algorithm>
#include <limits>

namespace {

// Keep untrusted tags from causing excessive allocation or parsing work. The
// aggregate text limit applies to the whole Tags element, not each string.
constexpr ULONGLONG MAX_TAG_TEXT_SIZE = 64 * 1024;
constexpr ULONGLONG MAX_TOTAL_TAG_TEXT_SIZE = 1024 * 1024;
constexpr unsigned int MAX_TAG_DEPTH = 32;
constexpr size_t MAX_SIMPLE_TAGS = 4096;
constexpr DWORD READ_CACHE_SIZE = 4096;
constexpr size_t MAX_INDEX_POSITIONS = 4096;
constexpr size_t MAX_EBML_ELEMENTS = 65536;
constexpr size_t MAX_CACHE_FILLS = 4096;
constexpr size_t MAX_FALLBACK_CACHE_FILLS = 256;

constexpr ULONG EBML_ID_SEGMENT = 0x18538067;
constexpr ULONG EBML_ID_SEEKHEAD = 0x114D9B74;
constexpr ULONG EBML_ID_SEEK = 0x4DBB;
constexpr ULONG EBML_ID_SEEK_ID = 0x53AB;
constexpr ULONG EBML_ID_SEEK_POSITION = 0x53AC;
constexpr ULONG EBML_ID_TAGS = 0x1254C367;
constexpr ULONG EBML_ID_CLUSTER = 0x1F43B675;
constexpr ULONG EBML_ID_INFO = 0x1549A966;
constexpr ULONG EBML_ID_TAG = 0x7373;
constexpr ULONG EBML_ID_TARGETS = 0x63C0;
constexpr ULONG EBML_ID_TAG_TRACK_UID = 0x63C5;
constexpr ULONG EBML_ID_TAG_EDITION_UID = 0x63C9;
constexpr ULONG EBML_ID_TAG_CHAPTER_UID = 0x63C4;
constexpr ULONG EBML_ID_TAG_ATTACHMENT_UID = 0x63C6;
constexpr ULONG EBML_ID_TARGET_TYPE_VALUE = 0x68CA;
constexpr ULONG EBML_ID_SIMPLE_TAG = 0x67C8;
constexpr ULONG EBML_ID_TAG_NAME = 0x45A3;
constexpr ULONG EBML_ID_TAG_STRING = 0x4487;
constexpr ULONG EBML_ID_TAG_LANGUAGE = 0x447A;
constexpr ULONG EBML_ID_TAG_LANGUAGE_BCP47 = 0x447B;
constexpr ULONG EBML_ID_TAG_DEFAULT = 0x4484;
constexpr ULONG EBML_ID_TITLE = 0x7BA9;

struct EbmlElement {
    ULONG id = 0;
    ULONGLONG dataPosition = 0;
    ULONGLONG dataSize = 0;
    bool unknownSize = false;
};

struct SimpleTag {
    CString name;
    CString value;
    CString language;
    CString languageBcp47;
    bool isDefault = true;
    std::vector<SimpleTag> children;
};

struct TagReadLimits {
    ULONGLONG totalTextSize = 0;
    size_t simpleTags = 0;
    int titlePriority = -1;
    int releaseDatePriority = -1;

    bool AddText(ULONGLONG size)
    {
        if (size > MAX_TAG_TEXT_SIZE || size > MAX_TOTAL_TAG_TEXT_SIZE - totalTextSize) {
            return false;
        }

        totalTextSize += size;
        return true;
    }

    bool AddSimpleTag()
    {
        return simpleTags++ < MAX_SIMPLE_TAGS;
    }

};

class CEbmlFile
{
private:
    HANDLE m_file = INVALID_HANDLE_VALUE;
    ULONGLONG m_size = 0;
    ULONGLONG m_position = 0;
    ULONGLONG m_cachePosition = 0;
    DWORD m_cacheSize = 0;
    std::vector<BYTE> m_cache;
    const std::atomic_bool* m_cancelled = nullptr;
    HANDLE m_readEvent = nullptr;
    size_t m_elementsRead = 0;
    size_t m_cacheFills = 0;
    const ULONGLONG m_startTime;

    bool IsCancelled() const
    {
        return m_cancelled && m_cancelled->load();
    }

    bool IsTimeBudgetExhausted() const
    {
        return GetTickCount64() - m_startTime >= CMkvMetadataReader::READ_TIMEOUT_MS;
    }

    bool ShouldStop() const
    {
        if (IsCancelled()) {
            return true;
        }
        if (IsTimeBudgetExhausted()) {
            return true;
        }
        return false;
    }

    bool ReadAt(ULONGLONG position, void* buffer, DWORD size, DWORD& bytesRead)
    {
        bytesRead = 0;
        if (ShouldStop() || !size || position > m_size || size > m_size - position || !ResetEvent(m_readEvent)) {
            return false;
        }

        // Every physical read is overlapped. Apart from user cancellation, the
        // absolute deadline below also interrupts a stalled SMB redirector.
        OVERLAPPED overlapped = {};
        overlapped.Offset = static_cast<DWORD>(position);
        overlapped.OffsetHigh = static_cast<DWORD>(position >> 32);
        overlapped.hEvent = m_readEvent;
        if (!ReadFile(m_file, buffer, size, &bytesRead, &overlapped)) {
            if (GetLastError() != ERROR_IO_PENDING) {
                return false;
            }

            DWORD waitResult;
            do {
                waitResult = WaitForSingleObject(m_readEvent, 50);
                if (ShouldStop()) {
                    CancelIoEx(m_file, &overlapped);
                    break;
                }
            } while (waitResult == WAIT_TIMEOUT);

            if (waitResult != WAIT_OBJECT_0 && !ShouldStop()) {
                return false;
            }
            if (!GetOverlappedResult(m_file, &overlapped, &bytesRead, TRUE)) {
                return false;
            }
        }

        return !ShouldStop() && bytesRead != 0;
    }

    bool FillCache()
    {
        if (ShouldStop() || m_position >= m_size || m_cacheFills >= MAX_CACHE_FILLS) {
            return false;
        }

        m_cacheFills++;
        DWORD bytesRead = 0;
        const DWORD bytesToRead = static_cast<DWORD>(std::min<ULONGLONG>(m_cache.size(), m_size - m_position));
        if (!ReadAt(m_position, m_cache.data(), bytesToRead, bytesRead)) {
            return false;
        }

        m_cachePosition = m_position;
        m_cacheSize = bytesRead;
        return true;
    }

public:
    explicit CEbmlFile(LPCTSTR path, const std::atomic_bool* cancelled, ULONGLONG startTime)
        : m_cache(READ_CACHE_SIZE)
        , m_cancelled(cancelled)
        , m_startTime(startTime ? startTime : GetTickCount64())
    {
        if (IsCancelled()) {
            return;
        }

        m_file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_OVERLAPPED, nullptr);
        if (m_file != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER size;
            if (GetFileSizeEx(m_file, &size) && size.QuadPart >= 0) {
                m_size = size.QuadPart;
            } else {
                CloseHandle(m_file);
                m_file = INVALID_HANDLE_VALUE;
            }
        }
        if (m_file != INVALID_HANDLE_VALUE) {
            m_readEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            if (!m_readEvent) {
                CloseHandle(m_file);
                m_file = INVALID_HANDLE_VALUE;
            }
        }
    }

    ~CEbmlFile()
    {
        if (m_file != INVALID_HANDLE_VALUE) {
            CloseHandle(m_file);
        }
        if (m_readEvent) {
            CloseHandle(m_readEvent);
        }
    }

    bool IsOpen() const
    {
        return m_file != INVALID_HANDLE_VALUE;
    }

    ULONGLONG GetSize() const
    {
        return m_size;
    }

    bool AddElement()
    {
        return m_elementsRead++ < MAX_EBML_ELEMENTS;
    }

    size_t GetCacheFills() const
    {
        return m_cacheFills;
    }

    bool Seek(ULONGLONG position)
    {
        if (ShouldStop() || position > m_size) {
            return false;
        }

        m_position = position;
        return true;
    }

    bool GetPosition(ULONGLONG& position) const
    {
        position = m_position;
        return !ShouldStop();
    }

    bool Read(void* buffer, DWORD size)
    {
        BYTE* destination = static_cast<BYTE*>(buffer);
        while (size) {
            if (ShouldStop() || m_position >= m_size) {
                return false;
            }

            if (m_position < m_cachePosition || m_position - m_cachePosition >= m_cacheSize) {
                if (!FillCache()) {
                    return false;
                }
            }

            const DWORD offset = static_cast<DWORD>(m_position - m_cachePosition);
            const DWORD available = m_cacheSize - offset;
            const DWORD count = std::min(size, available);
            memcpy(destination, m_cache.data() + offset, count);
            destination += count;
            m_position += count;
            size -= count;
        }

        return true;
    }
};

bool AddSize(ULONGLONG position, ULONGLONG size, ULONGLONG limit, ULONGLONG& end)
{
    if (size > limit || position > limit - size) {
        return false;
    }

    end = position + size;
    return true;
}

bool ReadByte(CEbmlFile& file, BYTE& value)
{
    return file.Read(&value, sizeof(value));
}

bool ReadId(CEbmlFile& file, ULONG& id)
{
    BYTE first;
    if (!ReadByte(file, first)) {
        return false;
    }

    unsigned int length = 1;
    BYTE mask = 0x80;
    while (!(first & mask) && length < 5) {
        mask >>= 1;
        length++;
    }
    if (length > 4) {
        return false;
    }

    id = first;
    for (unsigned int i = 1; i < length; i++) {
        BYTE byte;
        if (!ReadByte(file, byte)) {
            return false;
        }
        id = (id << 8) | byte;
    }

    return true;
}

bool ReadSize(CEbmlFile& file, ULONGLONG& size, bool& unknown)
{
    BYTE first;
    if (!ReadByte(file, first)) {
        return false;
    }

    unsigned int length = 1;
    BYTE mask = 0x80;
    while (!(first & mask) && length < 9) {
        mask >>= 1;
        length++;
    }
    if (length > 8) {
        return false;
    }

    size = first & (mask - 1);
    for (unsigned int i = 1; i < length; i++) {
        BYTE byte;
        if (!ReadByte(file, byte)) {
            return false;
        }
        size = (size << 8) | byte;
    }

    unknown = size == ((1ULL << (7 * length)) - 1);
    return true;
}

bool ReadElement(CEbmlFile& file, EbmlElement& element)
{
    if (!file.AddElement() || !ReadId(file, element.id) || !ReadSize(file, element.dataSize, element.unknownSize)) {
        return false;
    }

    return file.GetPosition(element.dataPosition);
}

bool GetElementEnd(const EbmlElement& element, ULONGLONG parentEnd, ULONGLONG& end)
{
    if (element.unknownSize) {
        return false;
    }

    return AddSize(element.dataPosition, element.dataSize, parentEnd, end);
}

bool ReadUtf8(CEbmlFile& file, ULONGLONG size, CString& value, TagReadLimits* limits = nullptr)
{
    if (size > MAX_TAG_TEXT_SIZE || size > std::numeric_limits<DWORD>::max() || (limits && !limits->AddText(size))) {
        return false;
    }

    std::vector<char> buffer(static_cast<size_t>(size) + 1, '\0');
    if (size && !file.Read(buffer.data(), static_cast<DWORD>(size))) {
        return false;
    }

    value = UTF8To16(CStringA(buffer.data(), static_cast<int>(size)));
    value.TrimRight(L'\0');
    return true;
}

bool ReadUnsigned(CEbmlFile& file, ULONGLONG size, ULONGLONG& value)
{
    if (size > sizeof(value)) {
        return false;
    }

    value = 0;
    for (ULONGLONG i = 0; i < size; i++) {
        BYTE byte;
        if (!ReadByte(file, byte)) {
            return false;
        }
        value = (value << 8) | byte;
    }

    return true;
}

bool ReadSimpleTag(CEbmlFile& file, ULONGLONG end, unsigned int depth, SimpleTag& tag, TagReadLimits& limits)
{
    if (depth >= MAX_TAG_DEPTH || !limits.AddSimpleTag()) {
        return false;
    }

    while (true) {
        ULONGLONG position;
        if (!file.GetPosition(position) || position == end) {
            return position == end;
        }
        if (position > end) {
            return false;
        }

        EbmlElement element;
        ULONGLONG elementEnd;
        if (!ReadElement(file, element) || !GetElementEnd(element, end, elementEnd)) {
            return false;
        }
        switch (element.id) {
            case EBML_ID_TAG_NAME:
                if (!ReadUtf8(file, element.dataSize, tag.name, &limits)) {
                    return false;
                }
                break;
            case EBML_ID_TAG_STRING:
                if (!ReadUtf8(file, element.dataSize, tag.value, &limits)) {
                    return false;
                }
                break;
            case EBML_ID_TAG_LANGUAGE:
                if (!ReadUtf8(file, element.dataSize, tag.language, &limits)) {
                    return false;
                }
                break;
            case EBML_ID_TAG_LANGUAGE_BCP47:
                if (!ReadUtf8(file, element.dataSize, tag.languageBcp47, &limits)) {
                    return false;
                }
                break;
            case EBML_ID_TAG_DEFAULT: {
                ULONGLONG value;
                if (!ReadUnsigned(file, element.dataSize, value)) {
                    return false;
                }
                tag.isDefault = value != 0;
                break;
            }
            case EBML_ID_SIMPLE_TAG: {
                SimpleTag child;
                if (!ReadSimpleTag(file, elementEnd, depth + 1, child, limits)) {
                    return false;
                }
                tag.children.emplace_back(std::move(child));
                break;
            }
            default:
                if (!file.Seek(elementEnd)) {
                    return false;
                }
                break;
        }
    }
}

struct TagTarget {
    bool hasSpecificTarget = false;
    ULONGLONG typeValue = 50;
};

bool ReadTargets(CEbmlFile& file, ULONGLONG end, TagTarget& target, TagReadLimits& limits)
{
    while (true) {
        ULONGLONG position;
        if (!file.GetPosition(position) || position == end) {
            return position == end;
        }
        if (position > end) {
            return false;
        }

        EbmlElement element;
        ULONGLONG elementEnd;
        if (!ReadElement(file, element) || !GetElementEnd(element, end, elementEnd)) {
            return false;
        }
        switch (element.id) {
            case EBML_ID_TAG_TRACK_UID:
            case EBML_ID_TAG_EDITION_UID:
            case EBML_ID_TAG_CHAPTER_UID:
            case EBML_ID_TAG_ATTACHMENT_UID: {
                ULONGLONG value;
                if (!ReadUnsigned(file, element.dataSize, value)) {
                    return false;
                }
                // A UID of zero targets all instances of that kind, but it is
                // still not a Segment-level tag.
                target.hasSpecificTarget = true;
                break;
            }
            case EBML_ID_TARGET_TYPE_VALUE:
                if (element.dataSize && !ReadUnsigned(file, element.dataSize, target.typeValue)) {
                    return false;
                }
                break;
            default:
                if (!file.Seek(elementEnd)) {
                    return false;
                }
                break;
        }
    }
}

void AddCreator(MkvMetadata& metadata, UINT nameResource, const CString& value)
{
    if (!value.IsEmpty()) {
        metadata.creators.push_back({ ResStr(nameResource), value });
    }
}

void AddCastMember(MkvMetadata& metadata, const SimpleTag& tag)
{
    if (tag.value.IsEmpty()) {
        return;
    }

    CString roles;
    for (const auto& child : tag.children) {
        if (!child.name.CompareNoCase(L"CHARACTER") && !child.value.IsEmpty()) {
            if (!roles.IsEmpty()) {
                roles += L"; ";
            }
            roles += child.value;
        }
    }

    metadata.cast.push_back({ tag.value, roles });
}

int GetTagPriority(const SimpleTag& tag)
{
    // Matroska's TagDefault marks the preferred translation. Untagged or
    // "und" text is the least surprising fallback when no default is set.
    const CString& language = tag.languageBcp47.IsEmpty() ? tag.language : tag.languageBcp47;
    return (tag.isDefault ? 2 : 0) + (language.IsEmpty() || !language.CompareNoCase(L"und") ? 1 : 0);
}

void AddTag(MkvMetadata& metadata, const SimpleTag& tag, TagReadLimits& limits)
{
    if (tag.name.IsEmpty() || tag.value.IsEmpty()) {
        return;
    }

    if (!tag.name.CompareNoCase(L"TITLE")) {
        const int priority = GetTagPriority(tag);
        if (metadata.title.IsEmpty() || priority > limits.titlePriority) {
            metadata.title = tag.value;
            limits.titlePriority = priority;
        }
    } else if (!tag.name.CompareNoCase(L"DATE_RELEASED")) {
        const int priority = GetTagPriority(tag);
        if (metadata.releaseDate.IsEmpty() || priority > limits.releaseDatePriority) {
            metadata.releaseDate = tag.value;
            limits.releaseDatePriority = priority;
        }
    } else if (!tag.name.CompareNoCase(L"ACTOR")) {
        AddCastMember(metadata, tag);
    } else if (!tag.name.CompareNoCase(L"DIRECTOR")) {
        AddCreator(metadata, IDS_MKVMETADATA_DIRECTOR, tag.value);
    } else if (!tag.name.CompareNoCase(L"SCREENPLAY_BY")) {
        AddCreator(metadata, IDS_MKVMETADATA_SCREENPLAY, tag.value);
    } else if (!tag.name.CompareNoCase(L"WRITTEN_BY") || !tag.name.CompareNoCase(L"WRITER")) {
        AddCreator(metadata, IDS_MKVMETADATA_WRITER, tag.value);
    } else if (!tag.name.CompareNoCase(L"PRODUCER")) {
        AddCreator(metadata, IDS_MKVMETADATA_PRODUCER, tag.value);
    } else if (!tag.name.CompareNoCase(L"EXECUTIVE_PRODUCER")) {
        AddCreator(metadata, IDS_MKVMETADATA_EXECUTIVE_PRODUCER, tag.value);
    } else if (!tag.name.CompareNoCase(L"DIRECTOR_OF_PHOTOGRAPHY")) {
        AddCreator(metadata, IDS_MKVMETADATA_DIRECTOR_OF_PHOTOGRAPHY, tag.value);
    } else if (!tag.name.CompareNoCase(L"COMPOSER") || !tag.name.CompareNoCase(L"MUSIC_BY")) {
        AddCreator(metadata, IDS_MKVMETADATA_MUSIC, tag.value);
    } else if (!tag.name.CompareNoCase(L"EDITED_BY") || !tag.name.CompareNoCase(L"EDITOR")) {
        AddCreator(metadata, IDS_MKVMETADATA_EDITOR, tag.value);
    } else if (!tag.name.CompareNoCase(L"ART_DIRECTOR")) {
        AddCreator(metadata, IDS_MKVMETADATA_ART_DIRECTOR, tag.value);
    } else if (!tag.name.CompareNoCase(L"PRODUCTION_DESIGNER")) {
        AddCreator(metadata, IDS_MKVMETADATA_PRODUCTION_DESIGNER, tag.value);
    } else if (!tag.name.CompareNoCase(L"COSTUME_DESIGNER")) {
        AddCreator(metadata, IDS_MKVMETADATA_COSTUME_DESIGNER, tag.value);
    } else if (!tag.name.CompareNoCase(L"SOUND_ENGINEER")) {
        AddCreator(metadata, IDS_MKVMETADATA_SOUND_ENGINEER, tag.value);
    } else if (!tag.name.CompareNoCase(L"CASTING_DIRECTOR")) {
        AddCreator(metadata, IDS_MKVMETADATA_CASTING_DIRECTOR, tag.value);
    } else if (!tag.name.CompareNoCase(L"ASSISTANT_DIRECTOR")) {
        AddCreator(metadata, IDS_MKVMETADATA_ASSISTANT_DIRECTOR, tag.value);
    }
}

bool ReadTag(CEbmlFile& file, ULONGLONG end, MkvMetadata& metadata, TagReadLimits& limits)
{
    TagTarget target;
    std::vector<SimpleTag> tags;

    while (true) {
        ULONGLONG position;
        if (!file.GetPosition(position) || position == end) {
            break;
        }
        if (position > end) {
            return false;
        }

        EbmlElement element;
        ULONGLONG elementEnd;
        if (!ReadElement(file, element) || !GetElementEnd(element, end, elementEnd)) {
            return false;
        }
        if (element.id == EBML_ID_TARGETS) {
            if (!ReadTargets(file, elementEnd, target, limits)) {
                return false;
            }
        } else if (element.id == EBML_ID_SIMPLE_TAG) {
            SimpleTag tag;
            if (!ReadSimpleTag(file, elementEnd, 0, tag, limits)) {
                return false;
            }
            tags.emplace_back(std::move(tag));
        } else if (!file.Seek(elementEnd)) {
            return false;
        }
    }

    // The same tag names can describe a collection, season, chapter, or track.
    // This page presents movie/episode metadata only.
    if (target.hasSpecificTarget || target.typeValue != 50) {
        return true;
    }

    for (const auto& tag : tags) {
        AddTag(metadata, tag, limits);
    }

    return true;
}

bool ReadTags(CEbmlFile& file, ULONGLONG end, MkvMetadata& metadata, TagReadLimits& limits)
{
    bool foundTag = false;
    while (true) {
        ULONGLONG position;
        if (!file.GetPosition(position) || position == end) {
            // Tag is mandatory in Tags. Rejecting an empty element also keeps
            // arbitrary payload bytes from being treated as metadata.
            return position == end && foundTag;
        }
        if (position > end) {
            return false;
        }

        EbmlElement element;
        ULONGLONG elementEnd;
        if (!ReadElement(file, element) || !GetElementEnd(element, end, elementEnd)) {
            return false;
        }
        if (element.id == EBML_ID_TAG) {
            if (!ReadTag(file, elementEnd, metadata, limits)) {
                return false;
            }
            foundTag = true;
        } else if (!file.Seek(elementEnd)) {
            return false;
        }
    }
}

bool AddPosition(std::vector<ULONGLONG>& positions, ULONGLONG position)
{
    if (std::find(positions.cbegin(), positions.cend(), position) == positions.cend()) {
        if (positions.size() == MAX_INDEX_POSITIONS) {
            return false;
        }
        positions.emplace_back(position);
    }

    return true;
}

bool ReadSeekHead(CEbmlFile& file, ULONGLONG end, ULONGLONG segmentDataPosition, ULONGLONG segmentEnd,
                  std::vector<ULONGLONG>& tagsPositions, std::vector<ULONGLONG>& infoPositions,
                  std::vector<ULONGLONG>& seekHeadPositions)
{
    while (true) {
        ULONGLONG position;
        if (!file.GetPosition(position) || position == end) {
            return position == end;
        }
        if (position > end) {
            return false;
        }

        EbmlElement seek;
        ULONGLONG seekEnd;
        if (!ReadElement(file, seek) || !GetElementEnd(seek, end, seekEnd)) {
            return false;
        }

        if (seek.id != EBML_ID_SEEK) {
            if (!file.Seek(seekEnd)) {
                return false;
            }
            continue;
        }

        ULONG targetId = 0;
        ULONGLONG targetPosition = 0;
        bool hasTargetPosition = false;
        while (true) {
            if (!file.GetPosition(position) || position == seekEnd) {
                break;
            }
            if (position > seekEnd) {
                return false;
            }

            EbmlElement element;
            ULONGLONG elementEnd;
            if (!ReadElement(file, element) || !GetElementEnd(element, seekEnd, elementEnd)) {
                return false;
            }

            if (element.id == EBML_ID_SEEK_ID && element.dataSize <= sizeof(targetId)) {
                ULONGLONG value;
                if (!ReadUnsigned(file, element.dataSize, value)) {
                    return false;
                }
                targetId = static_cast<ULONG>(value);
            } else if (element.id == EBML_ID_SEEK_POSITION) {
                if (!ReadUnsigned(file, element.dataSize, targetPosition)) {
                    return false;
                }
                hasTargetPosition = true;
            } else if (!file.Seek(elementEnd)) {
                return false;
            }
        }

        if (hasTargetPosition) {
            ULONGLONG candidate;
            if (AddSize(segmentDataPosition, targetPosition, segmentEnd, candidate)) {
                if (targetId == EBML_ID_TAGS) {
                    if (!AddPosition(tagsPositions, candidate)) {
                        return false;
                    }
                } else if (targetId == EBML_ID_INFO) {
                    if (!AddPosition(infoPositions, candidate)) {
                        return false;
                    }
                } else if (targetId == EBML_ID_SEEKHEAD) {
                    if (!AddPosition(seekHeadPositions, candidate)) {
                        return false;
                    }
                }
            }
        }
    }
}

bool ReadTagsAt(CEbmlFile& file, ULONGLONG position, ULONGLONG segmentEnd, MkvMetadata& metadata, TagReadLimits& limits)
{
    if (!file.Seek(position)) {
        return false;
    }

    EbmlElement element;
    ULONGLONG end;
    MkvMetadata tagsMetadata = metadata;
    TagReadLimits tagsLimits = limits;
    if (!ReadElement(file, element)
            || element.id != EBML_ID_TAGS
            || !GetElementEnd(element, segmentEnd, end)
            || !ReadTags(file, end, tagsMetadata, tagsLimits)) {
        return false;
    }

    // SeekHead positions are untrusted. Commit their metadata only after the
    // entire Tags element was parsed successfully, never after a partial or
    // timed-out read. Keeping the existing values in the copy also preserves
    // preference ordering across multiple Tags elements.
    metadata = std::move(tagsMetadata);
    limits = tagsLimits;
    return true;
}

bool ReadInfo(CEbmlFile& file, ULONGLONG end, CString& title)
{
    while (true) {
        ULONGLONG position;
        if (!file.GetPosition(position) || position == end) {
            return position == end;
        }
        if (position > end) {
            return false;
        }

        EbmlElement element;
        ULONGLONG elementEnd;
        if (!ReadElement(file, element) || !GetElementEnd(element, end, elementEnd)) {
            return false;
        }

        if (element.id == EBML_ID_TITLE && title.IsEmpty()) {
            if (!ReadUtf8(file, element.dataSize, title)) {
                return false;
            }
        } else if (!file.Seek(elementEnd)) {
            return false;
        }
    }
}

bool ReadInfoAt(CEbmlFile& file, ULONGLONG position, ULONGLONG segmentEnd, CString& title)
{
    if (!file.Seek(position)) {
        return false;
    }

    EbmlElement element;
    ULONGLONG end;
    return ReadElement(file, element)
           && element.id == EBML_ID_INFO
           && GetElementEnd(element, segmentEnd, end)
           && ReadInfo(file, end, title);
}

bool ReadSeekHeadAt(CEbmlFile& file, ULONGLONG position, ULONGLONG segmentDataPosition, ULONGLONG segmentEnd,
                    std::vector<ULONGLONG>& tagsPositions, std::vector<ULONGLONG>& infoPositions,
                    std::vector<ULONGLONG>& seekHeadPositions)
{
    if (!file.Seek(position)) {
        return false;
    }

    EbmlElement element;
    ULONGLONG end;
    return ReadElement(file, element)
           && element.id == EBML_ID_SEEKHEAD
           && GetElementEnd(element, segmentEnd, end)
           && ReadSeekHead(file, end, segmentDataPosition, segmentEnd, tagsPositions, infoPositions, seekHeadPositions);
}

bool ReadIndexedTags(CEbmlFile& file, ULONGLONG segmentDataPosition, ULONGLONG segmentEnd, MkvMetadata& metadata,
                     CString& segmentTitle, TagReadLimits& limits, bool& foundTags)
{
    std::vector<ULONGLONG> tagsPositions;
    std::vector<ULONGLONG> infoPositions;
    std::vector<ULONGLONG> seekHeadPositions;

    // Top-level metadata and SeekHead normally precede media Clusters. This is
    // the fast path for local and network files: only metadata headers are read.
    while (true) {
        ULONGLONG position;
        if (!file.GetPosition(position) || position == segmentEnd) {
            break;
        }
        if (position > segmentEnd) {
            return false;
        }

        EbmlElement element;
        if (!ReadElement(file, element)) {
            return false;
        }
        if (element.id == EBML_ID_CLUSTER || element.unknownSize) {
            break;
        }

        ULONGLONG end;
        if (!GetElementEnd(element, segmentEnd, end)) {
            return false;
        }

        if (element.id == EBML_ID_TAGS) {
            if (!AddPosition(tagsPositions, position)) {
                return false;
            }
        } else if (element.id == EBML_ID_INFO) {
            if (!AddPosition(infoPositions, position)) {
                return false;
            }
        } else if (element.id == EBML_ID_SEEKHEAD) {
            if (!AddPosition(seekHeadPositions, position)) {
                return false;
            }
        }

        if (!file.Seek(end)) {
            return false;
        }
    }

    for (size_t index = 0; index < seekHeadPositions.size(); index++) {
        if (!ReadSeekHeadAt(file, seekHeadPositions[index], segmentDataPosition, segmentEnd, tagsPositions, infoPositions, seekHeadPositions)) {
            return false;
        }
    }

    for (const ULONGLONG position : infoPositions) {
        if (!ReadInfoAt(file, position, segmentEnd, segmentTitle)) {
            return false;
        }
    }

    for (const ULONGLONG position : tagsPositions) {
        if (!ReadTagsAt(file, position, segmentEnd, metadata, limits)) {
            return false;
        }
    }

    foundTags = !tagsPositions.empty();
    return true;
}

bool IsLevel1Element(ULONG id)
{
    switch (id) {
        case EBML_ID_SEEKHEAD:
        case 0x1549A966: // Info
        case 0x1654AE6B: // Tracks
        case EBML_ID_CLUSTER:
        case 0x1C53BB6B: // Cues
        case 0x1941A469: // Attachments
        case 0x1043A770: // Chapters
        case EBML_ID_TAGS:
            return true;
        default:
            return false;
    }
}

bool SkipUnknownSizedCluster(CEbmlFile& file, ULONGLONG segmentEnd, size_t initialCacheFills, bool& budgetExhausted)
{
    // An unknown-sized Cluster ends when the next level-1 element begins. Its
    // children are skipped by size, so block payloads are never transferred.
    while (true) {
        if (file.GetCacheFills() - initialCacheFills >= MAX_FALLBACK_CACHE_FILLS) {
            budgetExhausted = true;
            return true;
        }

        ULONGLONG position;
        if (!file.GetPosition(position) || position >= segmentEnd) {
            return position == segmentEnd;
        }

        EbmlElement element;
        if (!ReadElement(file, element)) {
            return false;
        }
        if (IsLevel1Element(element.id)) {
            return file.Seek(position);
        }
        if (element.unknownSize) {
            return false;
        }

        ULONGLONG end;
        if (!GetElementEnd(element, segmentEnd, end) || !file.Seek(end)) {
            return false;
        }
    }
}

enum class SegmentScanResult {
    Complete,
    BudgetExhausted,
    Error,
};

SegmentScanResult ScanSegmentForTags(CEbmlFile& file, ULONGLONG segmentDataPosition, ULONGLONG segmentEnd, MkvMetadata& metadata,
                                     CString& segmentTitle, TagReadLimits& limits)
{
    if (!file.Seek(segmentDataPosition)) {
        return SegmentScanResult::Error;
    }

    const size_t initialCacheFills = file.GetCacheFills();

    while (true) {
        // SeekHead is optional. Do not turn that into a full-file SMB probe:
        // the fallback only examines a bounded number of cache windows.
        if (file.GetCacheFills() - initialCacheFills >= MAX_FALLBACK_CACHE_FILLS) {
            return SegmentScanResult::BudgetExhausted;
        }

        ULONGLONG position;
        if (!file.GetPosition(position) || position == segmentEnd) {
            return position == segmentEnd ? SegmentScanResult::Complete : SegmentScanResult::Error;
        }
        if (position > segmentEnd) {
            return SegmentScanResult::Error;
        }

        EbmlElement element;
        if (!ReadElement(file, element)) {
            return SegmentScanResult::Error;
        }
        if (element.unknownSize) {
            bool budgetExhausted = false;
            if (element.id != EBML_ID_CLUSTER || !SkipUnknownSizedCluster(file, segmentEnd, initialCacheFills, budgetExhausted)) {
                return SegmentScanResult::Error;
            }
            if (budgetExhausted) {
                return SegmentScanResult::BudgetExhausted;
            }
            continue;
        }

        ULONGLONG end;
        if (!GetElementEnd(element, segmentEnd, end)) {
            return SegmentScanResult::Error;
        }
        if (element.id == EBML_ID_INFO) {
            if (!ReadInfo(file, end, segmentTitle)) {
                return SegmentScanResult::Error;
            }
        } else if (element.id == EBML_ID_TAGS) {
            if (!ReadTags(file, end, metadata, limits)) {
                return SegmentScanResult::Error;
            }
        } else if (!file.Seek(end)) {
            return SegmentScanResult::Error;
        }
    }
}

} // namespace

bool MkvMetadata::HasDisplayableData() const
{
    return !title.IsEmpty() || !releaseDate.IsEmpty() || !creators.empty() || !cast.empty();
}

bool CMkvMetadataReader::Read(LPCTSTR path, MkvMetadata& metadata, const std::atomic_bool* cancelled, ULONGLONG startTime)
{
    metadata = {};
    MkvMetadata parsedMetadata;
    CString segmentTitle;
    TagReadLimits limits;

    CString extension = path;
    int dot = extension.ReverseFind(L'.');
    if (dot < 0) {
        return false;
    }
    extension = extension.Mid(dot);
    if (extension.CompareNoCase(L".mkv")) {
        return false;
    }

    if (cancelled && cancelled->load()) {
        return false;
    }

    CEbmlFile file(path, cancelled, startTime);
    if (!file.IsOpen()) {
        return false;
    }

    ULONGLONG segmentEnd = file.GetSize();
    ULONGLONG segmentDataPosition = 0;
    bool foundSegment = false;
    while (true) {
        ULONGLONG position;
        if (!file.GetPosition(position) || position == file.GetSize()) {
            return false;
        }

        EbmlElement element;
        if (!ReadElement(file, element)) {
            return false;
        }

        ULONGLONG end = file.GetSize();
        if (!element.unknownSize && !GetElementEnd(element, file.GetSize(), end)) {
            return false;
        }

        if (element.id == EBML_ID_SEGMENT) {
            segmentDataPosition = element.dataPosition;
            segmentEnd = end;
            foundSegment = true;
            break;
        }
        if (!file.Seek(end)) {
            return false;
        }
    }

    if (!foundSegment) {
        return false;
    }

    bool foundIndexedTags = false;
    if (!ReadIndexedTags(file, segmentDataPosition, segmentEnd, parsedMetadata, segmentTitle, limits, foundIndexedTags)) {
        return false;
    }

    // A SeekHead is the common fast path. Without one, follow the segment's
    // level-1 EBML structure and seek over media payloads. This avoids
    // mistaking a Tags byte pattern inside a Cluster for real metadata.
    if (!foundIndexedTags) {
        const SegmentScanResult scanResult = ScanSegmentForTags(file, segmentDataPosition, segmentEnd, parsedMetadata, segmentTitle, limits);
        if (scanResult != SegmentScanResult::Complete) {
            // A bounded scan is deliberately not equivalent to "there are no
            // Tags". Do not publish a partial result.
            return false;
        }
    }

    if (parsedMetadata.title.IsEmpty()) {
        parsedMetadata.title = segmentTitle;
    }

    if (!parsedMetadata.HasDisplayableData()) {
        return false;
    }

    metadata = std::move(parsedMetadata);
    return true;
}
