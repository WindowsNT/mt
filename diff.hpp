// Diff Library
// Chourdakis Michael
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <process.h>
#include <commctrl.h>
#include <stdio.h>
#include <shellapi.h>
#include <string>
#include <memory>
#include <vector>
#include <tchar.h>
#include <msrdc.h>
#include <comdef.h>
#include <atlbase.h>

#ifndef _DIFF_H
#define _DIFF_H

#pragma warning(disable: 4456)
#pragma warning(disable: 4458)
namespace DIFFLIB
	{
	using namespace std;

	// Class to read from memory
	class MemoryRdcFileReader : public IRdcFileReader
		{
		private:

			int refNum;
			const char* d;
			unsigned long long datasz;
			unsigned long long ptr;

		public:

			MemoryRdcFileReader(const char* dd,unsigned long long size)
				{
				d = dd;
				ptr = 0;
				datasz = size;
				refNum = 1;
				}

			unsigned long long size()
				{
				return datasz;
				}


			// IUnknown
			HRESULT WINAPI QueryInterface(REFIID riid,void ** object)
				{
				if (!object)
					return E_POINTER;
				*object = 0;
				if (IsEqualIID(riid, IID_IUnknown)) *object = this;
				if (IsEqualIID(riid, __uuidof(IRdcFileReader))) *object = this;
				if (*object)
					{
					IUnknown* u = (IUnknown*)*object;
					u->AddRef();
					}
				return *object ? S_OK : E_NOINTERFACE;
				}

			DWORD WINAPI AddRef()
				{
				return ++refNum;
				}

			DWORD WINAPI Release()
				{
				if (refNum == 1)
					{
					refNum--;
					delete this;
					return 0;
					}
				return --refNum;
				}


			virtual HRESULT STDMETHODCALLTYPE GetFileSize(ULONGLONG *fileSize)
				{
				if (!fileSize)
					return E_POINTER;
				*fileSize = datasz;
				return S_OK;
				}


			virtual HRESULT STDMETHODCALLTYPE Read(ULONGLONG offsetFileStart,ULONG bytesToRead,ULONG *bytesActuallyRead,BYTE *buffer,BOOL *eof)
				{
				ptr = offsetFileStart;
				unsigned long long sz = datasz;
				if (offsetFileStart >= sz)
					{
					*bytesActuallyRead = 0;
					*eof = TRUE;
					return S_OK;
					}
				unsigned long long available = sz - offsetFileStart;
				if (available >= bytesToRead)
					{
					*eof = FALSE;
					memcpy(buffer, d + offsetFileStart, bytesToRead);
					*bytesActuallyRead = bytesToRead;
					ptr += bytesToRead;
					}
				else
					{
					*eof = TRUE;
					memcpy(buffer, d + offsetFileStart, (size_t)available);
					*bytesActuallyRead = (unsigned long)available;
					ptr += available;
					}
				return S_OK;
				}

			virtual HRESULT STDMETHODCALLTYPE GetFilePosition(ULONGLONG *offsetFromStart)
				{
				if (!offsetFromStart)
					return E_POINTER;
				*offsetFromStart = ptr;
				return S_OK;
				}




		};

	// Class to read from file
	class FileRdcFileReader : public IRdcFileReader
		{
		private:

			int refNum;
			HANDLE hF = 0;
			void*d;

		public:

			FileRdcFileReader(HANDLE h)
				{
				hF = h;
				refNum = 1;
				}

			virtual unsigned long long size()
				{
				LARGE_INTEGER li;
				GetFileSizeEx(hF, &li);
				return li.QuadPart;
				}


			// IUnknown
			HRESULT WINAPI QueryInterface(REFIID riid,void ** object)
				{
				if (!object)
					return E_POINTER;
				*object = 0;
				if (IsEqualIID(riid, IID_IUnknown)) *object = this;
				if (IsEqualIID(riid, __uuidof(IRdcFileReader))) *object = this;
				if (*object)
					{
					IUnknown* u = (IUnknown*)*object;
					u->AddRef();
					}
				return *object ? S_OK : E_NOINTERFACE;
				}


			DWORD WINAPI AddRef()
				{
				return ++refNum;
				}

			DWORD WINAPI Release()
				{
				if (refNum == 1)
					{
					refNum--;
					delete this;
					return 0;
					}
				return --refNum;
				}

			virtual HRESULT STDMETHODCALLTYPE GetFileSize(ULONGLONG *fileSize)
				{
				if (!fileSize)
					return E_POINTER;
				*fileSize = size();
				return S_OK;
				}

			virtual HRESULT STDMETHODCALLTYPE Read(ULONGLONG offsetFileStart,ULONG bytesToRead,ULONG *bytesActuallyRead,BYTE *buffer,BOOL *eof)
				{
				LARGE_INTEGER li;
				li.QuadPart = offsetFileStart;

				unsigned long long sz = size();
				if (offsetFileStart >= sz)
					{
					*bytesActuallyRead = 0;
					*eof = TRUE;
					return S_OK;
					}
				if (!SetFilePointerEx(hF, li, 0, FILE_BEGIN))
					return E_FAIL;

				unsigned long long available = sz - offsetFileStart;
				if (available >= bytesToRead)
					{
					*eof = FALSE;
					DWORD ba = 0;
					ReadFile(hF, buffer, bytesToRead, &ba, 0);
					if (bytesActuallyRead)
						*bytesActuallyRead = ba;
					}
				else
					{
					*eof = TRUE;
					DWORD ba = 0;
					ReadFile(hF, buffer, bytesToRead, &ba, 0);
					if (bytesActuallyRead)
						*bytesActuallyRead = (ULONG)available;
					}
				return S_OK;
				}

			virtual HRESULT STDMETHODCALLTYPE GetFilePosition(ULONGLONG *offsetFromStart)
				{
				if (!offsetFromStart)
					return E_POINTER;
				LARGE_INTEGER li1 = { 0 };
				LARGE_INTEGER li2;
				SetFilePointerEx(hF, li1, &li2, FILE_CURRENT);
				*offsetFromStart = li2.QuadPart;
				return S_OK;
				}



		};


	// Class to read from file
	class PartialFileRdcFileReader : public IRdcFileReader
		{
		private:

			int refNum;
			unsigned long long from = 0;
			unsigned long long sz = 0;
			HANDLE hF = 0;
			void*d;

		public:

			PartialFileRdcFileReader(HANDLE h,unsigned long long frm, unsigned long long siz)
				{
				hF = h;
				from = frm;
				sz = siz;
				refNum = 1;
				}

			virtual unsigned long long size()
				{
				return sz;
				}


			// IUnknown
			HRESULT WINAPI QueryInterface(REFIID riid,void ** object)
				{
				if (!object)
					return E_POINTER;
				*object = 0;
				if (IsEqualIID(riid, IID_IUnknown)) *object = this;
				if (IsEqualIID(riid, __uuidof(IRdcFileReader))) *object = this;
				if (*object)
					{
					IUnknown* u = (IUnknown*)*object;
					u->AddRef();
					}
				return *object ? S_OK : E_NOINTERFACE;
				}


			DWORD WINAPI AddRef()
				{
				return ++refNum;
				}

			DWORD WINAPI Release()
				{
				if (refNum == 1)
					{
					refNum--;
					delete this;
					return 0;
					}
				return --refNum;
				}

			virtual HRESULT STDMETHODCALLTYPE GetFileSize(ULONGLONG *fileSize)
				{
				if (!fileSize)
					return E_POINTER;
				*fileSize = size();
				return S_OK;
				}

			virtual HRESULT STDMETHODCALLTYPE Read(ULONGLONG offsetFileStart,ULONG bytesToRead,ULONG *bytesActuallyRead,BYTE *buffer,BOOL *eof)
				{
				LARGE_INTEGER li;
				li.QuadPart = offsetFileStart + from;

				unsigned long long sz = size();
				if (offsetFileStart >= sz)
					{
					*bytesActuallyRead = 0;
					*eof = TRUE;
					return S_OK;
					}
				if (!SetFilePointerEx(hF, li, 0, FILE_BEGIN))
					return E_FAIL;

				unsigned long long available = sz - offsetFileStart;
				if (available >= bytesToRead)
					{
					*eof = FALSE;
					DWORD ba = 0;
					ReadFile(hF, buffer, bytesToRead, &ba, 0);
					if (bytesActuallyRead)
						*bytesActuallyRead = ba;
					}
				else
					{
					*eof = TRUE;
					DWORD ba = 0;
					ReadFile(hF, buffer, bytesToRead, &ba, 0);
					if (bytesActuallyRead)
						*bytesActuallyRead = (ULONG)available;
					}
				return S_OK;
				}

			virtual HRESULT STDMETHODCALLTYPE GetFilePosition(ULONGLONG *offsetFromStart)
				{
				if (!offsetFromStart)
					return E_POINTER;
				LARGE_INTEGER li1 = { 0 };
				LARGE_INTEGER li2;
				SetFilePointerEx(hF, li1, &li2, FILE_CURRENT);
				*offsetFromStart = li2.QuadPart - from;
				return S_OK;
				}



		};




	// Class to read from IStream
	class StreamRdcFileReader : public IRdcFileReader
		{
		private:

			int refNum;
			CComPtr<IStream> str;

		public:

			StreamRdcFileReader(CComPtr<IStream> s)
				{
				str = s;
				refNum = 1;
				}

			// IUnknown
			HRESULT WINAPI QueryInterface(REFIID riid,void ** object)
				{
				if (!object)
					return E_POINTER;
				*object = 0;
				if (IsEqualIID(riid,IID_IUnknown)) *object = this;
				if (IsEqualIID(riid,__uuidof(IRdcFileReader))) *object = this;
				if (*object)
					{
					IUnknown* u = (IUnknown*)*object;
					u->AddRef();
					}
				return *object ? S_OK : E_NOINTERFACE;
				}


			DWORD WINAPI AddRef()
				{
				return ++refNum;
				}

			DWORD WINAPI Release()
				{
				if (refNum == 1)
					{
					refNum--;
					delete this;
					return 0;
					}
				return --refNum;
				}


			virtual HRESULT STDMETHODCALLTYPE GetFileSize(ULONGLONG *fileSize)
				{
				if (!fileSize || !str)
					return E_POINTER;
				STATSTG ss = { 0 };
				HRESULT hr = str->Stat(&ss, STATFLAG_NONAME);
				if (FAILED(hr))
					return E_FAIL;
				*fileSize = ss.cbSize.QuadPart;
				return S_OK;
				}

			virtual HRESULT STDMETHODCALLTYPE Read(ULONGLONG offsetFileStart,ULONG bytesToRead,ULONG *bytesActuallyRead,BYTE *buffer,BOOL *eof)
				{
				if (!str)
					return E_POINTER;

				// Seek
				LARGE_INTEGER s;
				s.QuadPart = offsetFileStart;
				if (FAILED(str->Seek(s, STREAM_SEEK_SET, 0)))
					return E_FAIL;

				HRESULT hr = str->Read(buffer, bytesToRead, bytesActuallyRead);
				*eof = FALSE;
				if (*bytesActuallyRead != bytesToRead)
					*eof = TRUE;
				return hr;
				}

			virtual HRESULT STDMETHODCALLTYPE GetFilePosition(ULONGLONG *offsetFromStart)
				{
				if (!offsetFromStart || !str)
					return E_POINTER;
				LARGE_INTEGER li = { 0 };
				ULARGE_INTEGER ul = { 0 };
				if (FAILED(str->Seek(li, STREAM_SEEK_CUR, &ul)))
					return E_FAIL;
				*offsetFromStart = ul.QuadPart;
				return S_OK;
				}





		};


	// Class to get a RdcFileReader from a Structured OLE file
	class InputStructuredFile
		{

		};

	// Main writer that all our functions want
	class DIFFWRITER
		{
		public:

		virtual HRESULT Write(const char*,ULONG sz) = 0;
		virtual CComPtr<IRdcFileReader> GetReader(int idx = 0) = 0;
		virtual HRESULT WriteMulti(vector<RdcBufferPointer*>& outs) = 0;
		};

	// Class to write to file
	class FileDiffWriter : public DIFFWRITER
		{
		private:

			HANDLE hF = 0;
		public:
			FileDiffWriter(HANDLE h)
				{
				hF = h;
				}

			virtual HRESULT Write(const char* d,ULONG sz)
				{
				DWORD A = 0;
				if (!WriteFile(hF, d, sz, &A, 0))
					return E_FAIL;
				if (sz != A)
					return E_FAIL;
				return S_OK;
				}


			virtual CComPtr<IRdcFileReader> GetReader(int idx = 0)
				{
				CComPtr<IRdcFileReader> r;
				r.Attach(new FileRdcFileReader(hF));
				return r;
				}

			virtual HRESULT WriteMulti(vector<RdcBufferPointer*>& outs)
				{
				return E_NOTIMPL;
				}

		};

	// Class to write to IStream
	class StreamWriter : public DIFFWRITER
		{
		private:
			CComPtr<IStream> str = 0;
		public:
			StreamWriter(CComPtr<IStream> h)
				{
				str = h;
				}

			virtual HRESULT Write(const char* d,ULONG sz)
				{
				if (!str)
					return E_FAIL;
				ULONG r = 0;
				if (FAILED(str->Write(d, sz, &r)))
					return E_FAIL;
				if (r != sz)
					return E_FAIL;
				return S_OK;
				}

			virtual CComPtr<IRdcFileReader> GetReader(int idx = 0)
				{
				if (!str)
					return 0;
				CComPtr<IStream> s2;
				str->Clone(&s2);
				if (!s2)
					return 0;

				LARGE_INTEGER li = { 0 };
				s2->Seek(li, STREAM_SEEK_SET, 0);
				CComPtr<IRdcFileReader> streamreader;
				streamreader.Attach(new StreamRdcFileReader(s2));
				return streamreader;
				}

			virtual HRESULT WriteMulti(vector<RdcBufferPointer*>& outs)
				{
				return E_NOTIMPL;
				}

		};


	// Class to write multiple depth signatures to structured file 
	class MultipleLevelFileDiffWriter : public DIFFWRITER
		{
		private:

			HRESULT hr = S_OK;
			CComPtr<IStorage> pStorage;
			wstring fil;

		public:
			MultipleLevelFileDiffWriter(const wchar_t* f)
				{
				hr = StgCreateStorageEx(f, STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_CREATE, STGFMT_STORAGE, 0, 0, 0, __uuidof(IStorage), (void**)&pStorage);
				if (FAILED(hr))
					return;
				}
			virtual HRESULT Write(const char* d,ULONG sz)
				{
				return E_NOTIMPL;
				}

			virtual CComPtr<IRdcFileReader> GetReader(int idx = 0)
				{
				hr = 0;
				if (!pStorage)
					return 0;
				TCHAR xu[100] = { 0 };
				swprintf_s(xu, 100, L"%u", idx);
				CComPtr<IStream> str;
				pStorage->OpenStream(xu, 0, STGM_SHARE_EXCLUSIVE | STGM_READWRITE, 0, &str);
				if (!str)
					return 0;

				CComPtr<IRdcFileReader> streamreader;
				streamreader.Attach(new StreamRdcFileReader(str));
				return streamreader;
				}

			virtual HRESULT WriteMulti(vector<RdcBufferPointer*>& outs)
				{
				hr = 0;
				if (!pStorage)
					return E_FAIL;

				// These are the levels of storage
				for (unsigned int i = 0; i < outs.size(); i++)
					{
					auto& p = outs[i];
					TCHAR xu[100] = { 0 };
					swprintf_s(xu, 100, L"%u", i);
					CComPtr<IStream> str;
					hr = pStorage->OpenStream(xu, 0, STGM_SHARE_EXCLUSIVE | STGM_READWRITE, 0, &str);
					if (!str)
						// Create it...
						hr = pStorage->CreateStream(xu, STGM_SHARE_EXCLUSIVE | STGM_READWRITE, 0, 0, &str);
					if (!str)
						return E_FAIL;

					// Go to end of this stream
					LARGE_INTEGER li = { 0 };
					str->Seek(li, STREAM_SEEK_END, 0);

					ULONG cb = 0;
					hr = str->Write(p->m_Data, p->m_Used, &cb);
					if (cb != p->m_Used)
						return E_FAIL;
					}
				return S_OK;
				}


		};

	// Class to write to memory
	class MemoryDiffWriter : public DIFFWRITER
		{
		private:
			vector<char> d;

		public:

			vector<char>& p()
				{
				return d;
				}

			size_t sz()
				{
				return d.size();
				}

			HRESULT Write(const char* dd,ULONG sz)
				{
				if (!dd)
					return E_INVALIDARG;
				size_t c = d.size();
				d.resize(c + sz);
				memcpy(d.data() + c, dd, sz);
				return S_OK;
				}


			virtual CComPtr<IRdcFileReader> GetReader(int idx = 0)
				{
				CComPtr<IRdcFileReader> rdr;
				rdr.Attach(new MemoryRdcFileReader((const char*)d.data(), d.size()));
				return rdr;
				}

			virtual HRESULT WriteMulti(vector<RdcBufferPointer*>& outs)
				{
				return E_NOTIMPL;
				}


		};


	// Main class
	class DIFF
		{
		private:

			HRESULT hr;
			CComPtr<IRdcLibrary> pRdcLibrary = 0;

		public:

			CComPtr<IRdcLibrary> GetLib() { return pRdcLibrary; }
			DIFF()
				{
				// Load the RdcLibrary
				hr = pRdcLibrary.CoCreateInstance(__uuidof(RdcLibrary));
				if (FAILED(hr))
				{
					MessageBox(0, L"DiffLib not available", 0, 0);
					DebugBreak();
					throw(hr);
				}
				}



			HRESULT GenerateSignature(IRdcFileReader* r1,DIFFWRITER& sig)
				{
				HRESULT hr = 0;
				if (!r1)
					return E_INVALIDARG;


				// Create Generator Parameters
				CComPtr<IRdcGeneratorParameters> params = 0;
				hr = pRdcLibrary->CreateGeneratorParameters(RDCGENTYPE_FilterMax, MSRDC_MINIMUM_DEPTH, &params);
				if (FAILED(hr)) return hr;
				CComPtr<IRdcGenerator> gen = 0;
				IRdcGeneratorParameters* p2 = params; // Guess the next function needs a raw COM interface :)
				hr = pRdcLibrary->CreateGenerator(MSRDC_MINIMUM_DEPTH, &p2, &gen);
				if (FAILED(hr)) return hr;


				//		CComQIPtr<IRdcSimilarityGenerator> sim;
				//		sim = gen;
				//		hr = sim->EnableSimilarity();


				// Generate the signatures
				const ULONGLONG sz = 10000;
				char buff[sz] = { 0 };
				ULONGLONG read = 0;
				vector<char> Signature;
				for (;;)
					{
					ULONGLONG fs = 0;
					r1->GetFileSize(&fs);
					ULONGLONG cur = fs - read;
					bool EndOfInput = true;
					if (cur > sz)
						{
						cur = sz;
						EndOfInput = false;
						}
					BOOL EndOfOutput = 0;
					RDC_ErrorCode rerr;
					RdcBufferPointer rbp;
					rbp.m_Size = (ULONG)cur;
					rbp.m_Data = (BYTE*)buff;
					BOOL eof = 0;
					ULONG actual = 0;
					r1->Read(read, (ULONG)cur, &actual, (BYTE*)buff, &eof);
					if (actual != cur)
						return E_FAIL;

					rbp.m_Used = 0;
					vector<char> OutputBuffer(sz);
					RdcBufferPointer outx = { 0 };
					outx.m_Data = (BYTE*)OutputBuffer.data();
					outx.m_Size = (ULONG)sz;
					RdcBufferPointer* outputPointers = 0;
					outputPointers = &outx;

					hr = gen->Process(EndOfInput, &EndOfOutput, &rbp, MSRDC_MINIMUM_DEPTH, &outputPointers, &rerr);
					if (FAILED(hr))
						return hr;
					read += rbp.m_Used;

					// Append the siganture
					if (outx.m_Used)
						{
						if (FAILED(sig.Write((const char*)outx.m_Data, outx.m_Used)))
							return E_FAIL;
						}


					if (EndOfOutput)
						break;
					}

				//		CComQIPtr<ISimilarity> simi;
				//		simi.CoCreateInstance(__uuidof(ISimilarity));

				return S_OK;
				}


			HRESULT GenerateMultiDepthSignature(IRdcFileReader* r1,ULONG de,DIFFWRITER& sig)
				{
				HRESULT hr = 0;
				if (!r1)
					return E_INVALIDARG;

				ULONG fd = de - MSRDC_MINIMUM_DEPTH + 1;

				// Create Generator Parameters
				vector<CComPtr<IRdcGeneratorParameters>> params;

				for (ULONG d = 0; d < de; d++)
					{
					CComPtr<IRdcGeneratorParameters> p2;
					hr = pRdcLibrary->CreateGeneratorParameters(RDCGENTYPE_FilterMax, d + MSRDC_MINIMUM_DEPTH, &p2);
					if (FAILED(hr))
						return hr;
					params.push_back(p2);
					}


				CComPtr<IRdcGenerator> gen = 0;
				vector<IRdcGeneratorParameters*> p2(fd);
				for (ULONG d = 0; d < de; d++)
					p2[d] = params[d].operator->();

				hr = pRdcLibrary->CreateGenerator(de, p2.data(), &gen);
				if (FAILED(hr)) return hr;

				// Generate the signatures
				const ULONGLONG sz = 10000;
				char buff[sz] = { 0 };
				ULONGLONG read = 0;
				vector<char> Signature;
				for (;;)
					{
					ULONGLONG fs = 0;
					r1->GetFileSize(&fs);
					ULONGLONG cur = fs - read;
					bool EndOfInput = true;
					if (cur > sz)
						{
						cur = sz;
						EndOfInput = false;
						}
					BOOL EndOfOutput = 0;
					RDC_ErrorCode rerr;
					RdcBufferPointer rbp;
					rbp.m_Size = (ULONG)cur;
					rbp.m_Data = (BYTE*)buff;
					BOOL eof = 0;
					ULONG actual = 0;
					r1->Read(read, (ULONG)cur, &actual, (BYTE*)buff, &eof);
					if (actual != cur)
						return E_FAIL;
					rbp.m_Used = 0;


					vector<vector<char>> OutputBuffer(fd);
					vector<RdcBufferPointer> outputPointers(fd);
					for (ULONG d = 0; d < de; d++)
						{
						OutputBuffer[d].resize(sz);
						outputPointers[d].m_Data = (BYTE*)OutputBuffer[d].data();
						outputPointers[d].m_Size = (ULONG)sz;
						}
					vector<RdcBufferPointer*> outs(fd);
					for (ULONG d = 0; d < de; d++)
						outs[d] = &outputPointers[d];

					hr = gen->Process(EndOfInput, &EndOfOutput, &rbp, de, outs.data(), &rerr);
					if (FAILED(hr))
						return hr;
					read += rbp.m_Used;

					if (FAILED(sig.WriteMulti(outs)))
						return E_FAIL;



					if (EndOfOutput)
						break;
					}

				return S_OK;
				}

//			HRESULT GenerateMultipleLevelDiff(IRdcFileReader* first_sig,IRdcFileReader* second_sig,IRdcFileReader* r2,DIFFWRITER& diff)


			HRESULT GenerateDiff(IRdcFileReader* first_sig,IRdcFileReader* second_sig,IRdcFileReader* r2,DIFFWRITER& diff)
				{
				// Create the signature reader

				ULONGLONG firstsz = 0;
				first_sig->GetFileSize(&firstsz);
				CComPtr<IRdcComparator> pRdcComparator = 0;
				hr = pRdcLibrary->CreateComparator(first_sig, MSRDC_DEFAULT_COMPAREBUFFER, &pRdcComparator);
				if (FAILED(hr))
					return hr;

				const ULONGLONG sz = 10000;
				vector<char> buff(sz);
				ULONGLONG read = 0;
				bool EndOfInput = false;
				for (;;)
					{
					ULONGLONG fs = 0;
					second_sig->GetFileSize(&fs);
					ULONGLONG cur = fs - read;
					if (cur == 0)
						break;
					if (cur > sz)
						cur = sz;
					else
						EndOfInput = true;
					BOOL EndOfOutput = 0;
					RDC_ErrorCode rerr;

					RdcBufferPointer rbp = { 0 };
					rbp.m_Size = (ULONG)cur;
					// (second_sig.data() + read);
					ULONG a = 0;
					BOOL eof = 0;


					if (FAILED(second_sig->Read(read, (ULONG)cur, &a, (BYTE*)buff.data(), &eof)))
						return E_FAIL;
					if (a != cur)
						return E_FAIL;
					rbp.m_Data = (BYTE*)buff.data();
					rbp.m_Used = 0;

					int nArr = 16384;
					vector<RdcNeed> needsArray(nArr);
					RdcNeedPointer rnp;
					rnp.m_Data = needsArray.data();
					rnp.m_Size = nArr;
					rnp.m_Used = 0;

					EndOfOutput = 0;
					for (;;)
						{
						hr = pRdcComparator->Process(EndOfInput, &EndOfOutput, &rbp, &rnp, &rerr);
						if (FAILED(hr))
							return hr;
						read += rbp.m_Used;

						if (rnp.m_Used == 0)
							break;

						for (unsigned int ir = 0; ir < rnp.m_Used; ir++)
							{
							RdcNeed& rn = needsArray[ir];
							// copy it to diff
							if (FAILED(diff.Write((const char*)&rn, sizeof(rn))))
								return E_FAIL;

							if (rn.m_BlockType == RDCNEED_SOURCE)
								{
								if (r2)
									{
									std::unique_ptr<char> bu(new char[(size_t)rn.m_BlockLength]);
									BOOL eof = 0;
									ULONG a = 0;
									if (FAILED(r2->Read(rn.m_FileOffset, (ULONG)rn.m_BlockLength, &a, (BYTE*)bu.get(), &eof)))
										return E_FAIL;
									if (rn.m_BlockLength != a)
										return E_FAIL;
									if (FAILED(diff.Write(bu.get(), (ULONG)rn.m_BlockLength)))
										return E_FAIL;
									}
								}
							}

						if (EndOfOutput)
							break;
						EndOfOutput = 0;
						rnp.m_Data = needsArray.data();
						rnp.m_Size = nArr;
						rnp.m_Used = 0;
						cur = 0;
						//				rbp.m_Size = 0;
						//				rbp.m_Data = 0;
						rbp.m_Size = rbp.m_Size - rbp.m_Used;
						rbp.m_Used = 0;
						}



					}

				return S_OK;
				}

			HRESULT Reconstruct(IRdcFileReader* r1,IRdcFileReader* diff,IRdcFileReader* r2,DIFFWRITER& reco)
				{
				if (!r1)
					return E_INVALIDARG;

				unsigned long long diffsize = 0;
				diff->GetFileSize(&diffsize);
				for (unsigned long long ii = 0; ii < diffsize;)
					{
					//Read RdcNeed
					RdcNeed rn;
					rn.m_BlockLength = 0;
					rn.m_BlockType = RDCNEED_SOURCE;
					rn.m_FileOffset = 0;
					ULONG act = 0;
					BOOL eof = 0;
					if (FAILED(diff->Read(ii, sizeof(rn), &act, (BYTE*)&rn, &eof)))
						return E_FAIL;
					if (act != sizeof(rn))
						return E_FAIL;


					if (rn.m_BlockType == RDCNEED_SEED)
						{
						BOOL eof = 0;
						ULONG a = 0;
						std::unique_ptr<char> bu(new char[(size_t)rn.m_BlockLength]);

						if (FAILED(r1->Read(rn.m_FileOffset, (ULONG)rn.m_BlockLength, &a, (BYTE*)bu.get(), &eof)))
							return E_FAIL;
						if (rn.m_BlockLength != a)
							return E_FAIL;
						if (FAILED(reco.Write(bu.get(), (ULONG)rn.m_BlockLength)))
							return E_FAIL;
						ii += sizeof(RdcNeed);
						continue;
						}

					if (rn.m_BlockType == RDCNEED_SOURCE)
						{
						// Copy from remote
						ii += sizeof(RdcNeed);
						if (!r2)
							{
							BOOL eof = 0;
							ULONG a = 0;
							std::unique_ptr<char> bu(new char[(size_t)rn.m_BlockLength]);

							if (FAILED(diff->Read(ii, (ULONG)rn.m_BlockLength, &a, (BYTE*)bu.get(), &eof)))
								return E_FAIL;
							if (rn.m_BlockLength != a)
								return E_FAIL;
							if (FAILED(reco.Write(bu.get(), (ULONG)rn.m_BlockLength)))
								return E_FAIL;
							ii += rn.m_BlockLength;
							}
						else
							{
							std::unique_ptr<char> bu(new char[(size_t)rn.m_BlockLength]);
							DWORD a = 0;
							BOOL eof = 0;
							if (FAILED(r2->Read(rn.m_FileOffset, (ULONG)rn.m_BlockLength, &a, (BYTE*)bu.get(), &eof)))
								return E_FAIL;
							if (rn.m_BlockLength != a)
								return E_FAIL;
							if (FAILED(reco.Write(bu.get(), (ULONG)rn.m_BlockLength)))
								return E_FAIL;
							}
						}
					}


				return S_OK;
				}


		};


	};


#endif // _DIFF_H