// Thread / Message Queue Classess: ASThread.cpp
/*
 * TSAR (Tools Slightly Above the Runtime)
 * Filename: ASThread.cpp
 *
 * Copyright (c) 2026 International Business Machines Corporation
 * Copyright (c) 2001 Eric Kass
 *
 * SPDX-License-Identifier: MIT
 */
//

volatile const char Copyright0[] = "Copyright (c) 2026 International Business Machines Corporation";
volatile const char Copyright1[] = "Copyright (c) 1997, 2026 Eric Kass";


#ifdef _WIN32
        #ifndef _WIN32_WINNT
                #ifdef __BORLANDC__
                        #define _WIN32_WINNT 0x0400
                #else
                        #define _WIN32_WINNT 0x0500
                #endif
        #endif
#endif

#ifdef __MVS__
        #define _OPEN_THREADS 2
#endif

#if defined(_AIX) || defined(__OS400_TGTVRM__)
        #pragma priority(-4200)
#endif

#include <ASThread.h>
#include <assert.h>
#include <signal.h>
#include <string.h>

#ifdef _WIN32
        #include <windows.h>
        #include <process.h>
#else
        #include <sched.h>
        #include <sys/time.h>
#endif

#ifdef __OS400_TGTVRM__
        #include <stdio.h>
        #include <unistd.h>
        #include <mih/cmpswp.h>
#endif

#ifdef _AIX
        #include <unistd.h>
        #include <sys/atomic_op.h>
#endif
          
#ifdef __MVS__
        #define PTHREAD_CANCELED ((void *)(-1))
        #define pthread_getspecific pthread_getspecific_d8_np
#endif

#include <LevelTrace.h>

#define FreeCountMAX 64                 // MessageLinks kept in FreeList.

#define CriticalSectionSpinCount 4000   // MSDN writes 4000 used for HeapMgr.

namespace ASThreadNamespace {

// **************************************************************************
// **** ASThread Global Error Handlers **************************************
// **************************************************************************

static void ThrowConstructionError(const char *Error)
        {
        throw Error_Construction(Error);
        }

static void (*HandleConstructionError)(const char *) = ThrowConstructionError;

void SetASThreadConstructionErrorHandler(ASThread_ErrorFn_t ErrorFn)
        {
        HandleConstructionError = ErrorFn;
        return;
        }

// **************************************************************************
// **** Mutex / Semaphore ***************************************************
// **************************************************************************

#ifdef _WIN32

// ********************
// **** WIN: Mutex ****
// ********************

Mutex::Mutex()
        {
        hMutex = CreateMutex(NULL,FALSE,NULL);
        if (!hMutex) HandleConstructionError("Mutex");
        return;
        }

Mutex::Mutex(char *Name)
        {
        hMutex = CreateMutex(NULL,FALSE,Name);
        if (!hMutex) HandleConstructionError("Mutex");
        return;
        }

Mutex::~Mutex()
        {
        if (hMutex) CloseHandle(hMutex);
        return;
        }

#if _WIN32_WINNT < 0x0500
        #pragma message ("NOTE: Define _WIN32_WINNT >= 0x0500 for CS Spin-Locks")
#endif

// ******************************
// **** WIN: CriticalSection ****
// ******************************

CriticalSection::CriticalSection()
        {
   #if _WIN32_WINNT < 0x0500
        InitializeCriticalSection(&CS);
   #else
        InitializeCriticalSectionAndSpinCount(&CS,CriticalSectionSpinCount);
   #endif
        return;
        }

CriticalSection::~CriticalSection()
        {
        DeleteCriticalSection(&CS);
        return;
        }  

#if _WIN32_WINNT >= 0x0400

bool CriticalSection::TryTake() 
        {
        return TryEnterCriticalSection(&CS) == TRUE;
        }

#endif

// ************************
// **** WIN: Semaphore ****
// ************************

Semaphore::Semaphore(int InitialCount)
        {
        Created = false;
        hSemaphore = CreateSemaphore(NULL,
                                     InitialCount,
                                     LONG_MAX,
                                     NULL);
        if (!hSemaphore) 
                {
                HandleConstructionError("Semaphore");
                }
        else if (GetLastError() != ERROR_ALREADY_EXISTS) Created = true;
        return;
        }

Semaphore::Semaphore(int InitialCount, char *Name)
        {
        Created = false;
        hSemaphore = CreateSemaphore(NULL,
                                     InitialCount,
                                     LONG_MAX,
                                     Name);
        if (!hSemaphore) 
                {
                HandleConstructionError("Semaphore");
                }
        else if (GetLastError() != ERROR_ALREADY_EXISTS) Created = true;
        return;
        }

void Semaphore::Destruct()
        {
        if (hSemaphore) CloseHandle(hSemaphore);
        hSemaphore = NULL;
        return;
        }

#else /* Unix and OS400 */

// ***********************
// **** UNIX: PTMutex ****
// ***********************

PTMutex::PTMutex()
        {
        int rc;
        pthread_mutexattr_t Attributes;
        rc = pthread_mutexattr_init(&Attributes);
        if (rc) HandleConstructionError("Mutex-AttrInit");
                
  #ifdef __OS400_TGTVRM__                               /* iSeries */
        pthread_mutexattr_setkind_np(&Attributes,PTHREAD_MUTEX_RECURSIVE_NP);
  #endif
        pthread_mutexattr_settype(&Attributes,PTHREAD_MUTEX_RECURSIVE);

        rc = pthread_mutex_init(&hMutex, &Attributes);
        if (rc) HandleConstructionError("Mutex-Init");
        pthread_mutexattr_destroy(&Attributes);
        return;
        }

PTMutex::PTMutex(char *Name)
        {
        int rc;
        pthread_mutexattr_t Attributes;
        rc = pthread_mutexattr_init(&Attributes);
        if (rc) HandleConstructionError("Mutex-AttrInit");

  #ifdef __OS400_TGTVRM__                               /* iSeries */
        pthread_mutexattr_setname_np(&Attributes,Name);
        pthread_mutexattr_setkind_np(&Attributes,PTHREAD_MUTEX_RECURSIVE_NP);
  #endif
        pthread_mutexattr_settype(&Attributes,PTHREAD_MUTEX_RECURSIVE);
        pthread_mutexattr_setpshared(&Attributes,PTHREAD_PROCESS_SHARED);

        rc = pthread_mutex_init(&hMutex, &Attributes);
        if (rc) HandleConstructionError("Mutex-Init");
        pthread_mutexattr_destroy(&Attributes);
        return;
        }

PTMutex::~PTMutex()
        {
        pthread_mutex_destroy(&hMutex);
        return;
        }

// *************************
// **** UNIX: Semaphore ****
// *************************
         
Semaphore::Semaphore(int InitialCount)
        {
        KeyID = IPC_PRIVATE;
        hSemaphore = -1;
        SharedUNDO = false;
        SharedNoDTOR = false;
        Created = false;
        Construct(InitialCount);
        return;
        }

Semaphore::Semaphore(int InitialCount, int keyID)
        {
        KeyID = keyID;
        hSemaphore = -1;
        SharedUNDO = true;
        SharedNoDTOR = true;
        Created = false;
        Construct(InitialCount);
        return;
        }

bool Semaphore::ClearAlarm()
        {
        int rcT = setitimer(ITIMER_REAL,&OriginalTimer,NULL);
        int rcA = sigaction(SIGALRM,&OriginalAlarmHandler,NULL);
        return rcT == 0 && rcA == 0;
        }

bool Semaphore::Construct(int InitialCount)
        {
        unsigned RetryCount = 3;
        int Flags = S_IRUSR | S_IWUSR | 
                    S_IRGRP | S_IWGRP |
                    S_IROTH | S_IWOTH;
        if (KeyID == IPC_PRIVATE)
                {
                hSemaphore = semget(KeyID,1,Flags | IPC_CREAT);
                if (hSemaphore != -1) Created = true;
                }
        else do {
                hSemaphore = semget(KeyID,1,Flags);
                if (hSemaphore == -1)
                        {
                        hSemaphore = semget(KeyID,1,Flags | IPC_CREAT);
                        if (hSemaphore != -1) Created = true;
                        }
                RetryCount--;
                } while (hSemaphore == -1 && errno == EEXIST && RetryCount);
        if (hSemaphore == -1)
                {
                HandleConstructionError("Semaphore");
                }
        if (Created)
                {
  #ifdef __OS400_TGTVRM__                                       /* iSeries */
                semctl(hSemaphore,0,SETVAL,InitialCount);
  #else                                                         /* Unix */
                union semun
                        {
                        int val;
                        struct semid_ds *buf;
                        unsigned short *array;
                        } SETVALInfo = {0};
                SETVALInfo.val = InitialCount;
                semctl(hSemaphore,0,SETVAL,SETVALInfo);
  #endif
                }
        return true;
        }

void Semaphore::Destruct()
        {
        if (hSemaphore == -1) return;
        semctl(hSemaphore,0,IPC_RMID);
        hSemaphore = -1;
        return;
        }

void Semaphore::Release()
        {
        if (hSemaphore == -1) return;
        static sembuf unlockOperation = {0,1,0};
        static sembuf unlockOperationUNDO = {0,1,SEM_UNDO};
        if (SharedUNDO) semop(hSemaphore,&unlockOperationUNDO,1);
        else semop(hSemaphore,&unlockOperation,1);
        return;
        }

static void _AlarmHandler(int signalnum)
        {
        return;
        }

bool Semaphore::SetAlarm(unsigned TimeoutMS)    // THREAD WARNING: Only one
        {                                       // interval timer per process.
        struct sigaction SignalAction = {0};
        SignalAction.sa_handler = (void (*)(int))_AlarmHandler;
        sigemptyset(&SignalAction.sa_mask);
        SignalAction.sa_flags = 0;
        int rc = sigaction(SIGALRM,
                           &SignalAction,
                           &OriginalAlarmHandler);
        if (rc == 0) 
                {
                struct itimerval Timer = {0};
                Timer.it_value.tv_sec = TimeoutMS / 1000;
                Timer.it_value.tv_usec = (TimeoutMS % 1000) * 1000;
                rc = setitimer(ITIMER_REAL,&Timer,&OriginalTimer);
                }
        return rc == 0;
        }

void Semaphore::Take()
        {
        int rc;
        if (hSemaphore == -1) return;
        static sembuf lockOperation = {0,-1,0};
        static sembuf lockOperationUNDO = {0,-1,SEM_UNDO};
        do      {
                if (SharedUNDO) rc = semop(hSemaphore,&lockOperationUNDO,1);
                else rc = semop(hSemaphore,&lockOperation,1);
                } while (rc && errno == EINTR);
        return;
        }

bool Semaphore::Take(unsigned TimeoutMS)        // Since only one interval
        {                                       // timer exists per process,
        int rc;                                 // only one thread may safely
        bool Status = false;                    // use Take(TimeoutMS).
        static sembuf lockOpWAIT = {0,-1,0};
        static sembuf lockOpNOWAIT = {0,-1,IPC_NOWAIT};
        static sembuf lockOpWAITUNDO = {0,-1,SEM_UNDO};
        static sembuf lockOpNOWAITUNDO = {0,-1,IPC_NOWAIT | SEM_UNDO};
        if (hSemaphore == -1) return false;
        if (SharedUNDO) rc = semop(hSemaphore,&lockOpNOWAITUNDO,1);
        else rc = semop(hSemaphore,&lockOpNOWAIT,1);
        if (rc == 0) Status = true;
        else if (errno == EAGAIN && TimeoutMS)
                {
                SetAlarm(TimeoutMS);
                if (SharedUNDO) rc = semop(hSemaphore,&lockOpWAITUNDO,1);
                else rc = semop(hSemaphore,&lockOpWAIT,1);
                ClearAlarm();
                if (rc == 0) Status = true;
                else if (errno == EINTR) Status = false;
                }
        return Status;
        }

// *************************
// **** UNIX: Condition ****
// *************************

Condition::Condition(PTMutex &conditionMutex)
        :
        ConditionMutex(conditionMutex)
        {
        int rc;
        pthread_condattr_t Attributes;
        rc = pthread_condattr_init(&Attributes);
        if (rc) HandleConstructionError("Condition-AttrInit");
        pthread_condattr_setpshared(&Attributes,PTHREAD_PROCESS_SHARED);
        rc = pthread_cond_init(&hCondition, &Attributes);
        if (rc) HandleConstructionError("Condition-Init");
        pthread_condattr_destroy(&Attributes);
        return;
        }

Condition::~Condition()
        {
        pthread_cond_destroy(&hCondition);
        return;
        }

bool Condition::Take(unsigned TimeoutMS)
        {
        struct timeval timev;
        struct timezone timez;
        struct timespec Timeout;
        unsigned TimeoutSEC = TimeoutMS / 1000;
        unsigned TimeoutNSEC = (TimeoutMS % 1000) * 1000000; 
        gettimeofday(&timev,&timez);
        Timeout.tv_sec = timev.tv_sec + TimeoutSEC;
        Timeout.tv_nsec = timev.tv_usec * 1000 + TimeoutNSEC;
        int rc = pthread_cond_timedwait(&hCondition,
                                        &ConditionMutex.hMutex,
                                        &Timeout);
        return rc == 0;
        }

// *********************************
// **** UNIX: PThread Semaphore ****
// *********************************

PTSemaphore::PTSemaphore(int InitialCount)
        :
        SemCondition(SemMutex)
        {
        Count = InitialCount;
        if (!Count) SemCondition.Release();
        }

void PTSemaphore::Release()
        {
        SemMutex.Take();
        Count++;
        SemCondition.Release();
        SemMutex.Release();
        return;
        }

void PTSemaphore::Take()
        {
        SemMutex.Take();
        while (!Count)
                {
                SemCondition.Take();
                }
        if (--Count) SemCondition.Release();
        SemMutex.Release();
        return;
        }

bool PTSemaphore::Take(unsigned TimeoutMS)
        {
        bool Status = true;
        SemMutex.Take();
        while (Status && !Count)
                {
                Status = SemCondition.Take(TimeoutMS);
                }
        if (Status && --Count) SemCondition.Release();
        SemMutex.Release();
        return Status;
        }

#endif /* Unix and OS400 */

#if defined(__OS400_TGTVRM__)

// *******************************
// **** OS400: OwnerTermMutex ****
// *******************************

OwnerTermMutex::OwnerTermMutex()
        {
        int rc;
        LockDepth = 0;
        pthread_mutexattr_t Attributes;
        rc = pthread_mutexattr_init(&Attributes);
        if (rc) HandleConstructionError("Mutex-AttrInit");
        
        pthread_mutexattr_settype(&Attributes,PTHREAD_MUTEX_OWNERTERM_NP);
        
        rc = pthread_mutex_init(&hMutex, &Attributes);
        if (rc) HandleConstructionError("Mutex-Init");
        pthread_mutexattr_destroy(&Attributes);
        return;
        }

OwnerTermMutex::OwnerTermMutex(char *Name)
        {
        int rc;
        LockDepth = 0;
        pthread_mutexattr_t Attributes;
        rc = pthread_mutexattr_init(&Attributes);
        if (rc) HandleConstructionError("Mutex-AttrInit");

        pthread_mutexattr_setname_np(&Attributes,Name);
        pthread_mutexattr_settype(&Attributes,PTHREAD_MUTEX_OWNERTERM_NP);
        pthread_mutexattr_setpshared(&Attributes,PTHREAD_PROCESS_SHARED);
        
        rc = pthread_mutex_init(&hMutex, &Attributes);
        if (rc) HandleConstructionError("Mutex-Init");
        pthread_mutexattr_destroy(&Attributes);
        return;
        }

OwnerTermMutex::~OwnerTermMutex()
        {
        pthread_mutex_destroy(&hMutex);
        return;
        }

void OwnerTermMutex::TakeRCHandler(int rc)
        {
        static const char *ProcName = "OwnerTermMutex::TakeRCHandler";
        static volatile int traceNesting = 0;
        static int retval = -666;
        if (rc == EOWNERTERM)
                {
                const char *Msg = "%s: Mutex Owner Died. Ending Thread 0x%llX";
                if (traceNesting == 0)
                        {
                        traceNesting++;
                        TERROR((Msg,ProcName,GetThreadID()));
                        traceNesting--;
                        }
                else fprintf(stderr,Msg,ProcName,GetThreadID());
                pthread_exit(&retval);
                }
        else if (rc)
                {
                const char *Msg = "%s: Critical (%d). Ending Thread 0x%llX";
                if (traceNesting == 0)
                        {
                        traceNesting++;
                        TERROR((Msg,ProcName,rc,GetThreadID()));
                        traceNesting--;
                        }
                else fprintf(stderr,Msg,ProcName,rc,GetThreadID());
                pthread_exit(&retval);
                }
        return;
        }

#endif /* !__OS400_TGTVRM__ */

#if defined(__OS400_TGTVRM__) 

// ********************************
// **** OS400: CriticalSection ****
// ********************************

#define SPINLOCK_LOCKED 1
#define SPINLOCK_UNLOCKED 0

CriticalSection::CriticalSection()
        {
        CS = SPINLOCK_UNLOCKED;
        return;
        }

void CriticalSection::Take()
        {                                       // Infinite spin-lock.
        int SetLock = SPINLOCK_LOCKED;
        int IsUnlocked = SPINLOCK_UNLOCKED;
        while (_CMPSWP(&IsUnlocked,&CS,SetLock) == 0)
                {
                sched_yield();
                IsUnlocked = SPINLOCK_UNLOCKED;
                }
        return;
        }

bool CriticalSection::TryTake()
        {
        int SetLock = SPINLOCK_LOCKED;        
        int IsUnlocked = SPINLOCK_UNLOCKED;
        return _CMPSWP(&IsUnlocked,&CS,SetLock) != 0;
        }

void CriticalSection::Release()
        {
        int IsLocked = SPINLOCK_LOCKED;
        int SetUnlock = SPINLOCK_UNLOCKED;
        _CMPSWP(&IsLocked,&CS,SetUnlock);
        assert(IsLocked == SPINLOCK_LOCKED);    // Assume was locked.
        return;
        }

#elif defined(_AIX)

// ******************************
// **** AIX: CriticalSection ****
// ******************************

#define SPINLOCK_LOCKED 1
#define SPINLOCK_UNLOCKED 0

CriticalSection::CriticalSection()
        {
        CS = SPINLOCK_UNLOCKED;
        return;
        }

void CriticalSection::Take()
        {                                       // Infinite spin-lock.
        const int SetLock = SPINLOCK_LOCKED;        
        const int IsUnlocked = SPINLOCK_UNLOCKED;
        while (_check_lock(&CS,IsUnlocked,SetLock)) sched_yield();
        return;
        }

bool CriticalSection::TryTake()
        {
        const int SetLock = SPINLOCK_LOCKED;        
        const int IsUnlocked = SPINLOCK_UNLOCKED;
        return !_check_lock(&CS,IsUnlocked,SetLock);
        }

void CriticalSection::Release()
        {
        const int SetUnlock = SPINLOCK_UNLOCKED;
        _clear_lock(&CS,SetUnlock);
        return;
        }
        
#elif defined(__USE_XOPEN2K) /* Unix with Spinlocks */

// *******************************
// **** UNIX: CriticalSection ****
// *******************************

CriticalSection::CriticalSection()
        {
        pthread_spin_init(&CS,PTHREAD_PROCESS_SHARED);
        return;
        }

CriticalSection::~CriticalSection()
        {
        pthread_spin_destroy(&CS);
        return;
        }

#elif !defined(_WIN32) /* Unix */

        #warning Using Mutex for CriticalSection

#endif /* Unix */

// ********************
// **** Share Lock ****
// ********************

void ShareLock::TakeShared()
        {
        ShareGate.Take();
        ShareCountCS.Take();
        unsigned nShared = ++ShareCount;
        ShareCountCS.Release();
        if (nShared == 1) ExclusiveGate.Take();         // Will never wait.
        ShareGate.Release();
        return;
        }

void ShareLock::ReleaseShared()
        {
        ShareCountCS.Take();
        unsigned nShared = --ShareCount;
        ShareCountCS.Release();
        if (nShared == 0) ExclusiveGate.Release();
        return;
        }

void ShareLock::TakeExclusive()
        {
        ShareGate.Take();
        ExclusiveGate.Take();
        return;
        }

void ShareLock::ReleaseExclusive()
        {
        ExclusiveGate.Release();
        ShareGate.Release();
        return;
        }

void ShareLock::ReleaseExclusiveTakeShared()
        {
        ShareCountCS.Take();
        ++ShareCount;
        ShareCountCS.Release();
        ShareGate.Release();
        return;
        }

// **************************************************************************
// **** ASThread ************************************************************
// **************************************************************************

// ******************
// **** ASThread ****
// ******************

TLSKey_t ASThread::ASThreadKey = {0};

ASThread::ASThread() : StartSemaphore(0) 
        {
  #ifndef _WIN32
        Detached = true;
  #endif
        memset(&hThread,0,sizeof(hThread));
        Active = false;
        ReturnCode = 0;
        OSThreadID = (ASThreadID_t)-1;
        if (!ASThreadKey)
                {
                ASThreadKey = CreateTLS();
                assert(ASThreadKey != INVALID_TLSKEY);
                SetTLS(ASThreadKey,NULL);
                }
        return;
        }

ASThread::~ASThread()
        {
        static const char *ProcName = "ASThread::~ASThread";
        Kill(1);
  #ifdef _WIN32
        if (hThread) 
                {
                BOOL rc = CloseHandle(hThread);
                if (!rc)
                        {
                        TERROR(("%s: Invalid Handle: %x",ProcName,hThread));
                        }
                }
  #else
        if (!Detached) 
                {
    #ifdef __MVS__
                int rc = pthread_detach(&hThread);
    #else
                int rc = pthread_detach(hThread);
    #endif
                if (rc)
                        {
                        TERROR(("%s: Invalid Handle (errno=%u)",ProcName,
                                                                errno));
                        }
                }
  #endif
        return;
        }

#ifdef _WIN32

unsigned __stdcall ASThread::Bootstrap(void *ASThreadThis)

#else /* Unix */

void* ASThread::Bootstrap(void *ASThreadThis)

#endif /* Unix */
        {
        ASThread *This = (ASThread *)ASThreadThis;

  #ifndef _WIN32 /* Unix */
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,0);
  #endif

        SetTLS(This->ASThreadKey,This);
        This->OSThreadID = GetThreadID();
        This->StartSemaphore.Release();

        int rc = This->ThreadMain();

        This->ActiveMutex.Take();
        This->ReturnCode = rc;
        This->Active = false;
        This->ActiveMutex.Release();

        RunExitHandlers();

  #ifdef _WIN32

    #ifdef  _MT
        _endthreadex(rc);
    #endif
        return rc;

  #else /* Unix */

        pthread_exit(&This->ReturnCode);
        return &This->ReturnCode;

  #endif
        }

bool ASThread::IsCurrentThread()
        {
        return GetTLS(ASThreadKey) == this;
        }

bool ASThread::IsStarted()
        {
        ActiveMutex.Take();
        bool isStarted = Active;
        ActiveMutex.Release();
        return isStarted;
        }

bool ASThread::Start()
        {
        static const char *ProcName = "ASThread::Start";
        ActiveMutex.Take();
        if (Active)
                {
                ActiveMutex.Release();
                return true;
                }
  #ifdef _WIN32

    #ifdef _MT

        unsigned ThreadID;
        ASThreadHANDLE_t hPrivate;
        hPrivate = (HANDLE)_beginthreadex(NULL,
                                          0,
                                          Bootstrap,
                                          this,
                                          0,
                                          &ThreadID);
        if (hPrivate)
                {

      #ifndef __BORLANDC__

                hThread = hPrivate;
      #else                             // Borland _endthreadex closes the
                DWORD StatusFlags;      // handle; so make a copy.
                BOOL Inherit = false;
                HANDLE hProcess = GetCurrentProcess();
                if (!GetHandleInformation(hPrivate,&StatusFlags))
                        {
                        Inherit = StatusFlags & HANDLE_FLAG_INHERIT;
                        }
                DWORD Options = DUPLICATE_SAME_ACCESS;
                BOOL rc = DuplicateHandle(hProcess,
                                          hPrivate,
                                          hProcess,
                                          &hThread,
                                          0,
                                          Inherit,
                                          Options);
                if (!rc)
                        {
                        TERROR(("%s: DuplicateHandle Failed (errno=%u)",
                                ProcName,
                                GetLastError()));
                        hThread = hPrivate;
                        }

      #endif /*__BORLANDC__ */

                hPrivate = NULL;
                StartSemaphore.Take();
                Active = true;
                }
        else    {
                TERROR(("%s: Failed to Start Thread (errno=%u)",ProcName,
                                                                errno));
                Active = false;
                }

    #else /* !_MT */

        TERROR(("%s: Failed to Start Thread: Not a MT runtime",ProcName));
        Active = false;

    #endif /* !_MT */

  #else /* Unix */

        sigset_t sigsetALL;             // Mask signals so child inherits
        sigset_t sigsetORG;             // the mask and won't receive 
        sigfillset(&sigsetALL);         // signals by default.

        pthread_sigmask(0,NULL,&sigsetORG);
        pthread_sigmask(SIG_BLOCK,&sigsetALL,NULL);

        int rc = pthread_create(&hThread,NULL,Bootstrap,this);
        if (rc == 0)
                {
                StartSemaphore.Take();
                }
        else    {
                TERROR(("%s: Failed to Start Thread (errno=%u)",ProcName,
                                                                errno));
                }

        pthread_sigmask(SIG_SETMASK,&sigsetORG,NULL);

        Active = rc ? false : true;
        if (Active) Detached = false;

  #endif /* Unix */

        bool Status = Active;
        ActiveMutex.Release();
        return Status;
        }

int ASThread::Wait()
        {
        int rc;
        WaitMutex.Take();
        ActiveMutex.Take();
        if (!Active)
                {
                rc = (int)ReturnCode;
                ActiveMutex.Release();
                WaitMutex.Release();
                return rc;
                }
        ActiveMutex.Release();

  #ifdef _WIN32

        assert(hThread);
        WaitForSingleObject(hThread,INFINITE);
        ActiveMutex.Take();
        GetExitCodeThread(hThread,&ReturnCode);
        rc = (int)ReturnCode;

  #elif defined(__OS400_TGTVRM__)  /* iSeries */

        void *ExitCode;
        pthread_joinoption_np_t options;
        memset(&options, 0, sizeof(pthread_joinoption_np_t));
        options.leaveThreadAllocated = 1;
        rc = pthread_extendedjoin_np(hThread,&ExitCode,&options);
        ActiveMutex.Take();
        if (rc == 0)
                {
                if (ExitCode &&
                    ExitCode != PTHREAD_CANCELED &&
                    ExitCode != PTHREAD_EXCEPTION_NP)
                        {
                        ReturnCode = __INT(ExitCode);
                        }
                rc = ReturnCode;
                }

  #else /* Unix */

        void *ExitCode;
        rc = pthread_join(hThread,&ExitCode);
        ActiveMutex.Take();        
        if (rc == 0)
                {
                if (ExitCode && ExitCode != PTHREAD_CANCELED)
                        {
                        ReturnCode = *(int *)ExitCode;
                        }
                rc = ReturnCode;
                }
        Detached = true;                // POSIX detaches at join.

  #endif /* Unix */

        Active = false;
        ActiveMutex.Release();
        WaitMutex.Release();
        return rc;
        }

bool ASThread::_Kill(int ExitCode)
        {
        static const char *ProcName = "ASThread::_Kill";
        ActiveMutex.Take();
        if (!Active)
                {
                ActiveMutex.Release();
                return true;
                }
        ActiveMutex.Release();
  #ifdef _WIN32
        TERROR(("%s: Killing Thread %u Abnormally",ProcName,hThread));
        bool rc = TerminateThread(hThread,ExitCode) == TRUE;
  #else /* Unix */
        TERROR(("%s: Killing Thread Abnormally",ProcName));  
        bool rc = pthread_cancel(hThread) == 0;
  #endif /* Unix */
        ReturnCode = ExitCode;
        return rc;
        }

bool ASThread::Kill(int ExitCode)
        {
        ActiveMutex.Take();
        if (!Active)
                {
                ActiveMutex.Release();
                return true;
                }
        ActiveMutex.Release();
        bool rc = _Kill(ExitCode);
        Wait();
        return rc;
        }

// ************************
// **** At Thread Exit ****
// ************************
                                        // Note: MAX_THREAD_ATEXIT+1 handlers
static Mutex atexitMutex;               //       is NULL for unatexit().
static unsigned atexitCount = 0;
static atexitThreadFn_t atexitHandlers[MAX_THREAD_ATEXIT+1] = {0};
static void* atexitUserPtrs[MAX_THREAD_ATEXIT+1] = {0};
static unsigned atexitRegCount[MAX_THREAD_ATEXIT+1] = {0};

bool ASThread::atexit(atexitThreadFn_t Func, void *UserPtr)
        {
        atexitMutex.Take();
        for (unsigned i=0; i < atexitCount; i++)        // Alrady Registered?
                {
                if (atexitHandlers[i] == Func && atexitUserPtrs[i] == UserPtr)
                        {
                        atexitRegCount[i]++;
                        atexitMutex.Release();
                        return true;
                        }
                }
        if (atexitCount == MAX_THREAD_ATEXIT)           // Space Available?
                {
                atexitMutex.Release();
                assert(atexitCount < MAX_THREAD_ATEXIT);
                return false;
                }
        atexitHandlers[atexitCount] = Func;             // Register.
        atexitUserPtrs[atexitCount] = UserPtr;
        atexitRegCount[atexitCount] = 1;
        atexitCount++;
        atexitMutex.Release();
        return true;
        }

void ASThread::unatexit(atexitThreadFn_t Func, void *UserPtr)
        {
        atexitMutex.Take();
        for (unsigned i=0; i < atexitCount; i++)
                {
                if (atexitHandlers[i] == Func && atexitUserPtrs[i] == UserPtr)
                        {
                        if (--atexitRegCount[i]) break;
                        for (unsigned j=i; j < MAX_THREAD_ATEXIT; j++)
                                {
                                atexitHandlers[j] = atexitHandlers[j+1];
                                atexitUserPtrs[j] = atexitUserPtrs[j+1];
                                }
                        atexitCount--;
                        break;
                        }
                }
        atexitMutex.Release();
        return;
        }   

void ASThread::RunExitHandlers()
        {
        atexitMutex.Take();
        unsigned i = atexitCount;
        while (i--) atexitHandlers[i](atexitUserPtrs[i]);
        atexitMutex.Release();
        return;
        }

// **************************************************************************
// **** Thread Functions ****************************************************
// **************************************************************************

ASThread* GetActiveThread()
        {
        ASThread *Thread;
        if (ASThread::ASThreadKey)
                {
                Thread = (ASThread *)GetTLS(ASThread::ASThreadKey);
                }
        else Thread = NULL;
        return Thread; 
        }

bool IsActiveThread(ASThread *Thread)
        {
        return GetActiveThread() == Thread;
        }

ASThreadID_t GetThreadID()
        {
  #if defined(_WIN32)
        return (ASThreadID_t)GetCurrentThreadId();
  #elif defined(__OS400_TGTVRM__)
        pthread_id_np_t ID = pthread_getthreadid_np();
        return ((ASThreadID_t)ID.intId.hi << 32) + ID.intId.lo;
  #elif defined(__MVS__)
        pthread_t MySelf = pthread_self();
        return *(ASThreadID_t *)&MySelf;
  #else
        return (ASThreadID_t)pthread_self();
  #endif
        }

// **************************************************************************
// **** MessageList *********************************************************
// **************************************************************************

MessageList::MessageList() 
        {
        FreeCount = 0;
        SetAutoCleanup(true);
        FreeList.SetAutoCleanup(true);
        return;
        }

bool MessageList::PutMessage(void *Message)
        {
        MessageLink *Current;
        if (FreeCount)
                {
                Current = FreeList.GetFirst();
                FreeList.Remove(Current);
                FreeCount--;
                }
        else    {
                Current = new MessageLink;
                if (!Current) return false;
                }
        Current->Message = Message;
        AddBottom(Current);
        return true;
        }

bool MessageList::PutMessageFirst(void *Message)
        {
        MessageLink *Current;
        if (FreeCount)
                {
                Current = FreeList.GetFirst();
                FreeList.Remove(Current);
                FreeCount--;
                }
        else    {
                Current = new MessageLink;
                if (!Current) return false;
                }
        Current->Message = Message;
        AddTop(Current);
        return true;
        }

void* MessageList::GetMessage()
        {
        void *Message = NULL;
        MessageLink *Current = GetFirst();
        if (Current) 
                {
                Message = Current->Message;
                Remove(Current);
                if (FreeCount < FreeCountMAX)
                        {
                        Current->Message = NULL;
                        FreeList.AddTop(Current);
                        FreeCount++;
                        }
                else delete Current;
                }
        return Message;
        }

// **************************************************************************
// **** MessageQueue ********************************************************
// **************************************************************************

MessageQueue::MessageQueue() 
        : 
        MessageSemaphore(0)
        {
        MessageCount = 0;
        return;
        }

void MessageQueue::Clear()
        {
        while (GetMessageCount())
                {
                void *Msg = Receive();
                OnMessageDestruct(Msg);
                }
        return;
        }

void* MessageQueue::GetMessage()
        {
        QueueMutex.Take();
        void *Message = Messages.GetMessage();
        if (MessageCount) MessageCount--;
        QueueMutex.Release();
        return Message;
        }

unsigned MessageQueue::GetMessageCount()
        {
        unsigned Count;
        QueueMutex.Take();
        Count = MessageCount;
        QueueMutex.Release();
        return Count;
        }

void MessageQueue::OnMessageDestruct(void* /*Msg*/)
        {
        return;
        }

bool MessageQueue::PutMessage(void *Msg)
        {
        QueueMutex.Take();
        bool rc = Messages.PutMessage(Msg);
        if (rc) MessageCount++;
        QueueMutex.Release();
        return rc;
        }

bool MessageQueue::PutMessageFirst(void *Msg)
        {
        QueueMutex.Take();
        bool rc = Messages.PutMessageFirst(Msg);
        if (rc) MessageCount++;
        QueueMutex.Release();
        return rc;
        }

void* MessageQueue::Receive()
        {
        MessageSemaphore.Take();
        return GetMessage();
        }

void* MessageQueue::Receive(unsigned TimeoutMS)
        {
        bool rc = MessageSemaphore.Take(TimeoutMS);
        return rc ? GetMessage() : NULL;
        }

bool MessageQueue::Send(void *Msg)
        {
        bool rc = PutMessage(Msg);
        MessageSemaphore.Release();
        return rc;
        }

bool MessageQueue::SendPriority(void *Msg)
        {
        bool rc = PutMessageFirst(Msg);
        MessageSemaphore.Release();
        return rc;
        }

// **************************************************************************
// **** Message Queue Loop (Thread Core) ************************************
// **************************************************************************

int MessageQueueLoop::MessageQueueLoop_ShutdownMSG;  // Just use the address.

MessageQueueLoop::MessageQueueLoop()
        {
        Continue = true;
        return;
        }

void MessageQueueLoop::ClearQueue()
        {
        while (Queue.GetMessageCount())
                {
                void *Msg = Queue.Receive();
                if (Msg != &MessageQueueLoop_ShutdownMSG)
                        {
                        OnMessageDestruct(Msg);
                        }
                }
        return;
        }

bool MessageQueueLoop::OnShutdown() 
        {
        ClearQueue();
        return true;
        }

bool MessageQueueLoop::Send(void *Msg)
        {
        return Queue.Send(Msg);
        }

bool MessageQueueLoop::SendPriority(void *Msg)
        {
        return Queue.SendPriority(Msg);
        }

void* MessageQueueLoop::Receive()
        {
        return Queue.Receive();
        }

void* MessageQueueLoop::Receive(unsigned TimeoutMS)
        {
        return Queue.Receive(TimeoutMS);
        }

bool MessageQueueLoop::OnMessage(void* /*Msg*/)
        {
        return false;            // Message not handled.
        }

void MessageQueueLoop::OnMessageDestruct(void* /*Msg*/)
        {
        return;
        }

int MessageQueueLoop::Run()
        {
        while (Continue)
                {
                void *Msg = Queue.Receive();
                if (Msg == &MessageQueueLoop_ShutdownMSG)
                        {
                        OnShutdown();
                        Continue = false;
                        }
                else OnMessage(Msg);
                }
        Continue = true;
        return 0;
        }

bool MessageQueueLoop::Stop()                   // Stop self.
        {
        OnShutdown();
        _Break();
        ClearQueue();
        return true;
        }

bool MessageQueueLoop::Stop(bool Wait)          // Stop by other.
        {
        SendPriority(&MessageQueueLoop_ShutdownMSG);
        if (Wait) 
                {
                WaitToEnd();
                ClearQueue();
                }
        return true;
        }

// **************************************************************************
// **** Message Queue Thread ************************************************
// **************************************************************************

bool MessageQueueThread::Stop(bool WaitToEnd)
        {
        bool Status;
        if (IsCurrentThread()) Status = MessageQueueLoop::Stop();
        else Status = MessageQueueLoop::Stop(WaitToEnd);
        return Status;
        }

// **************************************************************************
// **** Thread Local Storage ************************************************
// **************************************************************************

#ifdef _WIN32

TLSKey_t CreateTLS()
        {
        return TlsAlloc();
        }

TLSKey_t CreateTLS(void (*)(void*))
        {
        return TlsAlloc();
        }

bool SetTLS(TLSKey_t &Key, const void *Value)
        {
        return TlsSetValue(Key,(void *)Value) == TRUE;
        }

void* GetTLS(TLSKey_t &Key)
        {
        return TlsGetValue(Key);
        }
           
bool FreeTLS(TLSKey_t &Key)
        {
        return TlsFree(Key) == TRUE;
        }

#else

TLSKey_t CreateTLS()
        {
        TLSKey_t Key;
        int rc = pthread_key_create((pthread_key_t *)&Key,NULL);
        return rc ? INVALID_TLSKEY : Key;
        }

TLSKey_t CreateTLS(void (*Destructor)(void*))
        {
        TLSKey_t Key;
        int rc = pthread_key_create((pthread_key_t *)&Key,Destructor);
        return rc ? INVALID_TLSKEY : Key;
        }

bool SetTLS(TLSKey_t &Key, const void *Value)
        {
        return pthread_setspecific((pthread_key_t &)Key,(void *)Value) == 0;
        }

void* GetTLS(TLSKey_t &Key)
        {
        return pthread_getspecific((pthread_key_t &)Key);
        }

bool FreeTLS(TLSKey_t &Key)
        {
        return pthread_key_delete((pthread_key_t &)Key);
        }

#endif /* Unix */

} /* namespace ASThreadNamespace */

// **************************************************************************
// **************************** End of File *********************************
// **************************************************************************
