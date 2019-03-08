#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <process.h>
#include <richedit.h>
#include <commctrl.h>
#include <stdio.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <tchar.h>
#include <sstream>
#include <memory>
#include <thread>
#include <mutex>
#include <map>
#include <unordered_map>
#include <atlbase.h>
#include <functional>

#include "diff.hpp"
#include "usm.hpp"
#include "filesnap.hpp"
#include "collab.hpp"


#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"comctl32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


// -------------------- Diff Lib
void DiffTest()
{
	using namespace DIFFLIB;
	DIFF d;

	DeleteFile(L"3.rar");

	// Original 1.rar
	HANDLE h1 = CreateFile(L"1.rar", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	FileRdcFileReader r1(h1);
	
	// Updated 2.rar
	HANDLE h2 = CreateFile(L"2.rar", GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	FileRdcFileReader r2(h2);

	// Signatures
	MemoryDiffWriter s1,s2;
	d.GenerateSignature(&r1, s1);
	d.GenerateSignature(&r2, s2);

	// Diff
	MemoryDiffWriter di;
	d.GenerateDiff(s1.GetReader(), s2.GetReader(), &r2, di);

	// Patch the first file to create the second without redownloading the entire data
	HANDLE h3 = CreateFile(L"3.rar", GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	FileDiffWriter result(h3);
	d.Reconstruct(&r1, di.GetReader(), 0, result);

	CloseHandle(h1);
	CloseHandle(h2);
	CloseHandle(h3);

	// 2.rar must be equal to 3.rar (which was created from 1.rar and the diff)
}

// -------------------- USM Lib

void USMTest()
{
	using namespace USMLIBRARY;
	usm<char> sm1(L"{4BF41A4E-ACF9-4E1D-A479-14DE9FF83BC2}", false, 1000, 2);
	sm1.Initialize();

	usm<char> sm2(L"{4BF41A4E-ACF9-4E1D-A479-14DE9FF83BC2}", false, 1000, 2);
	sm2.Initialize();


	sm1.WriteData("Hello", 5, 0, 0);

	char r[10] = { 0 };
	sm2.ReadData(r, 5, 0, 0);

	// r must contain "Hello"
}


// -------------------- FSnap Lib

int iw = 0;
void New(const wchar_t* f = 0)
{
	using namespace FILESNAPLIB;
	HINSTANCE h = GetModuleHandle(0);
	HWND MainWindow = CreateWindowEx(0,
		_T("CLASS"),
		L"FSnap Test",
		WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS |
		WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		0, LoadMenu(h, L"MENU_2"), h, 0);

	OPENFILENAME of = { 0 };
	of.lStructSize = sizeof(of);
	of.hwndOwner = MainWindow;
	of.lpstrFilter = L"*.stxt\0*.stxt\0\0";
	wchar_t fnx[1000] = { 0 };
	of.lpstrFile = fnx;
	of.nMaxFile = 10000;
	of.lpstrDefExt = L"stxt";
	of.Flags = OFN_PATHMUSTEXIST | OFN_CREATEPROMPT | OFN_EXPLORER;
	iw++;
	ShowWindow(MainWindow, SW_SHOW);
	if (f)
		wcscpy_s(fnx, 1000, f);
	else
	{
		if (!GetOpenFileName(&of))
		{
			DestroyWindow(MainWindow);
			return;
		}
	}
	auto fs = new FILESNAP(fnx);
	fs->Create(GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_ALWAYS);

	SetWindowLongPtr(MainWindow, GWLP_USERDATA, (LONG_PTR)fs);
	SendMessage(MainWindow, WM_COMMAND, 198, 0);


}

int PickCommit(HWND hh, FILESNAPLIB::FILESNAP* fs)
{
	using namespace FILESNAPLIB;
	using namespace std;

	vector<FILESNAP::FSITEM> its;
	fs->BuildMap(its);
	auto hX = CreatePopupMenu();
	for (size_t i = 0; i < its.size(); i++)
	{
		auto& a = its[i];
		wchar_t t[100];
		wstring tx;
		swprintf_s(t, 100, L"%llu", a.i);
		tx += t;
		swprintf_s(t, 100, L" - %llu bytes", a.sz);
		tx += t;
		swprintf_s(t, 100, L" - (-> %llu)", a.DiffAt);
		tx += t;
		AppendMenu(hX, MF_STRING, i + 1, tx.c_str());
	}
	POINT p4;
	GetCursorPos(&p4);
	int tcmd = TrackPopupMenu(hX, TPM_RIGHTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, p4.x, p4.y, 0, hh, 0);
	DestroyMenu(hX);
	return tcmd;
}

LRESULT CALLBACK Main_DP(HWND hh, UINT mm, WPARAM ww, LPARAM ll)
{
	using namespace FILESNAPLIB;
	using namespace std;
	FILESNAP* fs = (FILESNAP*)GetWindowLongPtr(hh, GWLP_USERDATA);
	HWND hE = GetDlgItem(hh, 901);
	switch (mm)
	{
	case WM_CREATE:
	{
		hE = CreateWindowEx(0, L"RichEdit20w", L"", ES_AUTOHSCROLL | ES_AUTOVSCROLL | WS_HSCROLL | WS_VSCROLL | WS_CHILD | WS_VISIBLE | ES_SAVESEL | ES_MULTILINE | ES_WANTRETURN, 0, 0, 0, 0, hh, (HMENU)901, 0, 0);
		SendMessage(hh, WM_SIZE, 0, 0);
		break;
	}

	case WM_SIZE:
	{
		RECT rc;
		GetClientRect(hh, &rc);
		SetWindowPos(hE, 0, 0, 0, rc.right, rc.bottom, SWP_SHOWWINDOW);
		return 0;
	}

	case WM_COMMAND:
	{
		int LW = LOWORD(ww);
		UNREFERENCED_PARAMETER(LW);

		if (LW == 198) // Revert
		{
			fs->SetFilePointer(0, FILE_BEGIN);
			auto ne = fs->Size();
			vector<wchar_t> n((size_t)ne + 1);
			fs->Read((char*)n.data(), ne * 2);
			SetWindowText(hE, n.data());

		}

		if (LW == 192)
		{
			fs->Finalize();
		}

		if (LW == 105 || LW == 106)
		{
			// Write all
			auto ne = GetWindowTextLength(hE);
			vector<wchar_t> n(ne + 10);
			GetWindowText(hE, n.data(), ne + 1);
			fs->SetFilePointer(0, FILE_BEGIN);
			fs->SetEnd();
			fs->Write((char*)n.data(), ne * 2);
			if (LW == 105)
				fs->Commit(0, 0);
			if (LW == 106)
			{
				// Show menu with items to pick the commit
				int tcmd = PickCommit(hh, fs);
				if (tcmd == 0)
					return 0;
				fs->Commit(tcmd - 1, 1);
			}
		}

		if (LW == 197)
		{
			int tcmd = PickCommit(hh, fs);
			if (tcmd == 0)
				return 0;
			fs->SetSnap(tcmd - 1, 2);
			fs->SetFilePointer(0, FILE_BEGIN);
			auto ne = fs->Size();
			vector<wchar_t> n((size_t)ne + 1);
			fs->Read((char*)n.data(), ne * 2);
			SetWindowText(hE, n.data());

		}


		if (LW == 101)
			New();
		if (LW == 102)
		{
			OPENFILENAME of = { 0 };
			of.lStructSize = sizeof(of);
			of.hwndOwner = hh;
			of.lpstrDefExt = L"stxt";
			of.lpstrFilter = L"*.stxt\0*.stxt\0\0";
			wchar_t fnx[1000] = { 0 };
			of.lpstrFile = fnx;
			of.nMaxFile = 10000;
			of.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
			if (!GetOpenFileName(&of))
				return 0;
			New(fnx);
		}
		return 0;
	}

	case WM_CLOSE:
	{
		DestroyWindow(hh);
		return 0;
	}

	case WM_DESTROY:
	{
		iw--;
		if (fs)
		{
			fs->Close();
			delete fs;
		}
		SetWindowLongPtr(hh, GWLP_USERDATA, 0);
		if (iw == 0)
			PostQuitMessage(0);
		return 0;
	}
	}
	return DefWindowProc(hh, mm, ww, ll);
}



void FSnapTest()
{
	WSADATA wData;
	LoadLibrary(L"RICHED20.DLL");
	WSAStartup(MAKEWORD(2, 2), &wData);
	INITCOMMONCONTROLSEX icex = { 0 };
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_WIN95_CLASSES;
	icex.dwSize = sizeof(icex);
	InitCommonControlsEx(&icex);
	InitCommonControls();

	WNDCLASSEX wClass = { 0 };
	wClass.cbSize = sizeof(wClass);

	wClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_PARENTDC;
	wClass.lpfnWndProc = (WNDPROC)Main_DP;
	wClass.hInstance = GetModuleHandle(0);
	wClass.hIcon = LoadIcon(0,IDI_APPLICATION);
	wClass.hCursor = LoadCursor(0, IDC_ARROW);
	wClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wClass.lpszClassName = _T("CLASS");
	wClass.hIconSm = LoadIcon(0, IDI_APPLICATION);
	RegisterClassEx(&wClass);

	New();


	MSG msg;
	auto ha = LoadAccelerators(GetModuleHandle(0), L"MENU_2");
	while (GetMessage(&msg, 0, 0, 0))
	{
		if (TranslateAccelerator(msg.hwnd, ha, &msg))
			continue;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

}

// -------------------- Collab Lib
COLLAB::ANYAUTH auth;
COLLAB::CLIENT c(&auth);


void RecoFromDiff(const char* d, size_t sz, const char* e, size_t sze, std::vector<char>& o)
{
	DIFFLIB::DIFF diff;
	DIFFLIB::MemoryRdcFileReader r1(e, sze);
	DIFFLIB::MemoryRdcFileReader diffi(d, sz);

	DIFFLIB::MemoryDiffWriter dw;
	diff.Reconstruct(&r1, &diffi, 0, dw);
	o = dw.p();
}

class ONFIRST : public COLLAB::ON
{
public:
	HWND hE1;
	GUID g;

	ONFIRST(HWND h, GUID gg)
	{
		g = gg;
		hE1 = h;
	}
	virtual void Update(CLSID cid, const char* d, size_t sz)
	{
		using namespace std;
		if (g != cid)
			return;

		vector<wchar_t> x;
		x.resize(GetWindowTextLength(hE1) + 1);
		GetWindowText(hE1, x.data(), (int)x.size());
		x.resize(x.size() - 1);
		vector<char> res;
		RecoFromDiff(d, sz, (char*)x.data(), x.size() * 2, res);
		res.resize(res.size() + 2);
		SendMessage(hE1, EM_SETEVENTMASK, 0, 0);
		CHARRANGE cr;
		SendMessage(hE1, EM_EXGETSEL, 0, (LPARAM)&cr);
		SetWindowText(hE1, (wchar_t*)res.data());
		SendMessage(hE1, EM_EXGETSEL, 0, (LPARAM)&cr);
		SendMessage(hE1, EM_SETEVENTMASK, 0, ENM_CHANGE);
	}

};



struct W
{
	GUID g;
	COLLAB::ON* o;
	HWND hE;
};

LRESULT CALLBACK Main2_DP(HWND hh, UINT mm, WPARAM ww, LPARAM ll)
{
	W* dg = (W*)GetWindowLongPtr(hh, GWLP_USERDATA);
	switch (mm)
	{
	case WM_CREATE:
	{
		CREATESTRUCT* cS = (CREATESTRUCT*)ll;
		GUID a;
		CLSIDFromString((wchar_t*)cS->lpCreateParams, &a);
		dg = new W;
		dg->g = a;
		dg->hE = CreateWindowEx(0, RICHEDIT_CLASS, L"", WS_BORDER | WS_CHILD | WS_VISIBLE | ES_WANTRETURN | ES_MULTILINE | WS_HSCROLL | WS_VSCROLL, 0, 0, 0, 0, hh, (HMENU)901, 0, 0);
		dg->o = new ONFIRST(dg->hE, dg->g);
		c.AddOn(dg->o);

		c.Open(dg->g);
		SetWindowLongPtr(hh, GWLP_USERDATA, (LONG_PTR)dg);

		LOGFONT lf = { 0 };
		GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), (void*)&lf);
		lf.lfHeight *= 2;
		HFONT hF = CreateFontIndirect(&lf);
		SendMessage(dg->hE, WM_SETFONT, (WPARAM)hF, 0);

		SendMessage(hh, WM_SIZE, 0, 0);
		break;
	}

	case WM_SIZE:
	{
		RECT rc;
		GetClientRect(hh, &rc);
		SetWindowPos(dg->hE, 0, 0, 0, rc.right, rc.bottom, SWP_SHOWWINDOW);
		SendMessage(dg->hE, EM_SETEVENTMASK, 0, ENM_CHANGE);
		return 0;
	}

	case WM_COMMAND:
	{
		int HW = HIWORD(ww);
		int LW = LOWORD(ww);
		UNREFERENCED_PARAMETER(LW);
		if (HW == EN_CHANGE && LW == 901)
		{
			std::vector<wchar_t> x;
			x.resize(GetWindowTextLength(dg->hE) + 1);
			GetWindowText(dg->hE, x.data(), (int)x.size());


			std::thread t([&]()
			{
				CoInitializeEx(0, COINIT_MULTITHREADED);
				c.Put(dg->g, (const char*)x.data(), (x.size() - 1) * 2);
			});
			t.join();
		}
		return 0;
	}

	case WM_CLOSE:
	{
		c.RemoveOn(dg->o);
		c.Close(dg->g);
		DestroyWindow(hh);
		return 0;
	}

	case WM_DESTROY:
	{
		delete dg->o;
		delete dg;
		SetWindowLongPtr(hh, GWLP_USERDATA, (LONG_PTR)0);
		return 0;
	}
	}
	return DefWindowProc(hh, mm, ww, ll);
}


struct ASKTEXT
{
	const wchar_t* ti;
	const wchar_t* as;
	wchar_t* re;
	int P;
	std::wstring* re2;
};

static INT_PTR CALLBACK A_DP(HWND hh, UINT mm, WPARAM ww, LPARAM ll)
{
	static ASKTEXT* as = 0;
	switch (mm)
	{
	case WM_INITDIALOG:
	{
		as = (ASKTEXT*)ll;
		SetWindowText(hh, as->ti);
		if (as->P != 2)
		{
			SetWindowText(GetDlgItem(hh, 101), as->as);
			if (as->re)
				SetWindowText(GetDlgItem(hh, 102), as->re);
			if (as->re2)
				SetWindowText(GetDlgItem(hh, 102), as->re2->c_str());
		}
		else
			SetWindowText(GetDlgItem(hh, 701), as->as);
		if (as->P == 1)
		{
			auto w = GetWindowLongPtr(GetDlgItem(hh, 102), GWL_STYLE);
			w |= ES_PASSWORD;
			SetWindowLongPtr(GetDlgItem(hh, 102), GWL_STYLE, w);
		}
		return true;
	}
	case WM_COMMAND:
	{
		if (LOWORD(ww) == IDOK)
		{
			wchar_t re1[1000] = { 0 };
			wchar_t re2[1000] = { 0 };
			//					MessageBox(0, L"foo", 0, 0);
			if (as->P == 2)
			{
				GetWindowText(GetDlgItem(hh, 101), re1, 1000);
				GetWindowText(GetDlgItem(hh, 102), re2, 1000);
				if (wcscmp(re1, re2) != 0 || wcslen(re1) == 0)
				{
					SetWindowText(GetDlgItem(hh, 101), L"");
					SetWindowText(GetDlgItem(hh, 102), L"");
					SetFocus(GetDlgItem(hh, 101));
					return 0;
				}
				wcscpy_s(as->re, 1000, re1);
				EndDialog(hh, IDOK);
				return 0;
			}

			if (as->re2)
			{
				int lex = GetWindowTextLength(GetDlgItem(hh, 102));
				std::vector<wchar_t> re(lex + 100);
				GetWindowText(GetDlgItem(hh, 102), re.data(), lex + 100);
				*as->re2 = re.data();
				EndDialog(hh, IDOK);
			}
			else
			{
				GetWindowText(GetDlgItem(hh, 102), as->re, 1000);
				EndDialog(hh, IDOK);
			}
			return 0;
		}
		if (LOWORD(ww) == IDCANCEL)
		{
			EndDialog(hh, IDCANCEL);
			return 0;
		}
	}
	}
	return 0;
}

inline bool AskText(HWND hh, const TCHAR* ti, const TCHAR* as, TCHAR* re, std::wstring* re2 = 0)
{
	const char* res = "\xC4\x0A\xCA\x90\x00\x00\x00\x00\x04\x00\x00\x00\x00\x00\x6D\x01\x3E\x00\x00\x00\x00\x00\x00\x00\x0A\x00\x54\x00\x61\x00\x68\x00\x6F\x00\x6D\x00\x61\x00\x00\x00\x01\x00\x00\x50\x00\x00\x00\x00\x0A\x00\x09\x00\x1C\x01\x1A\x00\x65\x00\xFF\xFF\x82\x00\x00\x00\x00\x00\x00\x00\x80\x00\x81\x50\x00\x00\x00\x00\x0A\x00\x29\x00\x1D\x01\x0F\x00\x66\x00\xFF\xFF\x81\x00\x00\x00\x00\x00\x00\x00\x00\x03\x01\x50\x00\x00\x00\x00\x2F\x01\x16\x00\x32\x00\x0E\x00\x01\x00\xFF\xFF\x80\x00\x4F\x00\x4B\x00\x00\x00\x00\x00\x00\x00\x00\x03\x01\x50\x00\x00\x00\x00\x2F\x01\x29\x00\x32\x00\x0E\x00\x02\x00\xFF\xFF\x80\x00\x43\x00\x61\x00\x6E\x00\x63\x00\x65\x00\x6C\x00\x00\x00\x00\x00";
	ASKTEXT a = { ti,as,re,0,re2 };
	return (DialogBoxIndirectParam(GetModuleHandle(0), (LPCDLGTEMPLATEW)res, hh, A_DP, (LPARAM)&a) == IDOK);
}


int CollabTestLoop()
{
	WNDCLASSEX wClass = { 0 };
	wClass.cbSize = sizeof(wClass);
	wClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_PARENTDC;
	wClass.lpfnWndProc = (WNDPROC)Main2_DP;
	wClass.hCursor = LoadCursor(0, IDC_ARROW);
	wClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wClass.lpszClassName = _T("CLASS2");
	wClass.hIconSm = LoadIcon(0,IDI_APPLICATION);
	RegisterClassEx(&wClass);

	LoadLibrary(L"RICHED20.DLL");


	TASKDIALOGCONFIG tc = { 0 };
	tc.cbSize = sizeof(tc);
	tc.hwndParent = 0;
	tc.pszWindowTitle = L"Collaboration Notepad";
	PCWSTR ic2 = TD_INFORMATION_ICON;
	tc.pszMainIcon = ic2;
	tc.pszMainInstruction = L"Collaboration Notepad";
	tc.pszContent = L"Please select an option:";
	tc.dwCommonButtons = 0;
	tc.dwFlags = TDF_USE_HICON_MAIN | TDF_USE_COMMAND_LINKS;
	tc.hMainIcon = LoadIcon(0, IDI_APPLICATION);
	tc.hFooterIcon = LoadIcon(0, IDI_APPLICATION);
	TASKDIALOG_BUTTON b2[7] = { 0 };
	b2[0].pszButtonText = L"Connect to server";
	b2[1].pszButtonText = L"Disconnect from server";
	b2[2].pszButtonText = L"New text document";
	b2[3].pszButtonText = L"Open text document";
	b2[4].pszButtonText = L"Cancel";
	b2[0].nButtonID = 51;
	b2[1].nButtonID = 52;
	b2[2].nButtonID = 101;
	b2[3].nButtonID = 102;
	b2[4].nButtonID = IDCANCEL;

	tc.pButtons = b2;
	tc.cButtons = 5;


	tc.pfCallback = [](_In_ HWND hh, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM, _In_ LONG_PTR) ->HRESULT
	{
		if (msg == TDN_BUTTON_CLICKED)
		{
			if (wParam == 101 || wParam == 102)
			{
				wchar_t re[100] = { 0 };
				CLSID g;
				CoCreateGuid(&g);
				if (wParam == 101)
					StringFromGUID2(g, re, 100);
				if (!AskText(hh, L"Open document", L"Enter CLSID:", re))
					return S_FALSE;

				if (FAILED(CLSIDFromString(re, &g)))
					return S_FALSE;
				HWND h = CreateWindowEx(0,
					_T("CLASS2"),
					re,
					WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS |
					WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
					0, 0, GetModuleHandle(0), (LPVOID)re);
				ShowWindow(h, SW_SHOW);
				return S_FALSE;
			}


			if (wParam == 51)
			{
				c.Connect("127.0.0.1",8765);
				return S_FALSE;
			}


			if (wParam == 52)
			{
				c.Disconnect();
				return S_FALSE;
			}


			return S_OK;
		}

		if (msg == TDN_CREATED)
		{
		}

		return S_OK;
	};


	int rv = 0;
	BOOL ch = 0;
	TaskDialogIndirect(&tc, &rv, 0, &ch);
	c.Disconnect();

	return 0;
}

void CollabServer()
{
	WSADATA wData;
	WSAStartup(MAKEWORD(2, 2), &wData);
	CoInitializeEx(0, COINIT_MULTITHREADED);

	COLLAB::ANYAUTH authh;
	COLLAB::SERVER s(&authh, 8765, true);
	s.Start();
	MessageBox(0,L"Server running at port 8765. Press OK to exit...",L"Collab Server",MB_OK);
	s.End();
}



void CollabTest()
{
	std::thread t(CollabServer);
	t.detach();
	CollabTestLoop();
}

