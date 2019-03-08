namespace FILESNAPLIB
{
	using namespace std;
	class FILESNAP
	{
	public:
		static const GUID GUID_HEADER;
		struct alignas(8)  FSITEM
		{
			CLSID cid = FILESNAP::GUID_HEADER;
			unsigned long long DiffAt = 0;
			FILETIME created = ti();
			FILETIME updated = ti();
			unsigned long long i = 0;
			unsigned long long at = 0;
			unsigned long long sz = 0;
			unsigned long long extradatasize = 0;
		};

	private:
		DIFFLIB::DIFF d;
		wstring f;
		HANDLE hX = INVALID_HANDLE_VALUE;
		HANDLE hR = INVALID_HANDLE_VALUE;
		wstring tf; // Temp File
		vector<wstring> temps;

		class MM
		{
		public:

			HANDLE hY = 0;
			char *m = 0;
			void* cm = 0;

			MM(HANDLE hYY = 0, void* mm = 0, unsigned long long ofs = 0)
			{
				hY = hYY;
				cm = mm;
				m = (char*)cm;
				m += ofs;
			}
			MM(const MM&) = delete;
			MM(MM&& me)
			{
				hY = me.hY;
				m = me.m;
				cm = me.cm;
				me.hY = 0;
				me.m = 0;
				me.cm = 0;
			}
			~MM()
			{
				if (cm)
					UnmapViewOfFile(cm);
				cm = 0;
				if (hY)
					CloseHandle(hY);
				hY = 0;
				m = 0;
			}

		};

		void DebugShow(wstring f)
		{
#ifdef _DEBUG
			HANDLE hX = CreateFile(f.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
			if (hX == INVALID_HANDLE_VALUE)
				return;
			LARGE_INTEGER sz = { 0 };
			GetFileSizeEx(hX, &sz);
			vector<char> d;
			d.resize((size_t)(sz.QuadPart + 10));
			DWORD A = 0;
			ReadFile(hX, d.data(), (DWORD)sz.QuadPart, &A, 0);
			CloseHandle(hX);
			OutputDebugStringW((LPWSTR)d.data());
#endif
		}

		template <typename T = char>
		inline MM MemoryMap(unsigned long long from, size_t sz, DWORD acc = GENERIC_READ | GENERIC_WRITE, HANDLE hT = 0)
		{
			if (hT == 0)
				hT = hX;
			DWORD pr = PAGE_READWRITE;
			if (acc == GENERIC_READ)
				pr = PAGE_READONLY;
			DWORD accr = FILE_MAP_WRITE;
			if (acc == GENERIC_READ)
				accr = FILE_MAP_READ;
			auto hY = CreateFileMapping(hT, 0, pr, 0, 0, 0);
			if (hY == 0)
			{
				return MM();
			}

			if (!sz)
				return MM(hY, 0);

			LPVOID d = MapViewOfFile(hY, accr, 0, 0, (size_t)(sz + from));
			if (!d)
			{
				CloseHandle(hY);
				return MM();
			}
			return MM(hY, d, from);
		}

	public:
		inline bool BuildMap(vector<FSITEM>& fsx)
		{
			fsx.clear();
			SetFP(0);
			for (;;)
			{
				FSITEM fs;
				if (!Rd((char*)&fs, sizeof(fs)))
					break;
				if (fs.cid != GUID_HEADER)
					return false;
				fsx.push_back(fs);
				SetFP(fs.sz, FILE_CURRENT);
			}
			return true;
		}

	private:
		inline BOOL SetFP(unsigned long long s, DWORD m = FILE_BEGIN, HANDLE hT = 0)
		{
			if (!hT)
				hT = hX;
			LARGE_INTEGER li = { 0 };
			li.QuadPart = s;
			return SetFilePointerEx(hT, li, 0, m);
		}

		inline unsigned long long FS(HANDLE hT)
		{
			LARGE_INTEGER li;
			GetFileSizeEx(hT, &li);
			return li.QuadPart;
		}

		inline unsigned long long GetFP(HANDLE hT = 0)
		{
			if (!hT)
				hT = hX;
			LARGE_INTEGER li = { 0 };
			SetFilePointerEx(hT, li, &li, FILE_CURRENT);
			return li.QuadPart;
		}

		inline BOOL Wr(const char* d, unsigned long long sz2, HANDLE hT = 0)
		{
			if (hT == 0)
				hT = hX;
			unsigned long long pt = 0;
			for (;;)
			{
				unsigned long long sz = sz2;
				if (sz >= 2147483648)
					sz = 2147483648;
				DWORD A = 0;
				if (!WriteFile(hT, d + pt, (DWORD)sz, &A, 0))
					return false;
				if (A != sz)
					return false;
				sz2 -= sz;
				if (sz2 == 0)
					break;
				pt += A;
			}
			return true;
		}

		inline BOOL Rd(char* d, unsigned long long sz2, HANDLE hT = 0)
		{
			if (hT == 0)
				hT = hX;
			unsigned long long pt = 0;
			for (;;)
			{
				unsigned long long sz = sz2;
				if (sz >= 2147483648)
					sz = 2147483648;
				DWORD A = 0;
				if (!ReadFile(hT, d + pt, (DWORD)sz, &A, 0))
					return false;
				if (A != sz)
					return false;
				sz2 -= sz;
				if (sz2 == 0)
					break;
				pt += A;
			}
			return true;
		}
		inline BOOL Rd(vector<char>& d, HANDLE hT = 0)
		{
			return Rd(d.data(), d.size(), hT);
		}
		inline BOOL Wr(const vector<char>& d, HANDLE hT = 0)
		{
			return Wr(d.data(), d.size(), hT);
		}

		inline wstring TempFile(const wchar_t* prf = L"tmp")
		{
			vector<wchar_t> td(1000);
			GetTempPathW(1000, td.data());
			vector<wchar_t> x(1000);
			GetTempFileNameW(td.data(), prf, 0, x.data());
			DeleteFile(x.data());
			temps.push_back(x.data());
			return x.data();
		}

		static FILETIME ti()
		{
			FILETIME ft = { 0 };
			SYSTEMTIME st;
			GetLocalTime(&st);
			SystemTimeToFileTime(&st, &ft);
			return ft;
		}

		inline wstring BuildUp(size_t idx, int Mode = 0)
		{
			vector<FSITEM> fsx;
			BuildMap(fsx);
			if (idx == -1)
				idx = fsx.size() - 1;
			if (fsx.empty())
				return TempFile();



			if (Mode == 1) // Incremental
			{
				auto tf = TempFile();
				auto hT = CreateFile(tf.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
				if (hT == INVALID_HANDLE_VALUE)
				{
					return L"";
				}

				auto m2 = MemoryMap(fsx[idx].at, (size_t)fsx[idx].sz);
				if (m2.hY == 0)
				{
					CloseHandle(hT);
					return L"";
				}
				if (!Wr((char*)m2.m, (size_t)fsx[idx].sz, hT))
				{
					CloseHandle(hT);
					return L"";
				}

				CloseHandle(hT);
				DebugShow(tf);
				return tf;
			}

			if (Mode == 0) // Differential (or Restore to read)
			{
				struct WN
				{
					size_t idx;
					wstring tef;
				};
				vector<WN> WeNeed;
				size_t cur = idx;
				for (;;)
				{
					WN w;
					w.idx = cur;
					WeNeed.push_back(w);
					if (cur == 0)
					{
						break;
					}

					cur = (size_t)fsx[cur].DiffAt;
				}

				std::reverse(WeNeed.begin(), WeNeed.end());


				if (WeNeed.size() == 1)
				{
					auto tf = TempFile();
					auto hT = CreateFile(tf.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
					if (hT == INVALID_HANDLE_VALUE)
					{
						return L"";
					}
					auto& wn = WeNeed[0];
					auto& fs = fsx[wn.idx];
					auto m = MemoryMap(fs.at, (size_t)fs.sz);
					if (m.hY == 0)
						return L"";
					if (!Wr((char*)m.m, fs.sz, hT))
					{
						CloseHandle(hT);
						return L"";
					}
					CloseHandle(hT);
					DebugShow(tf);
					return tf;
				}

				for (size_t i = 0; i < WeNeed.size(); i++)
				{
					auto& wn = WeNeed[i];
					if (wn.tef.empty())
						wn.tef = TempFile();

					// End?
					if (i == WeNeed.size() - 1)
					{
						DebugShow(wn.tef);
						return wn.tef;
					}

					auto& fs = fsx[wn.idx];

					// If first is 0, take it from the file, else another file
					auto hO = (i == 0) ? INVALID_HANDLE_VALUE : CreateFile(wn.tef.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
					if (hO == INVALID_HANDLE_VALUE && i > 0)
					{
						return L"";
					}
					LARGE_INTEGER li = { 0 };
					GetFileSizeEx(hO, &li);
					auto m = (i == 0) ? MemoryMap(fs.at, (size_t)fs.sz) : MemoryMap(0, (size_t)li.QuadPart, GENERIC_READ | GENERIC_WRITE, hO);
					if (m.hY == 0)
					{
						if (hO != INVALID_HANDLE_VALUE)
							CloseHandle(hO);
						return L"";
					}


					auto& wnn = WeNeed[i + 1];
					if (wnn.tef.empty())
						wnn.tef = TempFile();

					auto& f2 = fsx[wnn.idx];
					auto m2 = MemoryMap(f2.at, (size_t)f2.sz);
					if (m2.hY == 0)
					{
						if (hO != INVALID_HANDLE_VALUE)
							CloseHandle(hO);
						return L"";
					}

					DIFFLIB::MemoryRdcFileReader mr((char*)m.m, fs.sz);
					DIFFLIB::MemoryRdcFileReader di((char*)m2.m, f2.sz);
					auto hT = CreateFile(wnn.tef.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
					if (hT == INVALID_HANDLE_VALUE)
					{
						if (hO != INVALID_HANDLE_VALUE)
							CloseHandle(hO);
						return L"";
					}
					DIFFLIB::FileDiffWriter fw(hT);

					if (FAILED(d.Reconstruct(&mr, &di, 0, fw)))
					{
						if (hO != INVALID_HANDLE_VALUE)
							CloseHandle(hO);
						CloseHandle(hT);
						return L"";
					}
					if (hO != INVALID_HANDLE_VALUE)
						CloseHandle(hO);
					CloseHandle(hT);
				}
			}
			return L"";
		}

	public:
		FILESNAP(const wchar_t* fi)
		{
			f = fi;
		}

		inline bool SetSnap(size_t idx, int CommitForce = 0)
		{
			if (hX == INVALID_HANDLE_VALUE)
				return false;

			if (hR != INVALID_HANDLE_VALUE)
			{
				if (CommitForce == 0)
					return false;
				if (CommitForce == 1)
					Commit(0, 0);
				CloseHandle(hR);
				hR = INVALID_HANDLE_VALUE;
			}

			auto wt = BuildUp(idx);
			if (wt.empty())
				return false;
			hR = CreateFile(wt.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, idx == -1 ? OPEN_ALWAYS : OPEN_EXISTING, 0, 0);
			if (hR == INVALID_HANDLE_VALUE)
				return false;
			return true;
		}

		inline bool Create(DWORD Access = GENERIC_READ | GENERIC_WRITE, DWORD Share = 0, LPSECURITY_ATTRIBUTES Atrs = 0, DWORD Disp = CREATE_NEW, DWORD Flags = 0)
		{
			if (hX != INVALID_HANDLE_VALUE)
				return true;
			hX = CreateFile(f.c_str(), Access, Share, Atrs, Disp, Flags, 0);
			if (hX == INVALID_HANDLE_VALUE)
				return false;

			LARGE_INTEGER li = { 0 };
			GetFileSizeEx(hX, &li);

			if (Disp == CREATE_NEW || Disp == CREATE_ALWAYS || li.QuadPart == 0)
			{
			}

			vector<FSITEM> fsx;
			BuildMap(fsx);
			if (fsx.size())
				SetSnap(fsx.size() - 1);
			else
				SetSnap((size_t)-1);
			return true;
		}

		inline unsigned long long Size()
		{
			LARGE_INTEGER li = { 0 };
			GetFileSizeEx(hR, &li);
			return li.QuadPart;
		}

		inline bool Finalize()
		{

			return true;
		}

		inline bool Commit(size_t At, int Mode)
		{
			// Take current R
			if (hR == INVALID_HANDLE_VALUE)
				return false;

			vector<FSITEM> fsx;
			BuildMap(fsx);

			if (fsx.size() == 0)
			{
				// First
				LARGE_INTEGER li = { 0 };
				GetFileSizeEx(hR, &li);
				auto m = MemoryMap(0, (size_t)li.QuadPart, GENERIC_READ | GENERIC_WRITE, hR);
				if (m.hY == 0)
				{
					return false;
				}
				FSITEM fs;
				fs.i = 0;
				fs.DiffAt = At;
				fs.at = sizeof(fs);
				fs.sz = li.QuadPart;
				SetFP(0);
				SetEndOfFile(hX);
				if (!Wr((char*)&fs, sizeof(fs)))
				{
					return false;
				}
				if (!Wr((char*)m.m, li.QuadPart))
				{
					return false;
				}
				return true;
			}

			wstring tf = BuildUp(At, Mode);
			if (tf.empty())
				return false;

			LARGE_INTEGER li = { 0 };
			GetFileSizeEx(hR, &li);
			auto m = MemoryMap(0, (size_t)li.QuadPart, GENERIC_READ | GENERIC_WRITE, hR);
			if (m.hY == 0)
			{
				DeleteFile(tf.c_str());
				return false;
			}

			HANDLE hUp = CreateFile(tf.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
			if (!hUp)
				return false;
			size_t upsz = (size_t)FS(hUp);
			auto mup = MemoryMap(0, upsz, GENERIC_READ | GENERIC_WRITE, hUp);
			if (mup.hY == 0)
			{
				CloseHandle(hUp);
				DeleteFile(tf.c_str());
				return false;
			}
			DIFFLIB::MemoryRdcFileReader f1((char*)mup.m, upsz);
			DIFFLIB::MemoryRdcFileReader f2((char*)m.m, li.QuadPart);
			DIFFLIB::MemoryDiffWriter s1, s2;
			d.GenerateSignature(&f1, s1);
			d.GenerateSignature(&f2, s2);
			DIFFLIB::MemoryDiffWriter mw;
			if (FAILED(d.GenerateDiff(s1.GetReader(), s2.GetReader(), &f2, mw)))
			{
				CloseHandle(hUp);
				DeleteFile(tf.c_str());
				return false;
			}
			CloseHandle(hUp);


			// Add a FSITEM
			SetFP(0, FILE_END);
			unsigned long long filesize = GetFP();
			FSITEM fs;
			fs.DiffAt = At;
			fs.i = fsx.size();
			fs.at = sizeof(fs) + filesize;
			fs.sz = mw.sz();


			if (!Wr((char*)&fs, sizeof(fs)))
			{
				return false;
			}
			if (!Wr((char*)mw.p().data(), mw.sz()))
			{
				return false;
			}
			return true;
		}

		inline HANDLE GetH()
		{
			return hR;
		}

		inline BOOL Write(const char* d, unsigned long long sz2)
		{
			return Wr(d, sz2, hR);
		}

		inline BOOL Read(char* d, unsigned long long sz2)
		{
			return Rd(d, sz2, hR);
		}

		inline unsigned long long SetFilePointer(unsigned long long s, DWORD m = FILE_BEGIN)
		{
			return SetFP(s, m, hR);
		}

		inline BOOL SetEnd()
		{
			return SetEndOfFile(hR);
		}

		inline void Close()
		{
			for (auto& t : temps)
				DeleteFile(t.c_str());
			CloseHandle(hR);
		}

	};
	// {9533324F-88A1-4CD8-8053-6A501B5BCDAA}
	const GUID FILESNAP::GUID_HEADER = { 0x9533324f, 0x88a1, 0x4cd8,{ 0x80, 0x53, 0x6a, 0x50, 0x1b, 0x5b, 0xcd, 0xaa } };


	// --------

}