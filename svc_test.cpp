#include "BaseService.h"
#include "SvcSingleManager.h"
#include "cpl/LConv.h"
#include "cpl/CrossUtils.h"

#include "cpl/Containers/SafeUnboundedQueue.h"
#include "cpl/Containers/SafeBoundedQueue.h"
#include "cpl/TimeTriggers.h"

#include "cpl/CrossThread.h"

class MySvc
{
public:
	MySvc():Timer(0.05)
	{
		nMax = 0;
		m_nCount = 0;
		tProducer.SetData( this, &MySvc::thrQueueProducer );
		tConsumer.SetData( this, &MySvc::thrQueueConsumer );

		Timer.SetInterval( 5 ); // sec
	}

	virtual ~MySvc()
	{
	}

	int32_t thrQueueProducer()
	{
		//
		// Produce
		//
		srand(GetTickCount());

		const int nCount = 110;//rand() % 10;
		for( int i = 0 ; i < nCount ; i++ )
			m_Queue.Push(rand());

		sys::Sleep( 0 );
		return 0;
	}

	int32_t thrQueueConsumer()
	{
		//
		// Consume
		//

		const int nCount = 100;
		for( int i = 0 ; i < nCount + 1 ; i++ )
		{
			int nRes = 0;
			if( !m_Queue.Pop( nRes ) )
				break;

			{
				CSLocker aa(m_Lock);
				nMax = max( nMax, nRes );
				m_nCount++;
			}
		}

		return 0;
	}

	bool OnConfiguration()
	{
		size_t nMax = 100; // m_Queue.GetMaxSize()

		for( size_t i = 0 ; i < nMax ; i++ )
		{
			if( !m_Queue.Push(i) )
				return false;
		}

		printf( "%u", m_Queue.GetCount() );

		m_Queue.Pop();
		m_Queue.Pop();

		m_Queue.Clear();
#if 0
		m_Queue.SetMaxSize( m_Queue.GetMaxSize() -3 );
#endif

		return true;
	}

	bool OnRun()
	{
		if( !Timer.IsFired() )
			return true;

		{
			CSLocker aa(m_Lock);
			printf( "%I64u - %I64u items/sec - %u\n", nMax, m_nCount / Timer.GetInterval(), m_Queue.GetCount() );

#if 0
			m_Queue.SetMaxSize( m_Queue.GetMaxSize() + 10 );
#endif

			m_nCount = 0;
		}

		return true;
	}

	bool OnStart()
	{
		tProducer.Run();
		tConsumer.Run();
		return true;
	}

	bool OnStop()
	{
		tProducer.Stop( true );
		tConsumer.Stop( true );
		return true;
	}

private:
/*
	class MyThread : public CrossThread
	{
	public:
		virtual int32_t OnRun()
		{
			printf( "test \n" );
			util::Sleep( 2 );
			return 0;
		}
	};

	MyThread mm;
	*/
	CrossThreadNeighbor<MySvc> tProducer;
	CrossThreadNeighbor<MySvc> tConsumer;

	CriticalSection m_Lock;
	volatile uint64_t nMax;
	volatile uint64_t m_nCount;
	TimeTrigger Timer;

#if 1
	SafeUnboundedQueue<int> m_Queue;
#else
	SafeBoundedQueue<int> m_Queue;
#endif
};

bool g_RegisteredService = SvcSingleManager::GetInstance().RegisterService( new BaseServiceCreater<MySvc>( "Test service", "TestSvc", BaseService::Version( 0, 1, 10, "Debug" ), "Test service only" ) );
