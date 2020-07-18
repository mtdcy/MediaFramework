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


// File:    test.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version

#define LOG_TAG "MediaTest"
#include <MediaFramework/MediaFramework.h>
#include "algo/CRC.h"

#include <gtest/gtest.h>

USING_NAMESPACE_MFWK

static const char * gCurrentDir = NULL;

struct MyTest : public ::testing::Test {
    MyTest () {
    }

    virtual ~MyTest() {
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
};

void testCRC() {
    // https://crccalc.com/
    // http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
    const Char * data = "1234567890abcdefgh";
    
    ASSERT_EQ(GenCRC(kCRC8SMBUS, data, 18), 0x06);
    ASSERT_EQ(GenCRC(kCRC8ITU, data, 18), 0x53);
    ASSERT_EQ(GenCRC(kCRC8EBU, data, 18), 0x33);
    
    ASSERT_EQ(GenCRC(kCRC16IBM, data, 18), 0x233B);
    ASSERT_EQ(GenCRC(kCRC16UMTS, data, 18), 0xF173);
    
    ASSERT_EQ(GenCRC(kCRC16KERMIT, data, 18), 0xECD9);
    ASSERT_EQ(GenCRC(kCRC16A, data, 18), 0xC8B6);
    ASSERT_EQ(GenCRC(kCRC16B, data, 18), 0xC684);
    
    ASSERT_EQ(GenCRC(kCRC32ISO, data, 18), 0x83826287);
    ASSERT_EQ(GenCRC(kCRC32BZIP2, data, 18), 0x18F81443);
    ASSERT_EQ(GenCRC(kCRC32MPEG2, data, 18), 0xE707EBBC);
    ASSERT_EQ(GenCRC(kCRC32POSIX, data, 18), 0x55F4335A);
    
    ASSERT_EQ(GenCRC(kCRC32C, data, 18), 0xE92F8E88);
}

void testMediaTime() {
    MediaTime time = 0;
    
    // operator+()
    ASSERT_TRUE((time + MediaTime(1, 2)) == MediaTime(1, 2));
    
    // operator+=()
    time += MediaTime(1, 2);    // 1/2
    time += MediaTime(1, 3);    // 1/2 + 1/3 = 5/6
    ASSERT_TRUE(time == MediaTime(5,6));
    
    // operator-()
    ASSERT_TRUE(time - MediaTime(1, 3) == MediaTime(1,2));
    
    // operator-=()
    time -= MediaTime(1,3);     // 5/6 - 1/3 = 1/2
    ASSERT_TRUE(time == MediaTime(1,2));
    
    // useconds()
    ASSERT_TRUE(time.useconds() == Time::MicroSeconds(500000LL));
    
    // seconds()
    ASSERT_TRUE(time.seconds() == Time::Seconds(0.5f));
}

void testClock() {
    sp<SharedClock> clock = new SharedClock();
    sp<Clock> master = new Clock(clock, kClockRoleMaster);
    sp<Clock> slave = new Clock(clock);
    
    ASSERT_EQ(clock->get(), 0);
    ASSERT_EQ(master->get(), 0);
    ASSERT_EQ(slave->get(), 0);
    ASSERT_EQ(master->role(), kClockRoleMaster);
    ASSERT_EQ(slave->role(), kClockRoleSlave);
    ASSERT_EQ(clock->isPaused(), true);
    ASSERT_EQ(master->isPaused(), true);
    ASSERT_EQ(slave->isPaused(), true);
    ASSERT_EQ(clock->speed(), 1.0f);
    ASSERT_EQ(master->speed(), 1.0f);
    ASSERT_EQ(slave->speed(), 1.0f);
    
    clock->set(500);
    ASSERT_EQ(clock->get(), 500);
    ASSERT_EQ(master->get(), 500);
    ASSERT_EQ(slave->get(), 500);
    
    clock->start();
    ASSERT_EQ(clock->isPaused(), false);
    ASSERT_EQ(master->isPaused(), false);
    ASSERT_EQ(slave->isPaused(), false);
    
    // wait master clock to update
    ASSERT_EQ(clock->get(), 500);
    ASSERT_EQ(master->get(), 500);
    ASSERT_EQ(slave->get(), 500);
    
    master->update(1000);
    Timer().sleep(Time::MicroSeconds(1));
    ASSERT_GT(clock->get(), 1000);
    ASSERT_GT(master->get(), 1000);
    ASSERT_GT(slave->get(), 1000);
}

#define TEST_ENTRY(FUNC)                    \
    TEST_F(MyTest, FUNC) {                  \
        INFO("Begin Test MyTest."#FUNC);    \
        MemoryAnalyzerPrepare(); {          \
            FUNC();                         \
        } MemoryAnalyzerFinalize();         \
        INFO("End Test MyTest."#FUNC);      \
    }

TEST_ENTRY(testMediaTime);
TEST_ENTRY(testClock);
TEST_ENTRY(testCRC);

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    
    if (argc > 1) gCurrentDir = argv[1];

    int result = RUN_ALL_TESTS();

    return result;
}


