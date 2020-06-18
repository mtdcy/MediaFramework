/******************************************************************************
 * Copyright (c) 2016, Chen Fang <mtdcy.chen@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/


// File:    ID3.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "ID3"
//#define LOG_NDEBUG 0
#include "ID3.h"

#include <zlib.h>

// FIXME:  should provide this api
#include <stdio.h> // sscanf

// refers:
__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(ID3)

// supported v2.3.x & v2.4.x frames.
struct {
    const char *    id3;
    uint32_t        key;
} kSupportedID3v2_3Frames [] = {
    // XXX: correct if key is wrong.
    {"TALB",        kKeyAlbum       },
    {"TBPM",        kKeyBPM         },
    {"TCOM",        kKeyComposer    },
    {"TCON",        kKeyGenre       },
    {"TCOP",        kKeyCopyright   },
    {"TDAT",        kKeyDate        },
    //{"TDLY",        ''},
    {"TENC",        kKeyEncoder     },
    {"TEXT",        kKeyAuthor      },
    //{"TFLT",        ''},
    {"TIME",        FOURCC('time')  },
    {"TIT1",        kKeyTitle},
    {"TIT2",        kKeyTitle + 1   },
    {"TIT3",        kKeyTitle + 2   },
    //{"TKEY",        "initial-key"}, // ???
    {"TLAN",        kKeyLanguage},
    //{"TLEN",        "duration"},
    {"TPE1",        kKeyArtist      },
    {"TPE2",        kKeyAlbumArtist },
    {"TPE3",        kKeyPerformer   },
    {"TYER",        kKeyYear        },
    //{"TSIZ",        "size-in-bytes"},
    {"TRCK",        kKeyTrackNum    },
    //{"TDRC",        "recording-time"},
    {"TXXX",        kKeyCustom      },
    {0, NULL}
};

struct {
    const char *    id3;
    uint32_t        key;
} kSupportedID3v2_2Frames [] = {
    {"TAL",         kKeyAlbum       },
    {"TBP",         kKeyBPM         },
    {"TCM",         kKeyComposer    },
    {"TCO",         kKeyGenre       },
    {"TDA",         kKeyDate        },
    //{"TDY",         "playlist-delay"},
    {"TEN",         kKeyEncoder     },
    //{"TFT",         "file-type"},
    {"TIM",         FOURCC('time')  },
    //{'TKE',         "initial-key"},
    {"TLA",         kKeyLanguage    },
    //{'TLE',         "duration-in-ms"},
    //{'TMT',         ""},
    {"TP1",         kKeyArtist      },
    {"TP2",         kKeyAlbumArtist },
    {"TP3",         kKeyPerformer   },
    //{"TPA",         "group"},
    //{"TPB",         "publisher"},
    {"TRK",         kKeyTrackNum    },
    {"TT1",         kKeyTitle       },
    {"TT2",         kKeyTitle + 1   },
    {"TT3",         kKeyTitle + 2   },
    {"TYE",         kKeyYear        },
    {"TXX",         kKeyCustom      },
};

/* See Genre List at http://id3.org/id3v2.3.0 */
const int ID3GenreMax = 147;
const char * const ID3GenreList[] = {
    /*[0] =*/ "Blues",
    /*[1] =*/ "Classic Rock",
    /*[2] =*/ "Country",
    /*[3] =*/ "Dance",
    /*[4] =*/ "Disco",
    /*[5] =*/ "Funk",
    /*[6] =*/ "Grunge",
    /*[7] =*/ "Hip-Hop",
    /*[8] =*/ "Jazz",
    /*[9] =*/ "Metal",
    /*[10] =*/ "New Age",
    /*[11] =*/ "Oldies",
    /*[12] =*/ "Other",
    /*[13] =*/ "Pop",
    /*[14] =*/ "R&B",
    /*[15] =*/ "Rap",
    /*[16] =*/ "Reggae",
    /*[17] =*/ "Rock",
    /*[18] =*/ "Techno",
    /*[19] =*/ "Industrial",
    /*[20] =*/ "Alternative",
    /*[21] =*/ "Ska",
    /*[22] =*/ "Death Metal",
    /*[23] =*/ "Pranks",
    /*[24] =*/ "Soundtrack",
    /*[25] =*/ "Euro-Techno",
    /*[26] =*/ "Ambient",
    /*[27] =*/ "Trip-Hop",
    /*[28] =*/ "Vocal",
    /*[29] =*/ "Jazz+Funk",
    /*[30] =*/ "Fusion",
    /*[31] =*/ "Trance",
    /*[32] =*/ "Classical",
    /*[33] =*/ "Instrumental",
    /*[34] =*/ "Acid",
    /*[35] =*/ "House",
    /*[36] =*/ "Game",
    /*[37] =*/ "Sound Clip",
    /*[38] =*/ "Gospel",
    /*[39] =*/ "Noise",
    /*[40] =*/ "AlternRock",
    /*[41] =*/ "Bass",
    /*[42] =*/ "Soul",
    /*[43] =*/ "Punk",
    /*[44] =*/ "Space",
    /*[45] =*/ "Meditative",
    /*[46] =*/ "Instrumental Pop",
    /*[47] =*/ "Instrumental Rock",
    /*[48] =*/ "Ethnic",
    /*[49] =*/ "Gothic",
    /*[50] =*/ "Darkwave",
    /*[51] =*/ "Techno-Industrial",
    /*[52] =*/ "Electronic",
    /*[53] =*/ "Pop-Folk",
    /*[54] =*/ "Eurodance",
    /*[55] =*/ "Dream",
    /*[56] =*/ "Southern Rock",
    /*[57] =*/ "Comedy",
    /*[58] =*/ "Cult",
    /*[59] =*/ "Gangsta",
    /*[60] =*/ "Top 40",
    /*[61] =*/ "Christian Rap",
    /*[62] =*/ "Pop/Funk",
    /*[63] =*/ "Jungle",
    /*[64] =*/ "Native American",
    /*[65] =*/ "Cabaret",
    /*[66] =*/ "New Wave",
    /*[67] =*/ "Psychadelic", /* sic, the misspelling is used in the specification */
    /*[68] =*/ "Rave",
    /*[69] =*/ "Showtunes",
    /*[70] =*/ "Trailer",
    /*[71] =*/ "Lo-Fi",
    /*[72] =*/ "Tribal",
    /*[73] =*/ "Acid Punk",
    /*[74] =*/ "Acid Jazz",
    /*[75] =*/ "Polka",
    /*[76] =*/ "Retro",
    /*[77] =*/ "Musical",
    /*[78] =*/ "Rock & Roll",
    /*[79] =*/ "Hard Rock",
    /*[80] =*/ "Folk",
    /*[81] =*/ "Folk-Rock",
    /*[82] =*/ "National Folk",
    /*[83] =*/ "Swing",
    /*[84] =*/ "Fast Fusion",
    /*[85] =*/ "Bebob",
    /*[86] =*/ "Latin",
    /*[87] =*/ "Revival",
    /*[88] =*/ "Celtic",
    /*[89] =*/ "Bluegrass",
    /*[90] =*/ "Avantgarde",
    /*[91] =*/ "Gothic Rock",
    /*[92] =*/ "Progressive Rock",
    /*[93] =*/ "Psychedelic Rock",
    /*[94] =*/ "Symphonic Rock",
    /*[95] =*/ "Slow Rock",
    /*[96] =*/ "Big Band",
    /*[97] =*/ "Chorus",
    /*[98] =*/ "Easy Listening",
    /*[99] =*/ "Acoustic",
    /*[100] =*/ "Humour",
    /*[101] =*/ "Speech",
    /*[102] =*/ "Chanson",
    /*[103] =*/ "Opera",
    /*[104] =*/ "Chamber Music",
    /*[105] =*/ "Sonata",
    /*[106] =*/ "Symphony",
    /*[107] =*/ "Booty Bass",
    /*[108] =*/ "Primus",
    /*[109] =*/ "Porn Groove",
    /*[110] =*/ "Satire",
    /*[111] =*/ "Slow Jam",
    /*[112] =*/ "Club",
    /*[113] =*/ "Tango",
    /*[114] =*/ "Samba",
    /*[115] =*/ "Folklore",
    /*[116] =*/ "Ballad",
    /*[117] =*/ "Power Ballad",
    /*[118] =*/ "Rhythmic Soul",
    /*[119] =*/ "Freestyle",
    /*[120] =*/ "Duet",
    /*[121] =*/ "Punk Rock",
    /*[122] =*/ "Drum Solo",
    /*[123] =*/ "A capella",
    /*[124] =*/ "Euro-House",
    /*[125] =*/ "Dance Hall",
    /*[126] =*/ "Goa",
    /*[127] =*/ "Drum & Bass",
    /*[128] =*/ "Club-House",
    /*[129] =*/ "Hardcore",
    /*[130] =*/ "Terror",
    /*[131] =*/ "Indie",
    /*[132] =*/ "BritPop",
    /*[133] =*/ "Negerpunk",
    /*[134] =*/ "Polsk Punk",
    /*[135] =*/ "Beat",
    /*[136] =*/ "Christian Gangsta",
    /*[137] =*/ "Heavy Metal",
    /*[138] =*/ "Black Metal",
    /*[139] =*/ "Crossover",
    /*[140] =*/ "Contemporary Christian",
    /*[141] =*/ "Christian Rock",
    /*[142] =*/ "Merengue",
    /*[143] =*/ "Salsa",
    /*[144] =*/ "Thrash Metal",
    /*[145] =*/ "Anime",
    /*[146] =*/ "JPop",
    /*[147] =*/ "SynthPop",
};

///////////////////////////////////////////////////////////////////////
static FORCE_INLINE uint32_t ID3v2SynchSafeSize(uint32_t s) {
    // 0x80808080
    if (s & 0x80808080) return 0; // invalid sequence
    uint32_t v = s & 0x7f;
    v |= (s & 0x7f00) >> 1;
    v |= (s & 0x7f0000) >> 2;
    v |= (s & 0x7f000000) >> 3;
    return v;
}

static FORCE_INLINE uint32_t ID3v2SynchSafeSize(const char *size, size_t bytes) {
    uint32_t v = 0;
    for (size_t i = 0; i < bytes; i++) {
        if ((uint8_t)size[i] >= 0x80) return 0;  // invalid sequence.
        v = (v << 7) | size[i];
    }
    return v;
}

static FORCE_INLINE uint32_t ID3v2Size(const char *size, size_t bytes) {
    uint32_t v = 0;
    for (size_t i = 0; i < bytes; i++)  v = (((uint32_t)v)<<8) | (uint8_t)size[i];
    return v;
}

///////////////////////////////////////////////////////////////////////
//
// ID3v2/file identifier      "ID3"
// ID3v2 version              $04 00
// ID3v2 flags                %abcd0000
// ID3v2 size             4 * %0xxxxxxx
struct ID3v2Header {
    uint8_t     major;
    uint8_t     revision;
    uint8_t     flags;
    uint8_t     padding;    // for memory align.
    uint32_t    size;
    // The ID3v2 tag size is the size of the complete tag after unsychronisation,
    // including padding, excluding the header but not excluding the extended header
};

static bool isID3v2Header(const sp<Buffer>& data, ID3v2Header *header) {
    CHECK_GE(data->size(), ID3V2_HEADER_LENGTH);
    
    String magic        = data->rs(3);
    if (magic != "ID3") {
        DEBUG("no ID3v2 magic.");
        return false;
    }
    
    uint8_t major       = data->r8();
    uint8_t revision    = data->r8();
    uint8_t flags       = data->r8();
    uint32_t size       = ID3v2SynchSafeSize(data->rb32());
    
    // check version number.
    if (major == 0xff || revision == 0xff) {
        DEBUG("invalid version number 2.%d.%d",
              major, revision);
        return false;
    }
    
    // check size
    if (size == 0) {
        DEBUG("invalid size %d", size);
        return false;
    }
    
    // check flags.
    if (major == 2) { // %xx000000
        if (flags & 0x3f) {
            DEBUG("invalid 2.2.%d flags %#x", revision, flags);
            return false;
        }
    } else if (major == 3) { // %abc00000
        if (flags & 0x1f) {
            DEBUG("invalid 2.3.%d flags %#x", revision, flags);
            return false;
        }
    } else if (major == 4) { // %abcd0000
        if (flags & 0xf) {
            DEBUG("invalid 2.4.%d flags %#x", revision, flags);
            return false;
        }
    }
    
    if (header) {
        header->major       = major;
        header->revision    = revision;
        header->flags       = flags;
        header->size        = size;
    }
    return true;
}

static FORCE_INLINE bool isID3NumericString(const String& s) {
    DEBUG("isID3NumericString %s.", s.c_str());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    
    return true;
}

static FORCE_INLINE bool isID3v2TimeString(const String& s) {
    // valid time string:
    // yyyy,
    // yyyy-MM,
    // yyyy-MM-dd,
    // yyyy-MM-ddTHH,
    // yyyy-MM-ddTHH:mm and
    // yyyy-MM-ddTHH:mm:ss
    
    if (s.size() < 4) return false;
    
    // year
    if (!isID3NumericString(s.substring(0,4))) return false;
    
    // month
    if (s.size() >= 7) {
        if (s[4] != '-') return false;
        String month = s.substring(5, 2);
        if (!isID3NumericString(s.substring(5,2))) return false;
    }
    
    // day
    if (s.size() >= 10) {
        if (s[7] != '-') return false;
        if (!isID3NumericString(s.substring(8, 2))) return false;
    }
    
    // hour
    if (s.size() >= 13) {
        if (!isID3NumericString(s.substring(10, 3))) return false;
    }
    
    // minute
    if (s.size() >= 16) {
        if (s[13] != ':') return false;
        if (!isID3NumericString(s.substring(14, 2))) return false;
    }
    
    // seconds
    if (s.size() == 19) {
        if (s[16] != ':') return false;
        if (!isID3NumericString(s.substring(17,2))) return false;
    }
    
    if (s.size() > 19) return false;
    
    return true;
}

static FORCE_INLINE bool isID3v2FourCC(const String& s) {
    // this is not an accurate check.
    for (size_t i = 0; i < s.size(); i++) {
        if ((s[i] < 'A' || s[i] > 'Z') &&
            (s[i] < '0' || s[i] > '9')) {
            DEBUG("bad id3v2 fourcc [%s]", s.c_str());
            return false;
        }
    }
    
    return true;
}

static FORCE_INLINE size_t ID3v2RemoveUnsyncBytes(sp<Buffer>& buffer, const char* src, size_t srcLen) {
    size_t idx = 0;
    for (size_t i = 0; i < srcLen - 1; i++) {
        if (src[i] == '\xff' && src[i+1] == 0x0) {
            buffer->w8(src[i++]);
        } else {
            buffer->w8(src[i]);
        }
    }
    buffer->w8(src[srcLen-1]);
    
    return idx;
}

static FORCE_INLINE String ID3v2String(int encoding, const char* data, int length) {
    if (length == 0) return String();
    
    DEBUG("ID3v2String encoding = %d data = %p length = %d",
          encoding, data, length);
    
    String str;
    switch (encoding) {
        case 1:  // UTF16 with BOM
            if (length < 4) {
                DEBUG("UTF16 with BOM: not enough data.");
                break;
            }
            
            if (data[0] == '\xff' && data[1] == '\xfe') {
                DEBUG("UTF16BE with BOM");
                data    += 2;
                length  -= 2;
            } else if (data[0] == '\xfe' && data[1] == '\xff') {
                DEBUG("UTF16LE with BOM. swap bytes.");
                data    += 2;
                length  -= 2;
                
                // XXX: fixme
                char *buf = const_cast<char*>(data);
                // XXX: need swap utils.
                for (int i = 0; i < length; i += 2) {
                    char tmp = buf[i];
                    buf[i] = buf[i+1];
                    buf[i+1] = tmp;
                }
            } else {
                WARN("encoding == 1 but no BOM found.");
            }
            
        {
            bool eightBit = true;
            uint16_t *tmp = (uint16_t*)data;
            int len = length / 2;
            for (int i = 0; i < len; i++) {
                if (tmp[i] > 0xff) {
                    eightBit = false;
                    break;
                }
            }
            
            if (eightBit) {
                // let client to detect.
                DEBUG("collapse to 8 bit.");
                char *data8 = new char[len];
                for (int i = 0; i < len; i++) {
                    data8[i] = (char)tmp[i];
                }
                str = String(data8, len);
                delete [] data8;
            } else {
                str = String::UTF16(data, length);
            }
        }
            break;
        case 2:  // UTF16BE
            if (length < 2) {
                DEBUG("UTF16BE: not enought data.");
                break;
            }
            str = String::UTF16(data, length);
            break;
        case 0:  // ISO-8859-1
        case 3:  // UTF8
            // text encoding in id3v2 can not be trusted.
            str = String((char*)data, length);
            break;
        default:
            WARN("unknown text encoding %d.", encoding);
            break;
    }
    
    return str;
}

static FORCE_INLINE String ID3v2Text(const String& id, const char* data, int length) {
    // Text information frames excluding 'TXXX'
    // Text encoding    $xx
    // Information      <text string according to encoding>
    
    DEBUG("ID3v2Text: length = %d.", length);
    if (length <= 1) return String();
    
    const uint8_t encoding    = (uint8_t)data[0];
    String s = ID3v2String(encoding, &data[1], length - 1);
    
    DEBUG("text string: [%s], encoding %d.",
          s.c_str(), encoding);
    
    if (s.empty()) return s;
    
    if (id == "TCON" || id == "TCO") {
        int genre;
        if ((sscanf(s.c_str(), "(%d)", &genre) == 1 ||
             sscanf(s.c_str(), "%d", &genre) == 1)) {
            if (genre >= 0 && genre < ID3GenreMax) {
                s = ID3GenreList[genre];
            } else {
                ERROR("invalid genre string: [%s].", s.c_str());
                s.clear();
            }
        }
    } else if (id == "TYER" || id == "TYE") {
        if (!isID3NumericString(s)) {
            ERROR("invalid year string.");
            s.clear();
        }
    } else if (id == "TDRC") {
        if (!isID3v2TimeString(s)) {
            DEBUG("invalid time string.");
            s.clear();
        }
    }
    
    return s;
}

struct ID3v2CommentText {
    String  lang;
    String  desc;
    String  text;
};

static FORCE_INLINE ID3v2CommentText ID3v2Comment(const String& id, const char* data, int length) {
    // <Header for 'Comment', ID: "COMM">
    // Text encoding           $xx
    // Language                $xx xx xx
    // Short content descrip.  <text string according to encoding> $00 (00)
    // The actual text         <full text string according to encoding>
    
    // no actual text.
    if (length <= 5) return ID3v2CommentText();
    
    const uint8_t encoding  = data[0];
    const char *lang        = data + 1;
    const char *desc        = data + 4;
    const char *text        = desc;
    while ((*text) != '\0') ++text;
    while ((*text) == '\0') ++text; // skip '\0'. unicode may has two 0 bytes.
    
    // no actual text.
    if (text >= data + length) return ID3v2CommentText();
    
    ID3v2CommentText comment;
    comment.lang = String((char*)lang, 3);      // not NULL-terminated
    comment.desc = ID3v2String(encoding, desc, text - desc); // NULL-terminated.
    comment.text = ID3v2String(encoding, text, data + length - text); // not NULL-terminated.
    
    DEBUG("comment: [%s] [%s], encoding %d, language [%s]",
          comment.desc.c_str(),
          comment.text.c_str(),
          encoding,
          comment.lang.c_str());
    
    return comment;
}

struct ID3v2PictureText {
    String      mime;
    int         type;
    String      desc;
    sp<Buffer>  pic;
};

static FORCE_INLINE ID3v2PictureText ID3v2Picture(const String& id, const char* data, int length) {
    // <Header for 'Attached picture', ID: "APIC">
    // Text encoding   $xx
    // MIME type       <text string> $00
    // Picture type    $xx
    // Description     <text string according to encoding> $00 (00)
    // Picture data    <binary data>
    
    const char *tmp         = data;
    const uint8_t encoding  = *tmp++;
    const char *mime        = tmp;
    while ((*tmp) != '\0') ++tmp;
    while ((*tmp) == '\0') ++tmp; // skip '\0'. unicode may has two 0 bytes.
    const uint8_t type      = (uint8_t)(*tmp++);
    const char *descrip     = tmp;
    while ((*tmp) != '\0') ++tmp;
    while ((*tmp) == '\0') ++tmp; // skip '\0'. unicode may has two 0 bytes.
    const char *binary      = tmp;
    const int bsize         = data + length - binary;
    
    DEBUG("apic: %d, encoding %d, mime %s, descrip %s, size %d.",
          type, encoding, mime, descrip, bsize);
    
    ID3v2PictureText picture;
    picture.mime = (char*)mime;     // NULL-terminated
    picture.type = type;
    picture.desc = ID3v2String(encoding, descrip, binary - descrip);
    picture.pic  = new Buffer(binary, bsize);
    
    return picture;
}

static FORCE_INLINE ID3v2PictureText ID3v22Picture(const String& id, const char* data, int length) {
    // <Header for 'Attached picture', ID: "PIC">
    // Text encoding      $xx
    // Image format       $xx xx xx
    // Picture type       $xx
    // Description        <textstring> $00 (00)
    // Picture data       <binary data>
    
    const char *tmp         = data;
    const uint8_t encoding  = (uint8_t)(*tmp++);
    const char *format      = tmp; tmp += 3;
    const uint8_t type      = (uint8_t)(*tmp++);
    const char *descrip     = tmp;
    while ((*tmp) != '\0') ++tmp;
    while ((*tmp) == '\0') ++tmp; // skip '\0'. unicode may has two 0 bytes.
    const char *binary      = tmp;
    const int bsize         = length - (binary - data);
    
    const char *mime = "image/jpeg";
    if (String(format).startsWith("PNG")) {
        mime = "image/png";
    }
    
    ID3v2PictureText picture;
    picture.mime = (char*)mime;     // NULL-terminated
    picture.type = type;
    picture.desc = (char*)descrip;  // NULL-terminated
    picture.pic  = new Buffer(binary, bsize);
    
    return picture;
}

// Frame ID       $xx xx xx
// Size           $xx xx xx
static MediaError ID3v2_2(const sp<Buffer>& data,
                        const ID3v2Header& header,
                        sp<Message>& values) {
    struct ID3v2Frame {
        char    id[3];
        // The size is calculated as frame size excluding frame header
        char    size[3];
    };
    
    int offset = ID3V2_HEADER_LENGTH;
    const size_t length     = data->size();
    const char *buffer      = data->data();
    
    while (offset + sizeof(ID3v2Frame) < length) {
        ID3v2Frame *frame   = (ID3v2Frame*)(buffer + offset);
        offset              += sizeof(ID3v2Frame);
        
        String id(frame->id, 3);
        uint32_t frameLength = ID3v2Size(frame->size, 3);
        DEBUG("frame [%s] length %d.", id.c_str(), frameLength);
        
        if (frameLength == 0 || frameLength + offset > length) {
            ERROR("skip padding %" PRId64 " bytes.", length - offset);
            break;
        }
        
        const char* frameData = buffer + offset;
        offset += frameLength;
        
        if (id.startsWith("T")) {
            String text = ID3v2Text(id, frameData, frameLength);
            if (!text.empty()) {
                int match = 0;
                for (int i = 0; kSupportedID3v2_2Frames[i].key; i++) {
                    if (id == kSupportedID3v2_2Frames[i].id3) {
                        values->setString(kSupportedID3v2_2Frames[i].key, text);
                        match = 1;
                        break;
                    }
                }
                
                if (!match) {
                    DEBUG("been ignored frame [%s] - %s.",
                          id.c_str(), text.c_str());
                }
            }
        } else if (id == "COM") {
            ID3v2CommentText comment = ID3v2Comment(id, frameData, frameLength);
            if (!comment.text.empty()) {
                if (comment.desc.empty())
                    values->setString(kKeyComment, comment.text);
                else
                    values->setString(kKeyComment, comment.desc + comment.text);
            }
        } else if (id == "APIC") {
            ID3v2PictureText picture = ID3v22Picture(id, frameData, frameLength);
            // XXX: set the right tag.
            values->setObject(kKeyAlbumArt, picture.pic);
        } else {
            DEBUG("been skipped frame [%s] length %d.",
                  id.c_str(), frameLength);
        }
    }
    
    return kMediaNoError;
}

// Frame ID       $xx xx xx xx (four characters)
// Size           $xx xx xx xx
// Flags          $xx xx
static MediaError ID3v2_3(const sp<Buffer>& data,
                        const ID3v2Header& header,
                        sp<Message>& values) {
    sp<Buffer> local;   // for de-unsync
    
    struct ID3v2Frame {
        char    id[4];
        // The size is calculated as frame size excluding frame header
        char    size[4];
        // %abc00000 %ijk00000
        char    flags[2];
    };
    
    size_t offset = ID3V2_HEADER_LENGTH;
    // extend header
    if (header.flags & 0x40) {
        size_t exLen = ID3v2SynchSafeSize(data->data() + offset, 4);
        DEBUG("extend header length %zu", exLen);
        // in 2.4.x exlen include 4bytes size but 2.3.x is not.
        offset  += (exLen + 4);
    }
    
    const char *buffer  = data->data();
    size_t length       = data->size();
    
    // remove unsync bytes.
    // ID3v2.3 use only global unsync flag.
    // in frame header, size field may has false sync bytes.
    if (header.flags & 0x80) {
        local   = new Buffer(length);
        length  = ID3v2RemoveUnsyncBytes(local, buffer, length);
        
        buffer  = local->data();
        DEBUG("de-unsync size %d.", length);
    }
    
    while (offset + sizeof(ID3v2Frame) < length) {
        ID3v2Frame *frame   = (ID3v2Frame*)(buffer + offset);
        offset              += sizeof(ID3v2Frame);
        
        String id(frame->id, 4);
        // v2.3.x does NOT use synch safe integer.
        uint32_t frameLength = ID3v2Size(frame->size, 4);
        DEBUG("frame [%s] length %u, flags %#x %#x.",
              id.c_str(),
              frameLength,
              frame->flags[0], frame->flags[1]);
        
        if (frameLength == 0 && id == "PRIV") {
            // some 'PRIV' frame has 0 bytes data.
            WARN("0 bytes 'PRIV' frame.");
            continue;
        }
        
        if (frameLength == 0 || frameLength + offset > length) {
            ERROR("skip padding %d bytes.", length - offset);
            break;
        }
        
        const char* frameData = buffer + offset;
        offset += frameLength;
        
        // encrypted.
        if (frame->flags[1] & 0x40) {
            // XXX: not supported yet.
            INFO("TODO: add encrypted frame support.");
            continue;
        }
        
        if (frame->flags[1] & 0x20) {
            WARN("TODO: add group frames support.");
            //continue;
        }
        
        sp<Buffer> uncompData;
        if (frame->flags[1] & 0x80) {
            uLongf uncompLength = ID3v2Size(frameData, 4);
            uncompData  = new Buffer(uncompLength);
            
#if 0 // TODO
            const Bytef *compData = (const Bytef*)(frameData + 4);
            uLong compLength = frameLength - 4;
            
            int zret = uncompress((Bytef*)uncompData->data(), &uncompLength,
                                  compData, compLength);
            
            if (zret != Z_OK) {
                DEBUG("uncompress failed, ret = %d.\n", zret);
            } else {
                uncompData->stepBytes(uncompLength);
                frameData   = uncompData->data();
                frameLength = uncompLength;
            }
#endif
        }
        
        if (id.startsWith("T")) {
            String text = ID3v2Text(id, frameData, frameLength);
            if (!text.empty()) {
                int match = 0;
                for (int i = 0; kSupportedID3v2_3Frames[i].key; i++) {
                    if (id == kSupportedID3v2_3Frames[i].id3) {
                        values->setString(kSupportedID3v2_3Frames[i].key, text);
                        match = 1;
                        break;
                    }
                }
                
                if (!match) {
                    DEBUG("been ignored frame [%s] - %s.",
                          id.c_str(), text.c_str());
                }
            }
        } else if (id.startsWith("W")) {
            // url text.
            String url = ID3v2String(0, frameData, frameLength);
            DEBUG("URL text: [%s] [%s]",
                  id.c_str(), url.c_str());
        } else if (id == "COMM") {
            ID3v2CommentText comment = ID3v2Comment(id, frameData, frameLength);
            if (!comment.text.empty()) {
                if (comment.desc.empty())
                    values->setString(kKeyComment, comment.text);
                else
                    values->setString(kKeyComment, comment.desc + comment.text);
            }
        } else if (id == "APIC") {
            ID3v2PictureText picture = ID3v2Picture(id, frameData, frameLength);
            // XXX: set the right tag.
            values->setObject(kKeyAlbumArt, picture.pic);
        } else {
            DEBUG("been skipped frame [%s] length %d.",
                  id.c_str(),
                  frameLength);
        }
    }
    
    return kMediaNoError;
}

static MediaError ID3v2_4(const sp<Buffer>& data,
                        const ID3v2Header& header,
                        sp<Message>& values) {
    struct ID3v2Frame {
        char    id[4];
        // The size is calculated as frame size excluding frame header
        char    size[4];
        // %0abc0000 %0h00kmnp
        char    flags[2];
    };
    
    size_t offset = ID3V2_HEADER_LENGTH;
    // extend header
    if (header.flags & 0x40) {
        size_t exLen = ID3v2SynchSafeSize(data->data() + offset, 4);
        DEBUG("extend header length %d", exLen);
        offset  += exLen;
    }
    
    const int unsync = header.flags & 0x80;
    
    size_t length = data->size();
    while (offset + sizeof(ID3v2Frame) < length) {
        ID3v2Frame *frame   = (ID3v2Frame*)(data->data() + offset);
        offset              += sizeof(ID3v2Frame);
        
        String id(frame->id, 4);
        uint32_t frameLength = ID3v2SynchSafeSize(frame->size, 4);
        
#if 1
        // some program incorrectly use v2.3 size instead of syncsafe one
        if (frameLength > 0x7f || frameLength == 0) {
            // where frameLength < a.
            int32_t a = ID3v2Size(frame->size, 4);
            if (offset + a < length) {
                // if it is a valid frame header at the next offset
                // according to current syncsafe frame length.
                ID3v2Frame *next = (ID3v2Frame*)(data->data() + offset + frameLength);
                String nextId(next->id, 4);
                if (isID3v2FourCC(nextId)) {
                    DEBUG("correct frame length from %d -> %d.",
                          frameLength, a);
                    frameLength = a;
                }
            }
        }
#endif
        
        DEBUG("frame [%s] length %u, flags %#x %#x.",
              id.c_str(),
              frameLength,
              frame->flags[0], frame->flags[1]);
        
        if (frameLength == 0 || frameLength + offset > length) {
            ERROR("skip padding %d bytes.", length - offset);
            break;
        }
        
        const char* frameData = data->data() + offset;
        offset += frameLength;
        
        if (frameLength == 0 && id == "PRIV") {
            // some 'PRIV' frame has 0 bytes data.
            WARN("0 bytes 'PRIV' frame.");
            continue;
        }
        
        // encrypted.
        if (frame->flags[1] & 0x04) {
            INFO("TODO: add encrypted frame support.");
            offset += frameLength; // skip
            continue;
        }
        
        if (frame->flags[1] & 0x40) {
            WARN("TODO: add support for group frames.");
            //continue;
        }
        
        // unsync
        sp<Buffer> local; // for de-unsync or uncompress
        if (unsync || (frame->flags[1] & 0x02)) {
            DEBUG("unsync frame.");
            if (frame->flags[1] & 0x08) {
                DEBUG("data length indicator set.");
                frameData   += 4;
                frameLength -= 4;
            }
            
#if 0 // TODO
            local   = new Buffer(frameLength);
            
            frameLength = ID3v2RemoveUnsyncBytes(frameData, local->data(), frameLength);
            local->stepBytes(frameLength);
            
            frameData = local->data();
#endif
        }
        
        // compressed. unsync first, then uncompress
        if (frame->flags[1] & 0x08) {
            if (!(frame->flags[1] & 0x01)) {
                DEBUG("data length indicator not set.");
            }
#if 0   // TODO
            uLongf uncompLength     = ID3v2SynchSafeSize(frameData, 4);
            sp<Buffer> uncompData   = new Buffer(uncompLength);
            
            const Bytef *compData   = (const Bytef*)(frameData + 4);
            uLong compLength        = frameLength - 4;
            
            int zret = uncompress((Bytef*)uncompData->data(), &uncompLength,
                                  compData, compLength);
            
            if (zret != Z_OK) {
                DEBUG("uncompress failed, ret = %d.\n", zret);
            } else {
                uncompData->stepBytes(uncompLength);
                
                frameData   = uncompData->data();
                frameLength = uncompLength;
                local       = uncompData;   // keep a strong ref
            }
#endif
        }
        
        if (id.startsWith("T")) {
            String text = ID3v2Text(id, frameData, frameLength);
            if (!text.empty()) {
                int match = 0;
                for (int i = 0; kSupportedID3v2_3Frames[i].key; i++) {
                    if (id == kSupportedID3v2_3Frames[i].id3) {
                        values->setString(kSupportedID3v2_3Frames[i].key, text);
                        match = 1;
                        break;
                    }
                }
                
                if (!match) {
                    DEBUG("been ignored frame [%c%c%c%c] - %s.",
                          frame->id[0], frame->id[1],
                          frame->id[2], frame->id[3],
                          text.c_str());
                } else if (id == "TDRC") {
                    values->setString(kKeyYear, text.substring(0, 4));
                }
            }
        } else if (id.startsWith("W")) {
            // url text.
            String url = ID3v2String(0, frameData, frameLength);
            DEBUG("URL text: [%s] [%s]",
                  id.c_str(), url.c_str());
        } else if (id == "COMM") {
            ID3v2CommentText comment = ID3v2Comment(id, frameData, frameLength);
            if (!comment.text.empty()) {
                if (comment.desc.empty())
                    values->setString(kKeyComment, comment.text);
                else
                    values->setString(kKeyComment, comment.desc + comment.text);
            }
        } else if (id == "APIC") {
            ID3v2PictureText picture = ID3v2Picture(id, frameData, frameLength);
            // XXX: set the right tag.
            values->setObject(kKeyAlbumArt, picture.pic);
        } else {
            DEBUG("been skipped frame [%s] length %d.",
                  id.c_str(),
                  frameLength);
        }
        
    }
    
    return kMediaNoError;
}

////////////////////////////////////////////////////////////////////////////////////////////
static sp<Message> ParseID3v2(const sp<ABuffer>& data, ID3v2Header& header) {
    CHECK_GE(data->size(), header.size);
    DEBUG("ID3 v2.%d.%d length %d.", header.major, header.revision,
          header.size);
    
    sp<Message> values = new Message;
    switch (header.major) {
        case 2:
            return ID3v2_2(data, header, values) == kMediaNoError ? values : NULL;
        case 3:
            return ID3v2_3(data, header, values) == kMediaNoError ? values : NULL;
        case 4:
            return ID3v2_4(data, header, values) == kMediaNoError ? values : NULL;
        default:
            FATAL("FIXME: ID3v2.%u", header.major);
    }
    // FIX warning
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////
// ID3v1 routines.
//
static FORCE_INLINE String ID3v1String(const char* data, int length) {
    String str((char*)data, length);
    str.trim(); // this is neccessary for ID3v1
    return str;
}

////////////////////////////////////////////////////////////////////////////////////////////
static sp<Message> ParseID3v1(const sp<ABuffer>& data) {
    CHECK_GE(data->size(), ID3V1_LENGTH);
    
    if (data->rs(3) != "TAG") {
        DEBUG("missing TAG");
        return NULL;
    }
    
    sp<Message> values = new Message;
    
    // trim is neccessary for ID3v1
    String title = data->rs(30).trim();
    if (!title.empty()) {
        DEBUG("title: [%s].", title.c_str());
        values->setString(kKeyTitle, title);
    }
    
    String artist = data->rs(30).trim();
    if (!artist.empty()) {
        DEBUG("artist: [%s].", artist.c_str());
        values->setString(kKeyArtist, artist);
    }

    String album = data->rs(30).trim();
    if (!album.empty()) {
        DEBUG("album: [%s].", album.c_str());
        values->setString(kKeyAlbum, album);
    }
    
    String year = data->rs(4);
    if (!year.empty()) {
        DEBUG("year: [%s].", year.c_str());
        if (isID3NumericString(year))
            values->setString(kKeyYear, year);
        else {
            DEBUG("invalid id3 numeric string.");
        }
    }

    String comment = data->rs(30).trim();
    if (!comment.empty()) {
        DEBUG("comment: [%s].", comment.c_str());
        values->setString(kKeyComment, comment);
    }
    
    // ID3v1.1
    data->skipBytes(-2);
    if (data->r8() == 0) {
        uint8_t trck = data->r8();
        DEBUG("track: [%d].", trck);
        values->setString(kKeyTrackNum, String::format("%u", trck));
    } else {
        data->skipBytes(1);
    }
    
    uint8_t genre = data->r8();
    if (genre < 80) {
        DEBUG("genre: [%s].", ID3GenreList[genre]);
        values->setString(kKeyGenre, ID3GenreList[genre]);
    }

    return values;
}

MediaError SkipID3v2(const sp<ABuffer>& buffer) {
    if (buffer->size() < ID3V2_HEADER_LENGTH) {
        return kMediaErrorBadContent;
    }
    
    ID3v2Header header;
    if (isID3v2Header(buffer, &header) == false) {
        return kMediaErrorBadContent;
    }
    
    buffer->skipBytes(header.size);
    // stop at the end position of id3v2
    return kMediaNoError;
}

sp<Message> ReadID3v2(const sp<ABuffer>& buffer) {
    if (buffer->size() < ID3V2_HEADER_LENGTH) {
        return NULL;
    }
    
    sp<ABuffer> data = buffer->readBytes(ID3V2_HEADER_LENGTH);
    ID3v2Header header;
    if (isID3v2Header(data, &header) == false) {
        ERROR("NO ID3v2 header");
        buffer->skipBytes(-ID3V2_HEADER_LENGTH);
        return NULL;
    }
    
    if (buffer->size() < header.size) {
        ERROR("no enough data");
        buffer->skipBytes(-ID3V2_HEADER_LENGTH);
        return NULL;
    }
    
    data = buffer->readBytes(header.size);
    sp<Message> values = ParseID3v2(data, header);
    if (values.isNIL()) {
        ERROR("ID3v2 parse failed");
        buffer->skipBytes(-ID3V2_HEADER_LENGTH-header.size);
        return NULL;
    }
    
    // stop at the end position of id3v2
    return values;
}

sp<Message> ReadID3v1(const sp<ABuffer>& buffer) {
    // where id3v1 located
    const int64_t offset = buffer->offset();
    buffer->skipBytes(buffer->size() - ID3V1_LENGTH);
    
    sp<ABuffer> data = buffer->readBytes(ID3V1_LENGTH);
    if (data.isNIL() || data->size() < ID3V1_LENGTH) {
        // don't support seek ?
        return NULL;
    }
    
    sp<Message> values = ParseID3v1(data);
    
    // stop at the begin position of id3v1
    buffer->skipBytes(- ID3V1_LENGTH);
    return values;
}

__END_NAMESPACE(ID3)
__END_NAMESPACE_MPX

