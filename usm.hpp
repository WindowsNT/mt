#ifndef _USM_H
#define _USM_H


namespace USMLIBRARY
{
	using namespace std;

#ifndef _NO_MUTUAL

	class mutual
	{
	private:


		wstring ev1;
		wstring str;
		HANDLE h1 = 0;
		HANDLE h2 = 0;
		std::function<void(unsigned long long lp)> lpf;
		unsigned long long lpa = 0;

		mutual(const mutual&) = delete;
		void operator =(const mutual&) = delete;

		HANDLE CreateEventX(int i)
		{
			TCHAR n[1000] = { 0 };
			swprintf_s(n, L"%s{401F3B1E-6090-4DF2-95C0-F11C10F9F285}_%u", str.c_str(), i);
			HANDLE hX = CreateEvent(0, 0, 0, n);
			return hX;
		}

		void WaitForRequestNoLoop()
		{
			WaitForSingleObject(h1, INFINITE);
			if (lpf)
				lpf(lpa);
			SetEvent(h2);
		}

		void WaitForRequestLoop()
		{
			for (;;)
			{
				if (WaitForSingleObject(h1, INFINITE) != WAIT_OBJECT_0)
					break;
				if (lpf)
					lpf(lpa);
				SetEvent(h2);
			}
		}



	public:

		mutual(const wchar_t* strn, std::function<void(unsigned long long lp)> req, unsigned long long lp, bool LoopRequest)
		{
			str = strn;
			lpf = req;
			lpa = lp;
			h1 = CreateEventX(1);
			h2 = CreateEventX(2);
			if (req)
			{
				if (LoopRequest)
				{
					std::thread t(&mutual::WaitForRequestLoop, this);
					t.detach();
				}
				else
				{
					std::thread t(&mutual::WaitForRequestNoLoop, this);
					t.detach();
				}
			}
		}

		void request(DWORD Wait = INFINITE)
		{
			ResetEvent(h2);
			SetEvent(h1);
			if (!Wait)
				return;
			WaitForSingleObject(h2, Wait);
		}

		~mutual()
		{
			CloseHandle(h2);
			h2 = 0;
			CloseHandle(h1);
			h1 = 0;
		}
	};
#endif 

	template <typename T = char>
	class usm
	{
	private:
		struct USMHEADER
		{
		};

		struct USMTHREAD
		{
			DWORD id;
			int evidx;
		};

		// Strings of handle ids
		wstring cwmn;
		wstring fmn;
		wstring evrn;
		wstring evrn2;
		wstring evwn;

		wstring stringid;

		bool WasFirst = false;

		// Auto reset event that is set when reading thread finishes reading
		HANDLE hEventRead = 0;

		// Auto reset event that is set when writing thread finishes writing
		HANDLE hEventWrote = 0;

		// Locked when this thread is writing
		// Or when a thread prepares for initializing or exiting
		HANDLE hMutexWriting = 0;

		// Set when this thread is not reading
		// Unset when this thread is reading
		HANDLE hEventMeReading = 0;


		HANDLE hFM = 0;
		unsigned long long ClientSZ = 0;
		DWORD MaxThreads = 0;
		PVOID  Buff = 0;
		bool Executable = false;

		SECURITY_ATTRIBUTES sattr;
		SECURITY_DESCRIPTOR SD;



		void FillSA()
		{
			sattr.nLength = sizeof(sattr);
			BOOL fx = InitializeSecurityDescriptor(&SD, SECURITY_DESCRIPTOR_REVISION);
			fx = SetSecurityDescriptorDacl(&SD, TRUE, NULL, FALSE);
			sattr.bInheritHandle = true;
			sattr.lpSecurityDescriptor = &SD;
		}


		HANDLE CreateEvR(int idx)
		{
			TCHAR n[1000] = { 0 };
			swprintf_s(n, L"%s%i", evrn.c_str(), idx);
			HANDLE hX = CreateEvent(&sattr, TRUE, TRUE, n);
			return hX;
		}

		HANDLE CreateEvR2(int idx)
		{
			TCHAR n[1000] = { 0 };
			swprintf_s(n, L"%s%i", evrn2.c_str(), idx);
			HANDLE hX = CreateEvent(&sattr, 0, 0, n);
			return hX;
		}

		HANDLE CreateEvW()
		{
			TCHAR n[1000] = { 0 };
			swprintf_s(n, L"%s", evwn.c_str());
			HANDLE hX = CreateEvent(&sattr, 0, 0, n);
			return hX;
		}

		HANDLE CreateCWM()
		{
			HANDLE hX = OpenMutex(MUTEX_MODIFY_STATE | SYNCHRONIZE, false, cwmn.c_str());
			if (hX != 0)
				return hX;
			hX = CreateMutex(&sattr, 0, cwmn.c_str());
			return hX;
		}

		HANDLE CreateFM()
		{
			// Try to open the map , or else create it
			WasFirst = true;
			HANDLE hX = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE | (Executable ? FILE_MAP_EXECUTE : 0), false, fmn.c_str());
			if (hX != 0)
			{
				WasFirst = false;
				return hX;
			}

			unsigned long long  FinalSize = ClientSZ * sizeof(T) + MaxThreads * sizeof(USMTHREAD) + sizeof(USMHEADER);
			ULARGE_INTEGER ulx = { 0 };
			ulx.QuadPart = FinalSize;

			hX = CreateFileMapping(INVALID_HANDLE_VALUE, &sattr, (Executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE), ulx.HighPart, ulx.LowPart, fmn.c_str());
			if (hX != 0)
			{
				LPVOID Buff4 = MapViewOfFile(hX, FILE_MAP_READ | FILE_MAP_WRITE | (Executable ? FILE_MAP_EXECUTE : 0), 0, 0, 0);
				if (Buff4)
				{
					memset(Buff4, 0, (size_t)FinalSize);
					UnmapViewOfFile(Buff4);
				}
			}
			return hX;
		}


	public:


		HANDLE fmh()
		{
			return hX;
		}

		HANDLE GetFM() { return hFM; }
		wstring GetFMN() { return fmn; }
		int GetMaxThreads() { return MaxThreads; }

		void End()
		{
			// Remove the ID from the thread
			if (Buff)
			{
				USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));
				WaitForSingleObject(hMutexWriting, INFINITE);
				// Find 
				for (unsigned int y = 0; y < MaxThreads; y++)
				{
					USMTHREAD& tt = th[y];
					DWORD myid = GetCurrentThreadId();
					if (tt.id == myid)
					{
						tt.id = 0;
						tt.evidx = 0;
						break;
					}
				}
				ReleaseMutex(hMutexWriting);
			}


			if (hEventRead)
				CloseHandle(hEventRead);
			hEventRead = 0;
			if (hEventWrote)
				CloseHandle(hEventWrote);
			hEventWrote = 0;
			if (hFM)
				CloseHandle(hFM);
			hFM = 0;
			if (hEventMeReading)
				CloseHandle(hEventMeReading);
			hEventMeReading = 0;
			if (hMutexWriting)
				CloseHandle(hMutexWriting);
			hMutexWriting = 0;
		}


		bool IsFirst() { return WasFirst; }
		usm(const wchar_t* string_id = 0, bool Init = false, unsigned long long csz = 1048576, DWORD MaxTh = 100)
		{
			if (!string_id)
				return;
			CreateInit(string_id, Init, csz, MaxTh);
		}

		void operator =(const usm &x)
		{
			// Terminate current
			End();

			// Recreate
			CreateInit(x.stringid.c_str(), true, x.ClientSZ, x.MaxThreads);
		}

		usm(const usm& x)
		{
			operator=(x);
		}


		void CreateInit(const wchar_t* string_id, bool Init = false, unsigned long long csz = 1048576, DWORD MaxTh = 100, bool LocalOnly = true, bool Exex = false)
		{
			Executable = Exex;
			if (!string_id)
				return;
			if (wcslen(string_id) == 0)
				return;

			TCHAR xup[1000] = { 0 };
			stringid = string_id;

			FillSA();
			if (LocalOnly)
			{
				swprintf_s(xup, 1000, L"%s_cwmn", stringid.c_str());
				cwmn = xup;
				swprintf_s(xup, 1000, L"%s_evrn", stringid.c_str());
				evrn = xup;
				swprintf_s(xup, 1000, L"%s_evrn2", stringid.c_str());
				evrn2 = xup;
				swprintf_s(xup, 1000, L"%s_evwn", stringid.c_str());
				evwn = xup;
				swprintf_s(xup, 1000, L"%s_fmn", stringid.c_str());
				fmn = xup;
			}
			else
			{
				swprintf_s(xup, 1000, L"Global\\%s_cwmn", stringid.c_str());
				cwmn = xup;
				swprintf_s(xup, 1000, L"Global\\%s_evrn", stringid.c_str());
				evrn = xup;
				swprintf_s(xup, 1000, L"Global\\%s_evrn2", stringid.c_str());
				evrn2 = xup;
				swprintf_s(xup, 1000, L"Global\\%s_evwn", stringid.c_str());
				evwn = xup;
				swprintf_s(xup, 1000, L"Global\\%s_fmn", stringid.c_str());
				fmn = xup;
			}

			if (!csz)
				csz = 1048576;
			ClientSZ = csz;
			if (!MaxTh)
				MaxTh = 100;
			MaxThreads = MaxTh;
			if (Init)
			{
				int iv = Initialize();
				if (iv <= 0)
				{
					End();
					throw iv;
				}
			}
		}

		~usm()
		{
			End();
		}

		int Initialize()
		{
			hEventRead = 0;
			hEventWrote = 0;
			hMutexWriting = 0;
			hFM = 0;
			Buff = 0;
			hEventMeReading = 0;

			if (hMutexWriting == 0)
				hMutexWriting = CreateCWM();
			if (hMutexWriting == 0)
				return -1;
			if (hFM == 0)
				hFM = CreateFM();
			if (hFM == 0)
				return -1;
			if (hEventWrote == 0)
				hEventWrote = CreateEvW();
			if (hEventWrote == 0)
				return -1;
			if (Buff == 0)
				Buff = MapViewOfFile(hFM, FILE_MAP_READ | FILE_MAP_WRITE | (Executable ? FILE_MAP_EXECUTE : 0), 0, 0, 0);
			if (!Buff)
				return -1;

			// Acquire lock for Count variable
			// USMHEADER* h = (USMHEADER*)Buff;
			USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));
			WaitForSingleObject(hMutexWriting, INFINITE);
			// Find 
			for (unsigned int y = 0; y < MaxThreads; y++)
			{
				USMTHREAD& tt = th[y];
				if (tt.id == 0)
				{
					tt.id = GetCurrentThreadId();
					tt.evidx = (y + 1);
					hEventMeReading = CreateEvR(y + 1);
					hEventRead = CreateEvR2(y + 1);
					break;
				}
			}
			ReleaseMutex(hMutexWriting);

			if (!hEventMeReading)
				return -1;

			return 1;
		}

		const T* BeginRead(bool FailOnNotReady = false)
		{
			if (!Buff)
				return 0;

			// Is someone writing 
			if (FailOnNotReady)
			{
				DWORD x = WaitForSingleObject(hMutexWriting, 0);
				if (x != WAIT_OBJECT_0)
					return 0;
			}
			else
				WaitForSingleObject(hMutexWriting, INFINITE);

			// Reset our reading event
			ResetEvent(hEventMeReading);

			// Release the mutex, but now any writing thread that locks it must wait for is
			ReleaseMutex(hMutexWriting);

			// Return the pointer
			const char* a1 = (const char*)Buff;
			a1 += sizeof(USMHEADER);
			a1 += sizeof(USMTHREAD)*MaxThreads;

			return (T*)a1;
		}

		void EndRead()
		{
			SetEvent(hEventMeReading);
			SetEvent(hEventRead);
		}

		unsigned long long ReadData(T* b, size_t sz, size_t offset = 0, bool FailIfNotReady = false)
		{
			const T* ptr = BeginRead(FailIfNotReady);
			if (!ptr)
				return (unsigned long long) - 1;
			memcpy(b, ptr + offset, sz);
			EndRead();
			return sz;
		}


		DWORD NotifyOnRead(bool Wait)
		{
			// See if any thread is reading
			USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));
			vector<HANDLE> evs;

			// Find 
			bool S = true;
			for (unsigned int y = 0; y < MaxThreads; y++)
			{
				USMTHREAD& tt = th[y];
				if (tt.evidx > 0)
				{
					// Open the event
					TCHAR n[1000] = { 0 };
					swprintf_s(n, L"%s%i", evrn.c_str(), tt.evidx);
					HANDLE hEv = OpenEvent(SYNCHRONIZE, 0, n);
					if (hEv == 0) // duh
					{
						S = false;
						break;
					}
					evs.push_back(hEv);
				}
			}
			DWORD fi = 0;
			if (!S)
				return (DWORD)-1;
			if (evs.empty())
				return (DWORD)-2;

			// Wait for any thread to terminate reading
			fi = WaitForMultipleObjects((DWORD)evs.size(), &evs[0], FALSE, Wait ? INFINITE : 0);

			// Cleanup
			for (unsigned int i = 0; i < evs.size(); i++)
				CloseHandle(evs[i]);
			evs.clear();

			return fi;
		}

		T* BeginWrite(bool FailOnNotReady = false)
		{
			// Lock the writing mutex
			if (FailOnNotReady)
			{
				DWORD x = WaitForSingleObject(hMutexWriting, 0);
				if (x != WAIT_OBJECT_0)
					return 0;
			}
			else
				WaitForSingleObject(hMutexWriting, INFINITE);

			// Having locked the writing mutex, no reading thread can start now
			// After that, no new threads can read
			vector<HANDLE> evs;
			evs.reserve(MaxThreads);

			// Wait for threads that are already in read state
			USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));

			// Find 
			bool S = true;
			for (unsigned int y = 0; y < MaxThreads; y++)
			{
				USMTHREAD& tt = th[y];
				if (tt.evidx > 0)
				{
					// Open the event
					TCHAR n[1000] = { 0 };
					swprintf_s(n, L"%s%i", evrn.c_str(), tt.evidx);
					HANDLE hEv = OpenEvent(SYNCHRONIZE, 0, n);
					if (hEv == 0) // duh
					{
						S = false;
						break;
					}
					evs.push_back(hEv);
				}
			}
			DWORD fi = 0;
			if (S)
			{
				// Wait for all these threads to terminate reading
				fi = WaitForMultipleObjects((DWORD)evs.size(), &evs[0], TRUE, FailOnNotReady ? 0 : INFINITE);
				if (fi == -1 || fi == WAIT_TIMEOUT)
					S = false;
			}
			else
			{
				fi = (DWORD)-1;
			}

			// Cleanup
			for (unsigned int i = 0; i < evs.size(); i++)
				CloseHandle(evs[i]);
			evs.clear();
			if (!S)
			{
				ReleaseMutex(hMutexWriting);
				return 0;
			}

			// Return the pointer
			char* a1 = (char*)Buff;
			a1 += sizeof(USMHEADER);
			a1 += sizeof(USMTHREAD)*MaxThreads;

			ResetEvent(hEventWrote);
			return (T*)a1;
		}

		void EndWrite()
		{
			ReleaseMutex(hMutexWriting);
			SetEvent(hEventWrote);
		}

		DWORD NotifyWrite(bool Wait)
		{
			// Wait for all these threads to terminate reading
			return WaitForSingleObject(hEventWrote, Wait ? INFINITE : 0);
		}

		unsigned long long WriteData(const T* b, size_t sz, size_t offset = 0, bool FailIfNotReady = false)
		{
			T* ptr = BeginWrite(FailIfNotReady);
			if (!ptr)
				return (unsigned long long) - 1;
			memcpy(ptr + offset, b, sz);
			EndWrite();
			return sz;
		}

		// Sends data, then waits until all threads have read that data
		unsigned long long SendDataAndWait(const T*b, size_t sz, size_t offset = 0)
		{
			unsigned long long r = WriteData(b, sz, offset);
			if (r != sz)
				return r;

			USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));
			vector<HANDLE> evs;

			// Find 
			bool S = true;
			for (unsigned int y = 0; y < MaxThreads; y++)
			{
				USMTHREAD& tt = th[y];
				if (tt.id == GetCurrentThreadId())
					continue;
				if (tt.evidx > 0)
				{
					// Open the event
					TCHAR n[1000] = { 0 };
					swprintf_s(n, L"%s%i", evrn2.c_str(), tt.evidx);
					HANDLE hEv = OpenEvent(SYNCHRONIZE, 0, n);
					if (hEv == 0) // duh
					{
						S = false;
						break;
					}
					evs.push_back(hEv);
				}
			}
			if (!S)
				return (DWORD)-1;
			if (evs.empty())
				return (DWORD)-2;

			// Wait for all thread to terminate reading
			WaitForMultipleObjects((DWORD)evs.size(), &evs[0], TRUE, INFINITE);
			return r;
		}

	};

}

#endif // USM_H
