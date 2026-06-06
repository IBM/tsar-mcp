// TimerThread.h
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: TimerThread.h
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2002 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

#ifndef __ASThread_TimerThread

        #define __ASThread_TimerThread

#include <time.h>
#include <sys/types.h>

#include <ASThread.h>

#ifdef _WIN32
        typedef unsigned __int64 Ltime_t;
#else
        typedef unsigned long long Ltime_t;
#endif

struct TimerItem : public Link
        {
        unsigned EventTimeMS;
        MessageQueueIF *Queue;
        void *MessageToPost;
        };

DefineLinkList(TimerItemList,TimerItem);

class TimerThread : public ASThread
        {
        private:
                Ltime_t StartupTimeMS;
                TimerItemList TimerList;
                MessageQueue TimerQueue;
                void AddTimerItem(TimerItem *Item);
                virtual int ThreadMain();
        protected:
                void Clear();
                virtual void OnMessageDestruct(MessageQueueIF &Queue, 
                                               void *MessageToPost);
        public:
                TimerThread();
                virtual ~TimerThread();
                bool SetTimer(MessageQueueIF &Queue, 
                              void *MessageToPost,
                              unsigned TimeoutMS);
                virtual bool Stop(bool WaitToEnd);
        };

/* ***************************************************************************
 Notes:

              - OnMessageDestruct()

                        Called at Stop(true) or Timer destruction for each 
                        queued timer. It is up to the application to determin
                        how to cleanup, forward or ignore the messages,

                   ==>  If a derived class redefines OnMessageDestruct() then
                        that class must call Stop(true) explicitly in the 
                        destructor.
                        
               - SetTimer(MessageQueueIF &Queue, 
                          void *MessageToPost,
                          unsigned TimeoutMS)

                        At 'TimeoutMS', the message 'MessageToPost' is sent 
                        to the 'Queue.' "MessageToPost' may be NULL if the
                        application can handle NULL on its Queue.

              - Stop(flag WaitToEnd)

                        Call to stop the timer. An derived Timer class should
                        call Stop(true) explicitly in its destructor if the
                        class overrides OnMessageDestruct().

******************************************************************************
**** Example: ****************************************************************
***************

#include <stdio.h>
#include <ASThread.h>
#include <TimerThread.h>

#ifndef _WIN32
        #include <unistd.h>
        #define Sleep(x) sleep(x/1000)
#endif

class HelloWorldThread : public MessageQueueThread
        {
        protected:
                virtual bool OnMessage(void *Msg);
        };

bool HelloWorldThread::OnMessage(void *Message)
        {
        time_t CurrentTime;
        struct tm *tmCurrentTime;
        time(&CurrentTime);
        tmCurrentTime = localtime(&CurrentTime); 
        printf("Hello World! Time = %s",asctime(tmCurrentTime));
        return true;
        }

main() 
	{
        HelloWorldThread HelloWorld;
        TimerThread Timer(1000);
        HelloWorld.Start();
        Timer.Start();

        printf("Printing 4 times. Then press <enter>.\n");
        
        HelloWorld.Send(NULL);
        Timer.SetTimer(HelloWorld,NULL,1000);
        Timer.SetTimer(HelloWorld,NULL,10000);
        Timer.SetTimer(HelloWorld,NULL,7000);

        getc(stdin);
        Timer.Stop(true);
        HelloWorld.Stop(true);
	return 0;
	}

*************************************************************************** */

#endif /* __ASThread_TimerThread */
