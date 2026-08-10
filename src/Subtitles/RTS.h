/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2015, 2017 see Authors.txt
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

#include <list>
#include <memory>
#include "STS.h"
#include "Rasterizer.h"
#include "../SubPic/SubPicProviderImpl.h"
#include "RenderingCache.h"
#include "../../include/mpc-hc_config.h"

class Effect;
struct CTextDims;
struct CPolygonPath {
    CAtlArray<BYTE> typesOrg;
    CAtlArray<CPoint> pointsOrg;
    CSize size;
};

struct CAlphaMask;

struct alpha_mask_deleter {
    explicit alpha_mask_deleter(std::list<CAlphaMask>& alphaMaskPool)
        : m_alphaMaskPool(alphaMaskPool) {
    }

    void operator()(CAlphaMask* ptr) const noexcept;

    std::list<CAlphaMask>& m_alphaMaskPool;
};

struct CAlphaMask final : std::unique_ptr<BYTE[]> {
    CAlphaMask() = delete;
    CAlphaMask(CAlphaMask&&) = default;
    CAlphaMask(const CAlphaMask&) = delete;
    CAlphaMask& operator=(const CAlphaMask&) = delete;

    size_t m_size;

    explicit CAlphaMask(size_t size)
        : std::unique_ptr<BYTE[]>(DEBUG_NEW BYTE[size])
        , m_size(size) {
    }

    static std::shared_ptr<CAlphaMask> Alloc(std::list<CAlphaMask>& alphaMaskPool, size_t size) {
        for (auto it = alphaMaskPool.begin(); it != alphaMaskPool.end(); ++it) {
            auto& am = *it;
            if (am.m_size >= size) {
                auto ret = std::shared_ptr<CAlphaMask>(DEBUG_NEW CAlphaMask(std::move(am)), alpha_mask_deleter(alphaMaskPool));
                alphaMaskPool.erase(it);
                return std::move(ret);
            }
        }
        return std::shared_ptr<CAlphaMask>(DEBUG_NEW CAlphaMask(size), alpha_mask_deleter(alphaMaskPool));
    }
};

typedef std::shared_ptr<CPolygonPath> CPolygonPathSharedPtr;
struct SSATag;
typedef std::shared_ptr<CAtlList<SSATag>> SSATagsList;
typedef std::shared_ptr<CAlphaMask> CAlphaMaskSharedPtr;

typedef CRenderingCache<CTextDimsKey, CTextDims, CKeyTraits<CTextDimsKey>> CTextDimsCache;
typedef CRenderingCache<CPolygonPathKey, CPolygonPathSharedPtr, CKeyTraits<CPolygonPathKey>> CPolygonCache;
typedef CRenderingCache<CStringW, SSATagsList, CStringElementTraits<CStringW>> CSSATagsCache;
typedef CRenderingCache<CEllipseKey, CEllipseSharedPtr, CKeyTraits<CEllipseKey>> CEllipseCache;
typedef CRenderingCache<COutlineKey, COutlineDataSharedPtr, CKeyTraits<COutlineKey>> COutlineCache;
typedef CRenderingCache<COverlayKey, COverlayDataSharedPtr, CKeyTraits<COverlayKey>> COverlayCache;
typedef CRenderingCache<CClipperKey, CAlphaMaskSharedPtr, CKeyTraits<CClipperKey>> CAlphaMaskCache;

struct RenderingCaches {
    CTextDimsCache textDimsCache;
    CPolygonCache polygonCache;
    CSSATagsCache SSATagsCache;
    CEllipseCache ellipseCache;
    COutlineCache outlineCache;
    COverlayCache overlayCache;
    // Be careful about the order alphaMaskCache need to be destroyed before alphaMaskPool.
    std::list<CAlphaMask> alphaMaskPool;
    CAlphaMaskCache alphaMaskCache;

    RenderingCaches()
        : textDimsCache(2048)
        , polygonCache(2048)
        , SSATagsCache(2048)
        , ellipseCache(64)
        , outlineCache(128)
        , overlayCache(128)
        , alphaMaskCache(128) {}
};

class CMyFont : public CFont
{
public:
    int m_ascent, m_descent;

    CMyFont(const STSStyle& style);
};

struct CTextDims {
    int ascent, descent;
    int width;
};

class CPolygon;
class CLineBG;

class CWord : public Rasterizer
{
    friend class CLineBG;

    bool m_fDrawn;
    CPoint m_p;

    void Transform(CPoint org);

    void Transform_C(const CPoint& org);
    void Transform_SSE2(const CPoint& org);
    void Transform_quick_C(const CPoint& org);
    void Transform_quick_SSE2(const CPoint& org);
    bool CreateOpaqueBox();

protected:
    RenderingCaches& m_renderingCaches;

    double m_scalex, m_scaley;
    CStringW m_str;

    virtual bool CreatePath() = 0;

public:
    bool m_fWhiteSpaceChar, m_fLineBreak;

    STSStyle m_style;

    CPolygon* m_pOpaqueBox;

    int m_ktype, m_kstart, m_kend;

    int m_width, m_ascent, m_descent;

    // str[0] = 0 -> m_fLineBreak = true (in this case we only need and use the height of m_font from the whole class)
    CWord(const STSStyle& style, CStringW str, int ktype, int kstart, int kend, double scalex, double scaley,
          RenderingCaches& renderingCaches);
    CWord(RenderingCaches& renderingCaches);
    virtual ~CWord();

    virtual CWord* Copy() = 0;
    virtual bool Append(CWord* w);

    void Paint(const CPoint& p, const CPoint& org);

    friend class COutlineKey;
};

class CRenderedTextSubtitle;
class CText : public CWord
{
protected:
    virtual bool CreatePath();
    CRenderedTextSubtitle* m_RTS;
public:
    CText(const STSStyle& style, CStringW str, int ktype, int kstart, int kend, double scalex, double scaley,
          RenderingCaches& renderingCaches);
    CText(RenderingCaches& renderingCaches);

    virtual CWord* Copy();
    virtual bool Append(CWord* w);
    void SetRts(CRenderedTextSubtitle* RTS) { m_RTS = RTS; };
};

class CLine;
class CLineBG : public CText
{
public:
    CLineBG(RenderingCaches& renderingCaches);
    static std::shared_ptr<CLineBG> CLineBGFactory(CLine const* line, RenderingCaches& renderingCaches);
    CRect PaintLineShadow(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha);
    CRect PaintLineOutline(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha);
    virtual CLineBG* Copy() { return nullptr; }; //not used
};

class CPolygon : public CWord
{
    bool GetPOINT(LPCWSTR& str, POINT& point) const;
    bool ParseStr();

protected:
    int m_baseline;

    CPolygonPathSharedPtr m_pPolygonPath;

    virtual bool CreatePath();

public:
    CPolygon(const STSStyle& style, CStringW str, int ktype, int kstart, int kend, double scalex, double scaley, int baseline,
             RenderingCaches& renderingCaches);
    CPolygon(CPolygon&); // can't use a const reference because we need to use CAtlArray::Copy which expects a non-const reference
    virtual ~CPolygon();

    virtual CWord* Copy();
    virtual bool Append(CWord* w);
};

class Effect
{
public:
    enum eftype type = {};
    int param[9] = {};
    int t[4] = {};

    bool operator==(const Effect& rhs) const {
        return type == rhs.type
               && !memcmp(param, rhs.param, sizeof(param))
               && !memcmp(t, rhs.t, sizeof(t));
    }
};

class CClipper : public CPolygon
{
private:
    CWord* Copy();
    virtual bool Append(CWord* w);

public:
    CClipper(CStringW str, const CSize& size, double scalex, double scaley, bool inverse, const CPoint& cpOffset,
             RenderingCaches& renderingCaches);

    void SetEffect(const Effect& effect, int effectType) {
        m_effectType = effectType;
        m_effect = effect;
    }

    CAlphaMaskSharedPtr GetAlphaMask(const std::shared_ptr<CClipper>& clipper);

    ULONG Hash() const {
        ULONG hash = CStringElementTraits<CString>::Hash(m_str);
        hash += hash << 5;
        hash += m_inverse;
        hash += hash << 5;
        hash += m_effectType;
        hash += hash << 5;
        hash += m_size.cx;
        hash += hash << 5;
        hash += m_size.cy;
        hash += hash << 5;
        hash += int(m_scalex * 1e6);
        hash += hash << 5;
        hash += int(m_scaley * 1e6);
        for (const auto& param : m_effect.t) {
            hash += hash << 5;
            hash += param;
        }
        for (const auto& type : m_effect.t) {
            hash += hash << 5;
            hash += type;
        }
        return hash;
    }

    bool operator==(const CClipper& rhs) const {
        return m_str == rhs.m_str
               && std::abs(m_scalex - rhs.m_scalex) < 1e-6
               && std::abs(m_scaley - rhs.m_scaley) < 1e-6
               && m_size == rhs.m_size
               && m_inverse == rhs.m_inverse
               && m_effectType == rhs.m_effectType
               && m_effect == rhs.m_effect;
    }

private:
    const CSize m_size;
    const bool m_inverse;
    const CPoint m_cpOffset;
    CAlphaMaskSharedPtr m_pAlphaMask;
    Effect m_effect;
    int m_effectType;
};

using CClipperSharedPtr = std::shared_ptr<CClipper>;

class CLine : public CAtlList<CWord*>
{
public:
    int m_width   = 0;
    int m_ascent  = 0;
    int m_descent = 0;
    int m_borderX = 0;
    int m_borderY = 0;
    int m_linePadding = 0;

    virtual ~CLine();

    void Compact();

    CRect PaintShadow(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha);
    CRect PaintOutline(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha);
    CRect PaintBody(SubPicDesc& spd, CRect& clipRect, BYTE* pAlphaMask, CPoint p, CPoint org, int time, int alpha);
};

enum SSATagCmd {
    // /!\ Keep those four grouped together in that order
    SSA_1c,
    SSA_2c,
    SSA_3c,
    SSA_4c,
    // /!\ Keep those four grouped together in that order
    SSA_1a,
    SSA_2a,
    SSA_3a,
    SSA_4a,
    SSA_alpha,
    SSA_an,
    SSA_a,
    SSA_blur,
    SSA_bord,
    SSA_be,
    SSA_b,
    SSA_clip,
    SSA_c,
    SSA_fade,
    SSA_fe,
    SSA_fn,
    SSA_frx,
    SSA_fry,
    SSA_frz,
    SSA_fax,
    SSA_fay,
    SSA_fr,
    SSA_fscx,
    SSA_fscy,
    SSA_fsc,
    SSA_fsp,
    SSA_fs,
    SSA_iclip,
    SSA_i,
    SSA_kt,
    SSA_kf,
    SSA_ko,
    SSA_k,
    SSA_K,
    SSA_move,
    SSA_org,
    SSA_pbo,
    SSA_pos,
    SSA_p,
    SSA_q,
    SSA_r,
    SSA_shad,
    SSA_s,
    SSA_t,
    SSA_u,
    SSA_xbord,
    SSA_xshad,
    SSA_ybord,
    SSA_yshad,
    SSA_unknown
};

#define SSA_CMD_MIN_LENGTH 1
#define SSA_CMD_MAX_LENGTH 5

struct SSATag {
    SSATagCmd cmd;
    CAtlArray<CStringW, CStringElementTraits<CStringW>> params;
    CAtlArray<int> paramsInt;
    CAtlArray<double> paramsReal;
    SSATagsList subTagsList;

    SSATag() : cmd(SSA_unknown) {};

    SSATag(const SSATag& tag)
        : cmd(tag.cmd)
        , params()
        , paramsInt()
        , paramsReal()
        , subTagsList(tag.subTagsList) {
        params.Copy(tag.params);
        paramsInt.Copy(tag.paramsInt);
        paramsReal.Copy(tag.paramsReal);
    }
};

enum eftype {
    EF_MOVE = 0,    // {\move(x1=param[0], y1=param[1], x2=param[2], y2=param[3], t1=t[0], t2=t[1])} or {\pos(x=param[0], y=param[1])}
    EF_ORG,         // {\org(x=param[0], y=param[1])}
    EF_FADE,        // {\fade(a1=param[0], a2=param[1], a3=param[2], t1=t[0], t2=t[1], t3=t[2], t4=t[3])} or {\fad(t1=t[1], t2=t[2])
    EF_BANNER,      // Banner;delay=param[0][;lefttoright=param[1];fadeawaywidth=param[2]]
    EF_SCROLL       // Scroll up/down=param[3];top=param[0];bottom=param[1];delay=param[2][;fadeawayheight=param[4]]
};

#define EF_NUMBEROFEFFECTS 5

class CSubtitle : public CAtlList<CLine*>
{
    RenderingCaches& m_renderingCaches;

    int GetFullWidth();
    int GetFullLineWidth(POSITION pos);
    int GetWrapWidth(POSITION pos, int maxwidth);
    CLine* GetNextLine(POSITION& pos, int maxwidth);

public:
    int m_scrAlignment;
    int m_wrapStyle;
    bool m_fAnimated;
    bool m_bIsAnimated;
    STSStyle::RelativeTo m_relativeTo;

    Effect* m_effects[EF_NUMBEROFEFFECTS];

    CAtlList<CWord*> m_words;

    CClipperSharedPtr m_pClipper;

    CRect m_rect, m_clip;
    int m_topborder, m_bottomborder;
    bool m_clipInverse;

    double m_target_scale_x, m_target_scale_y;
    double m_script_scale_x, m_script_scale_y;
    double m_total_scale_x,  m_total_scale_y;

    bool m_allowLinePadding;

public:
    CSubtitle(RenderingCaches& renderingCaches);
    virtual ~CSubtitle();
    virtual void Empty();
    void EmptyEffects();

    void CreateClippers(CSize size);

    void MakeLines(CSize size, const CRect& marginRect);
};

class CScreenLayoutAllocator
{
    struct SubRect {
        CRect r;
        int segment, entry, layer;
    };

    CAtlList<SubRect> m_subrects;

public:
    /*virtual*/
    void Empty();

    void AdvanceToSegment(int segment, const CAtlArray<int>& sa);
    CRect AllocRect(const CSubtitle* s, int segment, int entry, int layer, int collisions);
};

class __declspec(uuid("537DCACA-2812-4a4f-B2C6-1A34C17ADEB0"))
    CRenderedTextSubtitle : public CSimpleTextSubtitle, public CSubPicProviderImpl, public ISubStream
{
    static CAtlMap<CStringW, SSATagCmd, CStringElementTraits<CStringW>> s_SSATagCmds;
    CAtlMap<int, CSubtitle*> m_subtitleCache;

    RenderingCaches m_renderingCaches;
    CCritSec renderLock;

    CScreenLayoutAllocator m_sla;

    CSize m_size;
    CRect m_vidrect;

    // temp variables, used when parsing the script
    int m_time, m_delay;
    int m_animStart, m_animEnd;
    double m_animAccel;
    int m_ktype, m_kstart, m_kend;
    int m_nPolygon;
    int m_polygonBaselineOffset;
    bool m_bOverridePlacement;
    bool m_bUseFreeType;
    CSize m_overridePlacement;
    bool m_bTopAlignedPlacement; // secondary subtitle track: forced top-anchored placement

    void ParseEffect(CSubtitle* sub, CString str);
    void ParseString(CSubtitle* sub, CStringW str, STSStyle& style);
    void ParsePolygon(CSubtitle* sub, CStringW str, STSStyle& style);
    bool ParseSSATag(SSATagsList& tagsList, const CStringW& str);
    bool CreateSubFromSSATag(CSubtitle* sub, const SSATagsList& tagsList, STSStyle& style, STSStyle& org, bool fAnimate = false);
    bool ParseHtmlTag(CSubtitle* sub, CStringW str, STSStyle& style, const STSStyle& org);

    double CalcAnimation(double dst, double src, bool fAnimate);

    CSubtitle* GetSubtitle(int entry);

protected:
    virtual void OnChanged();

public:
    CRenderedTextSubtitle(CCritSec* pLock);
    virtual ~CRenderedTextSubtitle();

    virtual void Copy(CSimpleTextSubtitle& sts);
    virtual void Empty();

    // call to signal this RTS to ignore any of the styles and apply the given override style
    void SetOverride(bool bOverrideDefault, bool bOverrideAll, const STSStyle& styleOverride);
 
    void SetUseFreeType(bool useFreeType) { m_bUseFreeType = useFreeType; }
    bool GetUseFreeType() { return m_bUseFreeType; }

    void SetAlignment(bool bOverridePlacement, LONG lHorPos, LONG lVerPos, bool bTopAligned = false) {
        m_bOverridePlacement = bOverridePlacement;
        m_overridePlacement.SetSize(lHorPos, lVerPos);
        m_bTopAlignedPlacement = bTopAligned;
    }

public:
    bool Init(CSize size, const CRect& vidrect); // will call Deinit()
    void Deinit();
    CString GetPath();

    bool m_webvtt_allow_clear;
    //FT cache for RTS
    FTLibraryData m_ftLibrary;
    //end FT cache

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ISubPicProvider
    STDMETHODIMP_(POSITION) GetStartPosition(REFERENCE_TIME rt, double fps);
    STDMETHODIMP_(POSITION) GetNext(POSITION pos);
    STDMETHODIMP_(REFERENCE_TIME) GetStart(POSITION pos, double fps);
    STDMETHODIMP_(REFERENCE_TIME) GetStop(POSITION pos, double fps);
    STDMETHODIMP_(bool) IsAnimated(POSITION pos);
    STDMETHODIMP Render(SubPicDesc& spd, REFERENCE_TIME rt, double fps, RECT& bbox);

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID);

    // ISubStream
    STDMETHODIMP_(int) GetStreamCount();
    STDMETHODIMP GetStreamInfo(int i, WCHAR** ppName, LCID* pLCID);
    STDMETHODIMP_(int) GetStream();
    STDMETHODIMP SetStream(int iStream);
    STDMETHODIMP Reload();
    STDMETHODIMP SetSourceTargetInfo(CString yuvMatrix, int targetBlackLevel, int targetWhiteLevel);
    void SetSubtitleTypeFromGUID(GUID subtype);
};
