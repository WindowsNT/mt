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


class RWMUTEXLOCKREAD
{
private:
	RWMUTEX* mm = 0;
	HANDLE lm = 0;
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

template <typename T> class tlock
{
private:
	mutable T t;
	mutable RWMUTEX m;

	class proxy
	{
		T *const p;
		RWMUTEX* m;
		HANDLE lm = 0;
		int me;
	public:
		proxy(T * const _p, RWMUTEX* _m, int _me) : p(_p), m(_m), me(_me) 
		{ 
			if (me == 2) 
				m->LockWrite(); 
			else lm = m->LockRead(); 
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


