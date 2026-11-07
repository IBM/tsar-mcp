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

#ifdef __OS400_TGTVRM__ /* iSeries */

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

#endif /* !iSeries */

#include <assert.h>

#include <LevelTrace.h>
#include <TimerThread.h>

#define timebToMS(CurrentTime)                                               \
        (CurrentTime.time * (Ltime_t)1000 + CurrentTime.millitm)

struct KillTimerINFO : public TimerMessage
        {
        MessageQueueIF *Queue;
        Semaphore Done;
        KillTimerINFO(MessageQueueIF &inQueue)
                :
                TimerMessage(TimerMsgKind_Kill),
                Done(0)
                {
                Queue = &inQueue;
                }
        };

static int TimerThread_ShutdownMSG;     // Fixed address is the message.

static Mutex IdleKillMutex;     // Serializes the stopped-Timer sweep.

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
                        TimerMessage *Message = (TimerMessage *)Msg;
                        if (Message->Kind == TimerMsgKind_Kill)
                                {
                                // ---------------------------------------
                                // Release the waiting KillTimer() caller.
                                // ---------------------------------------
                                KillTimerINFO *Kill = (KillTimerINFO *)Message;
                                Kill->Done.Release();
                                }
                        else    {
                                Current = (TimerItem *)Message;
                                OnMessageDestruct(*Current->Queue,
                                                  Current->MessageToPost);
                                delete Current;
                                }
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

void TimerThread::KillQueueTimers(MessageQueueIF *Queue)  // ThreadMain only.
        {
        TimerItem *Current = TimerList.GetFirst();
        while (Current)
                {
                TimerItem *Next = TimerList.GetNext(Current);
                if (Current->Queue == Queue)
                        {
                        TimerList.Remove(Current);
                        OnMessageDestruct(*Current->Queue,
                                          Current->MessageToPost);
                        delete Current;
                        }
                Current = Next;
                }
        return;
        }

void TimerThread::AbsorbTimerQueue()
        {
        static const char *ProcName = "TimerThread::AbsorbTimerQueue";
        // ---------------------------------------------------------
        // For stopped Timer: Moves pending SetTimer requests to the 
        // TimerList for removal by KillTimers().
        // ---------------------------------------------------------
        unsigned Count = TimerQueue.GetMessageCount();
        while (Count--)
                {
                void *Msg = TimerQueue.Receive(0);
                if (!Msg) break;
                if (Msg == &TimerThread_ShutdownMSG)
                        {
                        // ---------------
                        // Return to Queue
                        // ---------------
                        TimerQueue.SendPriority(Msg);
                        continue;
                        }
                TimerMessage *Message = (TimerMessage *)Msg;
                if (Message->Kind == TimerMsgKind_Kill)
                        {
                        // -----------------------------------------------
                        // Stopped with someone else's KillTimers() in
                        // flight -- blocked on a sweep that will not run.
                        // Returned to the queue so a correct Stop(true)
                        // can still Clear() them.
                        // -----------------------------------------------
                        TERROR(("%s: Stopped with a Kill Request pending",
                                ProcName));
                        TimerQueue.Send(Msg);
                        continue;
                        }
                AddTimerItem((TimerItem *)Message);
                }
        return;
        }

bool TimerThread::KillTimers(MessageQueueIF &Queue)
        {
        static const char *ProcName = "TimerThread::KillTimers";
        // --------------------------------------------------
        // Blocks until prior timers for 'Queue' are removed.
        // --------------------------------------------------
        if (!IsStarted())
                {
                // -------------------------------------------------------
                // Nobody would answer a queued request -- but nobody is
                // consuming TimerQueue or walking TimerList either, so
                // the caller can safely do the thread's own two steps:
                // absorb what was sent, then sweep the list.
                // -------------------------------------------------------
                IdleKillMutex.Take();
                AbsorbTimerQueue();
                KillQueueTimers(&Queue);
                IdleKillMutex.Release();
                return true;
                }
        KillTimerINFO Request(Queue);
        bool Status = TimerQueue.Send(&Request);
        if (!Status)
                {
                TERROR(("%s: Unable to post Kill Request",ProcName));
                }
        else    {
                Request.Done.Take();    // Released by thread or Clear().
                }
        return Status;
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
                // -------------------------
                // ---- Handle Requests ----
                // -------------------------
                if (!AddMsg) AddMsg = TimerQueue.Receive(0);
                while (AddMsg)
                        {
                        // --------
                        // Shutdown
                        // --------
                        if (AddMsg == &TimerThread_ShutdownMSG)
                                {
                                goto Exit;      // End of thread -- bye.
                                }
                        TimerMessage *Message = (TimerMessage *)AddMsg;
                        // ----------
                        // Kill Timer
                        // ----------
                        if (Message->Kind == TimerMsgKind_Kill)
                                {
                                KillTimerINFO *Kill = (KillTimerINFO *)Message;
                                KillQueueTimers(Kill->Queue);
                                Kill->Done.Release(); // Release caller.
                                }
                        // ---------
                        // Add Timer
                        // ---------
                        else AddTimerItem((TimerItem *)Message);
                        AddMsg = TimerQueue.Receive(0);
                        }
                // --------------
                // ---- Idle ----
                // --------------
                TimerItem *Current = TimerList.GetFirst();
                if (!Current) 
                        {
                        AddMsg = TimerQueue.Receive();
                        continue;
                        }
                // ----------------------------------
                // ---- Wait for Timeout or Work ----
                // ----------------------------------
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
  Exit: return 0;
        }

// ***************************************************************************
// ****************************** End of File ********************************
// ***************************************************************************

