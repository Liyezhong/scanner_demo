#include"pch.h"
#include"libusb.h"
#include<windows.h>
#include<stdio.h>
#include<fstream>
#include<iostream>
#include <process.h>
#include <conio.h>
#include <tchar.h>
#include <Dbt.h>
#include <setupapi.h>

#include "scanner.h"

using namespace std;

#pragma comment (lib, "Kernel32.lib")
#pragma comment (lib, "User32.lib")


#define THRD_MESSAGE_EXIT WM_USER + 1
const _TCHAR CLASS_NAME[] = _T("Scanner Hotplug Class");

HWND hWnd;

static const GUID GUID_DEVINTERFACE = { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };


void UpdateDevice(PDEV_BROADCAST_DEVICEINTERFACE pDevInf, WPARAM wParam)
{
	wcout << pDevInf->dbcc_name << endl;

	std::wstring usbinfo = pDevInf->dbcc_name;

	if ((ssize_t)usbinfo.find(L"VID_0483") < 0)
		return;
	if ((ssize_t)usbinfo.find(L"PID_5740") < 0)
		return;

	if (wParam == DBT_DEVICEARRIVAL) {
		printf("scanner inserted!\n");
		Scanner::GetInstance()->attach();

	} else if (wParam == DBT_DEVICEREMOVECOMPLETE) {
		printf("scanner remove!\n");
		Scanner::GetInstance()->detach();
	}
}

LRESULT DeviceChange(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
		PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
		PDEV_BROADCAST_DEVICEINTERFACE pDevInf;

		if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
			pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE)pHdr;
			UpdateDevice(pDevInf, wParam);
		}
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_DEVICECHANGE)
		return DeviceChange(message, wParam, lParam);

	return DefWindowProc(hWnd, message, wParam, lParam);
}

ATOM MyRegisterClass()
{
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = CLASS_NAME;
	return RegisterClass(&wc);
}

bool CreateMessageOnlyWindow()
{
	hWnd = CreateWindowEx(0, CLASS_NAME, _T(""), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,       // Parent window    
		NULL,       // Menu
		GetModuleHandle(NULL),  // Instance handle
		NULL        // Additional application data
	);

	return hWnd != NULL;
}

void RegisterDeviceNotify()
{
	HDEVNOTIFY hDevNotify;
	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE;
	hDevNotify = RegisterDeviceNotification(hWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
}

// ----------------------------------------------------------------------------------------------------------------

unsigned char photo_flag = 0;
unsigned char barcode_flag = 0;
byte tmp[8 * 1024 * 1024];

void keyboardMessage(void *) {
	unsigned char key;
	while (1)
	{
		key = _getch();
		if (key == 'b')
		{
			barcode_flag = 1;
		}
		if (key == 'p')
		{
			photo_flag = 1;
		}
	}
}

void hotplug_handle(void*)
{
	if (!MyRegisterClass())
		return;

	if (!CreateMessageOnlyWindow())
		return;

	RegisterDeviceNotify();
	MSG msg;
	while (GetMessage(&msg, hWnd, 0, 0)) {
		if (msg.message == THRD_MESSAGE_EXIT) {
			cout << "worker receive the exiting Message..." << endl;
			return;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void CmdResultTransfer(int type)
{
	std::cout << "_ali: func: " << __FUNCTION__ << " line: " << __LINE__ << " type: " << type << std::endl;
}

void BarcodeTransfer(int id, uint8_t* barcode, int len)
{
	std::cout << "_ali: func: " << __FUNCTION__ << " line: " << __LINE__ << " id: " << id << std::endl;
	for (int i = 0; i < len; i++)
		printf("%c", barcode[i]);
	printf("\n");
}

void ImageTransfer(int id, uint8_t* image, int len)
{
	std::cout << "_ali: func: " << __FUNCTION__ << " line: " << __LINE__ << " id: " << id << std::endl;

	char filename[20];
	sprintf_s(filename, "%d.jpg", id);
	ofstream fout;
	fout.open(filename, std::ios::binary);
	fout.write((char*)tmp, len);
	fout.close();
}

//#define E0        0
//#define E1        1
//#define E2        2
//#define E3        3
void ErrorCodeTransfer(int errorno)
{
	std::cout << "_ali: func: " << __FUNCTION__ << " line: " << __LINE__ << " errorno: " << errorno << std::endl;
}

int main()
{
	int a = 0;
	int r;
	int actual_length;


	_beginthread(hotplug_handle, 0, NULL);

	// init scanner reciver callback functions.
	Scanner::GetInstance()->CmdResultTransfer = CmdResultTransfer;
	Scanner::GetInstance()->BarcodeTransfer = BarcodeTransfer;
	Scanner::GetInstance()->ImageTransfer = ImageTransfer;
	Scanner::GetInstance()->ErrorCodeTransfer = ErrorCodeTransfer;


	while (1) {
		unsigned char key;
		key = _getch();

		switch (key) {
		case 'b':
			Scanner::GetInstance()->BarcodeMode();
			break;
		case 'p':
			Scanner::GetInstance()->PhotoMode();
			break;
		case 's':
			Scanner::GetInstance()->Sleep();
			break;
		case 'w':
			Scanner::GetInstance()->Wakeup();
			break;
		}
	}

#if 1
	  
	_beginthread(keyboardMessage, 0, NULL);

	r = libusb_init(NULL);
	if (r < 0) 
		return r;

	libusb_device_handle* handle = libusb_open_device_with_vid_pid(NULL, 0x0483, 0x5740);
	if (handle == nullptr)
		return -1;

	libusb_detach_kernel_driver(handle, 0);

	r = libusb_set_configuration(handle, 1);
	printf("libusb_set_configuration: %s\n", libusb_strerror((libusb_error)r));
	if (r < 0)
		return r;

	r = libusb_claim_interface(handle, 1);
	if (r < 0)
		return r;

	putchar('\n');

	while (1)
	{
		byte cmd[10] = {0,0,0,0,0,0,0,0,0,0};
		byte ack1[3] = { 0xaa, 0xdd, 0x02 };
		byte ack2[3] = { 0xaa, 0xdd, 0x01 };
		byte barcode_cmd[3] = { 0xaa, 0xdd, 0x03 };
		byte photo_cmd[3] = { 0xaa, 0xdd, 0x04 };
		long len, crc, id, count;
		int ii;
		char filename[20];

		do
		{
			if (photo_flag)
			{
				r = libusb_bulk_transfer(handle, 0x01, photo_cmd, 3, &actual_length, 0);
				photo_flag = 0;
			}
			if (barcode_flag)
			{
				r = libusb_bulk_transfer(handle, 0x01, barcode_cmd, 3, &actual_length, 0);
				barcode_flag = 0;
			}
			r = libusb_bulk_transfer(handle, 0x81, cmd, 3, &actual_length, 500);
			if (r == 0) printf("recv cmd: 0x%.2x 0x%.2x 0x%.2x\n", cmd[0], cmd[1], cmd[2]);
			//else
			//	printf("libusb_bulk_transfer: ret: %d, %s\n", r, libusb_error_name(r));
		}
		while ((cmd[2] != 0x02)&& (cmd[2] != 0x01));

		if (cmd[2] == 0x02)
		{
			ofstream fout;
			r = libusb_bulk_transfer(handle, 0x81, cmd + 3, 7, &actual_length, 0);
			len = cmd[3] * 256 * 256 * 256 + cmd[4] * 256 * 256 + cmd[5] * 256 + cmd[6];
			crc = cmd[7] * 256 + cmd[8];
			id = cmd[9];
			count = 0;
			if (r == 0) printf("recv pic header: len = %d bytes, crc = 0x%.4x , id = %d\n", len, crc, id );
			//else
			//	printf("libusb_bulk_transfer: ret: %d, %s\n", r, libusb_error_name(r));
			r = libusb_bulk_transfer(handle, 0x01, ack1, 3, &actual_length, 0);
			ii = 0;
			do
			{
				r = libusb_bulk_transfer(handle, 0x81, tmp + count, min(len, 504), &actual_length, 200);
				len = len - actual_length;
				count = count + actual_length;
				ii++;
				//if (r == 0) libusb_bulk_transfer(handle, 0x01, ack1, 3, &actual_length, 0);
			} while (len > 0);//((r == 0));//(actual_length == 512)&&
			if (r == 0) libusb_bulk_transfer(handle, 0x01, ack2, 3, &actual_length, 0);
			//else 
			//	printf("libusb_bulk_transfer: ret: %d, %s\n", r, libusb_error_name(r));
			sprintf_s(filename, "%d.jpg", a);
			fout.open(filename, std::ios::binary);
			fout.write((char*)tmp, count);
			fout.close();
			a++;
		}
		if (cmd[2] == 0x01)
		{
			r = libusb_bulk_transfer(handle, 0x81, cmd + 3, 7, &actual_length, 0);
			len = cmd[3] * 256 * 256 * 256 + cmd[4] * 256 * 256 + cmd[5] * 256 + cmd[6];
			crc = cmd[7] * 256 + cmd[8];
			id = cmd[9];
			count = 0;
			if (r == 0) printf("recv barcode header: len = %d bytes, crc = 0x%.4x , id = %d\n", len, crc, id);
			printf("libusb_bulk_transfer: ret: %d, %s\n", r, libusb_error_name(r));
			ii = 0;
			do
			{
				r = libusb_bulk_transfer(handle, 0x81, tmp + count, min(len, 504), &actual_length, 200);
				len = len - actual_length;
				count = count + actual_length;
				ii++;
			} while (len > 0);//((r == 0));//(actual_length == 512)&&
			if (r == 0) libusb_bulk_transfer(handle, 0x01, ack2, 3, &actual_length, 0);
			//else
			//	printf("libusb_bulk_transfer: ret: %d, %s\n", r, libusb_error_name(r));
			printf("recv barcode:");
			int i = 0;
			for (i = 0; i < count; i++) printf("0x%.2x ", tmp[i]);
			printf("\n");
			a++;
		}
	}
#endif

	return 0;
}