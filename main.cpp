#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <process.h>
#include <commctrl.h>
#include <stdio.h>
#include <iostream>
#include <any>
#include <string>
#include <vector>
#include <tchar.h>
#include <memory>
#include <thread>
#include <map>
#include <functional>
using namespace std;

#include "rw.hpp"


HWND MainWindow = 0;
HBRUSH hWB;
HBRUSH hBB;

int sq = 5;
tlock<int> n(0);

void thr(int idxd,bool fx)
{
	// center box

	if (!fx && idxd == ((sq*sq) - 1) / 2)		return;


	HDC hDC = GetDC(MainWindow);
	SelectObject(hDC, (HFONT)GetStockObject(DEFAULT_GUI_FONT));
	RECT rc = { 0 };

	int ms = 200 + idxd*20;
	if (fx)
		ms = 0;
	bool br = false;
	for (;;)
	{
		n.readlock([&](const int& nn) {

			for (int ii = 0; ii < 2; ii++)
			{
				Sleep(ms);
				int idx = idxd;
				GetClientRect(MainWindow, &rc);
				int StepX = rc.right / sq;
				int StepY = rc.bottom / sq;
				int StartY = 0;
				while (idx >= sq)
				{
					StartY += StepY;
					idx -= sq;
				}
				int StartX = StepX * idx;
				RECT r2 = { 0 };
				r2.left = StartX;
				r2.top = StartY;
				r2.right = r2.left + StepX;
				r2.bottom = r2.top + StepY;
				if (!fx)
					FillRect(hDC, &r2, br ? hWB : hBB);
				br = !br;

				wchar_t t[10];
				swprintf_s(t, 10, L"%i", nn);
				if (fx)
				{
					SetBkColor(hDC, RGB(255, 255, 255));
					SetTextColor(hDC, RGB(255, 0, 0));
					swprintf_s(t, 10, L"* %i *", nn);

				}
				else
					if (br)
					{
						SetBkColor(hDC, RGB(255, 255, 255));
						SetTextColor(hDC, 0);
					}
					else
					{
						SetBkColor(hDC, RGB(0, 0, 0));
						SetTextColor(hDC, RGB(255, 255, 255));
					}
				DrawText(hDC, t, -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				GdiFlush();
			}
		});
		if (fx)
			break;
	}



	ReleaseDC(MainWindow, hDC);
}

INT_PTR CALLBACK D_DP(HWND hh, UINT mm, WPARAM ww, LPARAM)
{
	switch (mm)
	{
		case WM_INITDIALOG:
		{
			MainWindow = hh;
			hWB = (HBRUSH) GetStockObject(WHITE_BRUSH);
			hBB = (HBRUSH)GetStockObject(BLACK_BRUSH);
			for (int i = 0; i < (sq*sq) ; i++)
			{
				thread t(thr, i,false);
				t.detach();
			}
			break;
		}
		case WM_CLOSE:
		{
			EndDialog(hh, 0);
			return 0;
		}
		case WM_COMMAND:
		{
			int LW = LOWORD(ww);
			int s = ((sq*sq) - 1) / 2;
			if (LW == 101)
			{
				// read
				thr(s, true);
			}
			if (LW == 102)
			{
				n.direct()++;
				thr(s, true);
			}
			if (LW == 121)
			{
				thread tx([&]()
				{
					n.writelock([](int& nn) {
						nn++;
						int s = ((sq*sq) - 1) / 2;
						thr(s, true);
					});
				});
				tx.detach();
			}
			if (LW == 122)
			{
				thread tx([&]()
				{
					n.rwlock([&](const int&, std::function<void(std::function<void(int &)>)> upgrfunc) {
						int s = ((sq*sq) - 1) / 2;
						thr(s, true);
						Sleep(1000);
						upgrfunc([&](int& nn) {
							nn++;
							thr(s, true);
							Sleep(1000);
						});
					});
				});
				tx.detach();
			}
		}
	}
	return 0;
}

void CollabTest();
void USMTest();
void FSnapTest();
void DiffTest();
void PoolTest();

int __stdcall WinMain(HINSTANCE h, HINSTANCE, LPSTR, int)
{
	//PoolTest();

	srand((unsigned int)time(0));
	WSADATA wData;
	WSAStartup(MAKEWORD(2, 2), &wData);
	CoInitializeEx(0, COINIT_MULTITHREADED);
	INITCOMMONCONTROLSEX icex = { 0 };
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES | ICC_WIN95_CLASSES;
	icex.dwSize = sizeof(icex);
	InitCommonControlsEx(&icex);
	InitCommonControls();


	// Uncomment to test these tools

	// DiffTest();
	// USMTest();
	// FSnapTest();
	// CollabTest();
	// PoolTest();
	DialogBox(h, L"DIALOG_1", 0, D_DP);
}

