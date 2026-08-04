#pragma once
#include "ass/ass.h"
#include <string>
#include <streambuf>
#include "SubRendererSettings.h"
#include "SubtitleHelpers.h"
#include "../SubPic/SubPicProviderImpl.h"
#include "STSStyle.h"

struct ASS_LibraryDeleter {
    void operator()(ASS_Library* p) { if (p) ass_library_done(p); }
};

struct ASS_RendererDeleter {
    void operator()(ASS_Renderer* p) { if (p) ass_renderer_done(p); }
};

struct ASS_TrackDeleter {
    void operator()(ASS_Track* p) { if (p) ass_free_track(p); }
};

std::string ConsumeAttribute(const char** ppsz_subtitle, std::string& attribute_value);
ASS_Track* srt_read_file(ASS_Library* library, CStringW fname, const STSStyle& style, OpenTypeLang::HintStr openTypeLangHint);
ASS_Track* srt_read_data(ASS_Library* library, ASS_Track* track, std::istream &stream, const STSStyle& style, OpenTypeLang::HintStr openTypeLangHint);
void srt_header(char (&outBuffer)[1024], const STSStyle& style, OpenTypeLang::HintStr openTypeLangHint);
void ConvertCPToUTF8(int charset, std::string& codepage_str);
std::string GetTag(const char** line, bool b_closing);
bool IsClosed(const char* psz_subtitle, const char* psz_tagname);
void ParseSrtLine(std::string& srtLine, const STSStyle& style);
std::string GetTag(const char** line, bool b_closing);
void MatchColorSrt(std::string& fntColor);
std::string ws2s(const std::wstring& wstr);
std::wstring s2ws(const std::string& str);

inline void swapRGBtoBGR(std::string& color) {
    std::string tmp = color;

    color[0] = tmp[4];
    color[1] = tmp[5];
    color[4] = tmp[0];
    color[5] = tmp[1];
}

static const struct s_color_tag {
    const char* color;
    const char* hex;
} color_tag[] = {
    //    name              hex value
        { "aqua",           "00FFFF" },
        { "azure",          "F0FFFF" },
        { "beige",          "F5F5DC" },
        { "black",          "000000" },
        { "blue",           "0000FF" },
        { "brown",          "A52A2A" },
        { "chartreuse",     "7FFF00" },
        { "chocolate",      "D2691E" },
        { "coral",          "FF7F50" },
        { "crimson",        "DC143C" },
        { "cyan",           "00FFFF" },
        { "fuchsia",        "FF00FF" },
        { "gold",           "FFD700" },
        { "gray",           "808080" },
        { "green",          "008000" },
        { "grey",           "808080" },
        { "indigo",         "4B0082" },
        { "ivory",          "FFFFF0" },
        { "khaki",          "F0E68C" },
        { "lavender",       "E6E6FA" },
        { "lime",           "00FF00" },
        { "linen",          "FAF0E6" },
        { "magenta",        "FF00FF" },
        { "maroon",         "800000" },
        { "navy",           "000080" },
        { "olive",          "808000" },
        { "orange",         "FFA500" },
        { "orchid",         "DA70D6" },
        { "pink",           "FFC0CB" },
        { "plum",           "DDA0DD" },
        { "purple",         "800080" },
        { "red",            "FF0000" },
        { "salmon",         "FA8072" },
        { "sienna",         "A0522D" },
        { "silver",         "C0C0C0" },
        { "snow",           "FFFAFA" },
        { "tan",            "D2B48C" },
        { "teal",           "008080" },
        { "thistle",        "D8BFD8" },
        { "tomato",         "FF6347" },
        { "turquoise",      "40E0D0" },
        { "violet",         "EE82EE" },
        { "white",          "FFFFFF" },
        { "yellow",         "FFFF00" },
};

class CSimpleTextSubtitle;

class LibassContext {
public:
    LibassContext(CSimpleTextSubtitle* sts);
    ~LibassContext();
    bool m_renderUsingLibass;

    bool m_assloaded;
    bool m_assfontloaded;
    std::string m_trackData;

    IFilterGraph* m_pGraph;
    std::unique_ptr<ASS_Library, ASS_LibraryDeleter> m_ass;
    std::unique_ptr<ASS_Renderer, ASS_RendererDeleter> m_renderer;
    std::unique_ptr<ASS_Track, ASS_TrackDeleter> m_track;

    boolean LibassEnabled();
    boolean CheckSubType();
    void InitLibASS();
    bool LoadASSFile(Subtitle::SubType subType);
    bool LoadASSTrack(char* data, int size, Subtitle::SubType subType);
    void Unload();
    void LoadASSSample(char* data, int dataSize, REFERENCE_TIME tStart, REFERENCE_TIME tStop);
    void LoadTrackData(ASS_Track* track, char* data, int size);
    void StylesChanged();
    void LoadASSFont();
    bool RenderRelativeToWindow();
    CRect GetSPDRect(SubPicDesc& spd);
    POSITION GetStartPosition(REFERENCE_TIME rt, double fps);
    REFERENCE_TIME GetCurrent(POSITION pos);
    POSITION GetNext(POSITION pos);
    STDMETHODIMP Render(REFERENCE_TIME rt, SubPicDesc& spd, RECT& bbox, CSize& size, CRect& vidRect);
    bool RenderFrame(long long now, SubPicDesc& spd, CRect& rcDirty);
    void SetFilterGraphFromFilter(IBaseFilter* f);
    void SetFilterGraph(IFilterGraph* g);
    void AssFlattenSSE2(ASS_Image* imagee, SubPicDesc& spd, CRect& rcDirty);
    //void AssFlatten(ASS_Image* image, SubPicDesc& spd, CRect& rcDirty); // unused C reference implementation
    void SetFrameSize(int w, int h);
    bool IsLibassActive() { return m_assloaded; }

protected:
    CSimpleTextSubtitle* m_STS;
    IPin* m_pPin;
    std::unique_ptr<uint32_t[]> m_pixels;
    CRect lastDirty, lastUncroppedDirty;
    CRect lastSPDRect;
    REFERENCE_TIME rtCurrent;
    bool curTimeInitialized;
    bool m_bOverrideDefaultStyleActive = false;
    bool m_bOverrideAllStylesActive = false;
    CSTSStyleMap origStyles;
    bool hasStyleOverrides;
};
