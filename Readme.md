# Multithreading Tools for Windows


# tlock
A C++ library for Windows for efficient read/write locks with template generalization.

Based on my tlock article (https://www.codeproject.com/Articles/1186797/tlock-Any-Cplusplus-object-read-write-thread-safe)
Based on my RWMutex (https://www.codeproject.com/Articles/1053865/RWMutex-A-Shared-Exclusive-Recursive-Mutex)
And now, tlock2<> which uses shared_mutex, no Windows dependency.

Quick example:

```C++
tlock<vector<int>> s;
tlock2<vector<int>> s;
```

This allows you to use a vector<int> in a form of lockable object.

```C++
s.writelock([&](vector<int>& ss)
    {
    ss.push_back(100);
    ss.push_back(150); 
    ss.erase(ss.begin());
    // Safe operations, s is locked while in this function. s.readlock() would block.
    })
```


```C++
s.readlock([&](const vector<int>& ss)
    {
 
    });
```

Or you can use direct calls:

```C++
tlock<vector<int>> s;
std::thread t1([&]() { s->push_back(0); });
std::thread t2([&]() { s->push_back(1); });
std::thread t3([&]() { s->push_back(2); });
std::thread t4([&]() { s->push_back(3); });
std::thread t5([&]() { s->push_back(4); });
t1.join();t2.join(); t3.join(); t4.join(); t5.join();
```


The r() method is called when you want read-only access to the object. This is the default when operator -> is called on a const object. Many threads can call r() simultaneously.

The w() method is called when you want write access to the object. This is the default for operator -> if the object is not constant. w() blocks until all threads that are within r() return, and then does not allow any other thread to complete r() or w() until it returns.

The readlock() method is called when you want many operations in a locked read-only object, so it calls your function, passing a reference to the constant, locked object.

The writelock() method is called when you want many operations in a locked read-write object, so it calls your function, passing a reference to the locked object.

You can also use tlock upgrade function

```C++
s.rwlock([&](const vector<int>& vv, std::function<void(std::function<void(vector<int>&)>)> upgrfunc) 
{
	// vv read access
	upgrfunc([&](vector<int>& nn) 
	{
		// nn write access
		// function end downgrades
	});
});
```

You can also use my RWMUTEX functions

```C++
RWMUTEX m;
void foo1()
{
  RWMUTEXLOCKREAD lock(&m);
  // while in here, m is read-locked
}

void foo2()
{
  RWMUTEXLOCKWRITE lock(&m);
  // while in here, m is write-locked
}

void foo3()
{
  RWMUTEXLOCKREADWRITE lock(&m);
  // while in here, m is read-locked
  
  ...
  m.Upgrade(); // now m is write-locked
  ...
  m.Downgrade(); // m back read-locked
  
  // end of function, m released
}


```

# cow
A copy on write class that automatically duplicates an object when writing.

```C++
    struct FOO
    {
	    int a1 = 0;
	    int a2 = 0;
    };

	std::shared_ptr<FOO> x = std::make_shared<FOO>();
	// read only
	cow<FOO> c(x);
	const FOO& j1 = *c;
	cow<FOO> c2 = c;

	// Write
	c->a2 = 2; // 1 copy 
	c->a1 = 4; // 2 copies, but the previous vanishes (only 1 ref)

	c2.write([](std::shared_ptr<FOO> t) // 3rd copy, now there are 3 smart pointers with count 1
		{
			t->a1 = 10;
			t->a2 = 20;
		});


```
# Collaboration Tools
Welcome to my collaboration tools for Windows.

## Diff Lib
Article: https://www.codeproject.com/Articles/861419/DIFF-Your-IRdcLibrary-API-for-Remote-Differential

A library to implement incremental updates to data.

```C++
// Say that hX1 and hX2 are handles to an original file and a newer file
CComPtr<IRdcFileReader> fil1;
CComPtr<IRdcFileReader> fil2;
fil1.Attach(new DIFF::FileRdcFileReader(hX1));
fil2.Attach(new DIFF::FileRdcFileReader(hX2));

// Instantiate the library
DIFF::DIFF d;

// Create a signature for both files
DIFF::FileDiffWriter sig1(hS1);
DIFF::FileDiffWriter sig2(hS2);
d.GenerateSignature(fil1,sig1);
d.GenerateSignature(fil2,sig1);

// Generate diff between old and new file
CComPtr<IRdcFileReader> xsig1;
CComPtr<IRdcFileReader> xsig2;
sig1.GetReader(&xsig1);
sig2.GetReader(&xsig2);
DIFF::MemoryDiffWriter diff1_2_sc;
d.GenerateDiff(xsig1,xsig2,r2,diff1_2_sc));

 // Recreate the new file based on the old file and the diff
DIFF::FileDiffWriter reco1(hX3);
d.Reconstruct(r1,xdiff1,0,reco1);

```

## ThreadPool API
Article: https://www.codeproject.com/Articles/1012843/Win-Thread-Pools-and-Cplusplus-A-quick-wrapper



## Shared Memory
Article: https://www.codeproject.com/Articles/835818/Ultimate-Shared-Memory-A-flexible-class-for-sharin

```C++

// Process 1
usm<char> sm1(L"{4BF41A4E-ACF9-4E1D-A479-14DE9FF83BC2}", false, 1000, 2);
sm1.Initialize();

// Process 2
usm<char> sm2(L"{4BF41A4E-ACF9-4E1D-A479-14DE9FF83BC2}", false, 1000, 2);
sm2.Initialize();

// Process 1
sm1.WriteData("Hello", 5, 0, 0);

// Process 2
char r[10] = { 0 };
sm2.ReadData(r, 5, 0, 0);

// r must contain "Hello"
```

## FileSnap
Article: https://www.codeproject.com/Articles/1209648/FileSnap-A-Windows-library-for-differential-file-s

```C++

class FILESNAP
{
public:
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

    inline bool BuildMap(vector<FSITEM>& fsx);
    FILESNAP(const wchar_t* fi);
    inline bool SetSnap(size_t idx,int CommitForce = 0);
    inline bool Create(DWORD Access= GENERIC_READ | GENERIC_WRITE, DWORD Share = 0, 
          LPSECURITY_ATTRIBUTES Atrs = 0, DWORD Disp = CREATE_NEW, DWORD Flags = 0);
    inline unsigned long long Size();
    inline bool Finalize();
    inline bool Commit(size_t At,int Mode);
    inline HANDLE GetH();
    inline BOOL Write(const char* d, unsigned long long sz2);
    inline BOOL Read(char* d, unsigned long long sz2);
    inline unsigned long long SetFilePointer(unsigned long long s, DWORD m = FILE_BEGIN);
    inline BOOL SetEnd();
    inline void Close();
};


```

* You want to create a new file, you simply pass the name to the FILESNAP constructor.
* You call Create() with the CreateFile flag CREATE_NEW to create the file.
* You write to the file using Write().
* When you want to save the current version, you call Commit(0,0). For the first save, Commit() ignores the two parameters.
* You keep writing to the file using Write().
* The next Commit() call will save the current contents as a differential backup. If the Mode parameter is 0, then Commit() saves as a differential backup to the first commit always. If the Mode parameter is 1, then Commit() saves as a differential backup to the specified commit number as the At parameter. This allows you to either use differential or incremental backups.
* You keep writing, reading or else to the current handle using the Read(), Write(), SetEnd(), SetFilePointer(), Size() and GetH() function which returns the current HANDLE that you can use in other file functions.
* If you want to revert to a specific saving, use SetSnap. CommitForce can be 0, 1 or 2: 0 means that if the file exists already, the function fails, 1 means that if the file exists already, a commit is taken before reverting, 2 does not commit before reverting.
* After you have used SetSnap(), Read()/Write()/GetH() etc. operate on the reverted data.
* Close() will close the file (it does not commit), and delete all temporary files used by the class.
 
When you open an existing file, the class automatically calls SetSnap() on the last snapshot found in the file. You can call BuildMap() to return a FSITEM array of all snapshots included in the file.

If you build files with mostly differential backups (related to the first snapshot), then the file will be larger, but quicker to open because only one operation will be done (the comparison between the first and the opened snapshot).
If you build files with mostly incremental backups (related to the last snapshot), then the file will be smaller, but slower to open because all snapshots must be processed when a snapshot is loaded.

## Real time collaboration
Article: https://www.codeproject.com/Articles/1158754/Real-time-collaboration-A-quick-Cplusplus-Windows

Server Setup:

```C++
COLLAB::ANYAUTH auth;
COLLAB::SERVER s(&auth, 8765,true); // Port 8765, and true to use filesystem instead of in-memory docs
s.Start(); // Start the server
...
...
...
s.End(); // Ends the server when we want to close it
```

Client Setup:
```C++

//
class ON
    {
    public:
        virtual void Update(CLSID cid,const char* d,size_t sz) = 0;
    };

// Reconstruct from an ON update
void RecoFromDiff(const char* d, size_t sz, const char* e, size_t sze, vector<char>& o)
    {
    DIFFLIB::DIFF diff;
    DIFFLIB::MemoryRdcFileReader r1(e, sze);
    DIFFLIB::MemoryRdcFileReader diffi(d, sz);

    DIFFLIB::MemoryDiffWriter dw;
    diff.Reconstruct(&r1, &diffi, 0, dw);
    o = dw.p();
    }

COLLAB::ANYAUTH auth;
COLLAB::CLIENT c1(&auth);
MYON on; // some class that implements Update() of COLLAB::ON 
c1.AddOn(&on); // check below for ON class
c1.Connect("localhost",8765);
c1.Open(DOCUMENT_GUID); // If guid does not exist, server creates such a document
//
...
...
...
c1.Close(DOCUMENT_GUID);
...
...
...
c1.RemoveOn(&on);
c1.Disconnect();

// Put updates to the server
HRESULT Put(GUID g, const char* d,size_t sz);

```