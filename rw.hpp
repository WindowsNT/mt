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

	void ClearThreads()
	{
		for (auto& a : Threads)
			CloseHandle(a.second);
		Threads.clear();
	}

	~RWMUTEX()
	{
		CloseHandle(hChangeMap);
		hChangeMap = 0;
		ClearThreads();
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

	std::vector<HANDLE> AllThreads;
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

	~RWMUTEXLOCKREAD()
	{
		if (mm)
		{
			mm->ReleaseRead(lm);
			lm = 0;
			mm = 0;
		}
	}
};


class RWMUTEXLOCKWRITE
{
private:
	RWMUTEX* mm = 0;
public:
	RWMUTEXLOCKWRITE(RWMUTEX*m)
	{
		if (m)
		{
			mm = m;
			mm->LockWrite();
		}
	}

	~RWMUTEXLOCKWRITE()
	{
		if (mm)
		{
			mm->ReleaseWrite();
			mm = 0;
		}
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
#include <shared_mutex>
#include <functional>


template <typename T> class tlock2
{
private:
	mutable T t;
	mutable std::shared_mutex m;

	class proxy
	{
		T* const p;
		std::shared_mutex* m;
		int me;
	public:
		proxy(T* const _p, std::shared_mutex* _m, int _me) : p(_p), m(_m), me(_me)
		{
			if (me == 2)
				m->lock();
			else
				m->lock_shared();
		}
		~proxy()
		{
			if (me == 2)
				m->unlock();
			else
				m->unlock_shared();
		}
		T* operator -> () { return p; }
		const T* operator -> () const { return p; }
		T* getp() { return p; }
		const T* getpc() const { return p; }

	};

public:
	template< typename ...Args>
	tlock2(Args ... args) : t(args...) {}
	const proxy r() const
	{
		return proxy(&t, &m, 1);
	}
	proxy w()
	{
		return proxy(&t, &m, 2);
	}

	std::shared_mutex& mut() { return m; }
	T& direct()
	{
		return t;
	}

	const T& direct() const
	{
		return t;
	}


	void readlock(::std::function<void(const T&)> f) const
	{
		proxy mx(&t, &m, 1);
		f(*mx.getp());
	}
	void writelock(::std::function<void(T&)> f)
	{
		proxy mx(&t, &m, 2);
		f(*mx.getp());
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


template <typename T> class cow
{
private:
	mutable ::std::shared_ptr<T> t;

public:

	template< typename ...Args>
	cow(Args ... args) : { t = ::std::make_shared<T>(args...);  }

	cow(::std::shared_ptr<T> t2)
	{
		t = t2;
	}

	operator const ::std::shared_ptr<T>() const { return t; }
	const T operator *()
	{
		return *t;
	}

	// Write
	shared_ptr<T> operator ->()
	{
		if (t.use_count() == 1)
			return t;
		::std::shared_ptr<T> t2 = ::std::make_shared<T>(*t);
		t = t2;
		return t;
	}

	void write(::std::function<void(::std::shared_ptr<T>)> f)
	{
		f(operator ->());
	}

};

