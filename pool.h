// 
#include <queue>

class TPOOL
{

private:

	bool Ending = false;

	class TTHREAD
	{
	public:

			TPOOL* p = 0;
			HANDLE hE2 = 0;
			HANDLE hM = 0;
			std::function<void()> f;
			void thr()
			{
				for (;;)
				{
					HANDLE hh[2] = { hM,hE2 };
					auto wi = WaitForMultipleObjects(2,hh,false,INFINITE);
					if (wi == WAIT_OBJECT_0 + 1)
						break;
					if (f)
						f();
					f = nullptr;
					ReleaseMutex(hM);
				}

			}

			void start(TPOOL* pp)
			{
				p = pp;
				hM = CreateMutex(0, TRUE, 0);
				hE2 = CreateEvent(0, 0, 0, 0);
				std::thread t(&TTHREAD::thr, this);
				t.detach();
			}



	};

	std::vector<HANDLE> mutexes;
	std::vector<std::shared_ptr<TTHREAD>> threads;
	std::queue<std::function<void()>> q;
public:

	TPOOL()
	{

	}

	void End()
	{
		Join();
		Ending = true;
		for(auto& t : threads)
			SetEvent(t->hE2);
		Join();
	}

	void Init(size_t mt = 1)
	{
		threads.resize(mt);
		for (auto& t : threads)
		{
			t = std::make_shared<TTHREAD>();
			t->start(this);
			mutexes.push_back(t->hM);
		}
	}

	HRESULT Add(std::function<void()> f)
	{
		// Instant run
		DWORD e = WaitForMultipleObjects(mutexes.size(), mutexes.data(), FALSE, 0);
		if (e == WAIT_FAILED)
			return E_FAIL;
		if (e != WAIT_TIMEOUT)
		{
			size_t idx = e - WAIT_OBJECT_0;
			threads[idx]->f = f;
			ReleaseMutex(threads[idx]->hM);
			ReleaseMutex(threads[idx]->hM);
			return S_OK;
		}
		q.push(f);
		return S_FALSE;
	}

	void Join()
	{
		WaitForMultipleObjects(mutexes.size(), mutexes.data(), TRUE, INFINITE);
	}
};