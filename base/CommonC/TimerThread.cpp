// TimerThread.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: TimerThread.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2002 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef _WIN32
        #include <unistd.h>
	#include <sys/socket.h>
	#include <sys/types.h>
#endif

#if defined(__OS400_TGTVRM__) || defined(__GNUC__) /* iSeries and Linux */

        #include <sys/time.h>

struct timeb 
        {
        time_t time;
        unsigned short millitm;
        short timezone;
        short dstflag;
        };

static void ftime(struct timeb *Timeb)
        {
        struct timeval time;
        struct timezone timez;
        gettimeofday(&time, &timez);
        Timeb->time = time.tv_sec;
        Timeb->millitm = time.tv_usec / 1000;
        Timeb->timezone = timez.tz_minuteswest;
        Timeb->dstflag = timez.tz_dsttime;
        return;
        }

#else

        #include <sys/timeb.h>

#endif /* !iSeries && !Linux */

#include <TimerThread.h>

#define timebToMS(CurrentTime)                                               \
        (CurrentTime.time * (Ltime_t)1000 + CurrentTime.millitm)

static int TimerThread_ShutdownMSG;             // Just use the address.

TimerThread::TimerThread()
        {
        timeb CurrentTime;
        ftime(&CurrentTime);
        StartupTimeMS = timebToMS(CurrentTime);
        return;
        }

TimerThread::~TimerThread()
        {
        Stop(true);
        return;
        }

void TimerThread::AddTimerItem(TimerItem *Item)
        {
        TimerItem *Current = TimerList.GetFirst();
        while (Current)
                {
                if (Item->EventTimeMS < Current->EventTimeMS)
                        {
                        TimerList.AddBefore(Item,Current);
                        break;
                        }
                Current = TimerList.GetNext(Current);
                }
        if (!Current) TimerList.AddBottom(Item);
        return;         
        }

void TimerThread::Clear()       // Call only from thread or when stoped.
        {
        TimerItem *Current;
        // *********************
        // **** Clear Queue ****
        // *********************
        void *Msg = TimerQueue.Receive(0);
        while (Msg)
                {
                if (Msg != &TimerThread_ShutdownMSG)
                        {
                        Current = (TimerItem *)Msg;
                        OnMessageDestruct(*Current->Queue,
                                          Current->MessageToPost);
                        delete Current;
                        }
                Msg = TimerQueue.Receive(0);
                }
        // ***************************
        // **** Clear Active List ****
        // ***************************
        Current = TimerList.GetFirst();
        while (Current)
                {
                TimerList.Remove(Current);
                OnMessageDestruct(*Current->Queue,
                                  Current->MessageToPost);
                delete Current;
                Current = TimerList.GetFirst();
                }
        return;         
        }

void TimerThread::OnMessageDestruct(MessageQueueIF& /*Queue*/, void* /*Msg*/)
        {
        return;
        }

bool TimerThread::SetTimer(MessageQueueIF &Queue, 
                           void *MessageToPost,
                           unsigned TimeoutMS)
        {
        timeb CurrentTime;
        Ltime_t CurrentTimeMS;
        unsigned ElapsedTimeMS;
        TimerItem *Item = new TimerItem;
        if (!Item) return false;
        Item->MessageToPost = MessageToPost;
        Item->Queue = &Queue;
        ftime(&CurrentTime);
        CurrentTimeMS = timebToMS(CurrentTime);
        ElapsedTimeMS = (unsigned)(CurrentTimeMS - StartupTimeMS);
        Item->EventTimeMS = TimeoutMS + ElapsedTimeMS;
        return TimerQueue.Send(Item);
        }

bool TimerThread::Stop(bool WaitToEnd)
        {
        TimerQueue.SendPriority(&TimerThread_ShutdownMSG);
        if (WaitToEnd) 
                {
                Wait();
                Clear();
                }
        return true;
        }

int TimerThread::ThreadMain()
        {
        timeb CurrentTime;
        Ltime_t CurrentTimeMS;
        unsigned ElapsedTimeMS;
        void *AddMsg = NULL;
        while (1)
                {
                if (!AddMsg) AddMsg = TimerQueue.Receive(0);
                while (AddMsg)
                        {
                        if (AddMsg == &TimerThread_ShutdownMSG)
                                {
                                return 0;       // End of Thread.
                                }
                        AddTimerItem((TimerItem *)AddMsg);
                        AddMsg = TimerQueue.Receive(0);
                        }
                TimerItem *Current = TimerList.GetFirst();
                if (!Current) 
                        {
                        AddMsg = TimerQueue.Receive();
                        continue;
                        }
                ftime(&CurrentTime);
                CurrentTimeMS = timebToMS(CurrentTime);
                ElapsedTimeMS = (unsigned)(CurrentTimeMS - StartupTimeMS);
                if (ElapsedTimeMS < Current->EventTimeMS)
                        {
                        unsigned TimeoutMS;
                        TimeoutMS = Current->EventTimeMS - ElapsedTimeMS;
                        AddMsg = TimerQueue.Receive(TimeoutMS);
                        if (AddMsg) continue;
                        }
                Current->Queue->Send(Current->MessageToPost);
                TimerList.Remove(Current);
                delete Current;
                }
        }

// ***************************************************************************
// ****************************** End of File ********************************
// ***************************************************************************

