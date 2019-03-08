// RWMUTEX 
class RWMUTEX
{
private:
	HANDLE hChangeMap = 0;
	std::map<DWORD, HANDLE> Threads;
	DWORD wi = INFINITE;
	RWMUTEX(const RWMUTEX&) = delete;
	RWMUTEX(RWMUTEX&&) = delete;



public:

	RWMUTEX(bool D = false, const wchar_t* mutname = 0)
	{
		if (D)
			wi = 10000;
		else
			wi = INFINITE;
		hChangeMap = CreateMutex(0, 0, mutname);
		if (!hChangeMap)
			hChangeMap = OpenMutex(SYNCHRONIZE, false, mutname);
	}

	~RWMUTEX()
	{
		CloseHandle(hChangeMap);
		hChangeMap = 0;
		for (auto& a : Threads)
			CloseHandle(a.second);
		Threads.clear();
	}

	HANDLE CreateIf(bool KeepReaderLocked = false, const wchar_t* mutname = 0)
	{
		auto tim = WaitForSingleObject(hChangeMap, INFINITE);
		if (tim == WAIT_TIMEOUT && wi != INFINITE)
			OutputDebugString(L"LockRead debug timeout!");
		DWORD id = GetCurrentThreadId();
		if (Threads[id] == 0)
		{
			HANDLE e0 = CreateMutex(0, 0, mutname);
			if (!e0)
				e0 = OpenMutex(SYNCHRONIZE, false, mutname);
			Threads[id] = e0;
		}
		HANDLE e = Threads[id];
		if (!KeepReaderLocked)
			ReleaseMutex(hChangeMap);
		return e;
	}

	HANDLE LockRead()
	{
		auto z = CreateIf();
		auto tim = WaitForSingleObject(z, wi);
		if (tim == WAIT_TIMEOUT && wi != INFINITE)
			OutputDebugString(L"LockRead debug timeout!");
		return z;
	}

	vector<HANDLE> AllThreads;
	void LockWrite(DWORD updThread = 0)
	{
		CreateIf(true);

		// Wait for all 
		AllThreads.reserve(Threads.size());
		AllThreads.clear();
		for (auto& a : Threads)
		{
			if (updThread == a.first) // except ourself if in upgrade operation
				continue;
			AllThreads.push_back(a.second);
		}
		auto tim = WaitForMultipleObjects((DWORD)AllThreads.size(), AllThreads.data(), TRUE, wi);

		if (tim == WAIT_TIMEOUT && wi != INFINITE)
			OutputDebugString(L"LockWrite debug timeout!");

		// We don't want to keep threads, the hChangeMap is enough
		// We also release the handle to the upgraded thread, if any
		for (auto& a : Threads)
			ReleaseMutex(a.second);

		// Reader is locked
	}

	void ReleaseWrite()
	{
		ReleaseMutex(hChangeMap);
	}

	void ReleaseRead(HANDLE f)
	{
		ReleaseMutex(f);
	}

	void Upgrade()
	{
		LockWrite(GetCurrentThreadId());
	}

	HANDLE Downgrade()
	{
		DWORD id = GetCurrentThreadId();
		auto z = Threads[id];
		auto tim = WaitForSingleObject(z, wi);
		if (tim == WAIT_TIMEOUT && wi != INFINITE)
			OutputDebugString(L"Downgrade debug timeout!");
		ReleaseMutex(hChangeMap);
		return z;
	}
};

/*
// RWMUTEX2
class RWMUTEX2
{
private:


	RWMUTEX2(const RWMUTEX2&) = delete;
	RWMUTEX2(RWMUTEX2&&) = delete;
	SRWLOCK m;
	thread_local static int st;

public:

	RWMUTEX2()
	{
		InitializeSRWLock(&m);
	}

	~RWMUTEX2()
	{
	}


	bool LockRead(bool Try = false)
	{
		if (st != 0)
			return true;
		if (Try)
		{
			bool r = TryAcquireSRWLockShared(&m);
			if (r)
				st = 1;
			return r;
		}
		AcquireSRWLockShared(&m);
		st = 1;
		return true;
	}

	bool LockWrite(bool Try = false)
	{
		if (st == 2)
			return true;
		if (st == 1)
			ReleaseRead();
		if (st != 0)
			return false;

		if (Try)
		{
			bool r = TryAcquireSRWLockExclusive(&m);
			if (r)
				st = 2;
			return r;
		}
		AcquireSRWLockExclusive(&m);
		st = 2;
		return true;
	}

	bool ReleaseWrite()
	{
		if (st == 0)
			return true;
		if (st == 1)
			return false; 
		ReleaseSRWLockExclusive(&m);
		st = 0;
		return true;
	}

	bool ReleaseRead(HANDLE = nullptr)
	{
		if (st == 0)
			return true;
		if (st == 2)
			return false;
		ReleaseSRWLockShared(&m);
		st = 0;
		return true;

	}

	void Upgrade()
	{
		ReleaseRead();
		LockWrite();
	}

	HANDLE Downgrade()
	{
		ReleaseWrite();
		LockRead();
		return nullptr;
	}

	int State()
	{
		return st;
	}

	void Revert(int ns)
	{
		if (ns == st)
			return;

		ReleaseWrite();
		ReleaseRead();
		if (ns == 2)
			LockWrite();
		if (ns == 1)
			LockRead();
	}
};
inline thread_local int RWMUTEX2::st = 0;
*/
class RWMUTEXLOCKREAD
{
private:
	RWMUTEX* mm = 0;
	HANDLE lm = 0;
    //RWMUTEX2* mm2;
	//int mm2st = 0;

public:

	RWMUTEXLOCKREAD(const RWMUTEXLOCKREAD&) = delete;
	void operator =(const RWMUTEXLOCKREAD&) = delete;

	RWMUTEXLOCKREAD(RWMUTEX*m)
	{
		if (m)
		{
			mm = m;
			lm = mm->LockRead();
		}
	}

	/*
	RWMUTEXLOCKREAD(RWMUTEX2*m)
	{
		if (m)
		{
			mm2 = m;
			mm2st = m->State();
			mm2->LockRead();
		}
	}
	*/
	~RWMUTEXLOCKREAD()
	{
		if (mm)
		{
			mm->ReleaseRead(lm);
			lm = 0;
			mm = 0;
		}
/*		if (mm2)
		{
			mm2->Revert(mm2st);
			mm2 = 0;
		}
*/
	}
};


class RWMUTEXLOCKWRITE
{
private:
	RWMUTEX* mm = 0;
//	RWMUTEX2* mm2;
	//int mm2st = 0;

public:
	RWMUTEXLOCKWRITE(RWMUTEX*m)
	{
		if (m)
		{
			mm = m;
			mm->LockWrite();
		}
	}
/*
	RWMUTEXLOCKWRITE(RWMUTEX2*m)
	{
		if (m)
		{
			mm2 = m;
			mm2st = m->State();
			mm2->LockWrite();
		}
	}
	*/

	~RWMUTEXLOCKWRITE()
	{
		if (mm)
		{
			mm->ReleaseWrite();
			mm = 0;
		}
		/*if (mm2)
		{
			mm2->Revert(mm2st);
			mm2 = 0;
		}
		*/
	}
};

class RWMUTEXLOCKREADWRITE
{
private:
	RWMUTEX* mm = 0;
	HANDLE lm = 0;
	bool U = false;
public:

	RWMUTEXLOCKREADWRITE(const RWMUTEXLOCKREADWRITE&) = delete;
	void operator =(const RWMUTEXLOCKREADWRITE&) = delete;

	RWMUTEXLOCKREADWRITE(RWMUTEX*m)
	{
		if (m)
		{
			mm = m;
			lm = mm->LockRead();
		}
	}

	void Upgrade()
	{
		if (mm && !U)
		{
			mm->Upgrade();
			lm = 0;
			U = 1;
		}
	}

	void Downgrade()
	{
		if (mm && U)
		{
			lm = mm->Downgrade();
			U = 0;
		}
	}

	~RWMUTEXLOCKREADWRITE()
	{
		if (mm)
		{
			if (U)
				mm->ReleaseWrite();
			else
				mm->ReleaseRead(lm);
			lm = 0;
			mm = 0;
		}
	}
};

template <typename T,typename M = RWMUTEX> class tlock
{
private:
	mutable T t;
	mutable M m;

	class proxy
	{
		T *const p;
		M* m;
		HANDLE lm = 0;
		int me;
	public:
		proxy(T * const _p, M* _m, int _me) : p(_p), m(_m), me(_me) 
		{ 
			if (me == 2) 
				m->LockWrite(); 
			else lm = (HANDLE)m->LockRead(); 
		}
		~proxy() 
		{
			if (me == 2) 
				m->ReleaseWrite(); 
			else 
			{ 
				m->ReleaseRead(lm); 
				lm = 0; 
			} 
		}
		T* operator -> () { return p; }
		const T* operator -> () const { return p; }
		T* getp() { return p; }
		const T* getpc() const { return p; }
		void upgrade() 
		{
			if (me == 1)
			{
				lm = 0;
				m->Upgrade();
				me = 2;
			}
		}

		void downgrade()
		{
			if (me == 2)
			{
				lm = m->Downgrade();
				me = 1;
			}
		}

	};

public:
	template< typename ...Args>
	tlock(Args ... args) : t(args...) {}
	const proxy r() const
	{
		return proxy(&t, &m, 1);
	}
	proxy w()
	{
		return proxy(&t, &m, 2);
	}

	M& mut() { return m; }
	T& direct()
	{
		return t;
	}

	const T& direct() const
	{
		return t;
	}


	void readlock(std::function<void(const T&)> f) const
	{
		proxy mx(&t, &m, 1);
		f(*mx.getp());
	}
	void writelock(std::function<void(T&)> f)
	{
		proxy mx(&t, &m, 2);
		f(*mx.getp());
	}

	void rwlock(std::function<void(const T&,std::function<void(std::function<void(T&)>)>)> f)
	{
		proxy mx(&t, &m, 1);
		auto upfunc = [&](std::function<void(T&)> f2)
		{
			mx.upgrade();
			f2(*mx.getp());
			mx.downgrade();
		};
		f(*mx.getp(), upfunc);
	}

	proxy operator -> () { return w(); }
	const proxy operator -> () const { return r(); }

};


class reverse_semaphore
{
private:
	HANDLE hE = 0;
	HANDLE hM = 0;
	volatile unsigned long long m = 0;

	reverse_semaphore(const reverse_semaphore&) = delete;
	reverse_semaphore& operator =(const reverse_semaphore&) = delete;

public:

	reverse_semaphore()
	{
		m = 0;
		hE = CreateEvent(0, TRUE, TRUE, 0);
		hM = CreateMutex(0, 0, 0);
	}

	~reverse_semaphore()
	{
		CloseHandle(hM);
		hM = 0;
		CloseHandle(hE);
		hE = 0;
	}

	void lock()
	{
		WaitForSingleObject(hM, INFINITE);
		m++;
		ResetEvent(hE);
		ReleaseMutex(hM);
	}

	void unlock()
	{
		WaitForSingleObject(hM, INFINITE);
		if (m > 0)
			m--;
		if (m == 0)
			SetEvent(hE);
		ReleaseMutex(hM);
	}

	DWORD Wait(DWORD dw = INFINITE)
	{
		return WaitForSingleObject(hE, dw);
	}

	void WaitAndLock()
	{
		HANDLE h[2] = { hE,hM };
		WaitForMultipleObjects(2, h, TRUE, INFINITE);
		lock();
		ReleaseMutex(hM);
	}
	HANDLE WaitAndBlock()
	{
		HANDLE h[2] = { hE,hM };
		WaitForMultipleObjects(2, h, TRUE, INFINITE);
		return hM;
	}


};