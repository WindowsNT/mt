// -------------------------
namespace tpoollib
	{
	// Handles template
	
	// Destruction Policy	
	template<typename T>
	class destruction_policy
		{
		public:
			static void destruct(T h)
				{
				static_assert(false,"Must define destructor");
				}
		};

	// Policies Specialization
	template<>	class destruction_policy<PTP_POOL> { public: static void destruct(PTP_POOL h) { CloseThreadpool(h); } };
	template<>	class destruction_policy<PTP_WORK> { public: static void destruct(PTP_WORK h) { CloseThreadpoolWork(h); } };
	template<>	class destruction_policy<PTP_WAIT> { public: static void destruct(PTP_WAIT h) { CloseThreadpoolWait(h); } };
	template<>	class destruction_policy<PTP_TIMER> { public: static void destruct(PTP_TIMER h) { CloseThreadpoolTimer(h); } };
	template<>	class destruction_policy<PTP_IO> { public: static void destruct(PTP_IO h) { CloseThreadpoolIo(h); } };
	template<>	class destruction_policy<PTP_CLEANUP_GROUP> { public: static void destruct(PTP_CLEANUP_GROUP h) { CloseThreadpoolCleanupGroup(h); } };
	

	// Template for Handles
	template <typename T,typename Destruction = destruction_policy<T>>
	class handle
		{
		private:
			T hX = 0;
			bool NoDestruct = true;
			std::shared_ptr<size_t> ptr = std::make_shared<size_t>();

		public:

			// Closing items
			void Close()
				{
				if (!ptr || ptr.use_count() > 1)
					{
					ptr.reset();
					return;
					}
				ptr.reset();
				if (hX != 0 && !NoDestruct)
					Destruction::destruct(hX);
				hX = 0;
				}

			handle()
				{
				hX = 0;
				}
			~handle()
				{
				Close();
				}
			handle(const handle& h)
				{
				Dup(h);
				}
			handle(handle&& h)
				{
				Move(std::forward<handle>(h));
				}
			handle(T hY,bool NoDestructOnClose)
				{
				hX = hY;
				NoDestruct = NoDestructOnClose;
				}

			handle& operator =(const handle& h)
				{
				Dup(h);
				return *this;
				}
			handle& operator =(handle&& h)
				{
				Move(std::forward<handle>(h));
				return *this;
				}

			void Dup(const handle& h)
				{
				Close();
				NoDestruct = h.NoDestruct;
				hX = h.hX;
				ptr = h.ptr;
				}
			void Move(handle&& h)
				{
				Close();
				hX = h.hX;
				ptr = h.ptr;
				NoDestruct = h.NoDestruct;
				h.ptr.reset();
				h.hX = 0;
				h.NoDestruct = false;
				}
			operator T() const
				{
				return hX;
				}

		};


	template <bool AutoDestruct = true>
	class tpool
		{
		private:
			handle<PTP_POOL> p;
			handle<PTP_CLEANUP_GROUP> pcg;
			TP_CALLBACK_ENVIRON env;

			tpool(const tpool&) = delete;
			tpool(tpool&&) = delete;
			void operator=(const tpool&) = delete;
			void operator=(tpool&&) = delete;

		public:

			tpool()
				{
				}

			~tpool()
				{
				End();
				}

			void End()
				{
				Join();
				DestroyThreadpoolEnvironment(&env);
				p.Close();
				}

			// Creates the interfaces
			bool Create(unsigned long nmin = 1,unsigned long nmax = 1)
				{ 
				bool jauto = AutoDestruct;

				// Env
				InitializeThreadpoolEnvironment(&env);

				// Pool and Min/Max
				handle<PTP_POOL> cx(CreateThreadpool(0),false);
				p = cx;
				if (!p)
					{
					End();
					return false;
					}
				if (!SetThreadpoolThreadMinimum(p,nmin))
					{
					End();
					return false;
					}
				SetThreadpoolThreadMaximum(p,nmax);

				// Cleanup Group
				if (jauto)
					{
					handle<PTP_CLEANUP_GROUP> cx3(CreateThreadpoolCleanupGroup(),false);
					pcg = cx3;
					if (!pcg)
						{
						End();
						return false;
						}
					}

				// Sets
				SetThreadpoolCallbackPool(&env,p);
				SetThreadpoolCallbackCleanupGroup(&env,pcg,0);

				return true;
				}


			// Templates for each of the items, to be specialized later
			template <typename J>
			void Wait(handle<J> h,bool Cancel = false)
				{
				static_assert(false,"No Wait function is defined");
				}
			template <typename J,typename CB_J>
			handle<J> CreateItem(CB_J cb,PVOID opt = 0,HANDLE hX = 0)
				{
				static_assert(false,"No Create function is defined");
				}
			template <typename J,typename ...A>
			void RunItem(handle<J> h,std::tuple<A...> = std::make_tuple<>())
				{
				static_assert(false,"No Run function is defined");
				}


			// Work Stuff
			template <> handle<PTP_WORK> CreateItem<PTP_WORK,PTP_WORK_CALLBACK>(PTP_WORK_CALLBACK cb,PVOID opt,HANDLE)
				{
				handle<PTP_WORK> a(CreateThreadpoolWork(cb,opt,&env),AutoDestruct);
				return a;
				}
			template <> void RunItem<PTP_WORK>(handle<PTP_WORK> h,std::tuple<>)
				{
				SubmitThreadpoolWork(h);
				}
			template <> void Wait<PTP_WORK>(handle<PTP_WORK> h,bool Cancel)
				{
				WaitForThreadpoolWorkCallbacks(h,Cancel);
				}


			// Wait  stuff
			template <> handle<PTP_WAIT> CreateItem<PTP_WAIT,PTP_WAIT_CALLBACK>(PTP_WAIT_CALLBACK cb,PVOID opt,HANDLE)
				{
				handle<PTP_WAIT> a(CreateThreadpoolWait(cb,opt,&env),AutoDestruct);
				return a;
				}
			template <> void Wait<PTP_WAIT>(handle<PTP_WAIT> h,bool Cancel)
				{
				WaitForThreadpoolWaitCallbacks(h,Cancel);
				}

			// Timer stuff
			template <> handle<PTP_TIMER> CreateItem<PTP_TIMER,PTP_TIMER_CALLBACK>(PTP_TIMER_CALLBACK cb,PVOID opt,HANDLE)
				{
				handle<PTP_TIMER> a(CreateThreadpoolTimer(cb,opt,&env),AutoDestruct);
				return a;
				}
			template <> void RunItem<PTP_TIMER>(handle<PTP_TIMER> h,std::tuple<FILETIME*,DWORD,DWORD>t)
				{
				SetThreadpoolTimer(h,std::get<0>(t),std::get<1>(t),std::get<2>(t));
				}
			template <> void Wait<PTP_TIMER>(handle<PTP_TIMER> h,bool Cancel)
				{
				WaitForThreadpoolTimerCallbacks(h,Cancel);
				}

			// IO Stuff
			template <> handle<PTP_IO> CreateItem<PTP_IO,PTP_WIN32_IO_CALLBACK>(PTP_WIN32_IO_CALLBACK cb,PVOID opt,HANDLE hY)
				{
				handle<PTP_IO> a(CreateThreadpoolIo(hY,cb,opt,&env),AutoDestruct);
				return a;
				}
			template <> void RunItem<PTP_IO>(handle<PTP_IO> h,std::tuple<bool> t)
				{
				bool Cancel = std::get<0>(t);
				if (Cancel)
					CancelThreadpoolIo(h);
				else
					StartThreadpoolIo(h);
				}
			template <> void Wait<PTP_IO>(handle<PTP_IO> h,bool Cancel)
				{
				WaitForThreadpoolIoCallbacks(h,Cancel);
				}

			// Join functions, one for each option (AutoDestruct or not)
			template <bool Q = AutoDestruct>
			typename std::enable_if<Q,void>::type
			Join(bool Cancel = false)
				{
				if (pcg)
					{
					CloseThreadpoolCleanupGroupMembers(pcg,Cancel,0);
					pcg.Close();
					}
				}

			template <bool Q = AutoDestruct>
			typename std::enable_if<!Q,void>::type
				Join(bool Cancel = false,
				std::initializer_list<handle<PTP_WORK>> h1 = std::initializer_list<handle<PTP_WORK>>({}),
				std::initializer_list<handle<PTP_TIMER>> h2 = std::initializer_list<handle<PTP_TIMER>>({}),
				std::initializer_list<handle<PTP_WAIT>> h3 = std::initializer_list<handle<PTP_WAIT>>({}),
				std::initializer_list<handle<PTP_IO>> h4 = std::initializer_list<handle<PTP_IO>>({})
				)
				{
				for (auto a : h1)
					Wait<PTP_WORK>(a,Cancel);
				for (auto a : h2)
					Wait<PTP_TIMER>(a,Cancel);
				for (auto a : h3)
					Wait<PTP_WAIT>(a,Cancel);
				for (auto a : h4)
					Wait<PTP_IO>(a,Cancel);
				}

		};

	}
