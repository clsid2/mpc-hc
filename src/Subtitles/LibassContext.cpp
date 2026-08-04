
#include "stdafx.h"
#include "../../include/mpc-hc_config.h"

#if USE_LIBASS
#pragma comment( lib, "libass" )
#include <ios>
#include <algorithm>
#include <fstream>
#include <codecvt>
#include <Shlwapi.h>
#include "LibassContext.h"
#include "STS.h"
#include "Utf8.h"
#include "../DSUtil/PathUtils.h"
#include <sstream>
#include "../DSUtil/DSMPropertyBag.h"
#include  <comutil.h>
#include <ppl.h>

std::string ConsumeAttribute(const char** ppsz_subtitle, std::string& attribute_value) {
    const char* psz_subtitle = *ppsz_subtitle;
    char psz_attribute_name[BUFSIZ];
    char psz_attribute_value[BUFSIZ];

    while (*psz_subtitle == ' ')
        psz_subtitle++;

    size_t attr_len = 0;
    char delimiter;

    while (*psz_subtitle && isalpha(*psz_subtitle)) {
        psz_subtitle++;
        attr_len++;
    }

    if (!*psz_subtitle || attr_len == 0)
        return std::string();

    strncpy_s(psz_attribute_name, psz_subtitle - attr_len, attr_len);
    psz_attribute_name[attr_len] = 0;

    // Skip over to the attribute value
    while (*psz_subtitle && *psz_subtitle != '=')
        psz_subtitle++;

    // Skip the '=' sign
    psz_subtitle++;

    // Aknowledge the delimiter if any
    while (*psz_subtitle && isspace(*psz_subtitle))
        psz_subtitle++;

    if (*psz_subtitle == '\'' || *psz_subtitle == '"') {
        // Save the delimiter and skip it
        delimiter = *psz_subtitle;
        psz_subtitle++;
    } else
        delimiter = 0;

    // Skip spaces, just in case
    while (*psz_subtitle && isspace(*psz_subtitle))
        psz_subtitle++;

    // Skip the first #
    if (*psz_subtitle == '#')
        psz_subtitle++;

    attr_len = 0;
    while (*psz_subtitle && ((delimiter != 0 && *psz_subtitle != delimiter) ||
        (delimiter == 0 && (isalnum(*psz_subtitle) || *psz_subtitle == '#')))) {
        psz_subtitle++;
        attr_len++;
    }

    strncpy_s(psz_attribute_value, psz_subtitle - attr_len, attr_len);
    psz_attribute_value[attr_len] = 0;
    attribute_value.assign(psz_attribute_value);

    // Finally, skip over the final delimiter
    if (delimiter != 0 && *psz_subtitle)
        psz_subtitle++;

    *ppsz_subtitle = psz_subtitle;

    return std::string(psz_attribute_name);
}

void ParseSrtLine(std::string& srtLine, const STSStyle& style) {
    const char* psz_subtitle = srtLine.data();
    std::string subtitle_output;

    while (*psz_subtitle) {
        /* HTML extensions */
        if (*psz_subtitle == '<') {
            std::string tagname = GetTag(&psz_subtitle, false);
            if (!tagname.empty()) {
                // Convert tagname to lowercase
                std::transform(tagname.begin(), tagname.end(), tagname.begin(), ::tolower);
                if (tagname == "br") {
                    subtitle_output.append("\\N");
                } else if (tagname == "b") {
                    subtitle_output.append("{\\b1}");
                } else if (tagname == "i") {
                    subtitle_output.append("{\\i1}");
                } else if (tagname == "u") {
                    subtitle_output.append("{\\u1}");
                } else if (tagname == "s") {
                    subtitle_output.append("{\\s1}");
                } else if (tagname == "font") {
                    std::string attribute_name;
                    std::string attribute_value;

                    attribute_name = ConsumeAttribute(&psz_subtitle, attribute_value);
                    while (!attribute_name.empty()) {
                        // Convert attribute_name to lowercase
                        std::transform(attribute_name.begin(), attribute_name.end(), attribute_name.begin(), ::tolower);
                        if (attribute_name == "face") {
                            subtitle_output.append("{\\fn" + attribute_value + "}");
                        } else if (attribute_name == "family") {
                        }
                        if (attribute_name == "size") {
                            try {
                                int font_size = (int)std::round(std::stod(attribute_value));
                                subtitle_output.append("{\\fs" + std::to_string(font_size) + "}");
                            } catch (...) {}
                        } else if (attribute_name == "color") {
                            MatchColorSrt(attribute_value);

                            // If color is invalid, use WHITE
                            if ((strtoul(attribute_value.c_str(), NULL, 16) == 0) && (attribute_value != "000000"))
                                attribute_value.assign("FFFFFF");

                            // HTML is RGB and we need BGR for libass
                            swapRGBtoBGR(attribute_value);
                            subtitle_output.append("{\\c&H" + attribute_value + "&}");
                        }
                        attribute_name = ConsumeAttribute(&psz_subtitle, attribute_value);
                    }
                } else {
                    // This is an unknown tag. We need to hide it if it's properly closed, and display it otherwise
                    if (!IsClosed(psz_subtitle, tagname.c_str())) {
                        //subtitle_output.append("<" + tagname + ">");
                    } else {
                    }
                    // In any case, fall through and skip to the closing tag.
                }
                // Skip potential spaces & end tag
                while (*psz_subtitle && *psz_subtitle != '>')
                    psz_subtitle++;
                if (*psz_subtitle == '>')
                    psz_subtitle++;

            } else if (!strncmp(psz_subtitle, "</", 2)) {
                tagname = GetTag(&psz_subtitle, true);
                if (!tagname.empty()) {
                    std::transform(tagname.begin(), tagname.end(), tagname.begin(), ::tolower);
                    if (tagname == "b") {
                        subtitle_output.append("{\\b0}");
                    } else if (tagname == "i") {
                        subtitle_output.append("{\\i0}");
                    } else if (tagname == "u") {
                        subtitle_output.append("{\\u0}");
                    } else if (tagname == "s") {
                        subtitle_output.append("{\\s0}");
                    } else if (tagname == "font") {
                        int font_size = (int)std::round(style.fontSize);
                        subtitle_output.append("{\\c}");
                        CT2CA tmpFontName(style.fontName);
                        subtitle_output.append("{\\fn" + std::string(tmpFontName) + "}");
                        subtitle_output.append("{\\fs" + std::to_string(font_size) + "}");
                    } else {
                        // Unknown closing tag. If it is closing an unknown tag, ignore it. Otherwise, display it
                        //subtitle_output.append("</" + tagname + ">");
                    }
                    while (*psz_subtitle == ' ')
                        psz_subtitle++;
                    if (*psz_subtitle == '>')
                        psz_subtitle++;
                }
            } else {
                /* We have an unknown tag, just append it, and move on.
                * The rest of the string won't be recognized as a tag, and
                * we will ignore unknown closing tag
                */
                subtitle_output.push_back('<');
                psz_subtitle++;
            }
        }
        /* MicroDVD extensions */
        /* FIXME:
        *  - Currently, we don't do difference between X and x, and we should:
        *    Capital Letters applies to the whole text and not one line
        *  - We don't support Position and Coordinates
        *  - We don't support the DEFAULT flag (HEADER)
        */
        else if (psz_subtitle[0] == '{' && psz_subtitle[2] == ':' && strchr(&psz_subtitle[2], '}')) {
            const char* psz_tag_end = strchr(&psz_subtitle[2], '}');
            size_t i_len = psz_tag_end - &psz_subtitle[3];

            if (psz_subtitle[1] == 'Y' || psz_subtitle[1] == 'y') {
                if (psz_subtitle[3] == 'i') {
                    subtitle_output.append("{\\i1}");
                    psz_subtitle++;
                }
                if (psz_subtitle[3] == 'b') {
                    subtitle_output.append("{\\b1}");
                    psz_subtitle++;
                }
                if (psz_subtitle[3] == 'u') {
                    subtitle_output.append("{\\u1}");
                    psz_subtitle++;
                }
            } else if ((psz_subtitle[1] == 'C' || psz_subtitle[1] == 'c')
                && psz_subtitle[3] == '$' && i_len >= 7) {
                /* Yes, they use BBGGRR */
                char psz_color[7];
                psz_color[0] = psz_subtitle[4]; psz_color[1] = psz_subtitle[5];
                psz_color[2] = psz_subtitle[6]; psz_color[3] = psz_subtitle[7];
                psz_color[4] = psz_subtitle[8]; psz_color[5] = psz_subtitle[9];
                psz_color[6] = '\0';
                subtitle_output.append("{\\c&H").append(psz_color).append("&}");
            } else if (psz_subtitle[1] == 'F' || psz_subtitle[1] == 'f') {
                std::string font_name(&psz_subtitle[3], i_len);
                subtitle_output.append("{\\fn" + font_name + "}");
            } else if (psz_subtitle[1] == 'S' || psz_subtitle[1] == 's') {
                int size = atoi(&psz_subtitle[3]);
                if (size) {
                    subtitle_output.append("{\\fs" + std::to_string(size) + "}");
                }
            }
            // Hide other {x:y} atrocities, notably {o:x}
            psz_subtitle = psz_tag_end + 1;
        } else {
            if (*psz_subtitle == '\n' || !_strnicmp(psz_subtitle, "\\n", 2)) {
                subtitle_output.append("\\N");

                if (*psz_subtitle == '\n')
                    psz_subtitle++;
                else
                    psz_subtitle += 2;
            } else if (*psz_subtitle == '\r') {
                psz_subtitle++;
            } else {
                subtitle_output.push_back(*psz_subtitle);
                psz_subtitle++;
            }
        }
    }
    srtLine.assign(subtitle_output);
}

void srt_header(char (&outBuffer)[1024], const STSStyle& style, OpenTypeLang::HintStr openTypeLangHint) {
    CT2CA tmpFontName(style.fontName);

    // Generate a standard ass header
    CStringA langTagStr = "";
    if (openTypeLangHint[0]) {
        CStringA tagLang(openTypeLangHint);
        tagLang.Replace(" ", "");
        langTagStr.Format("Language: %s\n", tagLang.GetBuffer());
    }

    _snprintf_s(outBuffer, _TRUNCATE, "[Script Info]\n"
        "Title: MPC-HC generated file\n"
        "ScriptType: v4.00+\n"
        "WrapStyle: 0\n"
        "ScaledBorderAndShadow: %s\n"
        "Kerning: %s\n"
        "YCbCr Matrix: TV.709\n"
        "PlayResX: 384\n"
        "PlayResY: 288\n"
        "%s" /*language if set*/
        "[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, "
        "BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, "
        "BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
        "Style: Default,%s,%u,&H%X,&H%X,&H%X,&H%X,%u,%u,0,0,%lf,%lf,%lf,0,%u,%lf,%lf,%u,%u,%u,%u,0"
        "\n\n[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n\n",
        style.ScaledBorderAndShadow ? "yes" : "no",
        style.Kerning ? "yes" : "no",
        (LPCSTR)langTagStr,
        std::string(tmpFontName).c_str(), (int)std::round(style.fontSize), style.colors[0],
        style.colors[1], style.colors[2], style.colors[3],
        style.fontWeight == FW_BOLD ? 1 : 0,
        style.fItalic ? 1 : 0,
        style.fontScaleX, style.fontScaleY, style.fontSpacing, (style.borderStyle == 1 ? 4 : 1), style.outlineWidthX / 5,
        style.shadowDepthX / 5, style.scrAlignment, (int)style.marginRect.left, (int)style.marginRect.right, (int)style.marginRect.top);
}

ASS_Track* srt_read_file(ASS_Library* library, CStringW fname, const STSStyle& style, OpenTypeLang::HintStr openTypeLangHint) {
    std::ifstream srtFile(fname, std::ios::in);
    ASS_Track* track = ass_new_track(library);
    track->name = _strdup(UTF16To8(fname));
    return srt_read_data(library, track, srtFile, style, openTypeLangHint);
}

ASS_Track* srt_read_data(ASS_Library* library, ASS_Track* track, std::istream &stream, const STSStyle& style, OpenTypeLang::HintStr openTypeLangHint) {
    // Convert SRT to ASS
    std::string lineIn;
    std::string lineOut;
    char inBuffer[1024];
    char outBuffer[1024];
    int start[4], end[4];

    srt_header(outBuffer, style, openTypeLangHint);
    ass_process_data(track, outBuffer, static_cast<int>(strnlen_s(outBuffer, sizeof(outBuffer))));

    char BOM[4];
    stream.read(BOM, 3);
    if (stream.fail()) {
        return track;
    }
    bool streamIsUTF8 = true;
    if (BOM[0] == 0xEF && BOM[1] == 0xBB && BOM[2] == 0xBF) { //utf-8 BOM is here for some reason, we don't need it
        //we seeked past it
    } else {
        stream.seekg(std::ios_base::beg);
    }

    while (!stream.eof()) {
        stream.getline(inBuffer, sizeof(inBuffer) - 1);
        lineIn.assign(inBuffer);
        if (lineIn.empty())
            continue;

        if (streamIsUTF8) {
            streamIsUTF8 &= Utf8::isStringValid((const unsigned char*)lineIn.c_str(), lineIn.length());
        }

        // Read the timecodes
        if (sscanf_s(inBuffer, "%d:%2d:%2d%*1[,.]%3d --> %d:%2d:%2d%*1[,.]%3d", &start[0], &start[1],
            &start[2], &start[3], &end[0], &end[1], &end[2], &end[3]) == 8) {
            lineOut.clear();
            stream.getline(inBuffer, sizeof(inBuffer) - 1);
            lineIn.assign(inBuffer);
            while (!lineIn.empty()) {
                if (streamIsUTF8) {
                    streamIsUTF8 &= Utf8::isStringValid((const unsigned char*)lineIn.c_str(), lineIn.length());
                }
                lineOut.append(lineIn);
                stream.getline(inBuffer, sizeof(inBuffer) - 1);
                lineIn.assign(inBuffer);
                if (!lineIn.empty())
                    lineOut.append("\\N");
            }
            //lineOut.append("\n");

            if (!streamIsUTF8) {
                // Convert to UTF-8 only if UTF-8 not detected
                if (style.charSet != 0) {
                    auto saveLineOut = lineOut;
                    UINT CP = CharSetToCodePage(style.charSet);
                    ConvertCPToUTF8(CP, lineOut);
                    if (lineOut.empty()) { //don't allow convert to destroy our text in case it's not encoded
                        lineOut = saveLineOut;
                    }
                }
                  
            }

            if (lineOut.empty()) {
                ASSERT(false);
            } else {
                ParseSrtLine(lineOut, style);

                CT2CA tmpCustomTags(style.customTags);
                _snprintf_s(outBuffer, _TRUNCATE, "Dialogue: 0,%d:%02d:%02d.%02d,%d:%02d:%02d.%02d,Default,,0,0,0,,{\\blur%u}%s%s",
                    start[0], start[1], start[2],
                    (int)floor((double)start[3] / 10.0), end[0], end[1],
                    end[2], (int)floor((double)end[3] / 10.0), style.fBlur, std::string(tmpCustomTags).c_str(), lineOut.c_str());
                ass_process_data(track, outBuffer, static_cast<int>(strnlen_s(outBuffer, sizeof(outBuffer))));
            }
        }
    }
    ass_process_force_style(track);
    return track;
}

ASS_Track* ass_read_fileW(ASS_Library* library, CStringW fname, const STSStyle& style) {
    std::ifstream t(fname, std::ios::in);
    std::stringstream buffer;
    buffer << t.rdbuf();
    auto str = buffer.str();

    ASS_Track* track = ass_read_memory(library, (char*)str.c_str(), str.size(), "UTF-8");
    if (!track) { //failed, try ansi file encoding
        if (style.charSet == 0) {
            track = ass_read_memory(library, (char*)str.c_str(), str.size(), 0);
        } else {
            DWORD cp = CharSetToCodePage(style.charSet);
            std::string cpString = "CP" + std::to_string(cp);
            track = ass_read_memory(library, (char*)str.c_str(), str.size(), (char*)cpString.c_str());
        }
    }
    return track;
}

void ConvertCPToUTF8(int charset, std::string& codepage_str) {
    UINT CP = (UINT)charset;
    int size = MultiByteToWideChar(CP, MB_PRECOMPOSED, codepage_str.c_str(),
        (int)codepage_str.length(), nullptr, 0);

    std::wstring utf16_str(size, '\0');
    MultiByteToWideChar(CP, MB_PRECOMPOSED, codepage_str.c_str(),
        (int)codepage_str.length(), &utf16_str[0], size);

    int utf8_size = WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(),
        (int)utf16_str.length(), nullptr, 0,
        nullptr, nullptr);

    std::string utf8_str(utf8_size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(),
        (int)utf16_str.length(), &utf8_str[0], utf8_size,
        nullptr, nullptr);
    codepage_str.assign(utf8_str);
}

std::string GetTag(const char** line, bool b_closing) {
    const char* psz_subtitle = *line;

    if (*psz_subtitle != '<')
        return std::string();

    // Skip the '<'
    psz_subtitle++;

    if (b_closing && *psz_subtitle == '/')
        psz_subtitle++;

    // Skip potential spaces
    while (*psz_subtitle == ' ')
        psz_subtitle++;

    // Now we need to verify if what comes next is a valid tag:
    if (!isalpha(*psz_subtitle))
        return std::string();

    size_t tag_size = 1;
    while (isalnum(psz_subtitle[tag_size]) || psz_subtitle[tag_size] == '_')
        tag_size++;

    char psz_tagname[BUFSIZ];
    strncpy_s(psz_tagname, psz_subtitle, tag_size);
    psz_tagname[tag_size] = 0;
    psz_subtitle += tag_size;
    *line = psz_subtitle;

    return std::string(psz_tagname);
}

bool IsClosed(const char* psz_subtitle, const char* psz_tagname) {
    const char* psz_tagpos = StrStrIA(psz_subtitle, psz_tagname);

    if (!psz_tagpos)
        return false;

    // Search for '</' and '>' immediatly before & after (minding the potential spaces)
    const char* psz_endtag = psz_tagpos + strlen(psz_tagname);

    while (*psz_endtag == ' ')
        psz_endtag++;

    if (*psz_endtag != '>')
        return false;

    // Skip back before the tag itself
    psz_tagpos--;
    while (*psz_tagpos == ' ' && psz_tagpos > psz_subtitle)
        psz_tagpos--;

    if (*psz_tagpos-- != '/')
        return false;

    if (*psz_tagpos != '<')
        return false;

    return true;
}

void MatchColorSrt(std::string& fntColor) {
    for (auto i = 0; i < _countof(color_tag); ++i) {
        if (strcmp(fntColor.c_str(), color_tag[i].color) == 0) {
            fntColor.assign(color_tag[i].hex);
            break;
        }
    }
}

std::wstring s2ws(const std::string& str) {
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.from_bytes(str);
}

std::string ws2s(const std::wstring& wstr) {
    using convert_typeX = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_typeX, wchar_t> converterX;

    return converterX.to_bytes(wstr);
}

namespace {
    inline POINT GetRectPos(RECT rect) {
        return { rect.left, rect.top };
    }

    inline SIZE GetRectSize(RECT rect) {
        return { rect.right - rect.left, rect.bottom - rect.top };
    }
}


LibassContext::LibassContext(CSimpleTextSubtitle* sts)
    : m_STS(sts)
    , m_renderUsingLibass(false)
    , m_assloaded(false)
    , m_assfontloaded(false)
    , m_pGraph(nullptr)
    , m_pPin(nullptr)
    , m_ass(nullptr)
    , m_renderer(nullptr)
    , m_track(nullptr)
    , rtCurrent(0)
    , curTimeInitialized(false)
{
}

LibassContext::~LibassContext() {
    Unload();
}

boolean LibassContext::LibassEnabled() {
    return m_STS->m_subtitleType == Subtitle::SubType::SRT && m_STS->m_SubRendererSettings.renderSRTUsingLibass
        || (m_STS->m_subtitleType == Subtitle::SubType::SSA || m_STS->m_subtitleType == Subtitle::SubType::ASS) && m_STS->m_SubRendererSettings.renderSSAUsingLibass;
}

boolean LibassContext::CheckSubType() {
    m_renderUsingLibass = LibassEnabled();
    return m_renderUsingLibass;
}

static void detect_style_changes(STSStyle* before, STSStyle* after, const wchar_t* name, std::vector<CStringA>& styles_overrides) {
    if (!after) return;
    CStringA prefix;
    if (name) {
        prefix = UTF16To8(name);
        prefix.AppendChar('.');
    } else prefix = "";
    const char* prefix_cstr = prefix.GetString();
    if (!before || before->fontName != after->fontName) {
        CStringA tmp;
        CStringA font_name_utf8 = UTF16To8(after->fontName);
        tmp.Format("%sFontName=%s", prefix_cstr, font_name_utf8.GetString());
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->colors[0] != after->colors[0] || before->alpha[0] != after->alpha[0]) {
        CStringA tmp;
        tmp.Format("%sPrimaryColour=&H%8X", prefix_cstr, (after->alpha[0] << 24) | after->colors[0]);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->colors[1] != after->colors[1] || before->alpha[1] != after->alpha[1]) {
        CStringA tmp;
        tmp.Format("%sSecondaryColour=&H%8X", prefix_cstr, (after->alpha[1] << 24) | after->colors[1]);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->colors[2] != after->colors[2] || before->alpha[2] != after->alpha[2]) {
        CStringA tmp;
        tmp.Format("%sOutlineColour=&H%8X", prefix_cstr, (after->alpha[2] << 24) | after->colors[2]);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->colors[3] != after->colors[3] || before->alpha[3] != after->alpha[3]) {
        CStringA tmp;
        tmp.Format("%sBackColour=&H%8X", prefix_cstr, (after->alpha[3] << 24) | after->colors[3]);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fontSize != after->fontSize) {
        CStringA tmp;
        tmp.Format("%sFontSize=%f", prefix_cstr, after->fontSize);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fontWeight != after->fontWeight) {
        CStringA tmp;
        tmp.Format("%sBold=%d", prefix_cstr, after->fontWeight);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fItalic != after->fItalic) {
        CStringA tmp;
        tmp.Format("%sItalic=%d", prefix_cstr, after->fItalic ? 1 : 0);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fUnderline != after->fUnderline) {
        CStringA tmp;
        tmp.Format("%sUnderline=%d", prefix_cstr, after->fUnderline ? 1 : 0);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fStrikeOut != after->fStrikeOut) {
        CStringA tmp;
        tmp.Format("%sStrikeOut=%d", prefix_cstr, after->fStrikeOut ? 1 : 0);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fontSpacing != after->fontSpacing) {
        CStringA tmp;
        tmp.Format("%sSpacing=%f", prefix_cstr, after->fontSpacing);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fontAngleZ != after->fontAngleZ) {
        CStringA tmp;
        tmp.Format("%sAngle=%f", prefix_cstr, after->fontAngleZ);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->borderStyle != after->borderStyle) {
        CStringA tmp;
        tmp.Format("%sBorderStyle=%d", prefix_cstr, after->borderStyle ? 3 : 1);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->scrAlignment != after->scrAlignment) {
        int Alignment = ((after->scrAlignment - 1) % 3) + 1;  // horizontal alignment
        if (after->scrAlignment <= 3)
            Alignment |= VALIGN_SUB;
        else if (after->scrAlignment <= 6)
            Alignment |= VALIGN_CENTER;
        else
            Alignment |= VALIGN_TOP;
        CStringA tmp;
        tmp.Format("%sAlignment=%d", prefix_cstr, Alignment);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->marginRect != after->marginRect) {
        const CRect& r = after->marginRect;
        CStringA tmp1, tmp2, tmp3;
        tmp1.Format("%sMarginL=%ld", prefix_cstr, r.left);
        tmp2.Format("%sMarginR=%ld", prefix_cstr, r.right);
        tmp3.Format("%sMarginV=%ld", prefix_cstr, r.bottom); // may not equal to r.top
        styles_overrides.push_back(std::move(tmp1));
        styles_overrides.push_back(std::move(tmp2));
        styles_overrides.push_back(std::move(tmp3));
    }
    if (!before || before->charSet != after->charSet) {
        CStringA tmp;
        tmp.Format("%sEncoding=%d", prefix_cstr, after->charSet);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fontScaleX != after->fontScaleX) {
        CStringA tmp;
        tmp.Format("%sScaleX=%f", prefix_cstr, after->fontScaleX / 100.0);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fontScaleY != after->fontScaleY) {
        CStringA tmp;
        tmp.Format("%sScaleY=%f", prefix_cstr, after->fontScaleY / 100.0);
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->outlineWidthX != after->outlineWidthX) {
        CStringA tmp;
        tmp.Format("%sOutline=%f", prefix_cstr, after->outlineWidthX / 5); // equal to outlineWidthY
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->shadowDepthX != after->shadowDepthX) {
        CStringA tmp;
        tmp.Format("%sShadow=%f", prefix_cstr, after->shadowDepthX / 5); // equal to shadowDepthY
        styles_overrides.push_back(std::move(tmp));
    }
    if (!before || before->fGaussianBlur != after->fGaussianBlur) {
        CStringA tmp;
        tmp.Format("%sBlur=%f", prefix_cstr, after->fGaussianBlur); // maybe not equal to fBlur
        styles_overrides.push_back(std::move(tmp));
    }
}

void LibassContext::StylesChanged() {
    if (!m_assloaded) {
        ASSERT(false);
        return;
    }

    std::vector<CStringA> styles_overrides;
    POSITION pos = m_STS->m_styles.GetStartPosition();
    for (int k = 0; pos; k++) {
        CString styleName;
        STSStyle* style;
        m_STS->m_styles.GetNextAssoc(pos, styleName, style);
        if (styleName != L"Default" && origStyles.Lookup(styleName)) {
            detect_style_changes(origStyles[styleName], style, nullptr, styles_overrides);
        }
    }
    bool hadStyleOverrides = hasStyleOverrides;
    hasStyleOverrides = styles_overrides.size() > 0;

    bool need_override = hasStyleOverrides || m_STS->m_subtitleType == Subtitle::SubType::SRT || m_STS->m_SubRendererSettings.overrideDefaultStyle || m_STS->m_SubRendererSettings.overrideAllStyles || m_bOverrideDefaultStyleActive && !m_bOverrideAllStylesActive;
    bool need_reload = !need_override && (m_bOverrideDefaultStyleActive || m_bOverrideAllStylesActive || hadStyleOverrides) || m_bOverrideAllStylesActive && !m_STS->m_SubRendererSettings.overrideAllStyles;

    if (need_reload) {
        m_bOverrideDefaultStyleActive = false;
        m_bOverrideAllStylesActive = false;

        // Reload to get original styles back
        if (!m_STS->m_path.IsEmpty()) {
            LoadASSFile(m_STS->m_subtitleType);
        } else if (!m_trackData.empty()) {
            // note: buffered embedded subs will be discarded, so it will take a few seconds or a seek to show subs again
            LoadASSTrack((char*)m_trackData.c_str(), m_trackData.length(), m_STS->m_subtitleType);
        }
    }

    if (need_override) {

        if (m_STS->m_subtitleType == Subtitle::SubType::SRT) {
            STSStyle defStyle = m_STS->m_SubRendererSettings.defaultStyle;
            if (defStyle == m_STS->m_originalDefaultStyle) {
                return;
            }
            detect_style_changes(nullptr, &defStyle, nullptr, styles_overrides);
        } else if (m_STS->m_SubRendererSettings.overrideAllStyles) {
            STSStyle defStyle = m_STS->m_SubRendererSettings.defaultStyle;
            detect_style_changes(nullptr, &defStyle, nullptr, styles_overrides);
            m_bOverrideAllStylesActive = true;
        } else if (m_STS->m_SubRendererSettings.overrideDefaultStyle) {
            STSStyle defStyle = m_STS->m_SubRendererSettings.defaultStyle;
            detect_style_changes(nullptr, &defStyle, L"Default", styles_overrides);
            m_bOverrideDefaultStyleActive = true;
        } else if (m_bOverrideDefaultStyleActive) {
            // this is the original default style
            STSStyle defStyle = m_STS->m_SubRendererSettings.defaultStyle;
            detect_style_changes(nullptr, &defStyle, L"Default", styles_overrides);
            m_bOverrideDefaultStyleActive = false;
        }

        std::unique_ptr<char* []> tmp = std::make_unique<char* []>(styles_overrides.size() + 1);
        for (size_t i = 0; i < styles_overrides.size(); ++i) {
            tmp[i] = const_cast<char*>(styles_overrides[i].GetString());
        }
        tmp[styles_overrides.size()] = NULL;

        ass_set_style_overrides(m_ass.get(), tmp.get());
        ass_process_force_style(m_track.get());

        // workaround to clear caches, no direct way to do that?
        ass_set_selective_style_override_enabled(m_renderer.get(), 0);
        ass_set_selective_style_override_enabled(m_renderer.get(), 2);
    }
}


void LibassContext::InitLibASS() {
    Unload();
    m_STS->CopyToStyles(origStyles);
    hasStyleOverrides = false;
    m_assfontloaded = false;
    m_ass = decltype(m_ass)(ass_library_init());
    ass_set_fonts_dir(m_ass.get(), NULL); //initialize it or we get free() errors in debug mode
    m_renderer = decltype(m_renderer)(ass_renderer_init(m_ass.get()));
#if WIN64
    ass_set_cache_limits(m_renderer.get(), 0, 768); // libass default is 192
#endif
    m_track = decltype(m_track)(ass_new_track(m_ass.get()));
}

bool LibassContext::LoadASSFile(Subtitle::SubType subType) {
    if (m_STS->m_path.IsEmpty() || !PathUtils::Exists(m_STS->m_path)) return false;

    InitLibASS();
    ass_set_extract_fonts(m_ass.get(), true);
    ass_set_style_overrides(m_ass.get(), NULL);

    m_renderer = decltype(m_renderer)(ass_renderer_init(m_ass.get()));
    ass_set_use_margins(m_renderer.get(), false);
    ass_set_font_scale(m_renderer.get(), 1.0);

    STSStyle& defStyle = m_STS->m_SubRendererSettings.defaultStyle;

    if (subType == Subtitle::SRT) {
        m_track = decltype(m_track)(srt_read_file(m_ass.get(), m_STS->m_path, defStyle, m_STS->m_SubRendererSettings.openTypeLangHint));
    } else { //subType == Subtitle::SSA/ASS
        m_track = decltype(m_track)(ass_read_fileW(m_ass.get(), m_STS->m_path, defStyle));
    }

    if (!m_track) return false;

    CT2CA tmpFontName(defStyle.fontName);
    ass_set_fonts(m_renderer.get(), NULL, std::string(tmpFontName).c_str(), ASS_FONTPROVIDER_AUTODETECT, NULL, 0);

    m_assloaded = true;

    return true;
}

bool LibassContext::LoadASSTrack(char* data, int size, Subtitle::SubType subType) {
    InitLibASS();

    if (!m_track) return false;

    STSStyle& defStyle = m_STS->m_SubRendererSettings.defaultStyle;

    if (subType == Subtitle::SRT) {
        std::stringstream srtData;
        srtData.write(data, size);
        srt_read_data(m_ass.get(), m_track.get(), srtData, defStyle, m_STS->m_SubRendererSettings.openTypeLangHint);
    } else { //subType == Subtitle::SSA/ASS
        LoadTrackData(m_track.get(), data, size);
    }
    CT2CA tmpFontName(defStyle.fontName);
    ass_set_fonts(m_renderer.get(), NULL, std::string(tmpFontName).c_str(), ASS_FONTPROVIDER_AUTODETECT, NULL, 0);
    //don't set m_assfontloaded here, in case we can load embedded fonts later?

    m_assloaded = true;
    return true;
}

void LibassContext::SetFilterGraphFromFilter(IBaseFilter* f) {
    if (!m_pGraph) {
        IFilterGraph* fg = GetGraphFromFilter(f);
        SetFilterGraph(fg);
    }
}

void LibassContext::SetFilterGraph(IFilterGraph* g) {
    m_pGraph = g;
    IBaseFilter* f = FindFirstFilter(m_pGraph);
    m_pPin = GetFirstPin(f);
}

void LibassContext::LoadASSFont() {
    if (m_assfontloaded || !m_pPin) {
        return;
    }
    ASS_Library* ass = m_ass.get();
    ASS_Renderer* renderer = m_renderer.get();
    // Try to load fonts in the container
    CComPtr<IAMGraphStreams> graphStreams;
    CComPtr<IDSMResourceBag> bag;
    if (m_pGraph && SUCCEEDED(m_pGraph->QueryInterface(IID_PPV_ARGS(&graphStreams))) &&
        SUCCEEDED(graphStreams->FindUpstreamInterface(m_pPin, IID_PPV_ARGS(&bag), AM_INTF_SEARCH_FILTER))) {
        for (DWORD i = 0; i < bag->ResGetCount(); ++i) {
            _bstr_t name, desc, mime;
            BYTE* pData = nullptr;
            DWORD len = 0;
            if (SUCCEEDED(bag->ResGet(i, &name.GetBSTR(), &desc.GetBSTR(), &mime.GetBSTR(), &pData, &len, nullptr))) {
                if (wcscmp(mime.GetBSTR(), L"application/x-truetype-font") == 0 // see https://gitlab.com/mbunkus/mkvtoolnix/-/issues/3137
                    || wcscmp(mime.GetBSTR(), L"application/vnd.ms-opentype") == 0
                    || wcscmp(mime.GetBSTR(), L"application/x-font-otf") == 0
                    || wcscmp(mime.GetBSTR(), L"application/x-font-ttf") == 0
                    || wcscmp(mime.GetBSTR(), L"application/font-sfnt") == 0
                    || wcscmp(mime.GetBSTR(), L"font/otf") == 0
                    || wcscmp(mime.GetBSTR(), L"font/ttf") == 0
                    || wcscmp(mime.GetBSTR(), L"font/sfnt") == 0
                    || wcscmp(mime.GetBSTR(), L"font/collection") == 0
                )
                {
                    ass_add_font(ass, (char*)name, (char*)pData, len);
                    // TODO: clear these fonts somewhere?
                }
                CoTaskMemFree(pData);
            }
        }
        m_assfontloaded = true;
        STSStyle defStyle = m_STS->m_SubRendererSettings.defaultStyle;
        CT2CA tmpFontName(defStyle.fontName);
        ass_set_fonts(renderer, NULL, std::string(tmpFontName).c_str(), ASS_FONTPROVIDER_AUTODETECT, NULL, 0);
    }
}

bool LibassContext::RenderRelativeToWindow() {
    auto relativeTo = m_STS->m_SubRendererSettings.defaultStyle.relativeTo;
    CSimpleTextSubtitle::UpdateSubRelativeTo(m_STS->m_subtitleType, relativeTo);
    return (relativeTo == STSStyle::WINDOW);
}

CRect LibassContext::GetSPDRect(SubPicDesc& spd) {
    CRect spdRect;
    if (RenderRelativeToWindow()) {
        spdRect = CRect(0, 0, spd.w, spd.h);
    } else {
        spdRect = CRect(spd.vidrect);
    }
    return spdRect;
}

POSITION LibassContext::GetStartPosition(REFERENCE_TIME rt, double fps) {
    if (m_assloaded) {
        rtCurrent = rt;
        curTimeInitialized = true;
        return (POSITION)1;
    }
    return (POSITION)0;
}

POSITION LibassContext::GetNext(POSITION pos) {
    return (POSITION)0;
}

REFERENCE_TIME LibassContext::GetCurrent(POSITION pos) {
    if (m_assloaded && curTimeInitialized) {
        return rtCurrent;
    }
    return 0;
}

STDMETHODIMP LibassContext::Render(REFERENCE_TIME rt, SubPicDesc& spd, RECT& bbox, CSize& size, CRect& frameRect) {
    if (m_assloaded) {
        if (spd.bpp != 32) {
            ASSERT(FALSE);
            return E_INVALIDARG;
        }

        LoadASSFont();

        bool relToWin = RenderRelativeToWindow();
        if (relToWin) {
            frameRect = CRect(0, 0, spd.w, spd.h);
        } else {
            frameRect = CRect(spd.vidrect);
        }
        size = CSize(frameRect.Width(), frameRect.Height());
        ASS_Renderer* renderer = m_renderer.get();
        if (relToWin && (spd.vidrect.top > 0 || spd.h > spd.vidrect.bottom || spd.vidrect.left > 0 && spd.w > spd.vidrect.right)) {
            // handle pan&scan outside visible area
            int t = std::max(0, (int)spd.vidrect.top);
            int b = std::max(0, spd.h - (int)spd.vidrect.bottom);
            int l = std::max(0, (int)spd.vidrect.left);
            int r = std::max(0, spd.w - (int)spd.vidrect.right);
            ass_set_margins(renderer, t, b, l, r);
            ass_set_use_margins(renderer, true);
        } else {
            ass_set_margins(renderer, 0, 0, 0, 0);
            ass_set_use_margins(renderer, false);
        }
        SetFrameSize(size.cx, size.cy);

        CRect rcDirty;
        if (!RenderFrame(rt / 10000, spd, rcDirty)) {
            return E_FAIL;
        }

        bbox = rcDirty;
        return S_OK;
    }
    return E_POINTER;
}

// The flattened image shares the subpic's pixel format (premultiplied color,
// inverted alpha), and the subpic dirty rect holds no prior content, so the
// image can be copied row by row instead of alpha blended.
void CopyToSPD(const BYTE* src, int w, int h, int pitch, int srcXOffset, int srcYOffset, BYTE* dst, int dst_pitch) {
    src += srcYOffset * pitch + srcXOffset;
    for (int i = 0; i < h; i++, src += pitch, dst += dst_pitch) {
        memcpy(dst, src, (size_t)w * 4);
    }
}

bool LibassContext::RenderFrame(long long now, SubPicDesc& spd, CRect& rcDirty) {
    int changed = 1;
    ASS_Image* image = ass_render_frame(m_renderer.get(), m_track.get(), now, &changed);
    if (!image) {
        return false;
    }
    CRect curSPDRect = CRect(0, 0, spd.w, spd.h);
    if (changed || curSPDRect != lastSPDRect) {
        AssFlattenSSE2(image, spd, rcDirty);
        lastUncroppedDirty = rcDirty;
        rcDirty.IntersectRect(rcDirty, curSPDRect);

        lastDirty = rcDirty;
        lastSPDRect = curSPDRect;
    } else {
        rcDirty = lastDirty;
    }

    BYTE* pixelBytes = (BYTE*)(spd.bits + spd.pitch * rcDirty.top + rcDirty.left * 4);
    CopyToSPD(reinterpret_cast<uint8_t*>(m_pixels.get()), rcDirty.Width(), rcDirty.Height(), 4 * lastUncroppedDirty.Width(), 4 * (rcDirty.left - lastUncroppedDirty.left), rcDirty.top - lastUncroppedDirty.top, pixelBytes, spd.pitch);
    return true;
}

static __forceinline void pixmix_sse2(DWORD* dst, DWORD color, DWORD alpha)
{
    alpha = ((alpha + 1) * (color >> 24)) >> 8;
    color &= 0xffffff;
    __m128i zero = _mm_setzero_si128();
    __m128i a = _mm_set1_epi32(((alpha + 1) << 16) | (0x100 - alpha));
    __m128i d = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*dst), zero);
    __m128i s = _mm_unpacklo_epi8(_mm_cvtsi32_si128(color), zero);
    __m128i r = _mm_unpacklo_epi16(d, s);
    r = _mm_madd_epi16(r, a);
    r = _mm_srli_epi32(r, 8);
    r = _mm_packs_epi32(r, r);
    r = _mm_packus_epi16(r, r);
    *dst = (DWORD)_mm_cvtsi128_si32(r) + (alpha << 24);
}

static __forceinline __m128i packed_pix_mix_sse2(const __m128i& dst,
    const __m128i& c_r, const __m128i& c_g, const __m128i& c_b, const __m128i& a) {
    __m128i d_a, d_r, d_g, d_b;

    d_a = _mm_srli_epi32(dst, 24);

    d_r = _mm_slli_epi32(dst, 8);
    d_r = _mm_srli_epi32(d_r, 24);

    d_g = _mm_slli_epi32(dst, 16);
    d_g = _mm_srli_epi32(d_g, 24);

    d_b = _mm_slli_epi32(dst, 24);
    d_b = _mm_srli_epi32(d_b, 24);

    //d_a = _mm_or_si128(d_a, c_a);
    d_r = _mm_or_si128(d_r, c_r);
    d_g = _mm_or_si128(d_g, c_g);
    d_b = _mm_or_si128(d_b, c_b);

    __m128i ones = _mm_set1_epi32(0x1);
    d_a = _mm_add_epi32(d_a, ones);

    // The alpha channel is inverted (0xff = transparent), so a's low word
    // contains 256 - src alpha. Multiplying it by the incremented destination
    // alpha can produce 65536, which wraps to zero in this 16-bit multiply.
    // The subtraction below still leaves the correct value in bits 8-15.
    d_a = _mm_mullo_epi16(d_a, a);
    d_a = _mm_sub_epi32(d_a, ones);
    d_r = _mm_madd_epi16(d_r, a);
    d_g = _mm_madd_epi16(d_g, a);
    d_b = _mm_madd_epi16(d_b, a);

    d_a = _mm_srli_epi32(d_a, 8);
    d_r = _mm_srli_epi32(d_r, 8);
    d_g = _mm_srli_epi32(d_g, 8);
    d_b = _mm_srli_epi32(d_b, 8);

    d_a = _mm_slli_epi32(d_a, 24);
    d_r = _mm_slli_epi32(d_r, 16);
    d_g = _mm_slli_epi32(d_g, 8);

    d_b = _mm_or_si128(d_b, d_g);
    d_b = _mm_or_si128(d_b, d_r);
    return _mm_or_si128(d_b, d_a);
}

static __forceinline void packed_pix_mix_sse2(BYTE* dst, const BYTE* alpha, int w, DWORD color) {
    __m128i c_r = _mm_set1_epi32((color & 0xFF0000));
    __m128i c_g = _mm_set1_epi32((color & 0xFF00) << 8);
    __m128i c_b = _mm_set1_epi32((color & 0xFF) << 16);
    __m128i c_a = _mm_set1_epi16((color & 0xFF000000) >> 24);

    __m128i zero = _mm_setzero_si128();

    __m128i ones = _mm_set1_epi16(0x1);

    const BYTE* alpha_end0 = alpha + (w & ~15);
    const BYTE* alpha_end = alpha + w;
    for (; alpha < alpha_end0; alpha += 16, dst += 16 * 4) {
        __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(alpha));
        __m128i d1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
        __m128i d2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 16));
        __m128i d3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 32));
        __m128i d4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst + 48));

        __m128i ra = _mm_setzero_si128();
        __m128i a1 = _mm_unpacklo_epi8(a, zero);
        a1 = _mm_add_epi16(a1, ones);
        a1 = _mm_mullo_epi16(a1, c_a);
        a1 = _mm_srli_epi16(a1, 8);

        __m128i a2 = _mm_unpackhi_epi8(a, zero);
        a2 = _mm_add_epi16(a2, ones);
        a2 = _mm_mullo_epi16(a2, c_a);
        a2 = _mm_srli_epi16(a2, 8);

        a = _mm_packus_epi16(a1, a2);

        ra = _mm_cmpeq_epi32(ra, ra);
        ra = _mm_xor_si128(ra, a);
        a1 = _mm_unpacklo_epi8(ra, a);
        a2 = _mm_unpackhi_epi8(a1, zero);
        a1 = _mm_unpacklo_epi8(a1, zero);
        a1 = _mm_add_epi16(a1, ones);
        a2 = _mm_add_epi16(a2, ones);

        __m128i a3 = _mm_unpackhi_epi8(ra, a);
        __m128i a4 = _mm_unpackhi_epi8(a3, zero);
        a3 = _mm_unpacklo_epi8(a3, zero);
        a3 = _mm_add_epi16(a3, ones);
        a4 = _mm_add_epi16(a4, ones);

        d1 = packed_pix_mix_sse2(d1, c_r, c_g, c_b, a1);
        d2 = packed_pix_mix_sse2(d2, c_r, c_g, c_b, a2);
        d3 = packed_pix_mix_sse2(d3, c_r, c_g, c_b, a3);
        d4 = packed_pix_mix_sse2(d4, c_r, c_g, c_b, a4);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), d1);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 16), d2);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 32), d3);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + 48), d4);
    }
    DWORD* dst_w = reinterpret_cast<DWORD*>(dst);
    for (; alpha < alpha_end; alpha++, dst_w++) {
        // pixmix_sse2 accumulates straight alpha; recompute the inverted alpha channel
        const DWORD dst_alpha = *dst_w >> 24;
        const DWORD src_alpha = ((static_cast<DWORD>(*alpha) + 1) * (color >> 24)) >> 8;
        pixmix_sse2(dst_w, color, *alpha);
        const DWORD output_alpha = (((dst_alpha + 1) * (0x100 - src_alpha) - 1) >> 8) << 24;
        *dst_w = (*dst_w & 0x00FFFFFF) | output_alpha;
    }
}

void LibassContext::AssFlattenSSE2(ASS_Image* image, SubPicDesc& spd, CRect& rcDirty) {
    if (image) {
        CRect pRect;
        for (auto i = image; i != nullptr; i = i->next) {
            CRect rect2(i->dst_x, i->dst_y, i->dst_x + i->w, i->dst_y + i->h);
            pRect.UnionRect(pRect, rect2);
        }

        CRect spdRect = GetSPDRect(spd);
        rcDirty.IntersectRect(pRect + spdRect.TopLeft(), spdRect);

        size_t pixelCount = (size_t)pRect.Width() * pRect.Height();
        m_pixels.reset(new uint32_t[pixelCount]);
        std::fill_n(m_pixels.get(), pixelCount, 0xFF000000u); // transparent, inverted alpha

        for (auto i = image; i != nullptr; i = i->next) {
            for (int y=0; y<i->h; y++) {
                //BYTE* dst = &pixelBytes[((ptrdiff_t)i->dst_y + y - pRect.top) * spd.pitch + ((ptrdiff_t)i->dst_x - pRect.left) * 4];
                auto dst = reinterpret_cast<uint8_t*>(m_pixels.get() + (i->dst_y + y - pRect.top) * pRect.Width() + (i->dst_x - pRect.left));
                auto alpha = i->bitmap + y * i->stride;
                packed_pix_mix_sse2(dst, alpha, i->w, (i->color >> 8) | (~(i->color) << 24));
            }
        }
    }
}

#if 0
// Unused C reference implementation of AssFlattenSSE2
void LibassContext::AssFlatten(ASS_Image* image, SubPicDesc& spd, CRect& rcDirty) {
    if (image) {
        CRect pRect;
        for (auto i = image; i != nullptr; i = i->next) {
            CRect rect2(i->dst_x, i->dst_y, i->dst_x + i->w, i->dst_y + i->h);
            pRect.UnionRect(pRect, rect2);
        }
        CRect spdRect = GetSPDRect(spd);
        rcDirty.IntersectRect(pRect + spdRect.TopLeft(), spdRect);

        size_t pixelCount = (size_t)pRect.Width() * pRect.Height();
        m_pixels.reset(new uint32_t[pixelCount]);
        std::fill_n(m_pixels.get(), pixelCount, 0xFF000000u); // transparent, inverted alpha

        for (auto i = image; i != nullptr; i = i->next) {
            uint32_t opacity = 0xff - (i->color & 0x000000ff);
            uint32_t iR = (i->color & 0xff000000) >> 24;
            uint32_t iG = (i->color & 0x00ff0000) >> 16;
            uint32_t iB = (i->color & 0x0000ff00) >> 8;

            for (int y = 0; y < i->h; y++) {
                auto dst = reinterpret_cast<uint8_t*>(m_pixels.get() + ((ptrdiff_t)i->dst_y + y - pRect.top) * pRect.Width() + ((ptrdiff_t)i->dst_x - pRect.left));
                auto alpha = i->bitmap + y * i->stride;
                for (int x = 0; x < i->w; ++x, ++alpha, dst += 4) {
                    uint32_t srcA = ((*alpha + 1u) * opacity) >> 8;

                    dst[0] = (dst[0] * (256 - srcA) + iB * (srcA + 1)) >> 8; //B
                    dst[1] = (dst[1] * (256 - srcA) + iG * (srcA + 1)) >> 8; //G
                    dst[2] = (dst[2] * (256 - srcA) + iR * (srcA + 1)) >> 8; //R
                    dst[3] = ((dst[3] + 1u) * (256 - srcA) - 1) >> 8;        //A (inverted: 0xff = transparent)
                }
            }
        }
    }
}
#endif

void LibassContext::SetFrameSize(int w, int h) {
    if (m_STS->m_subtitleType != Subtitle::SRT) {
        ass_set_storage_size(m_renderer.get(), m_STS->m_storageRes.cx, m_STS->m_storageRes.cy);
    }
    ass_set_frame_size(m_renderer.get(), w, h);
}

void LibassContext::Unload() {
    m_assloaded = false;
    if (m_track) m_track.reset();
    if (m_renderer) m_renderer.reset();
    if (m_ass) {
        m_ass.reset();
    }
}

void LibassContext::LoadASSSample(char *data, int dataSize, REFERENCE_TIME tStart, REFERENCE_TIME tStop) {
    if (m_renderUsingLibass) {
        if (m_STS->m_subtitleType == Subtitle::SRT) { //received SRT sample, try to use libass to handle
            STSStyle& defStyle = m_STS->m_SubRendererSettings.defaultStyle;
            if (!m_assloaded) { //create ass header
                InitLibASS();

                char outBuffer[1024];
                srt_header(outBuffer, defStyle, m_STS->m_SubRendererSettings.openTypeLangHint);
                LoadTrackData(m_track.get(), outBuffer, static_cast<int>(strnlen_s(outBuffer, sizeof(outBuffer))));
                m_assloaded = true;
            }

            if (m_assloaded) {
                char subLineData[1024]{};
                strncpy_s(subLineData, _countof(subLineData), data, dataSize);
                std::string str = subLineData;

                // This is the way i use to get a unique id for the subtitle line
                // It will only fail in the case there is 2 or more lines with the same start timecode
                // (Need to check if the matroska muxer join lines in such a case)
                REFERENCE_TIME m_iSubLineCount = tStart / 10000;

                // Change srt tags to ass tags
                ParseSrtLine(str, defStyle);

                // Add the custom tags
                CT2CA tmpCustomTags(defStyle.customTags);
                str.insert(0, std::string(tmpCustomTags));

                // Add blur
                char blur[20]{};
                _snprintf_s(blur, _TRUNCATE, "{\\blur%u}", defStyle.fBlur);
                str.insert(0, blur);

                // ASS in MKV: ReadOrder, Layer, Style, Name, MarginL, MarginR, MarginV, Effect, Text
                char outBuffer[1024]{};
                _snprintf_s(outBuffer, _TRUNCATE, "%lld,0,Default,Main,0,0,0,,%s", m_iSubLineCount, str.c_str());
                ass_process_chunk(m_track.get(), outBuffer, static_cast<int>(strnlen_s(outBuffer, sizeof(outBuffer))), tStart / 10000, (tStop - tStart) / 10000);
            }
        }
    }
}

void LibassContext::LoadTrackData(ASS_Track* track, char* data, int size) {
    m_trackData = data;
    ass_process_codec_private(m_track.get(), data, size);
}

#endif // USE_LIBASS
