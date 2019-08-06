#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <time.h>

// To work correctly ThinkPad must be configured in the "Only Classic TrackPoint Mode"

#if defined(_WIN32) || defined(_WIN64)
  #define snprintf _snprintf
  #define vsnprintf _vsnprintf
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
#endif

#include "SynKit.h"
int connectionTypes[] = { SE_ConnectionAny };
char* connectionTypeNames[] = { "(any connection)" };
int connectionTypeCount = 1;

int deviceTypes[] = {SE_DeviceMouse, SE_DeviceTouchPad, SE_DeviceWheelMouse, SE_DeviceIBMCompatibleStick,
	SE_DeviceStyk, SE_DeviceFiveButtonWheelMouse, SE_DevicecPad};
char* deviceTypeNames[] = {"Mouse", "TouchPad", "WheelMouse", "IBMCompatibleStick", "Styk", "FiveButtonWheelMouse", "cPad"};
int deviceTypeCount = 7;

int wantedDeviceTypes[] = { SE_DeviceIBMCompatibleStick, SE_DeviceStyk, SE_DeviceTouchPad };
char* wantedDeviceTypeNames[] = {"IBMCompatibleStick", "Styk", "TouchPad"};
int wantedDeviceTypeCount = 3;

static bool NORMAL_MODE = 0;
static bool DELTA_DEBUG = 0;
//#define DISABLE_ALL_EVENTS

void listDevices(ISynAPI *pAPI)
{
#ifdef _DEBUG
	__time64_t ltime;
	_time64( &ltime );
	printf("Device list: %s", _ctime64( &ltime ) );

	for (int conn = 0; conn < connectionTypeCount; ++conn)
		for (int dev = 0; dev < deviceTypeCount; ++dev)
		{
			long handle = -1;

			while (pAPI->FindDevice(connectionTypes[conn], deviceTypes[dev], &handle) == SYN_OK)
			{
				printf("Found: connection %d, device %d, %s on %s\n", conn, dev, deviceTypeNames[dev], connectionTypeNames[conn]);
			}
		}
#endif
}

#define MAX_DEVICE_COUNT 100

struct deviceState
{
	ISynDevice* device;
	HANDLE event;
	BOOL isMiddleClicked;
	ULONGLONG middleClickTimeout;

	SynDeviceType deviceType;
};
deviceState deviceStates[MAX_DEVICE_COUNT];
int connectedDeviceCount = 0;

HANDLE hEvents[MAX_DEVICE_COUNT + 1];

void connectDevices(ISynAPI *pAPI, bool resetOnly = false)
{
#ifdef _DEBUG
	__time64_t ltime;
	_time64( &ltime );
	printf("Connecting to devices: %s", _ctime64( &ltime ) );
#endif

	connectedDeviceCount = 0;
	for (int conn = 0; conn < connectionTypeCount; ++conn)
		for (int dev = 0; dev < wantedDeviceTypeCount; ++dev)
		{
			long handle = -1;

			while (pAPI->FindDevice(connectionTypes[conn], wantedDeviceTypes[dev], &handle) == SYN_OK)
			{
#ifdef _DEBUG
				printf("Connecting to: connection %d, device %d, logical_id %d, %s on %s\n", conn, dev, connectedDeviceCount, wantedDeviceTypeNames[dev], connectionTypeNames[conn]);
#endif
				if (pAPI->CreateDevice(handle, &deviceStates[connectedDeviceCount].device) != SYN_OK)
				{
#ifdef _DEBUG
					printf("Cannot obtain a device object.\n");
#else
					MessageBox(NULL, "Cannot obtain a device object.", "TP Middle", MB_ICONERROR | MB_OK);
#endif
					continue;
				}

				char eventName[100];
				sprintf(eventName, "tpmiddleDevice%d_%d", GetCurrentProcessId(), connectedDeviceCount);
				deviceStates[connectedDeviceCount].event = CreateEvent(0, 0, 0, eventName);
				deviceStates[connectedDeviceCount].device->SetEventNotification(deviceStates[connectedDeviceCount].event);
				hEvents[connectedDeviceCount + 1] = deviceStates[connectedDeviceCount].event;

				deviceStates[connectedDeviceCount].isMiddleClicked = FALSE;
				deviceStates[connectedDeviceCount].middleClickTimeout = 0;
				deviceStates[connectedDeviceCount].device->GetProperty(SP_DeviceType, (long *)&deviceStates[connectedDeviceCount].deviceType);

				if (NORMAL_MODE || resetOnly)
				{
					long lMask;
					deviceStates[connectedDeviceCount].device->GetProperty(SP_MiddleButtonAction, &lMask);
					if (resetOnly)
					{
						lMask &= ~SF_ActionAll;
						lMask |= SF_ActionAuxilliary;
					}
					if (NORMAL_MODE) lMask &= ~SF_ActionAll;
					deviceStates[connectedDeviceCount].device->SetProperty(SP_MiddleButtonAction, lMask);
				}

				++connectedDeviceCount;
			}
		}
}

void disconnectDevices(ISynAPI *pAPI)
{
#ifdef _DEBUG
	__time64_t ltime;
	_time64( &ltime );
	printf("Disconnecting devices: %s", _ctime64( &ltime ) );
#endif

	for (int i = 0; i < connectedDeviceCount; ++i)
	{
		CloseHandle(deviceStates[i].event);
		deviceStates[i].device->Release();
		deviceStates[i].device = NULL;
	}

	connectedDeviceCount = 0;
}

void sendMiddleMouse(int devNum, bool buttondown)
{
	static bool lastbuttondown = false;
	if (NORMAL_MODE && buttondown == lastbuttondown) return;
	lastbuttondown = buttondown;
	INPUT Input={0};

	ZeroMemory(&Input,sizeof(INPUT));
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
	if (buttondown) Input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
	SendInput(1,&Input,sizeof(INPUT));
#ifdef _DEBUG
	__time64_t ltime;
	_time64( &ltime );
	printf("Packet: device %d, sendMiddleMouse=%i, %s", devNum, buttondown, _ctime64( &ltime ));
#endif
}

// Derived from http://www.cmake.org/pipermail/cmake/2004-June/005172.html
char** CommandLineToArgvA(LPSTR lpCmdLine, int& argc)
{
	// parse a few of the command line arguments
	// a space delimites an argument except when it is inside a quote

	char**       argv;
	unsigned int i;
	int          j;

	argc = 1;
	int pos = 0;
	for (i = 0; i < strlen(lpCmdLine); i++)
    { 
		while (lpCmdLine[i] == ' ' && i < strlen(lpCmdLine)) i++;
		if (lpCmdLine[i] == '\"')
		{
			i++;
			while (lpCmdLine[i] != '\"' && i < strlen(lpCmdLine))
			{
				i++;
				pos++;
			}
			argc++;
			pos = 0;
		}
		else
		{
			while (lpCmdLine[i] != ' ' && i < strlen(lpCmdLine))
			{
				i++;
				pos++;
			}
			argc++;
			pos = 0;
		}
	}
	
	argv = (char**)malloc(sizeof(char*)* (argc+1));
	
	argv[0] = (char*)malloc(1024);
	::GetModuleFileName(0, argv[0],1024);
	
	for(j=1; j<argc; j++) argv[j] = (char*)malloc(strlen(lpCmdLine)+10);

	argv[argc] = 0;

	argc = 1;
	pos = 0;
	for (i = 0; i < strlen(lpCmdLine); i++)
	{
		while (lpCmdLine[i] == ' ' && i < strlen(lpCmdLine)) i++;
		if (lpCmdLine[i] == '\"')
		{
			i++;
			while (lpCmdLine[i] != '\"' && i < strlen(lpCmdLine))
			{
				argv[argc][pos] = lpCmdLine[i];
				i++;
				pos++;
			}
			argv[argc][pos] = '\0';
			argc++;
			pos = 0;
		}
		else
		{
			while (lpCmdLine[i] != ' ' && i < strlen(lpCmdLine))
			{
				argv[argc][pos] = lpCmdLine[i];
				i++;
				pos++;
			}
			argv[argc][pos] = '\0';
			argc++;
			pos = 0;
		}
	}
	argv[argc] = 0;
	return argv;
}

#ifdef _DEBUG
int main(int argc, char *argv[])
{
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int argc;
	char** argv = CommandLineToArgvA(lpCmdLine, argc);
#endif

	bool resetOnly = false;
	if (argc >= 2)
	{
		if (strcasecmp((const char*)argv[1], "-r") == 0) resetOnly = true;
		if (strcasecmp((const char*)argv[1], "-n") == 0) NORMAL_MODE = 1;
	}
#ifdef _DEBUG
	printf("argc=%i, NORMAL_MODE=%i, resetOnly=%i\n", argc, NORMAL_MODE, resetOnly);
#else
#if 0
	char s[100];
	wsprintf(s, "argc=%i, NORMAL_MODE=%i, resetOnly=%i", argc, NORMAL_MODE, resetOnly);
	MessageBox(NULL, s, "NORMAL_MODE", MB_OK);
#endif
#endif
	ISynAPI *pAPI = NULL;
	
	int counter = 0;
	while (true)
	{
		HRESULT ret = SynCreateAPI(&pAPI);

		if (ret == SYN_OK)
			break;

#ifdef _DEBUG
		printf("Cannot obtain an API object: 0x%lx. Retrying...\n", ret);
#endif

		++counter;

		if (counter == 60)
		{
#ifdef _DEBUG
			printf("Initialization failed.\n", ret);
			getchar();
#else
			char buf[2000];
			sprintf(buf, "Cannot obtain an API object: 0x%lx.", ret);
			MessageBox(NULL, buf, "TP Middle", MB_ICONERROR | MB_OK);
#endif
			exit(-1);
		}

		Sleep(1000);
	}

	char eventName[100];
	sprintf(eventName, "tpmiddleApi%d", GetCurrentProcessId());
	hEvents[0] = CreateEvent(0, 0, 0, eventName);
	pAPI->SetEventNotification(hEvents[0]);
	
	listDevices(pAPI);

	connectDevices(pAPI, resetOnly);
	if (resetOnly)
	{
		disconnectDevices(pAPI);
		pAPI->Release();
		exit(0);
	}

	SynPacket Packet;
    bool deviceOldMiddleClick = FALSE;
	while (true)
	{
		DWORD res = MsgWaitForMultipleObjects(connectedDeviceCount + 1, hEvents, FALSE, INFINITE, QS_ALLINPUT);
		if (res == WAIT_OBJECT_0)
		{
			listDevices(pAPI);
			disconnectDevices(pAPI);
			connectDevices(pAPI);
		}
		else if (res <= WAIT_OBJECT_0 + connectedDeviceCount)
		{
			int devNum = res - WAIT_OBJECT_0 - 1;
			while (deviceStates[devNum].device->LoadPacket(Packet) != SYNE_FAIL)
			{
				bool deviceMiddleClick = FALSE;
				if (deviceStates[devNum].deviceType == SE_DeviceTouchPad)
                {
					deviceMiddleClick = (Packet.ButtonState() & SF_ButtonExtended3) == SF_ButtonExtended3;
				}
				else
                {
					deviceMiddleClick = (Packet.ButtonState() & SF_ButtonMiddle) == SF_ButtonMiddle;
				}

#if defined(_DEBUG) && !defined(DISABLE_ALL_EVENTS)
				__time64_t ltime;
				_time64( &ltime );


				if (deviceStates[devNum].deviceType == SE_DeviceTouchPad)
                {
					if (!DELTA_DEBUG || (DELTA_DEBUG && !!deviceMiddleClick != !!deviceStates[devNum].isMiddleClicked))
					printf("Packet: device %d, state %x, deviceIsM: %d, isM: %d, %s", devNum, Packet.ButtonState(), deviceMiddleClick, deviceStates[devNum].isMiddleClicked, _ctime64(&ltime));
				}
				else
                {
					if (!DELTA_DEBUG || (DELTA_DEBUG && !!deviceMiddleClick != !!deviceStates[devNum].isMiddleClicked))
					printf("Packet: device %d, state %x, xyz %d %d %d, deviceIsM: %d, isM: %d, %s", devNum, Packet.ButtonState(), Packet.XDelta(),
						Packet.YDelta(), Packet.ZDelta(), deviceMiddleClick, deviceStates[devNum].isMiddleClicked, _ctime64(&ltime));
				}
#endif
				if ((!deviceStates[devNum].isMiddleClicked) && deviceMiddleClick)
				{
					deviceStates[devNum].isMiddleClicked = TRUE;
					deviceStates[devNum].middleClickTimeout = GetTickCount64() + GetDoubleClickTime() / 2;
					if (NORMAL_MODE)
					{
						sendMiddleMouse(devNum, true);
					}
					else
					{
#ifdef _DEBUG
						__time64_t ltime;
						_time64( &ltime );
						printf("Middle click started: device %d, %s", devNum, _ctime64( &ltime ) );
#endif
					}
				}

				if (deviceStates[devNum].isMiddleClicked && !deviceMiddleClick)
				{
					deviceStates[devNum].isMiddleClicked = FALSE;
					if (NORMAL_MODE)
					{
						sendMiddleMouse(devNum, false);
					}
					else
					{
						if (deviceStates[devNum].middleClickTimeout > GetTickCount64() && false)
						{
							Sleep(50);
							sendMiddleMouse(devNum, true);
							Sleep(50);
							sendMiddleMouse(devNum, false);
#ifdef _DEBUG
							__time64_t ltime;
							_time64( &ltime );
							printf("Middle click finished: device %d, %s", devNum, _ctime64( &ltime ) );
#endif
						}
						else
						{
#ifdef _DEBUG
							__time64_t ltime;
							_time64( &ltime );
							printf("Middle click timeouted: device %d, %s", devNum, _ctime64( &ltime ) );
#endif
						}
					}
				}
			}
		}
		else if (res == WAIT_OBJECT_0 + connectedDeviceCount + 1)
		{
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
			return FALSE;
	}

	disconnectDevices(pAPI);
	pAPI->Release();

	return 0;
}
