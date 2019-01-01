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


/**
 * File:    mpx.h
 * Author:  mtdcy.chen
 * Changes:
 *          1. 20181214     initial version
 *
 */

#ifndef _MPX_MEDIA_CLOCK_H
#define _MPX_MEDIA_CLOCK_H

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaTime.h>

__BEGIN_DECLS

enum eClockRole {
    kClockRoleMaster,   ///< master clock
    kClockRoleSlave,    ///< slave clock
};

enum eClockState {
    kClockStateTicking,
    kClockStatePaused,
    kClockStateReset
};

__END_DECLS

namespace mtdcy {
    
    typedef TypedEvent<eClockState>  ClockEvent;

    /**
     * clock for av sync and speed control. all session share
     * the same SharedClock, multiple slave Clocks can exists at
     * the same time, but only one master Clock. and only the
     * SharedClock itself or master clock can modify the clock
     * context.
     */
    class Clock;
    class SharedClock {
        public:
            SharedClock();

            /**
             * set clock time, without alter clock state
             * @param   ts      media time
             */
            void        set(const MediaTime& ts);
            /**
             * start this clock.
             */
            void        start();
            /**
             * pause this clock. after paused, media time and system
             * time won't update, get() always return the same time.
             */
            void        pause();
            /**
             * reset this clock to paused state with initial media time
             * and system time.
             * have to start again.
             */
            void        reset();
            /**
             * get this clock's state.
             * @return return true if paused.
             */
            bool        isPaused() const;
            /**
             * set this clock's speed.
             * @param   s   speed of clock in double.
             */
            void        setSpeed(double s);
            /**
             * get this clock's speed.
             * @return clock speed in double.
             */
            double      speed() const;

            /**
             * get clock media time
             * @return  media time, always valid.
             */
            MediaTime   get() const;

        public:
            /**
             * get a shadow clock
             * @param   role    @see kClockRole*
             * @note only one master is allowed
             */
            sp<Clock>   getClock(eClockRole role = kClockRoleSlave);

        private:
            void        _regListener(const sp<ClockEvent>& ce);
            void        _unregListener(const sp<ClockEvent>& ce);
        
            void        notifyListeners_l(eClockState state);

        private:
            // atomic int
            volatile int    mGeneration;
            volatile int    mMasterClock;
            // clock internal context
            mutable Mutex   mLock;
            struct ClockInt {
                ClockInt();
                bool        mPaused;
                MediaTime   mMediaTime;
                MediaTime   mSystemTime;
                double      mSpeed;
            };
            friend class Clock;
            void            update(ClockInt&);
            ClockInt        mClockInt;
            List<sp<ClockEvent> >   mListeners;

        private:
            DISALLOW_EVILS(SharedClock);
    };

    /**
     * shadow of SharedClock
     * @see SharedClock
     */
    class Clock {
        protected:
            friend class SharedClock;
            /**
             * create a shadow of SharedClock
             * @param sc    reference to SharedClock
             * @param role  @see SharedClock::eClockRole
             */
            Clock(SharedClock& sc, eClockRole role);

        public:
            ~Clock();

        public:
            /**
             * get role of this clock
             * @return @see SharedClock::eClockRole
             */
            eClockRole role() const { return mRole; }
        
            /**
             * set event to this clock
             * @param ce    reference of ClockEvent. if ce = NULL,
             *              clear the listener
             */
            void        setListener(const sp<ClockEvent>& ce);

            /** @see SharedClock::get() */
            MediaTime   get() const;

            /** @see SharedClock::isPaused() */
            bool        isPaused() const;

            /** @see SharedClock::speed() */
            double      speed() const;
            /**
             * @see SharedClock::update();
             * @note update method is only for master clock
             */
            void        update(const MediaTime&);
            void        update(const MediaTime&, int64_t);
            /** @see SharedClock::set() */
            void        set(const MediaTime&);

        private:
            /**
             * shadow SharedClock internal context if generation
             * changed. otherwise do NOTHING.
             */
            void        reload() const;

        private:
            SharedClock&                    mClock;
            const eClockRole                mRole;
            sp<ClockEvent>                  mListener;
            /**
             * shadow of SharedClock::mGeneration
             */
            mutable int                     mGeneration;
            /**
             * shadow of SharedClock internal context
             * @see SharedClock::ClockInt
             */
            mutable SharedClock::ClockInt   mClockInt;

        private:
            DISALLOW_EVILS(Clock);
    };


};

#endif
