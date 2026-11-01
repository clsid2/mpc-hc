/*
 * (C) 2013-2018 see Authors.txt
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

#include <atlcoll.h>
#include <bitset>
#include <memory>

// Overload for a value type that counts against SetMaxBytes.
template<typename V>
size_t CacheEntryBytes(const V&) { return 0; }

template<typename K, typename V, class KTraits = CElementTraits<K>, class VTraits = CElementTraits<V>>
class CRenderingCache : private CAtlMap<K, POSITION, KTraits>
{
private:
    size_t m_maxSize;
    struct CPositionValue {
        POSITION pos;
        V value;
    };
    CAtlList<CPositionValue> m_list;

    // Once SetMaxBytes is called, evicted keys leave a bit here. Too many of
    // them coming back means the cache is too small for the script, so it grows.
    typedef std::bitset<1 << 20> CEvictedKeys;
    std::unique_ptr<CEvictedKeys> m_evicted;
    size_t m_maxBytes = 0, m_bytes = 0, m_inserts = 0, m_reMisses = 0;

public:
    CRenderingCache(size_t maxSize) : m_maxSize(maxSize) {};

    void SetMaxBytes(size_t maxBytes) {
        m_maxBytes = maxBytes;
        if (!m_evicted) {
            m_evicted = std::make_unique<CEvictedKeys>();
        }
    }

    bool Lookup(typename KTraits::INARGTYPE key, _Out_ typename VTraits::OUTARGTYPE value) {
        POSITION pos = nullptr;
        bool bFound = __super::Lookup(key, pos);

        if (bFound) {
            m_list.MoveToHead(pos);
            value = m_list.GetHead().value;
        }

        return bFound;
    };

    POSITION SetAt(typename KTraits::INARGTYPE key, typename VTraits::INARGTYPE value) {
        POSITION pos = nullptr;
        bool bFound = __super::Lookup(key, pos);

        if (bFound) {
            m_list.MoveToHead(pos);
            CPositionValue& posVal = m_list.GetHead();
            pos = posVal.pos;
            m_bytes += CacheEntryBytes(value) - CacheEntryBytes(posVal.value);
            posVal.value = value;
        } else {
            if (m_evicted) {
                const size_t bit = KTraits::Hash(key) % m_evicted->size();
                m_reMisses += (*m_evicted)[bit];
                m_evicted->reset(bit);
                const bool bGrow = m_reMisses > m_maxSize / 8;
                if (bGrow) {
                    m_maxSize *= 2;
                }
                if (bGrow || ++m_inserts >= 8 * m_maxSize) {
                    m_inserts = m_reMisses = 0;
                    m_evicted->reset();
                }
            }
            pos = __super::SetAt(key, m_list.AddHead());
            CPositionValue& posVal = m_list.GetHead();
            posVal.pos = pos;
            posVal.value = value;
            m_bytes += CacheEntryBytes(value);
        }

        while (m_list.GetCount() > 1 && (m_list.GetCount() > m_maxSize || (m_maxBytes && m_bytes > m_maxBytes))) {
            const CPositionValue& tail = m_list.GetTail();
            if (m_evicted) {
                m_evicted->set(KTraits::Hash(__super::GetKeyAt(tail.pos)) % m_evicted->size());
            }
            m_bytes -= CacheEntryBytes(tail.value);
            __super::RemoveAtPos(tail.pos);
            m_list.RemoveTailNoReturn();
        }

        return pos;
    };

    void Clear() {
        m_list.RemoveAll();
        __super::RemoveAll();
        m_bytes = m_inserts = m_reMisses = 0;
        if (m_evicted) {
            m_evicted->reset();
        }
    }
};

template <class Key>
class CKeyTraits : public CElementTraits<Key>
{
public:
    static ULONG Hash(_In_ const Key& element) {
        return element.GetHash();
    };

    static bool CompareElements(_In_ const Key& element1, _In_ const Key& element2) {
        return (element1 == element2);
    };
};

class STSStyle;

class CTextDimsKey
{
private:
    ULONG m_hash;

protected:
    CString m_str;
    CAutoPtr<STSStyle> m_style;

public:
    CTextDimsKey(const CStringW& str, const STSStyle& style);
    CTextDimsKey(const CTextDimsKey& textDimsKey);

    ULONG GetHash() const { return m_hash; };

    void UpdateHash();

    bool operator==(const CTextDimsKey& textDimsKey) const;
};

class CPolygonPathKey
{
private:
    ULONG m_hash;

protected:
    CString m_str;
    double m_scalex, m_scaley;

public:
    CPolygonPathKey(const CStringW& str, double scalex, double scaley);
    CPolygonPathKey(const CPolygonPathKey& polygonPathKey);

    ULONG GetHash() const { return m_hash; };

    void UpdateHash();

    bool operator==(const CPolygonPathKey& polygonPathKey) const;
};

class CEllipseKey
{
private:
    ULONG m_hash;

protected:
    int m_rx, m_ry;

public:
    CEllipseKey(int rx, int ry)
        : m_hash(ULONG((rx << 16) | (ry & WORD_MAX)))
        , m_rx(rx)
        , m_ry(ry) {}

    ULONG GetHash() const { return m_hash; };

    bool operator==(const CEllipseKey& ellipseKey) const {
        return (m_rx == ellipseKey.m_rx && m_ry == ellipseKey.m_ry);
    }
};

class CFontKey
{
private:
    ULONG m_hash;

protected:
    LOGFONT m_lf; // zero-initialized before filling, so it can be compared bytewise

public:
    CFontKey(const LOGFONT& lf);

    ULONG GetHash() const { return m_hash; };

    void UpdateHash();

    bool operator==(const CFontKey& fontKey) const {
        return memcmp(&m_lf, &fontKey.m_lf, sizeof(m_lf)) == 0;
    }
};

class CWord;

class COutlineKey : public CTextDimsKey
{
private:
    ULONG m_hash;

protected:
    double m_scalex, m_scaley;
    CPoint m_org;

public:
    COutlineKey(const CWord* word, CPoint org);
    COutlineKey(const COutlineKey& outLineKey);

    ULONG GetHash() const { return m_hash; };

    void UpdateHash();

    bool operator==(const COutlineKey& outLineKey) const;
};

class COverlayKey : public COutlineKey
{
private:
    CPoint m_subp;
    ULONG m_hash;

public:
    COverlayKey(const CWord* word, CPoint p, CPoint org);
    COverlayKey(const COverlayKey& overlayKey);

    ULONG GetHash() const { return m_hash; };

    void UpdateHash();

    bool operator==(const COverlayKey& overlayKey) const;
};

class CClipper;

class CClipperKey
{
    ULONG m_hash;
    std::shared_ptr<CClipper> m_clipper;

public:
    CClipperKey(const std::shared_ptr<CClipper>& clipper);

    ULONG GetHash() const { return m_hash; };

    void UpdateHash();

    bool operator==(const CClipperKey& clipperKey) const;
};
