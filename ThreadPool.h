#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <Windows.h>

typedef void ( *ThreadPoolFunc )( void* );
typedef struct SThreadPoolTask
{
    ThreadPoolFunc m_pFunc;
    void*          m_pPars;
} SThreadPoolTask;

typedef struct SQueue
{
    SThreadPoolTask* m_ptrQueue;
    SThreadPoolTask* m_ptrBegin;
    unsigned long m_ulSize;
    unsigned long m_ulCapacity;
} SQueue;

typedef struct SMemPool
{
    void** m_ptrPool;
    unsigned long m_ulSize;
    unsigned long m_ulCapacity;
} SMemPool;

typedef struct SThreadPool
{
    int m_iIsWorking;
    HANDLE m_hEventForThreads;
    HANDLE m_hEventForJoinAll;
    HANDLE m_hEventForPutTask;
    CRITICAL_SECTION m_cCriticalSection;
    SQueue m_cTaskQueue;
    HANDLE m_cThreadPool[ 5 ];
    DWORD m_dwThreadPoolSize;
    SMemPool m_cMemPool;
    unsigned long m_ulMaxQueueSize;
    unsigned long m_ulTaskRemained;
} SThreadPool;

void AllocQueue( SQueue* );
void FreeQueue( SQueue* );
void ReallocQueue( SQueue*, unsigned long );
void PushQueue( SQueue*, const SThreadPoolTask* );
void PopQueue( SQueue*, SThreadPoolTask* );
void PrintDebug( const SQueue* );

void AllocMemPool( SMemPool* );
void FreeMemPool( SMemPool* );
void ReallocMemPool( SMemPool*, unsigned long );
void PushMemPool( SMemPool*, void* );
void PopMemPool( SMemPool*, void** );

void AllocThreadPool( SThreadPool*, unsigned long, unsigned long );
void FreeThreadPool( SThreadPool* );
void AllocateTask( SThreadPool*, SThreadPoolTask* );
void PutTaskInQueue( SThreadPool*, const SThreadPoolTask* );
void ThreadPoolJoinAll( SThreadPool* );
DWORD WINAPI ThreadPoolWorkProc( LPVOID );

#endif//__THREAD_POOL_H__