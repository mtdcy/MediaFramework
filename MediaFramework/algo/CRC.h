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


// File:    CRC.h
// Author:  mtdcy.chen
// Changes:
//          1. 20200716     initial version
//

#ifndef MFWK_ALGO_CRC_H
#define MFWK_ALGO_CRC_H

#include "MediaTypes.h"

__BEGIN_DECLS

// https://en.wikipedia.org/wiki/Cyclic_redundancy_check#Polynomial_representations_of_cyclic_redundancy_checks
// http://reveng.sourceforge.net/crc-catalogue/all.htm
enum {
    // 8-bits crc algo with poly 0x07
    kCRC8SMBUS = 0, ///< width=8 poly=0x07 init=0x00 reflected=False xorout=0x00
    kCRC8ITU,       ///< width=8 poly=0x07 init=0x00 reflected=False xorout=0x55
    // 8-bits crc algo with poly 0x1D
    kCRC8EBU,       ///< width=8 poly=0x1D init=0xFF reflected=True xorout=0x00
    // 16-bits crc algo with poly 0x8005
    kCRC16IBM,      ///< width=16 poly=0x8005 init=0x0000 reflected=True xorout=0x0000
    kCRC16UMTS,     ///< width=16 poly=0x8005 init=0x0000 reflected=False xorout=0x0000
    // 16 bits crc algo with poly 0x1021
    kCRC16KERMIT,   ///< width=16 poly=0x1021 init=0x0000 reflected=True xorout=0x0000
    kCRC16A,        ///< width=16 poly=0x1021 init=0xC6C6 reflected=True xorout=0x0000
    kCRC16B,        ///< width=16 poly=0x1021 init=0xFFFF reflected=True xorout=0xFFFF
    // 32-bits crc algo with poly 0x04C11DB7
    kCRC32ISO,      ///< width=32 poly=0x04C11DB7 init=0xFFFFFFFF reflected=True xorout=0xFFFFFFFF
    kCRC32BZIP2,    ///< width=32 poly=0x04C11DB7 init=0xFFFFFFFF reflected=False xorout=0xFFFFFFFF
    kCRC32MPEG2,    ///< width=32 poly=0x04C11DB7 init=0xFFFFFFFF reflected=False xorout=0x00000000
    kCRC32POSIX,    ///< width=32 poly=0x04C11DB7 init=0x00000000 reflected=False xorout=0xFFFFFFFF
    // 32-bits crc algo with poly 0x1EDC6F41
    kCRC32C,        ///< width=32 poly=0x1EDC6F41 init=0xFFFFFFFF reflected=True xorout=0xFFFFFFFF
    kCRCMax,
    
    // alias
    kCRC8           = kCRC8SMBUS,
    kCRC8AES        = kCRC8EBU,
    kCRC8ATM        = kCRC8ITU,
    kCRC8CCITT      = kCRC8SMBUS,
    kCRC16          = kCRC16IBM,
    kCRC16ANSI      = kCRC16IBM,
    kCRC16CCITT     = kCRC16KERMIT,
    kCRC32          = kCRC32ISO,
    kCRC32IEEE      = kCRC32ISO,
    kCRC32ISCSI     = kCRC32C,
};
typedef UInt32 eCRCType;

// As far as we known, RefIn == RefOut is always true, so
// replace RefIn & RefOut with Reflected, which also simplifies
// the crc implementation.
// XXX: there is a error in crc algo structure which is Init
// was affected by Reflected but XorOut is not. this is very
// strange why nobody found it for decades.
typedef struct CRCAlgo {
    const char *    Name;       ///< human readable identify
    const char *    Alias;      ///< alias list seperate by semicolon
    UInt32          Width;      ///< 4, 8, 16, 32
    UInt32          Poly;       ///< poly
    UInt32          Init;       ///< init value
    Bool            Reflected;  ///< replace RefIn/RefOut
    UInt32          XorOut;     ///< XOR to final crc value.
    UInt32          Check;      ///< TODO:
} CRCAlgo;

/**
* get a predefined crc algo.
* @return return a predefined crc algo on success, otherwise Nil.
*/
const CRCAlgo * GetCRCAlgo(eCRCType crc);

/**
 * generate a crc look-up table.
 * about look-up table length:
 * a crc look-up table MUST has 256 entries at least, but
 * in 2006, Intel engineer came up with a 4x or 8x table.
 * TODO: implement 4x or 8x table.
 * @return return True on success, otherwise False.
 * @note the table bytes >= (length * algo.Width) / 8
 * @note for developer only.
 */
Bool GenCRCTable(const CRCAlgo * algo, void * table, UInt32 length);

/**
 * generate a crc for given byte stream
 * @only support predefined crc algo, otherwise use class CRC
 */
UInt64 GenCRC(eCRCType type, const Char * data, UInt32 length);

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MFWK

String GetCRCAlgoString(const CRCAlgo& algo);

class CRC : public StaticObject {
    public:
        /**
         * create a crc instance
         * @note table will be generated if not exists.
         */
        CRC(eCRCType);
        CRC(const CRCAlgo& algo);
        ~CRC();
        
        /**
         * calc/update crc value.
         * @param pointer to byte stream and its length
         * @return return new crc value
         * @note no ABuffer here.
         */
        UInt64  update(const Char * data, const UInt32 length);
    
        /**
         * reset crc context.
         */
        void    reset();
        
    private:
        const CRCAlgo       mAlgo;
        UInt64              mCRC;
        UInt32              mLength;    // table length
        const void *        mTable;     // predefined table
        void *              mGenTable;  // calculated table
    
        DISALLOW_EVILS(CRC);
};

__END_NAMESPACE_MFWK
#endif // __cplusplus

#endif // MFWK_ALGO_CRC_H
