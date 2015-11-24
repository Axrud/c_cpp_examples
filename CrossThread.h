#pragma once
#include <stdexcept>
#include <string>
#include <cpl/CriticalSection.h>
#include <cpl/CrossUtils.h>

#ifndef WIN32
#define USE_PTHREAD_THREAD_FORCE
#else
#endif

#ifndef USE_PTHREAD_THREAD_FORCE
#include <process.h>
#include <Windows.h>
#else
#include <pthread.h>
#include <signal.h>
#endif

#ifdef WIN32
namespace WindowsSpec
{
	const DWORD MS_VC_EXCEPTION = 0x406D1388;

	//
	// How to: Set a Thread Name in Native Code (http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx)
	//

#pragma pack(push,8)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
#pragma pack(pop)

	static void SetDbgThreadName( DWORD dwThreadID, const char * szName )
	{
		THREADNAME_INFO info;
		info.dwType = 0x1000;
		info.szName = szName;
		info.dwThreadID = dwThreadID;
		info.dwFlags = 0;

		__try
		{
			RaiseException( MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*) &info );
		}
		__except( EXCEPTION_EXECUTE_HANDLER )
		{
		}
	}
}
#endif

//!
//!	@brief	Thread main function
//!
class ThreadMainCall
{
public:
	virtual ~ThreadMainCall() {}

	//!
	//!	@brief	Internal main thread function
	//!	@return	Result 0 - if all OK and continue working, otherwise terminate thread
	//!
	virtual int mainThread() = 0;
};

//!
//!	@brief	Thread priority
//!
typedef enum ThreadPriority
{
	TP_Low								= 0,							//!< Low priority
	TP_Normal							= 1,							//!< Normal priority
	TP_High								= 2								//!< High priority
} ThreadPriority;

//!
//!	@brief	Thread template class
//!	@remark	main disadvantages:
//!		- critical section using - replace by atomics
//!		- sleep using on stop
//!		- after terminate, thread class useless
//!
template<typename _ThreadImp, typename _ThreadType>
class ThreadMainImplement : public ThreadMainCall
{
public:
	typedef _ThreadImp ThreadImplementation;
	typedef _ThreadType Handle;

	//!
	//!	@brief	Thread state
	//!
	typedef enum ThreadState
	{
		TS_Stop								= 1,							//!< Stopped
		TS_Running							= 2,							//!< Running
		TS_Terminating						= 3								//!< Terminated. Thread is dead
	} ThreadState;

public:
	//!
	//!	@brief	Constructor
	//!	@param	Priority Thread base priority
	//!	@throw	std::exception with error description
	//!
	ThreadMainImplement( ThreadPriority Priority = TP_Normal ):m_nThreadID(0),m_ThreadState(TS_Stop),m_ThreadNewState(TS_Stop)
	{
		if( !ThreadImplementation::createThread( m_hThread, this, m_nThreadID, Priority ) )
			throw std::runtime_error( "Create thread failed" );
	}

	virtual ~ThreadMainImplement()
	{
		//
		// End thread
		//
		Terminate( true );

		ThreadImplementation::FinalThread( m_hThread );
	}

	//!
	//!	@brief	Gets current thread state
	//!	@return	Thread state
	//!
	ThreadState GetThreadState() const
	{
		CSLocker alock( m_Lock );
		return m_ThreadState;
	}

	//!
	//!	@brief	Gets new thread state
	//!	@return	Thread state
	//!
	ThreadState GetThreadNewState() const
	{
		CSLocker alock( m_Lock );
		return m_ThreadNewState;
	}

	//!
	//!	@brief	Checks if thread is alive
	//!	@return	True/false
	//!
	inline bool IsThreadAlive() const
	{
		return ThreadImplementation::isAlive( m_hThread );
	}

	//!
	//!	@brief	Wait until thread will exit
	//!
	inline void Join()
	{
		ThreadImplementation::Join( m_hThread );
	}

	//!
	//!	@brief	Sets thread name
	//!	@param	sName Thread name
	//!	@remark	If this can be implemented
	//!
	inline void SetThreadName( const std::string & sName )
	{
		ThreadImplementation::SetThreadName( m_nThreadID, sName );
	}

	//!
	//!	@brief	Terminate thread
	//!
	inline void TerminateThread()
	{
		//
		// Set new status
		//
		SetThreadState( TS_Terminating );

		if( IsThreadAlive() )
		{
			//
			// End thread
			//
			ThreadImplementation::endThread( m_hThread );
		}
	}

	//!
	//!	@brief	Access to thread object
	//!	@return	Object
	//!
	inline Handle GetThread() const { return m_hThread; }

	//!
	//!	@brief	Runs thread
	//!	@param	bWait Wait until thread runs or thread terminated
	//!	@return	True;False if thread terminated
	//!
	inline bool Run( bool bWait = false ) { ChangeThreadState( TS_Running ); if( bWait ) return WaitForStatus( TS_Running ); return true; }

	//!
	//!	@brief	Stops thread
	//!	@param	bWait Wait until thread stops or thread terminated
	//!	@return	True;False if thread terminated
	//!
	inline bool Stop( bool bWait = false ) { ChangeThreadState( TS_Stop ); if( bWait ) return WaitForStatus( TS_Stop ); return true; }

	//!
	//!	@brief	Terminate thread
	//!	@param	bWait Wait until thread is terminating
	//!
	inline void Terminate( bool bWait = true ) { ChangeThreadState( TS_Terminating ); if( bWait ) Join(); }

	//!
	//!	@brief	Creates static thread
	//!	@param	thrMain Thread main callback
	//!	@param	Priority Thread priority
	//!	@return	True/false
	//!
	static bool CreateStaticThread( ThreadMainCall * thrMain, ThreadPriority Priority = TP_Normal )
	{
		Handle hThread;
		size_t nThreadID;

		//
		// Create thread
		//
		if( !ThreadImplementation::createThread( hThread, thrMain, nThreadID, Priority ) )
			return false;

		//
		// Free thread resources
		//
		ThreadImplementation::FinalThread( hThread );
		return true;
	}

private:
	//!
	//!	@brief	On thread start
	//!	@return	True/false
	//!
	virtual bool OnStart() { return true; }

	//!
	//!	@brief	On thread exit
	//!	@param	nErrorCode Thread exit code
	//!
	virtual void OnExit( int nErrorCode ) {}

	//!
	//!	@brief	Main thread function
	//!	@return	Continue error code, 0 - if continue work, else terminate thread
	//!
	virtual int OnRun()
	{
		//
		// Immediate exit
		//
		return -4;
	}

private:
	ThreadMainImplement( const ThreadMainImplement & );
	ThreadMainImplement & operator=( const ThreadMainImplement & );

protected:
	//!
	//!	@brief	Store new thread state value
	//!	@param	NewState New thread state
	//!	@param	bNewState If true then just signal thread to start
	//!
	inline void SetThreadState( ThreadState NewState, bool bNewState = true )
	{
		CSLocker alock( m_Lock );
		if( bNewState )
			m_ThreadNewState = NewState;
		else
			m_ThreadState = NewState;
	}

	//!
	//!	@brief	Change thread state
	//!	@param	NewState New thread state
	//!
	void ChangeThreadState( ThreadState NewState )
	{
		if( !IsThreadAlive() )
		{
			//
			// Thread already terminated, then update state to terminated
			//
			SetThreadState( TS_Terminating );
			return;
		}

		ThreadState CurState = GetThreadState();

		if( CurState == TS_Terminating )
			return;

		if( CurState != NewState )
			SetThreadState( NewState );
	}

	//!
	//!	@brief	Wait while thread change his state
	//!	@param	WaitState Wait state
	//!	@return	True/false
	//!
	bool WaitForStatus( ThreadState WaitState )
	{
		if( !IsThreadAlive() )
			return false;

		ThreadState nCurState;

		while( (nCurState = GetThreadState()) != WaitState )
		{
			if( nCurState == TS_Terminating || !IsThreadAlive() )
			{
				//
				// Terminating
				//
				return false;
			}

			sys::SleepMillisec( 100 );
		}

		return true;
	}

public:
	virtual int mainThread()
	{
		//
		// Initialize thread
		//
		if( !OnStart() )
			return -1;

		int32_t nResult = 0;

		ThreadState nState = TS_Stop, nPrevState = TS_Stop;
		while( nResult == 0 )
		{
			//
			// Check current status
			//
			nState = GetThreadNewState();

			if( nPrevState != nState )
			{
				//
				// State changed, update thread state
				//
				nPrevState = nState;
				SetThreadState( nState, false );
				continue;
			}

			if( nState == TS_Stop )
			{
				//
				// Thread stopped, delay
				//
				sys::SleepMillisec( 1000 );
				continue;
			}
			else if( nState == TS_Terminating )
			{
				//
				// Exit from thread
				//
				nResult = -2;
				break;
			}

			//
			// Call original thread main implementation
			//
			nResult = OnRun();
		}

		OnExit( nResult );
		SetThreadState( TS_Terminating, false );

		return nResult;
	}

private:
	Handle									m_hThread;						//!< Thread handle
	size_t									m_nThreadID;					//!< Thread ID
	ThreadState								m_ThreadState;					//!< Thread current state
	ThreadState								m_ThreadNewState;				//!< Thread new state
	CriticalSection							m_Lock;							//!< Lock for thread state synchronization
};

#ifdef USE_PTHREAD_THREAD_FORCE
//!
//!	@brief	Thread implementation based on pthread
//!
class ThreadImplementPthread
{
public:
	static void * MainThread( void * pParam )
	{
		//
		// Block signals (we block it from main process)
		//
		sigset_t signal_mask;
		sigemptyset( &signal_mask );
		sigfillset( &signal_mask );
		pthread_sigmask( SIG_BLOCK, &signal_mask, NULL );

		//
		// Regular thread main
		//
		ThreadMainCall * pThreadWrap = (ThreadMainCall *) pParam;
		size_t nResult = pThreadWrap->mainThread();
		pthread_exit( NULL );
		return (void *) nResult;
	}

	static void FinalThread( pthread_t thrID )
	{
	}

	static bool createThread( pthread_t & thrID, ThreadMainCall * thrImpl, size_t & nID, ThreadPriority Priority )
	{
		if( thrImpl == NULL )
			return false;

		pthread_attr_t tattr;
		int ret = pthread_attr_init( &tattr );

		{
			//
			// Set priority
			//

			struct sched_param sp;
			int policy;

			memset( &sp, 0, sizeof(sp) );
			sp.sched_priority = Priority;

			pthread_attr_setschedparam( &tattr, &sp );
		}

		ret = pthread_create( &thrID, &tattr, MainThread, thrImpl );

		pthread_attr_destroy( &tattr );

		if( ret != 0 )
			return false;
		nID = thrID;
		return true;
	}

	static bool endThread( pthread_t thrID )
	{
		return pthread_cancel( thrID ) == 0;
	}

	static bool isAlive( pthread_t thrID )
	{
		if( pthread_kill( thrID, 0 ) == 0 )
			return true;
		return false;
	}

	static void Join( pthread_t thrID )
	{
		pthread_join( thrID, NULL );
	}

	static void SetThreadName( pthread_t thrID, const std::string & sName )
	{
#ifdef THREAD_DEBUG
		pthread_setname_np( thrID, sName.c_str() );
#endif
	}
};

typedef ThreadMainImplement<ThreadImplementPthread, pthread_t> ThreadPthread;
typedef ThreadPthread CrossThread;

#else
//!
//!	@brief	Thread implementation based of Windows API
//!
class ThreadImplementWin32
{
public:
	static DWORD WINAPI MainThread( LPVOID lpThreadParameter )
	{
		ThreadMainCall * pThreadWrap = (ThreadMainCall *) lpThreadParameter;
		return pThreadWrap->mainThread();
	}

	static bool createThread( HANDLE & thrID, ThreadMainCall * thrImpl, size_t & nID, ThreadPriority Priority )
	{
		if( thrImpl == NULL )
			return false;
		DWORD m_nThreadID;
		thrID = ::CreateThread( NULL, 0, MainThread, thrImpl, 0, &m_nThreadID );

		if( thrID == NULL )
			return false;

		nID = m_nThreadID;

		switch( Priority )
		{
		case TP_Low:
			SetThreadPriority( thrID, THREAD_PRIORITY_IDLE );
			break;

		case TP_Normal:
		default:
			SetThreadPriority( thrID, THREAD_PRIORITY_NORMAL );
			break;

		case TP_High:
			SetThreadPriority( thrID, THREAD_PRIORITY_TIME_CRITICAL );
			break;
		}

		return true;
	}

	static void FinalThread( HANDLE thrID )
	{
		CloseHandle( thrID );
	}

	static bool endThread( HANDLE thrID )
	{
		return !!::TerminateThread( thrID, 0 );
	}

	static bool isAlive( HANDLE thrID )
	{
		if( ::WaitForSingleObject( thrID, 0 ) == WAIT_OBJECT_0 )
			return false;
		return true;
	}

	static void Join( HANDLE thrID )
	{
		::WaitForSingleObject( thrID, INFINITE );
	}

	static void SetThreadName( size_t dwThreadID, const std::string & sName )
	{
#ifdef THREAD_DEBUG
		WindowsSpec::SetDbgThreadName( (DWORD) dwThreadID, sName.c_str() );
#endif
	}
};

typedef ThreadMainImplement<ThreadImplementWin32, HANDLE> ThreadWin32;

//!
//!	@brief	Thread implementation based on MS CRT library
//!
class ThreadImplementCRT
{
public:
	static unsigned int __stdcall MainThread( void * pThreadParam )
	{
		ThreadMainCall * pThreadWrap = (ThreadMainCall *) pThreadParam;
		int nResult = pThreadWrap->mainThread();
		_endthreadex( nResult );
		return nResult;
	}

	static void FinalThread( HANDLE thrID )
	{
		CloseHandle( thrID );
	}

	static bool createThread( HANDLE & thrID, ThreadMainCall * thrImpl, size_t & nID, ThreadPriority Priority )
	{
		if( thrImpl == NULL )
			return false;

		DWORD m_nThreadID;
		thrID = (HANDLE) _beginthreadex( NULL, 0, MainThread, thrImpl, 0, (unsigned int *)&m_nThreadID );

		if( thrID == INVALID_HANDLE_VALUE )
		{
			thrID = NULL;
			return false;
		}

		nID = m_nThreadID;

		switch( Priority )
		{
		case TP_Low:
			SetThreadPriority( thrID, THREAD_PRIORITY_IDLE );
			break;

		case TP_Normal:
		default:
			SetThreadPriority( thrID, THREAD_PRIORITY_NORMAL );
			break;

		case TP_High:
			SetThreadPriority( thrID, THREAD_PRIORITY_TIME_CRITICAL );
			break;
		}

		return true;
	}

	static bool endThread( HANDLE thrID )
	{
		return !!::TerminateThread( thrID, 0 );
	}

	static bool isAlive( HANDLE thrID )
	{
		if( ::WaitForSingleObject( thrID, 0 ) == WAIT_OBJECT_0 )
			return false;
		return true;
	}

	static void Join( HANDLE thrID )
	{
		::WaitForSingleObject( thrID, INFINITE );
	}

	static void SetThreadName( size_t dwThreadID, const std::string & sName )
	{
#ifdef THREAD_DEBUG
		WindowsSpec::SetDbgThreadName( (DWORD) dwThreadID, sName.c_str() );
#endif
	}
};

typedef ThreadMainImplement<ThreadImplementCRT, HANDLE> ThreadCRT;

typedef ThreadCRT CrossThread;
#endif

//!
//!	@brief	Thread class for call external function in any classes
//!
template<typename _NeighborClass>
class CrossThreadNeighbor : public CrossThread
{
public:
	//!
	//!	@brief	Type definition of class function
	//!
	typedef int (_NeighborClass::* NeighborCallBack)();

	//!
	//!	@brief	Default constructor
	//!	@param	Priority Base priority
	//!	@throw	std::exception with error description
	//!
	CrossThreadNeighbor( ThreadPriority Priority = TP_Normal ):CrossThread(Priority), m_Neighbor(NULL), m_Function(NULL) {}

	//!
	//!	@brief	Sets new function
	//!	@param	Neighbor Class object
	//!	@param	Function Method of class
	//!	@return	True/false
	//!
	inline bool SetData( _NeighborClass * Neighbor, NeighborCallBack Function )
	{
		if( GetThreadState() != CrossThread::TS_Stop )
			return false;

		if( Neighbor && Function == NULL )
			return false;

		m_Neighbor = Neighbor;
		m_Function = Function;
		return true;
	}

	virtual int OnRun()
	{
		if( m_Neighbor == NULL )
			return -1;

		//
		// Call owner's function
		//
		return (m_Neighbor->*m_Function)();
	}

private:
	_NeighborClass *						m_Neighbor;						//!< Neighbor class object
	NeighborCallBack						m_Function;						//!< Neighbor method
};
