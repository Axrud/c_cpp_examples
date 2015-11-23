#include "ThreadPool.h"

void AllocQueue( SQueue* ptrQueue )
{
    ptrQueue->m_ulCapacity = 100;
    ptrQueue->m_ptrQueue = ( SThreadPoolTask* )malloc( ptrQueue->m_ulCapacity * sizeof( ptrQueue->m_ptrQueue[ 0 ] ) );
    ptrQueue->m_ptrBegin = NULL;
    ptrQueue->m_ulSize = 0;
}

void FreeQueue( SQueue* ptrQueue )
{
    free( ptrQueue->m_ptrQueue );
    ptrQueue->m_ptrQueue = NULL;
    ptrQueue->m_ptrBegin = NULL;
    ptrQueue->m_ulSize = 0;
    ptrQueue->m_ulCapacity = 0;
}

void ReallocQueue( SQueue* ptrQueue, unsigned long ulRequired )
{
    unsigned long ulOldBeginOffset = ptrQueue->m_ptrBegin - ptrQueue->m_ptrQueue;
    unsigned long ulMove2End = 0;

    if( ulRequired < ptrQueue->m_ulCapacity )
        return;

    if( ptrQueue->m_ptrBegin + ptrQueue->m_ulSize > ptrQueue->m_ptrQueue + ptrQueue->m_ulCapacity )
        ulMove2End = ptrQueue->m_ulCapacity - ( ptrQueue->m_ptrBegin - ptrQueue->m_ptrQueue );
    while( ulRequired > ptrQueue->m_ulCapacity )
        ptrQueue->m_ulCapacity *= 2;

    ptrQueue->m_ptrQueue = ( SThreadPoolTask* )realloc( ptrQueue->m_ptrQueue, ptrQueue->m_ulCapacity * sizeof( ptrQueue->m_ptrQueue[ 0 ] ) );
    if( 0 != ulMove2End )
        memmove_s( ptrQueue->m_ptrQueue + ptrQueue->m_ulCapacity - ulMove2End, ulMove2End * sizeof( ptrQueue->m_ptrQueue[ 0 ] ), ptrQueue->m_ptrQueue + ulOldBeginOffset, ulMove2End * sizeof( ptrQueue->m_ptrQueue[ 0 ] ) );
    ptrQueue->m_ptrBegin = ptrQueue->m_ptrQueue + ptrQueue->m_ulCapacity - ulMove2End;
}

void PushQueue( SQueue* ptrQueue, const SThreadPoolTask* ptr )
{
    if( ptrQueue->m_ulCapacity == ptrQueue->m_ulSize )
        ReallocQueue( ptrQueue, ptrQueue->m_ulCapacity * 2 );

    if( ptrQueue->m_ptrBegin == ptrQueue->m_ptrQueue || NULL == ptrQueue->m_ptrBegin )
        ptrQueue->m_ptrBegin = ptrQueue->m_ptrQueue + ptrQueue->m_ulCapacity - 1;
    else
        ptrQueue->m_ptrBegin--;
    memcpy_s( ptrQueue->m_ptrBegin, sizeof( ptrQueue->m_ptrBegin[ 0 ] ), ptr, sizeof( *ptr ) );
    ptrQueue->m_ulSize++;
}

void PopQueue( SQueue* ptrQueue, SThreadPoolTask* ptr )
{
    const SThreadPoolTask* ptrPop = NULL;
    if( 0 == ptrQueue->m_ulSize )
        memset( ptr, 0, sizeof( *ptr ) );
    else
    {
        ptrPop = ptrQueue->m_ptrBegin + ptrQueue->m_ulSize - 1;
        if( ptrPop > ptrQueue->m_ptrQueue + ptrQueue->m_ulCapacity - 1 )
            ptrPop -= ptrQueue->m_ulCapacity;
        memcpy_s( ptr, sizeof( *ptr ), ptrPop, sizeof( *ptrPop ) );
        ptrQueue->m_ulSize--;
    }
}

void PrintDebug( const SQueue* ptrQueue )
{
    ptrQueue;
    /*const SThreadPoolTask* ptrIter = ptrQueue->m_ptrBegin;
    const unsigned long ulCapacity = ptrQueue->m_ulCapacity;
    unsigned long i = 0;
    for( i = 0; i < ptrQueue->m_ulSize; ++i )
    {
        wprintf_s( L"%u ", *ptrIter );
        if( ptrIter - ptrQueue->m_ptrQueue == ulCapacity - 1 )
            ptrIter = ptrQueue->m_ptrQueue;
        else
            ++ptrIter;
    }
    _putws( L"" );*/
}

void AllocMemPool( SMemPool* ptrPool )
{
    ptrPool->m_ulSize = 0;
    ptrPool->m_ulCapacity = 100;
    ptrPool->m_ptrPool = ( void** )malloc( ptrPool->m_ulCapacity * sizeof( ptrPool->m_ptrPool[ 0 ] ) );
}

void FreeMemPool( SMemPool* ptrPool )
{
    unsigned long i;
    for( i = 0; i < ptrPool->m_ulSize; ++i )
        free( ptrPool->m_ptrPool[ i ] );
    free( ptrPool->m_ptrPool );
    ptrPool->m_ptrPool = NULL;
    ptrPool->m_ulSize = 0;
    ptrPool->m_ulCapacity = 0;
}

void ReallocMemPool( SMemPool* ptrPool, unsigned long ulRequired )
{
    if( ulRequired < ptrPool->m_ulCapacity )
        return;

    while( ulRequired > ptrPool->m_ulCapacity )
        ptrPool->m_ulCapacity *= 2;

    ptrPool->m_ptrPool = ( void** )realloc( ptrPool->m_ptrPool, ptrPool->m_ulCapacity * sizeof( ptrPool->m_ptrPool[ 0 ] ) );
}

void PushMemPool( SMemPool* ptrPool, void* ptr )
{
    if( ptrPool->m_ulCapacity == ptrPool->m_ulSize )
        ReallocMemPool( ptrPool, ptrPool->m_ulCapacity * 2 );
    ptrPool->m_ptrPool[ ptrPool->m_ulSize++ ] = ptr;
}

void PopMemPool( SMemPool* ptrPool, void** ptr )
{
    if( 0 == ptrPool->m_ulSize )
        *ptr = malloc( 32 );
    else
        *ptr = ptrPool->m_ptrPool[ --( ptrPool->m_ulSize ) ];
}

void AllocThreadPool( SThreadPool* ppool, unsigned long ulThreadPoolSize, unsigned long ulMaxQueueSize )
{
    CRITICAL_SECTION* const pcs = &( ppool->m_cCriticalSection );
    unsigned long i;
    HANDLE thrd;

#if defined ( _WIN32_WINNT ) && ( _WIN32_WINNT >= 0x0403 )
    InitializeCriticalSectionAndSpinCount( pcs, 0x00000064 );
#else
    InitializeCriticalSection( pcs );
#endif

    if( ulThreadPoolSize > 5 )
        ulThreadPoolSize = 5;

    if( ulMaxQueueSize > 1500 )
        ulMaxQueueSize = 1500;

    EnterCriticalSection( pcs );
    ppool->m_iIsWorking = 1;
    ppool->m_dwThreadPoolSize = 0;
    ppool->m_ulMaxQueueSize = ulMaxQueueSize;
    ppool->m_ulTaskRemained = 0;
    AllocMemPool( &( ppool->m_cMemPool ) );
    AllocQueue( &( ppool->m_cTaskQueue ) );
    ppool->m_hEventForThreads = CreateEvent( NULL, FALSE, FALSE, NULL );
    ppool->m_hEventForJoinAll = CreateEvent( NULL, FALSE, FALSE, NULL );
    ppool->m_hEventForPutTask = CreateEvent( NULL, FALSE, FALSE, NULL );
    for( i = 0; i < ulThreadPoolSize; ++i )
    {
        thrd = CreateThread( NULL, 0, ThreadPoolWorkProc, ppool, 0, NULL );
        if( NULL != thrd )
            ppool->m_cThreadPool[ ppool->m_dwThreadPoolSize++ ] = thrd;
    }
    LeaveCriticalSection( pcs );
}

void FreeThreadPool( SThreadPool* ppool )
{
    CRITICAL_SECTION* const pcs = &( ppool->m_cCriticalSection );
    DWORD dwIndex;

    ThreadPoolJoinAll( ppool );

    EnterCriticalSection( pcs );
    ppool->m_iIsWorking = 0;
    LeaveCriticalSection( pcs );

    while( 0 != ppool->m_dwThreadPoolSize )
    {
        SetEvent( ppool->m_hEventForThreads );
        dwIndex = WaitForMultipleObjects( ppool->m_dwThreadPoolSize, ppool->m_cThreadPool, FALSE, INFINITE );
        dwIndex -= WAIT_OBJECT_0;
        if( dwIndex <= ppool->m_dwThreadPoolSize )
        {
            CloseHandle( ppool->m_cThreadPool[ dwIndex ] );
            memmove_s( ppool->m_cThreadPool + dwIndex, ( ppool->m_dwThreadPoolSize - dwIndex ) * sizeof( ppool->m_cThreadPool[ 0 ] ), ppool->m_cThreadPool + dwIndex + 1, ( ppool->m_dwThreadPoolSize - dwIndex - 1 ) * sizeof( ppool->m_cThreadPool[ 0 ] ) );
            ppool->m_dwThreadPoolSize--;
        }
        else
        {
            for( dwIndex = 0; dwIndex < ppool->m_dwThreadPoolSize; ++dwIndex )
                CloseHandle( ppool->m_cThreadPool[ dwIndex ] );
            ppool->m_dwThreadPoolSize = 0;
        }
    }

    EnterCriticalSection( pcs );
    FreeMemPool( &( ppool->m_cMemPool ) );
    FreeQueue( &( ppool->m_cTaskQueue ) );
    memset( &( ppool->m_cThreadPool ), 0, sizeof( ppool->m_cThreadPool ) );
    CloseHandle( ppool->m_hEventForThreads );
    CloseHandle( ppool->m_hEventForJoinAll );
    CloseHandle( ppool->m_hEventForPutTask );
    ppool->m_hEventForThreads = NULL;
    ppool->m_hEventForJoinAll = NULL;
    ppool->m_hEventForPutTask = NULL;
    ppool->m_dwThreadPoolSize = 0;
    ppool->m_ulMaxQueueSize = 0;
    ppool->m_ulTaskRemained = 0;
    LeaveCriticalSection( pcs );
    DeleteCriticalSection( pcs );
    memset( pcs, 0, sizeof( *pcs ) );
}

void AllocateTask( SThreadPool* ppool, SThreadPoolTask* ptask )
{
    CRITICAL_SECTION* const pcs = &( ppool->m_cCriticalSection );
    EnterCriticalSection( pcs );
    ptask->m_pFunc = NULL;
    PopMemPool( &( ppool->m_cMemPool ), &( ptask->m_pPars ) );
    LeaveCriticalSection( pcs );
}

void PutTaskInQueue( SThreadPool* ppool, const SThreadPoolTask* ptask )
{
    CRITICAL_SECTION* const pcs = &( ppool->m_cCriticalSection );
    unsigned long ulMaxQueueSize, ulQueueSize;
    HANDLE hEventForPutTask;
    int iIsWorking;

    if( NULL == ptask->m_pFunc )
        return;

    for(;;)
    {
        EnterCriticalSection( pcs );
        iIsWorking = ppool->m_iIsWorking;
        ulMaxQueueSize = ppool->m_ulMaxQueueSize;
        ulQueueSize = ppool->m_cTaskQueue.m_ulSize;
        hEventForPutTask = ppool->m_hEventForPutTask;
        LeaveCriticalSection( pcs );

        if( !iIsWorking )
            return;

        if( ulQueueSize >= ulMaxQueueSize )
            WaitForSingleObject( hEventForPutTask, INFINITE );
        else
            break;
    }

    EnterCriticalSection( pcs );
    PushQueue( &( ppool->m_cTaskQueue ), ptask );
    ppool->m_ulTaskRemained++;
    SetEvent( ppool->m_hEventForThreads );
    LeaveCriticalSection( pcs );
}

void ThreadPoolJoinAll( SThreadPool* ppool )
{
    CRITICAL_SECTION* const pcs = &( ppool->m_cCriticalSection );
    unsigned long ulTaskRemained;
    HANDLE hEventForJoinAll, hEventForThreads;

    for(;;)
    {
        EnterCriticalSection( pcs );
        ulTaskRemained = ppool->m_ulTaskRemained;
        hEventForJoinAll = ppool->m_hEventForJoinAll;
        hEventForThreads = ppool->m_hEventForThreads;
        LeaveCriticalSection( pcs );

        if( 0 != ulTaskRemained )
        {
            SetEvent( hEventForThreads );
            if( WAIT_FAILED == WaitForSingleObject( hEventForJoinAll, INFINITE ) )
                return;
        }
        else
            return;
    }
}

DWORD WINAPI ThreadPoolWorkProc( LPVOID lpParameter )
{
    SThreadPool* pThreadPool = ( SThreadPool* )lpParameter;
    SThreadPoolTask task;
    unsigned long ulSize;
    int iIsWorking;
    HANDLE hEventForThreads;
    CRITICAL_SECTION* const pcs = &( pThreadPool->m_cCriticalSection );

    for(;;)
    {
        task.m_pFunc = NULL;
        task.m_pPars = NULL;

        EnterCriticalSection( pcs );
        ulSize = pThreadPool->m_cTaskQueue.m_ulSize;
        hEventForThreads = pThreadPool->m_hEventForThreads;
        LeaveCriticalSection( pcs );

        if( 0 != ulSize || WAIT_FAILED != WaitForSingleObject( hEventForThreads, INFINITE ) )
        {
            EnterCriticalSection( pcs );
            iIsWorking = pThreadPool->m_iIsWorking;
            LeaveCriticalSection( pcs );

            if( 0 == iIsWorking )
                return 0;

            EnterCriticalSection( pcs );
            if( 0 != pThreadPool->m_cTaskQueue.m_ulSize )
            {
                PopQueue( &( pThreadPool->m_cTaskQueue ), &task );
                if( pThreadPool->m_ulMaxQueueSize - 1 == pThreadPool->m_cTaskQueue.m_ulSize )
                    SetEvent( pThreadPool->m_hEventForPutTask );
            }
            LeaveCriticalSection( pcs );
        }

        if( task.m_pFunc && task.m_pPars )
        {
            ( *task.m_pFunc )( task.m_pPars );
            EnterCriticalSection( pcs );
            pThreadPool->m_ulTaskRemained--;
            if( 0 == pThreadPool->m_ulTaskRemained )
                SetEvent( pThreadPool->m_hEventForJoinAll );
            PushMemPool( &( pThreadPool->m_cMemPool ), task.m_pPars );
            iIsWorking = pThreadPool->m_iIsWorking;
            LeaveCriticalSection( pcs );
            if( 0 == iIsWorking )
                return 0;
        }
    }
}