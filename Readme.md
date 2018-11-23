# tlock
A C++ library for Windows for efficient read/write locks with template generalization.

Based on my tlock article (https://www.codeproject.com/Articles/1186797/tlock-Any-Cplusplus-object-read-write-thread-safe)
Based on my RWMutex (https://www.codeproject.com/Articles/1053865/RWMutex-A-Shared-Exclusive-Recursive-Mutex)

Quick example:

```C++
tlock<vector<int>> s;
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

