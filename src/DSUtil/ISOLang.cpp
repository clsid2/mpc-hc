/*
 * (C) 2016-2017 see Authors.txt
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
#include "ISOLang.h"
#include "DSUtil.h"
#include "text.h"

namespace
{
    constexpr ISOLang s_isolangs[] = {  // TODO : fill LCID !!!
        { "Abkhazian", "abk", "ab" },
        { "Achinese", "ace", "" },
        { "Acoli", "ach", "" },
        { "Adangme", "ada", "" },
        { "Adyghe", "ady", "" },
        { "Afar", "aar", "aa" },
        { "Afrihili", "afh", "" },
        { "Afrikaans", "afr", "af",                  MAKELCID(MAKELANGID(LANG_AFRIKAANS, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Afro-Asiatic", "afa", "" },
        { "Akan", "aka", "ak" },
        { "Akkadian", "akk", "" },
        { "Albanian", "alb", "sq" },
        { "Albanian", "sqi", "sq",                   MAKELCID(MAKELANGID(LANG_ALBANIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Aleut", "ale", "" },
        { "Algonquian languages", "alg", "" },
        { "Altaic", "tut", "" },
        { "Amharic", "amh", "am" },
        { "Apache languages", "apa", "" },
        { "Arabic", "ara", "ar",                     MAKELCID(MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Aragonese", "arg", "an" },
        { "Aramaic", "arc", "" },
        { "Arapaho", "arp", "" },
        { "Araucanian", "arn", "" },
        { "Arawak", "arw", "" },
        { "Armenian", "arm", "hy",                   MAKELCID(MAKELANGID(LANG_ARMENIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Armenian", "hye", "hy",                   MAKELCID(MAKELANGID(LANG_ARMENIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Artificial (Other)", "art", "" },
        { "Assamese", "asm", "as",                   MAKELCID(MAKELANGID(LANG_ASSAMESE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Asturian", "ast", "at" },
        { "Bable", "ast", "at" },
        { "Athapascan languages", "ath", "" },
        { "Australian languages", "aus", "" },
        { "Austronesian", "map", "" },
        { "Avaric", "ava", "av" },
        { "Avestan", "ave", "ae" },
        { "Awadhi", "awa", "" },
        { "Aymara", "aym", "ay" },
        { "Azerbaijani", "aze", "az",                MAKELCID(MAKELANGID(LANG_AZERI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Balinese", "ban", "" },
        { "Baltic", "bat", "" },
        { "Baluchi", "bal", "" },
        { "Bambara", "bam", "bm" },
        { "Bamileke languages", "bai", "" },
        { "Banda", "bad", "" },
        { "Bantu", "bnt", "" },
        { "Basa", "bas", "" },
        { "Bashkir", "bak", "ba",                    MAKELCID(MAKELANGID(LANG_BASHKIR, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Basque", "baq", "eu",                     MAKELCID(MAKELANGID(LANG_BASQUE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Basque", "eus", "eu",                     MAKELCID(MAKELANGID(LANG_BASQUE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Batak", "btk", "" },
        { "Beja", "bej", "" },
        { "Belarusian", "bel", "be",                 MAKELCID(MAKELANGID(LANG_BELARUSIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Bemba", "bem", "" },
        { "Bengali", "ben", "bn",                    MAKELCID(MAKELANGID(LANG_BENGALI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Berber", "ber", "" },
        { "Bhojpuri", "bho", "" },
        { "Bihari", "bih", "bh" },
        { "Bikol", "bik", "" },
        { "Bini", "bin", "" },
        { "Bislama", "bis", "bi" },
        { "Blin", "byn", "" },
        { "Bokmål", "nob", "nb",                     MAKELCID(MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL), SORT_DEFAULT) },
        { "Bokmal", "nob", "nb",                     MAKELCID(MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL), SORT_DEFAULT) },
        { "Bosnian", "bos", "bs" },
        { "Braj", "bra", "" },
        { "Breton", "bre", "br",                     MAKELCID(MAKELANGID(LANG_BRETON, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Buginese", "bug", "" },
        { "Bulgarian", "bul", "bg",                  MAKELCID(MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Buriat", "bua", "" },
        { "Burmese", "bur", "my" },
        { "Burmese", "mya", "my" },
        { "Caddo", "cad", "" },
        { "Carib", "car", "" },
        { "Catalan", "cat", "ca",                    MAKELCID(MAKELANGID(LANG_CATALAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Caucasian", "cau", "" },
        { "Cebuano", "ceb", "" },
        { "Celtic", "cel", "" },
        { "Central American Indian", "cai", "" },
        { "Chagatai", "chg", "" },
        { "Chamic languages", "cmc", "" },
        { "Chamorro", "cha", "ch" },
        { "Chechen", "che", "ce" },
        { "Cherokee", "chr", "" },
        { "Cheyenne", "chy", "" },
        { "Chibcha", "chb", "" },
        { "Chinese (traditional)", "zht", "zt",      MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL), SORT_DEFAULT) },
        { "Chinese bilingual", "zhe", "ze" },
        { "Chinese", "chi", "zh",                    MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_NEUTRAL), SORT_DEFAULT) },
        { "Chinese", "zho", "zh",                    MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_NEUTRAL), SORT_DEFAULT) },
        { "Chinook jargon", "chn", "" },
        { "Chipewyan", "chp", "" },
        { "Choctaw", "cho", "" },
        { "Church Slavic", "chu", "cu" },
        { "Church Slavonic", "chu", "cu" },
        { "Chuukese", "chk", "" },
        { "Chuvash", "chv", "cv" },
        { "Classical Newari", "nwc", "" },
        { "Coptic", "cop", "" },
        { "Cornish", "cor", "kw" },
        { "Corsican", "cos", "co",                   MAKELCID(MAKELANGID(LANG_CORSICAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Cree", "cre", "cr" },
        { "Creek", "mus", "" },
        { "Creoles and pidgins", "crp", "" },
        { "Creoles and pidgins,", "cpe", "" },
        { "Creoles and pidgins,", "cpf", "" },
        { "Creoles and pidgins,", "cpp", "" },
        { "Crimean Turkish", "crh", "" },
        { "Croatian", "hrv", "hr",                   MAKELCID(MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Croatian", "scr", "hr",                   MAKELCID(MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Cushitic", "cus", "" },
        { "Czech", "cze", "cs",                      MAKELCID(MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Czech", "ces", "cs",                      MAKELCID(MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Dakota", "dak", "" },
        { "Danish", "dan", "da",                     MAKELCID(MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Dargwa", "dar", "" },
        { "Dayak", "day", "" },
        { "Delaware", "del", "" },
        { "Dinka", "din", "" },
        { "Divehi", "div", "dv",                     MAKELCID(MAKELANGID(LANG_DIVEHI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Dogri", "doi", "" },
        { "Dogrib", "dgr", "" },
        { "Dravidian", "dra", "" },
        { "Duala", "dua", "" },
        { "Dutch, Middle (ca. 1050-1350)", "dum", "" },
        { "Dutch", "dut", "nl",                      MAKELCID(MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Dutch", "nld", "nl",                      MAKELCID(MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Dyula", "dyu", "" },
        { "Dzongkha", "dzo", "dz" },
        { "Efik", "efi", "" },
        { "Egyptian (Ancient)", "egy", "" },
        { "Ekajuk", "eka", "" },
        { "Elamite", "elx", "" },
        { "English", "eng", "en",                    MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "English, Middle (1100-1500)", "enm", "" },
        { "English, Old (ca.450-1100)", "ang", "" },
        { "Erzya", "myv", "" },
        { "Extremaduran", "ext", "ex" },
        { "Esperanto", "epo", "eo" },
        { "Estonian", "est", "et",                   MAKELCID(MAKELANGID(LANG_ESTONIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Ewe", "ewe", "ee" },
        { "Ewondo", "ewo", "" },
        { "Fang", "fan", "" },
        { "Fanti", "fat", "" },
        { "Faroese", "fao", "fo",                    MAKELCID(MAKELANGID(LANG_FAEROESE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Fijian", "fij", "fj" },
        { "Filipino", "fil", "",                     MAKELCID(MAKELANGID(LANG_FILIPINO, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Finnish", "fin", "fi",                    MAKELCID(MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Finno-Ugrian", "fiu", "" },
        { "Flemish", "dut", "nl",                    MAKELCID(MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Fon", "fon", "" },
        { "French", "fre", "fr",                     MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "French", "fra", "fr",                     MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "French, Middle (ca.1400-1600)", "frm", "" },
        { "French, Old (842-ca.1400)", "fro", "" },
        { "Frisian", "fry", "fy",                    MAKELCID(MAKELANGID(LANG_FRISIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Friulian", "fur", "" },
        { "Fulah", "ful", "ff" },
        { "Ga", "gaa", "" },
        { "Gaelic", "gla", "gd",                     MAKELCID(MAKELANGID(LANG_GALICIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Galician", "glg", "gl" },
        { "Ganda", "lug", "lg" },
        { "Gayo", "gay", "" },
        { "Gbaya", "gba", "" },
        { "Geez", "gez", "" },
        { "Georgian", "geo", "ka",                   MAKELCID(MAKELANGID(LANG_GEORGIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Georgian", "kat", "ka",                   MAKELCID(MAKELANGID(LANG_GEORGIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "German", "ger", "de",                     MAKELCID(MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "German", "deu", "de",                     MAKELCID(MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "German, Low", "nds", "" },
        { "Saxon, Low", "nds", "" },
        { "German, Middle High (ca.1050-1500)", "gmh", "" },
        { "German, Old High (ca.750-1050)", "goh", "" },
        { "Germanic", "gem", "" },
        { "Gilbertese", "gil", "" },
        { "Gondi", "gon", "" },
        { "Gorontalo", "gor", "" },
        { "Gothic", "got", "" },
        { "Grebo", "grb", "" },
        { "Greek", "ell", "el",                      MAKELCID(MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Greek", "gre", "el",                      MAKELCID(MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Greek, Ancient (to 1453)", "grc", "",     MAKELCID(MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Guarani", "grn", "gn" },
        { "Gujarati", "guj", "gu",                   MAKELCID(MAKELANGID(LANG_GUJARATI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Gwich’in", "gwi", "" },
        { "Haida", "hai", "" },
        { "Haitian", "hat", "ht" },
        { "Hausa", "hau", "ha",                      MAKELCID(MAKELANGID(LANG_HAUSA, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Hawaiian", "haw", "" },
        { "Hearing impaired", "sdh", "hi" },
        { "Hebrew", "heb", "he",                     MAKELCID(MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Herero", "her", "hz" },
        { "Hiligaynon", "hil", "" },
        { "Himachali", "him", "" },
        { "Hindi", "hin", "hi",                      MAKELCID(MAKELANGID(LANG_HINDI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Hiri Motu", "hmo", "ho" },
        { "Hittite", "hit", "" },
        { "Hmong", "hmn", "" },
        { "Hungarian", "hun", "hu",                  MAKELCID(MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Hupa", "hup", "" },
        { "Iban", "iba", "" },
        { "Icelandic", "ice", "is",                  MAKELCID(MAKELANGID(LANG_ICELANDIC, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Icelandic", "isl", "is",                  MAKELCID(MAKELANGID(LANG_ICELANDIC, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Ido", "ido", "io" },
        { "Igbo", "ibo", "ig",                       MAKELCID(MAKELANGID(LANG_IGBO, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Ijo", "ijo", "" },
        { "Iloko", "ilo", "" },
        { "Inari Sami", "smn", "" },
        { "Indic", "inc", "" },
        { "Indo-European", "ine", "" },
        { "Indonesian", "ind", "id",                 MAKELCID(MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Ingush", "inh", "" },
        { "Interlingua (International", "ina", "ia" },
        { "Interlingue", "ile", "ie" },
        { "Inuktitut", "iku", "iu",                  MAKELCID(MAKELANGID(LANG_INUKTITUT, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Inupiaq", "ipk", "ik" },
        { "Iranian (Other)", "ira", "" },
        { "Irish", "gle", "ga",                      MAKELCID(MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Irish, Middle (900-1200)", "mga", "",     MAKELCID(MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Irish, Old (to 900)", "sga", "",          MAKELCID(MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Iroquoian languages", "iro", "" },
        { "Italian", "ita", "it",                    MAKELCID(MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Japanese", "jpn", "ja",                   MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Javanese", "jav", "jv" },
        { "Judeo-Arabic", "jrb", "" },
        { "Judeo-Persian", "jpr", "" },
        { "Kabardian", "kbd", "" },
        { "Kabyle", "kab", "" },
        { "Kachin", "kac", "" },
        { "Kalaallisut", "kal", "kl",                MAKELCID(MAKELANGID(LANG_GREENLANDIC, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Greenlandic", "kal", "kl",                MAKELCID(MAKELANGID(LANG_GREENLANDIC, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Kalmyk", "xal", "" },
        { "Kamba", "kam", "" },
        { "Kannada", "kan", "kn",                    MAKELCID(MAKELANGID(LANG_KANNADA, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Kanuri", "kau", "kr" },
        { "Kara-Kalpak", "kaa", "" },
        { "Karachay-Balkar", "krc", "" },
        { "Karen", "kar", "" },
        { "Kashmiri", "kas", "ks",                   MAKELCID(MAKELANGID(LANG_KASHMIRI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Kashubian", "csb", "" },
        { "Kawi", "kaw", "" },
        { "Kazakh", "kaz", "kk",                     MAKELCID(MAKELANGID(LANG_KAZAK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Khasi", "kha", "" },
        { "Khmer", "khm", "km",                      MAKELCID(MAKELANGID(LANG_KHMER, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Khoisan (Other)", "khi", "" },
        { "Khotanese", "kho", "" },
        { "Kikuyu", "kik", "ki" },
        { "Gikuyu", "kik", "ki" },
        { "Kimbundu", "kmb", "" },
        { "Kinyarwanda", "kin", "rw",                MAKELCID(MAKELANGID(LANG_KINYARWANDA, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Kirghiz", "kir", "ky" },
        { "Klingon", "tlh", "" },
        { "Komi", "kom", "kv" },
        { "Kongo", "kon", "kg" },
        { "Konkani", "kok", "",                      MAKELCID(MAKELANGID(LANG_KONKANI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Korean", "kor", "ko",                     MAKELCID(MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Kosraean", "kos", "" },
        { "Kpelle", "kpe", "" },
        { "Kru", "kro", "" },
        { "Kuanyama", "kua", "kj" },
        { "Kwanyama", "kua", "kj" },
        { "Kumyk", "kum", "" },
        { "Kurdish", "kur", "ku" },
        { "Kurukh", "kru", "" },
        { "Kutenai", "kut", "" },
        { "Ladino", "lad", "" },
        { "Lahnda", "lah", "" },
        { "Lamba", "lam", "" },
        { "Lao", "lao", "lo",                        MAKELCID(MAKELANGID(LANG_LAO, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Latin", "lat", "la" },
        { "Latvian", "lav", "lv",                    MAKELCID(MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Lezghian", "lez", "" },
        { "Limburgan", "lim", "li" },
        { "Limburger", "lim", "li" },
        { "Limburgish", "lim", "li" },
        { "Lingala", "lin", "ln" },
        { "Lithuanian", "lit", "lt",                 MAKELCID(MAKELANGID(LANG_LITHUANIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Lojban", "jbo", "" },
        { "Lower Sorbian", "dsb", "" },
        { "Lozi", "loz", "" },
        { "Luba-Katanga", "lub", "lu" },
        { "Luba-Lulua", "lua", "" },
        { "Luiseno", "lui", "" },
        { "Lule Sami", "smj", "" },
        { "Lunda", "lun", "" },
        { "Luo (Kenya and Tanzania)", "luo", "" },
        { "Lushai", "lus", "" },
        { "Luxembourgish", "ltz", "lb",              MAKELCID(MAKELANGID(LANG_LUXEMBOURGISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Macedonian", "mac", "mk",                 MAKELCID(MAKELANGID(LANG_MACEDONIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Macedonian", "mkd", "mk",                 MAKELCID(MAKELANGID(LANG_MACEDONIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Madurese", "mad", "" },
        { "Magahi", "mag", "" },
        { "Maithili", "mai", "" },
        { "Makasar", "mak", "" },
        { "Malagasy", "mlg", "mg" },
        { "Malay", "may", "ms",                      MAKELCID(MAKELANGID(LANG_MALAY, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Malay", "msa", "ms",                      MAKELCID(MAKELANGID(LANG_MALAY, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Malayalam", "mal", "ml",                  MAKELCID(MAKELANGID(LANG_MALAYALAM, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Maltese", "mlt", "mt",                    MAKELCID(MAKELANGID(LANG_MALTESE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Manchu", "mnc", "" },
        { "Mandar", "mdr", "" },
        { "Mandingo", "man", "" },
        { "Manipuri", "mni", "ma",                   MAKELCID(MAKELANGID(LANG_MANIPURI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Manobo languages", "mno", "" },
        { "Manx", "glv", "gv" },
        { "Maori", "mao", "mi",                      MAKELCID(MAKELANGID(LANG_MAORI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Maori", "mri", "mi",                      MAKELCID(MAKELANGID(LANG_MAORI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Marathi", "mar", "mr",                    MAKELCID(MAKELANGID(LANG_MARATHI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Mari", "chm", "" },
        { "Marshallese", "mah", "mh" },
        { "Marwari", "mwr", "" },
        { "Masai", "mas", "" },
        { "Mayan languages", "myn", "" },
        { "Mende", "men", "" },
        { "Micmac", "mic", "" },
        { "Minangkabau", "min", "" },
        { "Miscellaneous languages", "mis", "" },
        { "Mohawk", "moh", "",                       MAKELCID(MAKELANGID(LANG_MOHAWK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Moksha", "mdf", "" },
        { "Moldavian", "mol", "mo" },
        { "Mon-Khmer (Other)", "mkh", "" },
        { "Mongo", "lol", "" },
        { "Mongolian", "mon", "mn",                  MAKELCID(MAKELANGID(LANG_MONGOLIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Montenegrin", "mne", "me" },
        { "Mossi", "mos", "" },
        { "Multiple languages", "mul", "" },
        { "Munda languages", "mun", "" },
        { "Nahuatl", "nah", "" },
        { "Nauru", "nau", "na" },
        { "Navaho", "nav", "nv" },
        { "Navajo", "nav", "nv" },
        { "Ndebele, North", "nde", "nd" },
        { "Ndebele, South", "nbl", "nr" },
        { "Ndonga", "ndo", "ng" },
        { "Neapolitan", "nap", "" },
        { "Nepali", "nep", "ne",                     MAKELCID(MAKELANGID(LANG_NEPALI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Newari", "new", "" },
        { "Nias", "nia", "" },
        { "Niger-Kordofanian", "nic", "" },
        { "Nilo-Saharan", "ssa", "" },
        { "Niuean", "niu", "" },
        { "Nogai", "nog", "" },
        { "Norse, Old", "non", "" },
        { "North American Indian", "nai", "" },
        { "North Ndebele", "nde", "nd" },
        { "Northern Sami", "sme", "se" },
        { "Norwegian", "nor", "no",                  MAKELCID(MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Norwegian", "nob", "nb",                  MAKELCID(MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL), SORT_DEFAULT) },
        { "Norwegian", "nno", "nn",                  MAKELCID(MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK), SORT_DEFAULT) },
        { "Nubian languages", "nub", "" },
        { "Nyamwezi", "nym", "" },
        { "Nyanja", "nya", "ny" },
        { "Chichewa", "nya", "ny" },
        { "Chewa", "nya", "ny" },
        { "Nyankole", "nyn", "" },
        { "Nynorsk", "nno", "nn",                    MAKELCID(MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK), SORT_DEFAULT) },
        { "Nyoro", "nyo", "" },
        { "Nzima", "nzi", "" },
        { "Occitan", "oci", "oc",                    MAKELCID(MAKELANGID(LANG_OCCITAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Provençal", "oci", "oc",                  MAKELCID(MAKELANGID(LANG_OCCITAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Ojibwa", "oji", "oj" },
        { "Oriya", "ori", "or" },
        { "Oromo", "orm", "om" },
        { "Osage", "osa", "" },
        { "Ossetian", "oss", "os" },
        { "Ossetic", "oss", "os" },
        { "Otomian languages", "oto", "" },
        { "Pahlavi", "pal", "" },
        { "Palauan", "pau", "" },
        { "Pali", "pli", "pi" },
        { "Pampanga", "pam", "" },
        { "Pangasinan", "pag", "" },
        { "Punjabi", "pan", "pa",                    MAKELCID(MAKELANGID(LANG_PUNJABI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Papiamento", "pap", "" },
        { "Papuan (Other)", "paa", "" },
        { "Persian", "per", "fa",                    MAKELCID(MAKELANGID(LANG_PERSIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Persian", "fas", "fa",                    MAKELCID(MAKELANGID(LANG_PERSIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Persian, Old (ca.600-400 B.C.)", "peo", "" },
        { "Philippine (Other)", "phi", "" },
        { "Phoenician", "phn", "" },
        { "Pohnpeian", "pon", "" },
        { "Polish", "pol", "pl",                     MAKELCID(MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Portuguese", "por", "pt",                 MAKELCID(MAKELANGID(LANG_PORTUGUESE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        // custom codes for Brazilian Portuguese language
        { "Brazilian Portuguese", "pob", "pb",       MAKELCID(MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN), SORT_DEFAULT) },
        { "Brazilian", "pob", "pb",                  MAKELCID(MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN), SORT_DEFAULT) },
        { "Mozambican Portuguese", "pom", "pm" }, // Custom codes compatible with OpenSubtitles database
        { "Prakrit languages", "pra", "" },
        { "Provençal, Old (to 1500)", "pro", "" },
        { "Dari", "prs", "pr" },
        { "Pushto", "pus", "ps" },
        { "Quechua", "que", "qu",                MAKELCID(MAKELANGID(LANG_QUECHUA, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Raeto-Romance", "roh", "rm" },
        { "Rajasthani", "raj", "" },
        { "Rapanui", "rap", "" },
        { "Rarotongan", "rar", "" },
        { "Reserved for local use", "qaa-qtz", "" },
        { "Romance", "roa", "" },
        { "Romanian", "rum", "ro",               MAKELCID(MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Romanian", "ron", "ro",               MAKELCID(MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Romany", "rom", "" },
        { "Rundi", "run", "rn" },
        { "Russian", "rus", "ru",                MAKELCID(MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Salishan languages", "sal", "" },
        { "Samaritan Aramaic", "sam", "" },
        { "Sami languages", "smi", "" },
        { "Samoan", "smo", "sm" },
        { "Sandawe", "sad", "" },
        { "Sango", "sag", "sg" },
        { "Sanskrit", "san", "sa",               MAKELCID(MAKELANGID(LANG_SANSKRIT, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Santali", "sat", "sx" },
        { "Sardinian", "srd", "sc" },
        { "Sasak", "sas", "" },
        { "Scots", "sco", "" },
        { "Scottish Gaelic", "gla", "gd",        MAKELCID(MAKELANGID(LANG_GALICIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Selkup", "sel", "" },
        { "Semitic (Other)", "sem", "" },
        { "Serbian", "scc", "sr",                MAKELCID(MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_SERBIA_CYRILLIC), SORT_DEFAULT) },
        { "Serbian", "srp", "sr",                MAKELCID(MAKELANGID(LANG_SERBIAN_NEUTRAL, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Serer", "srr", "" },
        { "Shan", "shn", "" },
        { "Shona", "sna", "sn" },
        { "Sichuan Yi", "iii", "ii" },
        { "Sidamo", "sid", "" },
        { "Sign languages", "sgn", "" },
        { "Siksika", "bla", "" },
        { "Sindhi", "snd", "sd",                 MAKELCID(MAKELANGID(LANG_SINDHI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Sinhalese", "sin", "si",              MAKELCID(MAKELANGID(LANG_SINHALESE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Sino-Tibetan", "sit", "" },
        { "Siouan languages", "sio", "" },
        { "Skolt Sami", "sms", "" },
        { "Slave (Athapascan)", "den", "" },
        { "Slavic", "sla", "" },
        { "Slovak", "slo", "sk",                 MAKELCID(MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Slovak", "slk", "sk",                 MAKELCID(MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Slovenian", "slv", "sl",              MAKELCID(MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Sogdian", "sog", "" },
        { "Somali", "som", "so" },
        { "Songhai", "son", "" },
        { "Soninke", "snk", "" },
        { "Sorbian languages", "wen", "" },
        { "Sotho, Northern", "nso", "",          MAKELCID(MAKELANGID(LANG_SOTHO, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Sotho, Southern", "sot", "st",        MAKELCID(MAKELANGID(LANG_SOTHO, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "South American Indian", "sai", "" },
        { "South Ndebele", "nbl", "nr" },
        { "Southern Sami", "sma", "" },
        { "Spanish", "spa", "es",                MAKELCID(MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Castilian", "spa", "es",              MAKELCID(MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Selepet", "spl", "ea", },
        { "Sanapana", "spn", "sp" },
        { "Sukuma", "suk", "" },
        { "Sumerian", "sux", "" },
        { "Sundanese", "sun", "su" },
        { "Susu", "sus", "" },
        { "Swahili", "swa", "sw",                MAKELCID(MAKELANGID(LANG_SWAHILI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Swati", "ssw", "ss" },
        { "Swedish", "swe", "sv",                MAKELCID(MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Syriac", "syr", "sy",                 MAKELCID(MAKELANGID(LANG_SYRIAC, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tagalog", "tgl", "tl" },
        { "Tahitian", "tah", "ty" },
        { "Tai (Other)", "tai", "" },
        { "Tajik", "tgk", "tg",                  MAKELCID(MAKELANGID(LANG_TAJIK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tamashek", "tmh", "" },
        { "Tamil", "tam", "ta",                  MAKELCID(MAKELANGID(LANG_TAMIL, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tatar", "tat", "tt",                  MAKELCID(MAKELANGID(LANG_TATAR, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Telugu", "tel", "te",                 MAKELCID(MAKELANGID(LANG_TELUGU, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tereno", "ter", "" },
        { "Tetum", "tet", "" },
        { "Thai", "tha", "th",                   MAKELCID(MAKELANGID(LANG_THAI, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tibetan", "tib", "bo",                MAKELCID(MAKELANGID(LANG_TIBETAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tibetan", "bod", "bo",                MAKELCID(MAKELANGID(LANG_TIBETAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tigre", "tig", "" },
        { "Tigrinya", "tir", "ti",               MAKELCID(MAKELANGID(LANG_TIGRIGNA, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Timne", "tem", "" },
        { "Tiv", "tiv", "" },
        { "Tlingit", "tli", "" },
        { "Tok Pisin", "tpi", "" },
        { "Tokelau", "tkl", "" },
        { "Tonga (Nyasa)", "tog", "" },
        { "Toki Pona", "tok", "tp" },
        { "Tonga (Tonga Islands)", "ton", "to" },
        { "Tsimshian", "tsi", "" },
        { "Tsonga", "tso", "ts" },
        { "Tswana", "tsn", "tn",                 MAKELCID(MAKELANGID(LANG_TSWANA, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tumbuka", "tum", "" },
        { "Tupi languages", "tup", "" },
        { "Turkish", "tur", "tr",                MAKELCID(MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Turkish, Ottoman (1500-1928)", "ota", "", MAKELCID(MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Turkmen", "tuk", "tk",                MAKELCID(MAKELANGID(LANG_TURKMEN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Tuvalu", "tvl", "" },
        { "Tuvinian", "tyv", "" },
        { "Twi", "twi", "tw" },
        { "Udmurt", "udm", "" },
        { "Ugaritic", "uga", "" },
        { "Uighur", "uig", "ug",                 MAKELCID(MAKELANGID(LANG_UIGHUR, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Ukrainian", "ukr", "uk",              MAKELCID(MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Umbundu", "umb", "" },
        { "Undetermined", "und", "" },
        { "Upper Sorbian", "hsb", "" },
        { "Urdu", "urd", "ur",                   MAKELCID(MAKELANGID(LANG_URDU, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Uzbek", "uzb", "uz",                  MAKELCID(MAKELANGID(LANG_UZBEK, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Vai", "vai", "" },
        { "Venda", "ven", "ve" },
        { "Vietnamese", "vie", "vi",             MAKELCID(MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Volapuk", "vol", "vo" },
        { "Votic", "vot", "" },
        { "Wakashan languages", "wak", "" },
        { "Walamo", "wal", "" },
        { "Walloon", "wln", "wa" },
        { "Waray", "war", "" },
        { "Washo", "was", "" },
        { "Welsh", "wel", "cy",                  MAKELCID(MAKELANGID(LANG_WELSH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Welsh", "cym", "cy",                  MAKELCID(MAKELANGID(LANG_WELSH, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Wolof", "wol", "wo",                  MAKELCID(MAKELANGID(LANG_WOLOF, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Xhosa", "xho", "xh",                  MAKELCID(MAKELANGID(LANG_XHOSA, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Yakut", "sah", "",                    MAKELCID(MAKELANGID(LANG_YAKUT, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Yao", "yao", "" },
        { "Yapese", "yap", "" },
        { "Yiddish", "yid", "yi" },
        { "Yoruba", "yor", "yo",                 MAKELCID(MAKELANGID(LANG_YORUBA, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Yupik languages", "ypk", "" },
        { "Zande", "znd", "" },
        { "Zapotec", "zap", "" },
        { "Zenaga", "zen", "" },
        { "Zhuang", "zha", "za" },
        { "Chuang", "zha", "za" },
        { "Zulu", "zul", "zu",                   MAKELCID(MAKELANGID(LANG_ZULU, SUBLANG_DEFAULT), SORT_DEFAULT) },
        { "Zuni", "zun", "" },
        { "", "", "" },
        { "No subtitles", "---", "", (LCID)LCID_NOSUBTITLES },
    };
};

CString ISOLang::ISO6391ToLanguage(LPCSTR code)
{
    CHAR tmp[2 + 1];
    strncpy_s(tmp, code, 2);
    tmp[2] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6391, tmp)) {
            return CString(CStringA(s_isolangs[i].name));
        }
    }
    return _T("");
}

CString ISOLang::ISO6392ToLanguage(LPCSTR code)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, code, 3);
    tmp[3] = 0;
    ASSERT(tmp[2] != '-');
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6392, tmp)) {
            return CString(CStringA(s_isolangs[i].name));
        }
    }
    return CString(code);
}

bool ISOLang::IsISO639Language(LPCSTR code, LCID* lcid)
{
    size_t nLen = strlen(code) + 1;
    LPSTR tmp = DEBUG_NEW CHAR[nLen];
    strncpy_s(tmp, nLen, code, nLen);
    _strlwr_s(tmp, nLen);
    tmp[0] = (CHAR)toupper(tmp[0]);

    bool bFound = false;
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].name, tmp)) {
            bFound = true;
            if (lcid != 0) {
                *lcid = s_isolangs[i].lcid;
            }
            break;
        }
    }

    delete[] tmp;

    return bFound;
}

CString ISOLang::ISO639XToLanguage(LPCSTR code)
{
    CString lang;

    switch (size_t nLen = strlen(code)) {
        case 2:
            lang = ISO6391ToLanguage(code);
            break;
        case 3:
            if (code[2] == '-') { // Truncated BCP47
                lang = ISO6391ToLanguage(code);
            } else {
                lang = ISO6392ToLanguage(code);
            }
            if (lang == code) { // When it can't find a match, ISO6392ToLanguage returns the input string
                lang.Empty();
            }
            break;
        case 5:
            lang = code;
            if (_wcsicmp(lang, L"pt-BR") == 0) {
                lang = L"Portuguese (Brazil)";
            } else if (_wcsicmp(lang, L"pt-PT") == 0) {
                lang = L"Portuguese";
            } else if (_wcsicmp(lang, L"zh-CN") == 0) {
                lang = L"Chinese (Simplified)";
            } else if (_wcsicmp(lang, L"zh-TW") == 0) {
                lang = L"Chinese (Traditional)";
            } else {
                ASSERT(FALSE);
            }
            break;
        default:
            lang = code;
            ASSERT(FALSE);
    }

    return lang;
}

CString ISOLang::LCIDToLanguage(LCID lcid)
{
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (lcid == s_isolangs[i].lcid) {
            return CString(CStringA(s_isolangs[i].name));
        }
    }
    return _T("");
}

CString ISOLang::LCIDToISO6392(LCID lcid)
{
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (lcid == s_isolangs[i].lcid) {
            return CString(s_isolangs[i].iso6392);
        }
    }
    return _T("unk");
}

LCID ISOLang::ISO6391ToLcid(LPCSTR code)
{
    CHAR tmp[2 + 1];
    strncpy_s(tmp, code, 2);
    tmp[2] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6391, tmp)) {
            return s_isolangs[i].lcid;
        }
    }
    return 0;
}

LCID ISOLang::ISO6392ToLcid(LPCSTR code)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, code, 3);
    tmp[3] = 0;
    if (tmp[2] == '-') { // Truncated BCP47
        tmp[2] = 0;
        return ISO6391ToLcid(tmp);
    }
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6392, tmp)) {
            return s_isolangs[i].lcid;
        }
    }
    return 0;
}

BOOL ISOLang::IsISO6391(LPCSTR code)
{
    CHAR tmp[2 + 1];
    strncpy_s(tmp, code, 2);
    tmp[2] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6391, tmp)) {
            return true;
        }
    }
    return false;
}

BOOL ISOLang::IsISO6392(LPCSTR code)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, code, 3);
    tmp[3] = 0;
    ASSERT(tmp[2] != '-');
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6392, tmp)) {
            return true;
        }
    }
    return false;
}

CStringA ISOLang::ISO6391To6392(LPCSTR code)
{
    CHAR tmp[2 + 1];
    strncpy_s(tmp, code, 2);
    tmp[2] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6391, tmp)) {
            return CStringA(s_isolangs[i].iso6392);
        }
    }
    return "";
}

CString ISOLang::ISO6392To6391(LPCSTR code)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, code, 3);
    ASSERT(tmp[2] != '-');
    tmp[3] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6392, tmp)) {
            return CString(s_isolangs[i].iso6391);
        }
    }
    return _T("");
}

CString ISOLang::LanguageToISO6392(LPCTSTR lang)
{
    CString str = lang;
    str.MakeLower();
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        CAtlList<CString> sl;
        Explode(CString(s_isolangs[i].name), sl, _T(';'));
        POSITION pos = sl.GetHeadPosition();
        while (pos) {
            if (!str.CompareNoCase(sl.GetNext(pos))) {
                return CString(s_isolangs[i].iso6392);
            }
        }
    }
    return _T("");
}

ISOLang ISOLang::ISO6391ToISOLang(LPCSTR code)
{
    CHAR tmp[2 + 1];
    strncpy_s(tmp, code, 2);
    tmp[2] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6391, tmp)) {
            return s_isolangs[i];
        }
    }
    return ISOLang();
}

ISOLang ISOLang::ISO6392ToISOLang(LPCSTR code)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, code, 3);
    tmp[3] = 0;
    ASSERT(tmp[2] != '-');
    _strlwr_s(tmp);
    for (size_t i = 0, cnt = _countof(s_isolangs); i < cnt; i++) {
        if (!strcmp(s_isolangs[i].iso6392, tmp)) {
            return s_isolangs[i];
        }
    }
    return ISOLang();
}

ISOLang ISOLang::ISO639XToISOLang(LPCSTR code)
{
    ISOLang lang;

    switch (strlen(code)) {
        case 2:
            lang = ISO6391ToISOLang(code);
            break;
        case 3:
            lang = ISO6392ToISOLang(code);
            break;
    }

    return lang;
}

CStringW ISOLang::GetLocaleStringCompat(LCID lcid) {
    CStringW langName;
    WCHAR strNameBuffer[LOCALE_NAME_MAX_LENGTH];
    if (0 == LCIDToLocaleName(lcid, strNameBuffer, LOCALE_NAME_MAX_LENGTH, 0)) {
        GetLocaleString(lcid, LOCALE_SISO639LANGNAME2, langName);
    } else {
        langName = strNameBuffer;
    }
    return langName;
}
