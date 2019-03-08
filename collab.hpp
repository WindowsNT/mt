#include "diff.hpp"



namespace COLLAB
	{
	using namespace std;
#include "rw.hpp"

	class AUTH
		{
		public:
			virtual HRESULT Do(bool Server,vector<char>& r,CLSID doc = CLSID_NULL) = 0;
			virtual void SetSocket(SOCKET yy) = 0;
		};

	// Helpers

	template <int MR = 1>
	class TEVENT
		{
		public:
			HANDLE m = 0;
			TEVENT()
				{
				m = CreateEvent(0, MR, 0, 0);
				}
			void Close()
				{
				if (m)
					CloseHandle(m);
				m = 0;
				}
			DWORD Wait(DWORD i = INFINITE)
				{
				if (m)
					{
					return  WaitForSingleObject(m, i);
					}
				return WAIT_ABANDONED;
				}
			void Set()
				{
				if (m)
					SetEvent(m);
				}
			void Reset()
				{
				if (m)
					ResetEvent(m);
				}
			~TEVENT()
				{
				Close();
				}

			TEVENT(const TEVENT&m)
				{
				operator=(m);
				}
			TEVENT(TEVENT&&m)
				{
				operator=(std::forward<TEVENT<MR>>(m));
				}

			TEVENT& operator= (const TEVENT&b)
				{
				Close();
				DuplicateHandle(GetCurrentProcess(), b.m, GetCurrentProcess(), &m, 0, 0, DUPLICATE_SAME_ACCESS);
				return *this;
				}
			TEVENT& operator= (TEVENT&&b)
				{
				Close();
				m = b.m;
				b.m = 0;
				return *this;
				}
		};



	class XSOCKET
		{
		private:
			SOCKET X = 0;
			int fam;
			shared_ptr<int> ptr;


		public:



			bool Valid()
				{
				if (X == 0 || X == -1)
					return false;
				return true;
				}

			void CloseIf()
				{
				if (ptr.use_count() == 1)
					Close();
				}

			SOCKET h()
				{
				return X;
				}

			operator SOCKET()
				{
				return X;
				}

			~XSOCKET()
				{
				CloseIf();
				}


			XSOCKET(SOCKET t)
				{
				X = t;
				ptr = make_shared<int>(int(0));
				}

			XSOCKET()
				{
				}

			void Create(int af = AF_INET6, int ty = SOCK_STREAM, int pro = IPPROTO_TCP)
				{
				fam = af;
				X = socket(af, ty, pro);
				if (X == 0 || X == INVALID_SOCKET)
					{
					X = 0;
					return;
					}
				if (af == AF_INET6)
					{
					DWORD ag = 0;
					setsockopt(X, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&ag, 4);
					}
				ptr = make_shared<int>(int(0));
				}

			XSOCKET(const XSOCKET& x)
				{
				operator =(x);
				}

			XSOCKET& operator =(const XSOCKET& x)
				{
				CloseIf();
				ptr = x.ptr;
				X = x.X;
				return *this;
				}

			void Detach()
				{
				X = 0;
				}

			void Close()
				{
				if (X != INVALID_SOCKET && X != 0)
					closesocket(X);
				X = 0;
				}


			bool BindAndListen(int port)
				{
				if (fam == AF_INET6)
					{
					sockaddr_in6 sA = { 0 };
					sA.sin6_family = (ADDRESS_FAMILY)fam;
					sA.sin6_port = (u_short)htons((u_short)port);
					if (::bind(X, (sockaddr*)&sA, sizeof(sA)) < 0)
						return false;

					}
				else
					{
					sockaddr_in sA = { 0 };
					sA.sin_addr.s_addr = INADDR_ANY;
					sA.sin_family = (ADDRESS_FAMILY)fam;
					sA.sin_port = (u_short)htons((u_short)port);
					if (::bind(X, (sockaddr*)&sA, sizeof(sA)) < 0)
						return false;

					}
				listen(X, 3);
				return true;
				}

			SOCKET Accept()
				{
				return accept(X, 0, 0);
				}


			BOOL ConnectTo(const char* addr, int port, int sec = 0)
				{
				// Check the address
				if (!addr || !port)
					return false;
				wchar_t se[100] = { 0 };
				swprintf_s(se, 100, L"%u", port);
				timeval tv = { 0 };
				tv.tv_sec = sec;
				wchar_t adr[1000] = { 0 };
				MultiByteToWideChar(CP_UTF8, 0, addr, -1, adr, 1000);
				return WSAConnectByName(X, adr, se, 0, 0, 0, 0, sec ? &tv : 0, 0);
				}

			
			int transmit(const char *b, int sz, bool ForceAll = false, int p = 0, std::function<void(int)> cb = nullptr)
				{
				// same as send, but forces reading ALL sz
				if (!ForceAll)
					return send(X, b, sz, p);
				int rs = 0;
				for (;;)
					{
					int tosend = sz - rs;
					if (tosend > 10000)
						tosend = 10000;
					int rval = send(X, b + rs, tosend, p);
					if (rval == 0 || rval == SOCKET_ERROR)
						return rs;
					rs += rval;
					if (cb)
						cb(rs);
					if (rs == sz)
						return rs;
					}
				}


			int receive(char *b, int sz, bool ForceAll = false, int p = 0)
				{
				// same as recv, but forces reading ALL sz
				if (!ForceAll)
					return recv(X, b, sz, p);
				int rs = 0;
				for (;;)
					{
					int rval = recv(X, b + rs, sz - rs, p);
					if (rval == 0 || rval == SOCKET_ERROR)
						return rs;
					rs += rval;
					if (rs == sz)
						return rs;
					}
				}



		};

	struct DOCUMENT
		{
		CLSID id = { 0 };
		vector<char> d;
		HANDLE hF = 0;
		size_t hFCount = 0;
		RWMUTEX m;

		void LessClient()
			{
			m.LockWrite();
			if (hFCount > 0)
				hFCount--;
			if (hFCount == 0)
				{
				CloseHandle(hF);
				hF = 0;
				}
			m.ReleaseWrite();
			}
		};

	struct R
		{
		int Type;
		int Size;
		};

#define TYPE_OPEN_DOCUMENT 0
#define TYPE_GET_SIGNATURE 1
#define TYPE_UPDATE 2
#define TYPE_CLOSE_DOCUMENT 3
#define TYPE_SERVER_UPDATE 4


	inline HRESULT GetRequest(XSOCKET& X, vector<char>& rep, int& Type)
		{
		R r;
		if (X.receive((char*)&r, sizeof(r), true) != sizeof(r))
			return E_FAIL;
		rep.resize(r.Size);
		if (X.receive(rep.data(),(int) rep.size(), true) != (int)rep.size())
			return E_FAIL;
		Type = r.Type;
		return S_OK;
		}

	inline HRESULT SendReply(XSOCKET& X, vector<char>& rep, int T)
		{
		R r;
		r.Type = T;
		r.Size = (int)rep.size();
		if (X.transmit((char*)&r, sizeof(r), true) != sizeof(r))
			return E_FAIL;
		if (X.transmit(rep.data(), (int)rep.size(), true) != (int)rep.size())
			return E_FAIL;
		return S_OK;
		}

	inline HRESULT RequestAsync(XSOCKET& X, int T, const char* d, size_t sz)
		{
		R r;
		r.Type = T;
		r.Size = (int)sz;
		if (X.transmit((char*)&r, sizeof(r), true) != sizeof(r))
			return E_FAIL;
		if (X.transmit(d, (int)sz, true) != (int)sz)
			return E_FAIL;
		return S_OK;
		}

	inline HRESULT Request(XSOCKET& X,int T,const char* d,size_t sz,vector<char>& rep,bool StripHR = false)
		{
		R r;
		r.Type = T;
		r.Size = (int)sz;
		if (X.transmit((char*)&r, sizeof(r), true) != sizeof(r))
			return E_FAIL;
		if (X.transmit(d, (int)sz, true) != (int)sz)
			return E_FAIL;

		int ty = 0;
		auto hr = GetRequest(X, rep, ty);
		if (FAILED(hr))
			return hr;

		if (!StripHR)
			return hr;
		if (rep.size() < sizeof(HRESULT))
			return E_FAIL;
		memcpy(&hr, rep.data(), sizeof(HRESULT));
		memmove(rep.data(), rep.data() + sizeof(HRESULT), rep.size() - sizeof(HRESULT));
		rep.resize(rep.size() - 4);
		return hr;
		}

	class ANYAUTH : public AUTH
		{
		private:
			SOCKET y;
		public:
			virtual void SetSocket(SOCKET yy)
				{
				y = yy;
				}
			virtual HRESULT Do(bool Server, vector<char>& d,CLSID doc)
				{
				return S_OK;
				}
		};


	class ON
		{
		public:
			bool InUpdate = false;

			virtual void Update(CLSID cid,const char* d,size_t sz) = 0;
			void U(CLSID cid, const char* d, size_t sz)
				{
				InUpdate = true;
				Update(cid, d, sz);
				InUpdate = false;
				}
		};

	class SERVER
		{
		private:

			struct ACLIENTDOCUMENT
				{
				CLSID doc;
				bool Write = true;
				vector<char> CurrentSignature;
				};

			struct ACLIENT
				{
				XSOCKET s;
				RWMUTEX m;
				vector<char> authdata;
				vector<ACLIENTDOCUMENT> docs;
				};

			AUTH* auth = 0;
			int port = 0;
			bool FileStorage = false;
			XSOCKET lix;
			vector<shared_ptr<ACLIENT>> clients;
			vector<shared_ptr<DOCUMENT>> docs;
			RWMUTEX m;

			bool HasWriteAccess(ACLIENT& cl, CLSID c)
				{
				for (auto& d : cl.docs)
					{
					if (d.doc == c)
						{
						if (d.Write)
							return true;
						return false;
						}
					}
				return false;
				}

			shared_ptr<DOCUMENT> Find(CLSID c)
				{
				shared_ptr<DOCUMENT> d = 0;
				auto g1 = m.LockRead();
				for (auto& a : docs)
					{
					if (a->id == c)
						{
						d = a;
						break;
						}
					}
				m.ReleaseRead(g1);
				return d;
				}

			void EmptySend(int ty,XSOCKET& s,HRESULT hr)
				{
				vector<char> x(sizeof(HRESULT));
				memcpy(x.data(), &hr, sizeof(HRESULT));
				SendReply(s, x, ty);
				}

			void UpdateClientsOfDocument(DIFFLIB::DIFF& diff,shared_ptr<DOCUMENT> doc, ACLIENT* except,ACLIENT* inc)
				{
				HRESULT hr = 0;
				auto g3 = m.LockRead();
				for (auto& cl : clients)
					{
					if (inc == 0 && cl.get() == except)
						continue;
					if (except == 0 && cl.get() != inc)
						continue;
//					__nop();
					auto g2 = cl->m.LockRead();
					for (auto& dd : cl->docs)
						{
						if (dd.doc == doc->id)
							{
							auto g1 = doc->m.LockRead();

							DIFFLIB::MemoryDiffWriter s2;
							DIFFLIB::MemoryDiffWriter dw;
							if (doc->hF != 0)
								{
								DIFFLIB::FileRdcFileReader i1(doc->hF);
								hr = diff.GenerateSignature(&i1, s2);
								DIFFLIB::MemoryRdcFileReader s1(dd.CurrentSignature.data(), dd.CurrentSignature.size());
								hr = diff.GenerateDiff(&s1, s2.GetReader(), &i1, dw);
								}
							else
								{
								DIFFLIB::MemoryRdcFileReader i1(doc->d.data(), doc->d.size());
								hr = diff.GenerateSignature(&i1, s2);
								DIFFLIB::MemoryRdcFileReader s1(dd.CurrentSignature.data(), dd.CurrentSignature.size());
								hr = diff.GenerateDiff(&s1, s2.GetReader(), &i1, dw);
								}

							vector<char> rep(sizeof(CLSID) + dw.p().size());
							memcpy(rep.data(), &doc->id, sizeof(CLSID));
							memcpy(rep.data() + sizeof(CLSID), dw.p().data(),dw.p().size());
							RequestAsync(cl->s, TYPE_SERVER_UPDATE, rep.data(), rep.size());

							// Save new sig he must have
							dd.CurrentSignature = s2.p();
							doc->m.ReleaseRead(g1);
							}
						}
					cl->m.ReleaseRead(g2);
					}
				m.ReleaseRead(g3);
				}

			void RunClient(shared_ptr<ACLIENT> ccl)
				{
				ACLIENT& cl = *ccl;

				CoInitializeEx(0, COINIT_MULTITHREADED);
				for (;;)
					{
					vector<char> rep;
					int ty = 0;
					auto hr = GetRequest(cl.s, rep, ty);
					if (FAILED(hr))
						break;

					if (ty == TYPE_UPDATE)
						{
						CLSID c;
						memcpy(&c, rep.data(), sizeof(c));
						shared_ptr<DOCUMENT> d = Find(c);
						if (!d)
							{
							EmptySend(TYPE_UPDATE, cl.s, E_NOINTERFACE);
							continue;
							}
						bool wr = HasWriteAccess(*ccl, c);
						if (!wr)
							{
							EmptySend(TYPE_UPDATE, cl.s, E_ACCESSDENIED);
							continue;
							}

						d->m.LockWrite();
						DIFFLIB::DIFF diff;

						// Local Signature

						DIFFLIB::MemoryDiffWriter w1;
						DIFFLIB::MemoryDiffWriter reco;
						if (d->hF != 0)
							{
							DIFFLIB::FileRdcFileReader l1(d->hF);
							hr = diff.GenerateSignature(&l1, w1);
							if (FAILED(hr))
								{
								d->m.ReleaseWrite();
								EmptySend(TYPE_UPDATE, cl.s, E_FAIL);
								continue;
								}

							DIFFLIB::MemoryRdcFileReader df(rep.data() + sizeof(CLSID), rep.size() - sizeof(CLSID));
							hr = diff.Reconstruct(&l1, &df, 0, reco);

							// And write to file
							SetFilePointer(d->hF, 0, 0, FILE_BEGIN);
							SetEndOfFile(d->hF);
							DWORD A = 0;
							WriteFile(d->hF, reco.p().data(), (DWORD)reco.p().size(), &A, 0);
							if (reco.p().size() != A)
								{
								d->m.ReleaseWrite();
								EmptySend(TYPE_UPDATE, cl.s, E_FAIL);
								continue;
								}
							}
						else
							{
							DIFFLIB::MemoryRdcFileReader l1(d->d.data(), d->d.size());
							hr = diff.GenerateSignature(&l1, w1);
							if (FAILED(hr))
								{
								d->m.ReleaseWrite();
								EmptySend(TYPE_UPDATE, cl.s, E_FAIL);
								continue;
								}

							DIFFLIB::MemoryRdcFileReader df(rep.data() + sizeof(CLSID), rep.size() - sizeof(CLSID));
							hr = diff.Reconstruct(&l1, &df, 0, reco);
							d->d = reco.p();
							}




						d->m.ReleaseWrite();

						// This client we must update it's document signature
						for (auto& d : cl.docs)
							{
							if (d.doc == c)
								{
								DIFFLIB::MemoryDiffWriter dw;
								diff.GenerateSignature(reco.GetReader(), dw);
								cl.m.LockWrite();
								d.CurrentSignature = dw.p();
								cl.m.ReleaseWrite();
								}
							}

						EmptySend(TYPE_UPDATE, cl.s, S_OK);
						UpdateClientsOfDocument(diff,d, &cl,0);
						continue;
						}

					// Get Sig
					if (ty == TYPE_GET_SIGNATURE)
						{
						CLSID c;
						memcpy(&c, rep.data(), sizeof(c));
						shared_ptr<DOCUMENT> d = Find(c);
						if (!d)
							{
							EmptySend(TYPE_GET_SIGNATURE, cl.s, E_NOINTERFACE);
							continue;
							}

						auto g1 = d->m.LockRead();
						DIFFLIB::DIFF diff;
						// Local Signature
						DIFFLIB::MemoryDiffWriter w1;
						HRESULT hr;


						if (d->hF != 0)
							{
							DIFFLIB::FileRdcFileReader l1(d->hF);
							hr = diff.GenerateSignature(&l1, w1);
							}
						else
							{
							DIFFLIB::MemoryRdcFileReader l1(d->d.data(), d->d.size());
							hr = diff.GenerateSignature(&l1, w1);
							}
						d->m.ReleaseRead(g1);

						if (FAILED(hr))
							{
							EmptySend(TYPE_GET_SIGNATURE, cl.s, E_FAIL);
							continue;
							}

						vector<char> x(sizeof(HRESULT) + w1.sz());
						memcpy(x.data(), &hr, sizeof(HRESULT));
						memcpy(x.data() + sizeof(HRESULT), w1.p().data(), w1.sz());
						SendReply(cl.s, x, TYPE_GET_SIGNATURE);
						continue;
						}

					// NEW OR OPEN DOCUMENT
					if (ty == TYPE_OPEN_DOCUMENT)
						{
						CLSID c;
						memcpy(&c, rep.data(), sizeof(c));
						shared_ptr<DOCUMENT> d = Find(c);
						if (!d)
							{
							// new
							m.LockWrite();
							d = make_shared<DOCUMENT>();
							d->id = c;
							docs.push_back(d);
							m.ReleaseWrite();
							}

						bool W = true;
						if (auth)
							{
							for (;;)
								{
								auto hr = auth->Do(true, cl.authdata,c);
								if (hr == E_PENDING)
									continue;
								if (FAILED(hr))
									{
									EmptySend(TYPE_OPEN_DOCUMENT, cl.s, hr);
									continue;
									}
								if (hr == S_FALSE)
									W = false;
								break; // ok
								}
							}

						if (FileStorage)
							{
							d->m.LockWrite();
							if (!d->hF)
								{
								wchar_t t[100] = { 0 };
								StringFromGUID2(c, t, 100);
								d->hF = CreateFile(t, W ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ, 0, 0, OPEN_ALWAYS, 0, 0);
								if (d->hF == INVALID_HANDLE_VALUE)
									d->hF = 0;
								}
							if (d->hF)
								d->hFCount++;
							d->m.ReleaseWrite();
							}

						cl.m.LockWrite();
						ACLIENTDOCUMENT adc;
						adc.Write = W;
						
						// Create empty sig
						DIFFLIB::MemoryRdcFileReader r1(0, 0);
						DIFFLIB::DIFF diff;
						DIFFLIB::MemoryDiffWriter dw;
						diff.GenerateSignature(&r1, dw);
						adc.CurrentSignature = dw.p();

						adc.doc = c;
						cl.docs.push_back(adc);
						cl.m.ReleaseWrite();
						EmptySend(TYPE_OPEN_DOCUMENT, cl.s, S_OK);
				
						UpdateClientsOfDocument(diff, d, 0,&cl);
						continue;
						}

					// CLOSE DOC
					if (ty == TYPE_CLOSE_DOCUMENT)
						{
						CLSID c;
						memcpy(&c, rep.data(), sizeof(c));
						shared_ptr<DOCUMENT> d = Find(c);
						if (!d)
							{
							EmptySend(TYPE_CLOSE_DOCUMENT, cl.s, E_NOINTERFACE);
							continue;
							}

						if (d->hF)
							{
							d->LessClient();
							}

						cl.m.LockWrite();
						for (size_t i = 0; i < cl.docs.size(); i++)
							{
							if (cl.docs[i].doc == c)
								{
								cl.docs.erase(cl.docs.begin() + i);
								break;
								}
							}
						cl.m.ReleaseWrite();
						EmptySend(TYPE_CLOSE_DOCUMENT, cl.s, S_OK);
						continue;
						}



					}


				// Remove it and free all
				for (signed long long i = clients.size() - 1; i >= 0 ; i--)
					{
					if (clients[i]->s.h() == cl.s.h())
						{
						for (auto& a : clients[i]->docs)
							{
							auto d = Find(a.doc);
							if (!d)
								continue;
							d->LessClient();
							}
						clients.erase(clients.begin() + i);
						}
					}
				cl.s.Close();

				}


			void client(SOCKET e)
				{
				// Authenticate
				CoInitializeEx(0, COINIT_MULTITHREADED);
				shared_ptr<ACLIENT> a = make_shared<ACLIENT>();
				a->s = e;
				if (auth)
					{
					auth->SetSocket(e);
					for (;;)
						{
						auto hr = auth->Do(true,a->authdata);
						if (hr == E_PENDING)
							continue;
						if (FAILED(hr))
							{
							closesocket(e);
							return;
							}
						break; // ok
						}
					}

				m.LockWrite();
				clients.push_back(a);
				m.ReleaseWrite();
				RunClient(a);
				}

			void acc()
				{
				for (;;)
					{
					auto e = lix.Accept();
					if (e == SOCKET_ERROR)
						break;
					std::thread t(&SERVER::client, this,e);
					t.detach();
					}
				}

		public:

			XSOCKET& sock() { return lix; }

			void ForceUpdateDocument(GUID c)
				{
				shared_ptr<DOCUMENT> d = Find(c);
				if (!d)
					return;
				DIFFLIB::DIFF diff;
				UpdateClientsOfDocument(diff, d,0,0);
				}

			shared_ptr<DOCUMENT> GetDocument(GUID c)
				{
				shared_ptr<DOCUMENT> d = Find(c);
				if (!d)
					return 0;
				return d;
				}

			SERVER(AUTH* a,int Port,bool fs = false)
				{
				auth = a;
				port = Port;
				FileStorage = fs;
				}
			HRESULT Start()
				{
				lix.Create();
				if (!lix.BindAndListen(port))
					return E_FAIL;
				std::thread t(&SERVER::acc, this);
				t.detach();
				return S_OK;
				}
			HRESULT End()
				{
				lix.Close();
				for (auto& a : clients)
					a->s.Close();
				return S_OK;
				}

		};



	class CLIENT
		{
		private:
			AUTH* auth;
			XSOCKET x;
			vector<char> authdata;
			vector<ON*> ons;

			TEVENT<> hN;
			int NextReply = 0;
			bool NextStripHR = 0;
			vector<char> NextData;
			HRESULT NextHR;

			struct ACLIENTDOCUMENT
				{
				CLSID doc;
				vector<char> CurrentSignature;
				};
			vector<ACLIENTDOCUMENT> docs;

		public:
			CLIENT(AUTH* a)
				{
				auth = a;
				}
			void AddOn(ON* o)
				{
				ons.push_back(o);
				}
			void RemoveOn(ON* o)
				{
				for (size_t i = 0; i < ons.size(); i++)
					{
					if (ons[i] == o)
						{
						ons.erase(ons.begin() + i);
						break;
						}
					}
				}

			void ReceiveServerCommands()
				{
				CoInitializeEx(0, COINIT_MULTITHREADED);
				DIFFLIB::DIFF diff;
				for (;;)
					{
					vector<char> rep;
					int ty = 0;
					auto hr = GetRequest(x, rep, ty);
					if (FAILED(hr))
						break;

					if (ty == TYPE_SERVER_UPDATE)
						{
						// Get the guid
						if (rep.size() < sizeof(CLSID))
							continue;
						CLSID g;
						memcpy(&g, rep.data(), sizeof(CLSID));
						for(auto& on : ons)
							{
							on->U(g, rep.data() + sizeof(CLSID), rep.size() - sizeof(CLSID));
							}
						continue;
						}

					NextData = rep;
					NextHR = hr;
					if (NextStripHR)
						{
						if (NextData.size() < sizeof(HRESULT))
							break;
						memcpy(&hr, NextData.data(), sizeof(HRESULT));
						memmove(NextData.data(), NextData.data() + sizeof(HRESULT), NextData.size() - sizeof(HRESULT));
						NextData.resize(NextData.size() - 4);
						NextHR = hr;
						}
					hN.Set();
					}

				x.Close();
				return;
				}

			HRESULT Disconnect()
				{
				x.Close();
				return 0;
				}

			HRESULT Connect(const char* addr, int p)
				{
				if (x.Valid())
					return S_FALSE;
				x.Create();
				if (!x.ConnectTo(addr, p))
					return E_FAIL;
				if (auth)
					{
					auth->SetSocket(x);
					for (;;)
						{
						auto hr = auth->Do(false, authdata);
						if (hr == E_PENDING)
							continue;
						if (FAILED(hr))
							{
							closesocket(x);
							return E_ACCESSDENIED;
							}
						break; // ok
						}
					}
				std::thread t(&CLIENT::ReceiveServerCommands, this);
				t.detach();
				return S_OK;
				}

			HRESULT Open(GUID g)
				{
				for (auto& d : docs)
					{
					if (d.doc == g)
						return S_FALSE;
					}

				vector<char> rep;
				NextReply = TYPE_OPEN_DOCUMENT;
				hN.Reset();
				auto hr = RequestAsync(x, TYPE_OPEN_DOCUMENT, (char*)&g, sizeof(g));
				if (FAILED(hr))
					return hr;
				hN.Wait();
				ACLIENTDOCUMENT add;
				add.doc = g;
				docs.push_back(add);
				return S_OK;
				}

			HRESULT Close(GUID g)
				{
				for (size_t i = 0 ; i < docs.size() ; i++)
					{
					auto& d = docs[i];
					if (d.doc == g)
						{
						docs.erase(docs.begin() + i);
						auto hr = RequestAsync(x, TYPE_CLOSE_DOCUMENT, (char*)&g, sizeof(g));
						return hr;
						}
					}

				return E_NOINTERFACE;
				}
			
			HRESULT Put(GUID g, const char* d,size_t sz)
				{
				DIFFLIB::DIFF diff;
				
				// Remote Signature
				vector<char> rep;
				NextReply = TYPE_GET_SIGNATURE;
				NextStripHR = true;
				hN.Reset();
				auto hr = RequestAsync(x, TYPE_GET_SIGNATURE, (char*)&g, sizeof(g));
				if (FAILED(hr))
					return hr;
				hN.Wait();
				rep = NextData;

				DIFFLIB::MemoryRdcFileReader w2(rep.data(), rep.size());

				// Local Signature
				DIFFLIB::MemoryRdcFileReader l1(d,sz);
				DIFFLIB::MemoryDiffWriter w1;
				hr = diff.GenerateSignature(&l1, w1);
				if (FAILED(hr))
					return hr;
				for (auto& dd : docs)
					{
					if (dd.doc == g)
						{
						dd.CurrentSignature = w1.p();
						}
					}


				DIFFLIB::MemoryDiffWriter dw;
				hr = diff.GenerateDiff(&w2, w1.GetReader(), &l1, dw);
				if (FAILED(hr))
					return hr;

				vector<char> xx(dw.p().size() + sizeof(CLSID));
				memcpy(xx.data(), &g, sizeof(CLSID));
				memcpy(xx.data() + sizeof(CLSID), dw.p().data(), dw.p().size());
				NextReply = TYPE_UPDATE;
				hN.Reset();
				hr = RequestAsync(x, TYPE_UPDATE, (char*)xx.data(), xx.size());
				if (FAILED(hr))
					return hr;
				hN.Wait();
				rep = NextData;

				return S_OK;
				}
		};

	};