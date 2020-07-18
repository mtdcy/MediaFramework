/******************************************************************************
 * Copyright (c) 2020, Chen Fang <mtdcy.chen@gmail.com>
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


// File:    crc_main.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20200716     initial version
//

#define LOG_TAG "crc.main"
#include <ABE/ABE.h>
#include <stdio.h>
#include "algo/CRC.h"

USING_NAMESPACE_MFWK

static const Char * copyright =
"crc Copyright (c) 2020, Chen Fang <mtdcy.chen@gmail.com>\n";

static const Char * help = "Usage:                              \n\
  crc list                              : list crc algo names   \n\
  crc <algo> <path_to_file>             : print crc of a file   \n\
  crc gentable [options]                : generate crc table    \n\
    example: crc gentable width=8 poly=0x07 init=0x00 reflected=False xorout=0x00   \n\
\n";

static String read_value(const String& par) {
    Int at = par.indexOf("=");
    if (at < 0) {
        FATAL("bad parameter %s", par.c_str());
    }
    return par.substring(at + 1);
}

static CRCAlgo read_algo(int argc, char ** argv, UInt32 start) {
    CRCAlgo algo;
    algo.Name = algo.Alias = Nil;
    
    for (UInt32 i = start; i < argc; ++i) {
        String par = argv[i];
        // poly=0x07 init=0x00 reflected=False xorout=0x00 length=256
        if (par.startsWith("width=", True)) {
            algo.Width = read_value(par).toInt32();
        } else if (par.startsWith("poly=", True)) {
            algo.Poly = read_value(par).toInt32();
        } else if (par.startsWith("init=", True)) {
            algo.Init = read_value(par).toInt32();
        } else if (par.startsWith("reflected=", True)) {
            algo.Reflected = read_value(par).equals("True", True) ? True : False;
        } else if (par.startsWith("xorout=", True)) {
            algo.XorOut = read_value(par).toInt32();
        //} else if (par.startsWith("length=", True)) {
        //    algo.Length = read_value(par).toInt32();
        } else {
            ERROR("unknown parameter %s", par.c_str());
        }
    }
    return algo;
}

static Bool gentable(CRCAlgo& algo) {
    printf("gentable %s\n", GetCRCAlgoString(algo).c_str());
    
    const UInt32 length = 256;
    void * table = kAllocatorDefault->allocate((length * algo.Width) / 8);
    Bool success = GenCRCTable(&algo, table, length);
    if (success) {
        if (algo.Width == 8) {
            UInt8 * u8 = (UInt8 *)table;
            for (UInt32 i = 0; i < length;) {
                String line;
                for (UInt32 j = 0; j < 8 && i < length; ++j, ++i) {
                    line += String::format("0x%02" PRIX8 ", ", u8[i]);
                }
                printf("%s\n", line.c_str());
            }
        } else if (algo.Width == 16) {
            UInt16 * u16 = (UInt16 *)table;
            for (UInt32 i = 0; i < length;) {
                String line;
                for (UInt32 j = 0; j < 8 && i < length; ++j, ++i) {
                    line += String::format("0x%04" PRIX16 ", ", u16[i]);
                }
                printf("%s\n", line.c_str());
            }
        } else if (algo.Width == 32) {
            UInt32 * u32 = (UInt32 *)table;
            for (UInt32 i = 0; i < length;) {
                String line;
                for (UInt32 j = 0; j < 8 && i < length; ++j, ++i) {
                    line += String::format("0x%08" PRIX32 ", ", u32[i]);
                }
                printf("%s\n", line.c_str());
            }
        }
    } else {
        printf("gentable failed.\n");
    }
    
    kAllocatorDefault->deallocate(table);
    return success;
}

static const CRCAlgo * get_algo(const String& name) {
    for (eCRCType i = 0; i < kCRCMax; ++i) {
        const CRCAlgo * algo = GetCRCAlgo(i);
        if (name == algo->Name) return algo;
        else if (algo->Alias) {
            String alias = algo->Alias;
            if (alias.indexOf(name) >= 0) return algo;
        }
    }
    return Nil;
}

int main(int argc, char ** argv) {
    if (argc == 1) {
        printf("%s", copyright);
        printf("%s", help);
        return 0;
    }
    
    String first = argv[1];
    if (first == "gentable") {
        CRCAlgo algo = read_algo(argc, argv, 2);
        return gentable(algo);
    } else if (first == "list") {
        printf("available crc algo:\n");
        for (eCRCType i = 0; i < kCRCMax; ++i) {
            const CRCAlgo * algo = GetCRCAlgo(i);
            printf("  %s\n", GetCRCAlgoString(*algo).c_str());
        }
    } else {
        const CRCAlgo * algo = get_algo(first);
        if (algo == Nil) {
            printf("invalid command %s\n", first.c_str());
            return 1;
        }
        
        CRC crc(*algo);
        UInt64 x;
        
        sp<ABuffer> media = Content::Create(argv[2]);
        
        Time now = Time::Now();
        for (;;) {
            sp<ABuffer> data = media->readBytes(32*1024);
            if (data.isNil()) break;
            
            x = crc.update(data->data(), data->size());
        }
        
        Time delta = Time::Now() - now;
        printf("%s => %s: 0x%" PRIx64 ", takes %.3f(s), %.3f MBps \n",
               argv[2], algo->Name, x, delta.seconds(),
               (media->capacity() / delta.seconds()) / (1024 * 1024));
    }
    return 0;
}
