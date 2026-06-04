///////////////////////////////////////////////////////////////////////////////
//                                                                             
// TSAR (Tools Slightly Above the Runtime)                              
//                                                                             
// Filename: ASThread.h
//                                                                             
// The source code contained herein is licensed under the MIT License,
// which has been approved by the Open Source Initiative.         
// Copyright (C) 2012 
// All rights reserved.
//                    
// Author(s) : Eric Kass 
//                                                                             
///////////////////////////////////////////////////////////////////////////////
#ifndef __Thread_Classes

        #define __Thread_Classes

#ifdef _WIN32

#include <windows.h>

#else /* Unix */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/time.h>

#endif /* Unix */

#include <limits.h>

#include <Errors.h> 

#include <LinkList.h>

#if defined(_WIN32)
        typedef DWORD TLSKey_t; 
        #define INVALID_TLSKEY TLS_OUT_OF_INDEXES
#elif defined(__MVS__)
        typedef unsigned long TLSKey_t;
        #define INVALID_TLSKEY ((TLSKey_t)-1)
#else
        typedef pthread_key_t TLSKey_t;
        #define INVALID_TLSKEY ((TLSKey_t)-1)
#endif 

#if defined(__OS400_TGTVRM__)
        typedef unsigned long long ASThreadID_t;
#else
        typedef unsigned long ASThreadID_t;
#endif

#define MAX_THREAD_ATEXIT 31                    // Number of handlers.

typedef void (*atexitThreadFn_t)(void *);

namespace ASThreadNamespace {

// **************************************************************************
// **** Synchronization *****************************************************
// **************************************************************************

// ***************
// **** Mutex ****
// ***************

#ifdef _WIN32

class Mutex
        {
        private:
                HANDLE hMutex;
        public:
                Mutex();
                Mutex(char *Name);
                ~Mutex();
                void Release() {ReleaseMutex(hMutex);}
                void Take();
        };

inline void Mutex::Take() 
        {
        DWORD rc;
        do      {
                rc = WaitForSingleObjectEx(hMutex,INFINITE,TRUE);
                } while (rc == WAIT_IO_COMPLETION);
        return;
        }

// *************************
// **** CriticalSection ****
// *************************

class CriticalSection
        {
        private:
                CRITICAL_SECTION CS;
        public:
                CriticalSection();
                ~CriticalSection();
                void Release() {LeaveCriticalSection(&CS);}
                void Take() {EnterCriticalSection(&CS);}
                bool TryTake();
        };

// *******************
// **** Semaphore ****
// *******************

class Semaphore
        {
        private:
                bool Created;
                HANDLE hSemaphore;
        public:
                Semaphore(int InitialCount);
                Semaphore(int InitialCount, char *Name);
                ~Semaphore() {Destruct();}
                void Destruct();
                bool IsCreated() {return Created;}
                bool IsValid() {return hSemaphore != NULL;}
                void RetainLocks(bool) {}
                void Release() 
                        {
                        if (!hSemaphore) return;
                        ReleaseSemaphore(hSemaphore,1,NULL);
                        }
                void Take() {Take(INFINITE);}
                bool Take(unsigned TimeoutMS);
        };

inline bool Semaphore::Take(unsigned TimeoutMS) 
        {
        DWORD rc;
        do      {
                rc = WaitForSingleObjectEx(hSemaphore,TimeoutMS,TRUE);
                } while (rc == WAIT_IO_COMPLETION);
        return rc == WAIT_OBJECT_0;
        }

#else /* Unix */

// ***************
// **** Mutex ****
// ***************

class Mutex
        {
        private:
                pthread_mutex_t hMutex;
                friend class Condition;
        public:
                Mutex(); 
                Mutex(char *Name);
                ~Mutex();
                void Release() {pthread_mutex_unlock(&hMutex);}
                void Take() {pthread_mutex_lock(&hMutex);}
        };

// *******************
// **** Condition ****
// *******************

class Condition
        {
        private:
                Mutex &ConditionMutex;
                pthread_cond_t hCondition;
        public:
                Condition(Mutex &conditionMutex); 
                ~Condition();
                void Release() {pthread_cond_signal(&hCondition);}
                void Take() 
                        {
                        pthread_cond_wait(&hCondition,
                                          &ConditionMutex.hMutex);
                        }
                bool Take(unsigned TimeoutMS);
        };

// ********************
// **** Semaphores ****
// ********************

class PTSemaphore                       // PThread Semaphore
        {
        private:
                unsigned Count;
                Mutex SemMutex;
                Condition SemCondition;
        public:
                PTSemaphore(int InitialCount);
                void Release();
                void Take();
                bool Take(unsigned TimeoutMS);
        };

class Semaphore
        {
        private:
                int KeyID;
                bool SharedUNDO;        // UNDO at Program End.
                bool SharedNoDTOR;      // Leave Semaphores Arround.
                bool Created;
                int hSemaphore;
                struct itimerval OriginalTimer;
                struct sigaction OriginalAlarmHandler; 
        protected:
                bool ClearAlarm();
                bool Construct(int InitialValue);
                bool SetAlarm(unsigned TimeoutMS);
        public:
                Semaphore(int InitialCount);
                Semaphore(int InitialCount, int KeyID);
                ~Semaphore() {if (!SharedNoDTOR) Destruct();}
                void Destruct();
                bool IsCreated() {return Created;}
                bool IsValid() {return hSemaphore != -1;}
                void RetainLocks(bool Retain) {SharedUNDO = !Retain;}
                void Release();
                void Take();
                bool Take(unsigned TimeoutMS);
        };

#endif /* Unix */

#if defined(__OS400_TGTVRM__) || defined(_AIX)

// *************************
// **** CriticalSection ****
// *************************

class CriticalSection
        {
        private:
                int CS;
        public:
                CriticalSection();
                void Release();
                void Take();
                bool TryTake();
        };

#elif defined(__USE_XOPEN2K) /* Unix with Spinlocks */

class CriticalSection
        {
        private:
                pthread_spinlock_t CS;
        public:
                CriticalSection();
                ~CriticalSection();
                void Release() {pthread_spin_unlock(&CS);}
                void Take() {pthread_spin_lock(&CS);}
                bool TryTake() {return pthread_spin_trylock(&CS) == 0;}
        };

#elif !defined(_WIN32) /* Unix */

#define CriticalSection Mutex

#endif /* Unix */

// *********************************
// **** Shared / Exclusive Lock ****
// *********************************

class ShareLock
        {
        private:
                Semaphore ExclusiveGate;
                Semaphore ShareGate;
                unsigned ShareCount;
                CriticalSection ShareCountCS;
        public:
                ShareLock() : ExclusiveGate(1),
                              ShareGate(1) {ShareCount = 0;}
                void TakeShared();
                void ReleaseShared();
                void TakeExclusive();
                void ReleaseExclusive();
                void ReleaseExclusiveTakeShared();
        };

// **************************************************************************
// **** Thread **************************************************************
// **************************************************************************

class ASThread
        {
        private:
                bool Active;
                Mutex WaitMutex;
                Mutex ActiveMutex;
                static TLSKey_t ASThreadKey;
  #ifdef _WIN32
                HANDLE hThread;
                DWORD ReturnCode;                
                static unsigned __stdcall Bootstrap(void *ASThreadThis);
  #else /* Unix */
                bool Detached;
                int ReturnCode;
                pthread_t hThread;
                static void* Bootstrap(void *ASThreadThis);
  #endif /* Unix */
                Semaphore StartSemaphore;  
                friend ASThread* GetActiveThread();
                static void RunExitHandlers();
        protected:
                virtual int ThreadMain() = 0;
        public:
                ASThread();
                virtual ~ASThread();
                virtual const char* GetClassName() {return "ASThread";}
                bool Start(); 
                bool Kill(int ExitCode=0);
                bool IsCurrentThread();
                bool IsStarted();
                int Wait();
                static bool atexit(atexitThreadFn_t Func, void *UserPtr);
                static void unatexit(atexitThreadFn_t Func, void *UserPtr);
        };

// **************************************************************************
// **** Thread Functions ****************************************************
// **************************************************************************

ASThread* GetActiveThread();

bool IsActiveThread(ASThread *Thread);

ASThreadID_t GetThreadID();            // Operating System Thread ID.

// **************************************************************************
// **** Message Queue *******************************************************
// **************************************************************************

struct MessageLink : public Link
        {
        void *Message;
        };

DefineLinkList(_MessageList,MessageLink);

class MessageList : protected _MessageList
        {
        private:
                unsigned FreeCount;
                _MessageList FreeList;
        public:
                MessageList();
                bool PutMessage(void *Message);
                bool PutMessageFirst(void *Message);
                void* GetMessage();
        };

class MessageQueueIF
        {
        public:
                virtual void* Receive() = 0;
                virtual void* Receive(unsigned TimeoutMS) = 0;
                virtual bool Send(void *Msg) = 0;
                virtual bool SendPriority(void *Msg) = 0;
        };

class MessageQueue : public MessageQueueIF
        {
        private: 
                Mutex QueueMutex;
                MessageList Messages;
                unsigned MessageCount;
  #ifdef _WIN32
                Semaphore MessageSemaphore;
  #else /* Unix */
                PTSemaphore MessageSemaphore;
  #endif /* Unix */
        protected:
                void* GetMessage(); 
                virtual void OnMessageDestruct(void *Msg);
                bool PutMessage(void *Msg);
                bool PutMessageFirst(void *Msg);
                friend class MessageQueueThread;
        public:
                MessageQueue();
                virtual ~MessageQueue() {Clear();}
                void Clear();
                unsigned GetMessageCount();
                virtual void* Receive();
                virtual void* Receive(unsigned TimeoutMS);
                virtual bool Send(void *Msg);
                virtual bool SendPriority(void *Msg);
        };

// **************************************************************************
// **** Message Queue Thread ************************************************
// **************************************************************************

class MessageQueueLoop : public MessageQueueIF
        {
        private:
                bool Continue;
                MessageQueue Queue;
        protected:
                static int MessageQueueLoop_ShutdownMSG;
                virtual bool OnMessage(void *Msg);
                virtual void OnMessageDestruct(void *Msg);
                virtual bool OnShutdown();
                virtual void* Receive();
                virtual void* Receive(unsigned TimeoutMS);
                virtual void WaitToEnd() {return;}
                // **********************
                // **** Self Control ****
                // **********************
                void _Break() {Continue = false;}       // From self only.
                unsigned GetMessageCount() {return Queue.GetMessageCount();}
                bool Stop();                            // End self.
        public:
                MessageQueueLoop();
                virtual ~MessageQueueLoop() {Stop(true);}
                void ClearQueue();
                int Run();
                virtual bool Send(void *Msg);
                virtual bool SendPriority(void *Msg);
                virtual bool Stop(bool WaitToEnd);      // End nicely.
        };

// **************************************************************************
// **** Message Queue Thread ************************************************
// **************************************************************************

class MessageQueueThread : public ASThread,
                           public MessageQueueLoop
        {
        private:
                virtual int ThreadMain() {return Run();}
        protected:
                virtual void WaitToEnd() {Wait();}
        public:
                virtual ~MessageQueueThread() {Stop(true);}
                virtual bool Stop(bool WaitToEnd);      // End nicely.
        };

// **************************************************************************
// **** Thread Local Storage ************************************************
// **************************************************************************

TLSKey_t CreateTLS();

TLSKey_t CreateTLS(void (*Destructor)(void *));   // No Destructor in Win32.

bool FreeTLS(TLSKey_t &Key);
 
bool SetTLS(TLSKey_t &Key, const void *Value);

void* GetTLS(TLSKey_t &Key);

// **************************************************************************
// **** ASThread Global Error Handlers **************************************
// **************************************************************************

typedef void (*ASThread_ErrorFn_t)(const char *Error);

void SetASThreadConstructionErrorHandler(ASThread_ErrorFn_t ErrorFn);

} /* namespace ASThreadNamespace */

using namespace ASThreadNamespace;

/* ****************************************************************************
 Notes:

        Construction Errors:

                Many constructors throw Error_Construction objects by 
                default. The behavior can be overridden globally with:
                        
                        - SetASThreadConstructionErrorHandler()

                The handler function must throw an error or raise a signal
                or exit the program instead of returning.

        Semaphores:

                Constructor throws Error_Construction("Semaphore") 
                by default.

        Mutexes:

                Constructor throws Error_Construction("Mutex")
                by default.

        ShareLock:

                Allows multiple shared 'readers' but only one exclusive 
                'writer'. When the lock is requested exclusivly, all new
                lock requesters are blocked until the exclusive lock is 
                satisfied.

                To escalate a lock from shared to exclusive; you must 
                first release the shared lock and then aquire the exclusive.

                To reduce a lock from exclusive to shared atomically; use
                the ReleaseExclusiveTakeShared() method.

        ASThreads:
  
             1. At Thread Exit Handlers:

                      - ASThread::atexit(Func,UserPtr)

                                Registers a global handler. Multiple calls
                                to atexit with the same {Func,UserPtr} 
                                results in only on registration (the same 
                                number of unatexit() calls would then be 
                                required).

                                atexit() can be used to cleanup associated
                                TLS resources; where UserPtr is the TLSKey_t.

                      - ASThread::unatexit(Func,UserPtr)

                                Unregisters a handler for a particular 
                                {Func,UserPtr} pair.

                                Note: unatexit() is optional as no resources
                                      are associated with a registration.
    
                      - NOTES:

                        A thread handler can get access to the current 
                        thread exiting by calling:

                              - GetActiveThread()

                        The "type" of thread returned by GetActiveThread() 
                        can be determined by:

                              - GetClassName()

                        or by RTTI dynamic_cast() operator.

                        Example Handler:

                            void MYAtExit(void*)
                                  {
                                  MYThread *MyThread;
                                  ASThread *Active = GetActiveThread();
                                  MyThread = dynamic_cast<MYThread*>(Active);
                                  if (MyThread) MyThread->Bye();
                                  else    {
                                          printf("End of Non-MYThread: %s\n",
                                                 MyThread->GetClassName());
                                        }
                                  return;
                                  }

        Message Queue:

              - Clear()

                        Empties the queue, calling OnMessageDestruct()
                        for each message still residing on the queue.
                        
                        If a derived class redefines OnMessageDestruct() 
                        then that class must call Clear() explicitly in 
                        the destructor.

              - GetMessageCount()

                        Returns the number of items in the queue.

              - void* Receive()

                        Returns the next message on the queue or blocks
                        otherwise. The return value may be NULL if
                        'NULL' had been sent as a message. 

              - void* Receive(unsigned TimeoutMS)

                        Returns the next message on the queue or blocks
                        'TimeoutMS' milliseconds until one arrives.

                        The function returns NULL if 'TimeoutMS' expired
                        before a message arrived. 
                        
                        As 'NULL' is reserved to indicate timeout, don't 
                        Send(NULL) as a message if Receive(TimeoutMS) is 
                        to be used. Instead:
                        
                                int NULLMessage;
                                Send(&NULLMessage);

                        Receive(0) gets the next message but does not
                        block if none is available.

              - bool Send(void *Msg)

                        Sends 'Msg' to the queue. 'Msg" can be NULL if the
                        application can handle NULL on its Queue.

              - bool SendPriority(void *Msg)
                
                        Sends 'Msg' but as the first item on the queue.
        
        Message Queue Loop:

                - Run()

                        Blocking call that schedules OnMessage() whenever 
                        a message is posted via Send() or SendPriority().
  
            ==> If a derived class redefines OnMessageDestruct() then
                that class must call ClearQueue() or Stop(true) explicitly 
                in the destructor.

        Message Queue Thread:

                A Message Queue Loop that runs in a seperate thread. The
                'Run()' loop is start via Start().

            ==> See OnMessageDestruct() note above.

        ThreadLocalStorage:

                Contents of thread local storage can be cleaned up with 
                two methods:

                     1. Register a ASThread atexit() handler to get control
                        before the end of a thread. This does not work for
                        threads created by means other than ASThreads.

                     2. Use platform specific TLS cleanup:

                             a. WIN: DllMain()                  (Microsoft)
                                     DllEntryPoint()            (Borland)

                                        case DLL_THREAD_DETACH:

                             b. UNIX: Use CreateTLS(Destructor)

*******************************************************************************
**** Examples: ****************************************************************
***************

*******************************************************************************
 Example 1:
*******************************************************************************

#include <stdio.h>
#include <ASThread.h>

class ZooThread : public ASThread
        {
        private:
                char *Animal;
                char *Speak;
        protected:
                int ThreadMain();
        public:
                ZooThread(char *animal, char *speak)
                        {
                        Animal = animal;
                        Speak = speak;
                        return;
                        }
        };

int ZooThread::ThreadMain()
        {
        for (int i=0; i < 5; i++)
                {
                printf("The %s goes %s\n",Animal,Speak);
                Sleep(1000);
                }
        return 0;
        }

main(int argc, char *argv[]) 
	{
        ZooThread Cows("Cow","Moo");
        ZooThread Dogs("Dog","Ruff");
        Cows.Start();
        Dogs.Start();
        Cows.Wait();
        Dogs.Wait();
        printf("All Done\n");
	return 0;
	}

*******************************************************************************
 Example 2:
*******************************************************************************

#include <stdio.h>
#include <ASThread.h>

#ifndef _WIN32
        #include <unistd.h>
        #define Sleep(x) sleep(x/1000)
#endif

class AnimalThread: public ASThread, public MessageQueue
        {
        private:
                char *Animal;
                char *Speak;
        protected:
                int ThreadMain();
        public:
                AnimalThread(char *animal, char *speak)
                        {
                        Animal = animal;
                        Speak = speak;
                        return;
                        }
                char *GetName() {return Animal;}
        };

class ZooThread : public ASThread
        {
        private:
                int SleepTime;
                AnimalThread *WatchAnimal;
        protected:
                int ThreadMain();
        public:
                ZooThread(AnimalThread &watchAnimal, int sleepTime)
                        {
                        WatchAnimal = &watchAnimal;
                        SleepTime = sleepTime;
                        return;
                        }
        };

int AnimalThread::ThreadMain()
        {
        int *SpeakMessage = (int *)Receive();
        while (SpeakMessage && *SpeakMessage == 1)
                {
                printf("The %s goes %s\n",Animal,Speak);
                delete SpeakMessage;
                SpeakMessage = (int *)Receive();
                }
        printf("The %s says bye...\n",Animal);
        return 0;
        }

int ZooThread::ThreadMain()
        {
        int *SpeakMessage;      
        for (int i=0; i < 5; i++)
                {
                SpeakMessage = new int;
                if (SpeakMessage)
                        {
                        *SpeakMessage = 1;
                        printf("Sending to %s\n",WatchAnimal->GetName());
                        WatchAnimal->Send(SpeakMessage);
                        }
                Sleep(SleepTime);
                }
        SpeakMessage = new int;
        if (SpeakMessage)
                {
                *SpeakMessage = 0;
                WatchAnimal->Send(SpeakMessage);
                }
        return 0;
        }

main(int argc, char *argv[]) 
	{
        AnimalThread Cow("Cow","Moo");
        AnimalThread Dog("Dog","Ruff");
        ZooThread CowZoo(Cow,1000);
        ZooThread DogZoo(Dog,2000);
             
        printf("Starting Cow\n");
        Cow.Start();

        printf("Starting Dog\n");
        Dog.Start();

        printf("Starting CowZoo\n");
        CowZoo.Start();

        printf("Starting DogZoo\n");
        DogZoo.Start();

        Cow.Wait();
        Dog.Wait();

        printf("All Done\n");
	return 0;
	}

**************************************************************************** */

#endif
