// TestSMT-1.cpp : 定义控制台应用程序的入口点。



#include "stdafx.h"
#include "Windows.h"
 
#include<iostream>  

//#include <Usbiodef.h> 
#include <winioctl.h>
#include <setupapi.h>
#include <initguid.h>  

DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);
#define   GUID_CLASS_USB_DEVICE     GUID_DEVINTERFACE_USB_DEVICE 


#include <fstream>
#include <stdio.h> 
#include< Dbt.h>
#include <winioctl.h> 

using namespace std;

#pragma comment(lib,"User32.lib")
#pragma comment(lib, "gdi32.lib")


typedef HDESK(WINAPI *PFNOPENDESKTOP)(LPSTR, DWORD, BOOL, ACCESS_MASK);
typedef BOOL(WINAPI *PFNCLOSEDESKTOP)(HDESK);
typedef BOOL(WINAPI *PFNSWITCHDESKTOP)(HDESK);

#define SERVICE_NAME "srv_demo"

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hServiceStatusHandle;
void WINAPI service_main(int argc, char** argv);
//void WINAPI ServiceHandler(DWORD fdwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
DWORD WINAPI ServiceHandler(DWORD fdwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);


TCHAR szSvcName[80];
SC_HANDLE schSCManager;
SC_HANDLE schService;
int uaquit;
FILE* flog;
FILE* flogusb;

BOOL DoRegisterDeviceInterface(GUID InterfaceClassGuid, HDEVNOTIFY *hDevNotify);

//-------------------------------------------- 
//提取u盘的盘符
CHAR FirstDriveFromMask(ULONG unitmask)
{
	char i;
	for (i = 0; i < 26; ++i)
	{
		if (unitmask & 0x1)
			break;
		unitmask = unitmask >> 1;
	}
	return (i + 'A');
}


typedef struct MyRect {
	int x, y, width, height;
} MyRect;
BOOL PP_IsWorkStationLocked()
{
	// note: we can't call OpenInputDesktop directly because it's not
	// available on win 9x
	BOOL bLocked = FALSE;

	// load user32.dll once only
	static HMODULE hUser32 = LoadLibrary(_T("user32.dll"));

	if (hUser32)
	{
		static PFNOPENDESKTOP fnOpenDesktop = (PFNOPENDESKTOP)GetProcAddress(hUser32, "OpenDesktopA");
		static PFNCLOSEDESKTOP fnCloseDesktop = (PFNCLOSEDESKTOP)GetProcAddress(hUser32, "CloseDesktop");
		static PFNSWITCHDESKTOP fnSwitchDesktop = (PFNSWITCHDESKTOP)GetProcAddress(hUser32, "SwitchDesktop");

		if (fnOpenDesktop && fnCloseDesktop && fnSwitchDesktop)
		{
			HDESK hDesk = fnOpenDesktop("Default", 0, FALSE, DESKTOP_SWITCHDESKTOP);

			if (hDesk)
			{
				bLocked = !fnSwitchDesktop(hDesk);
				// cleanup
				fnCloseDesktop(hDesk);
			}
		}
	}


	return bLocked;

}

DWORD WINAPI srv_core_thread(LPVOID para)
{
	int i = 0;
	BOOL islock;
	for (;;)
	{
		if (uaquit)
		{
			break;
		}


		islock = PP_IsWorkStationLocked();
		//cout << "isLock: " << islock << endl;

		fprintf(flog, "isLock:%d\n", islock);
		fprintf(flog, "srv_core_thread run time count:%d\n", i++);
		Sleep(1000);
	}
	return NULL;
}


//void WINAPI ServiceHandler(DWORD fdwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
DWORD WINAPI ServiceHandler(DWORD fdwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	switch (fdwControl)
	{
	case SERVICE_CONTROL_STOP:
	{
		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(hServiceStatusHandle, &ServiceStatus);//报告服务运行状态 
		return 0; 
	}
	case SERVICE_CONTROL_SHUTDOWN:
	{
		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		ServiceStatus.dwCheckPoint = 0;
		ServiceStatus.dwWaitHint = 0;
		uaquit = 1;
		//add you quit code here
		if (flog != NULL)
			fclose(flog);
		SetServiceStatus(hServiceStatusHandle, &ServiceStatus);//报告服务运行状态
		return 0;

		break;
	}

	//当有设备插拨时触发
	case SERVICE_CONTROL_DEVICEEVENT:
	{
		ofstream f1("d:\\debug.txt");
		if (f1) 
		{
			f1 << "SERVICE_CONTROL_DEVICEEVENT" << endl;
			f1.close();
		}


		if (DBT_DEVICEARRIVAL == dwEventType)
		{
			FILE *fp;

			DEV_BROADCAST_HDR * pHdr = (DEV_BROADCAST_HDR *)lpEventData;
			PDEV_BROADCAST_DEVICEINTERFACE pDevInf;
			PDEV_BROADCAST_HANDLE pDevHnd;
			PDEV_BROADCAST_OEM pDevOem;
			PDEV_BROADCAST_PORT pDevPort;
			PDEV_BROADCAST_VOLUME pDevVolume;

			switch (pHdr->dbch_devicetype)
			{
			case DBT_DEVTYP_DEVICEINTERFACE:
				pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE)pHdr;

				fopen_s(&fp,"d:\\log.txt", "a+");
				if (fp)
				{
					_ftprintf(fp, _T("%s\n"), pDevInf->dbcc_name);
					fclose(fp);
				}
				break;
			case DBT_DEVTYP_HANDLE:
				pDevHnd = (PDEV_BROADCAST_HANDLE)pHdr;
				break;
			case DBT_DEVTYP_OEM:
				pDevOem = (PDEV_BROADCAST_OEM)pHdr;
				break;
			case DBT_DEVTYP_PORT:
				pDevPort = (PDEV_BROADCAST_PORT)pHdr;
				break;
			case DBT_DEVTYP_VOLUME:
				pDevVolume = (PDEV_BROADCAST_VOLUME)pHdr;
				break;
			}
		}
		break;
	}
	default:
		 
		break;
	};
	if (!SetServiceStatus(hServiceStatusHandle, &ServiceStatus))
	{
		DWORD nError = GetLastError();
		return nError;
	}
	return NO_ERROR;
}


void WINAPI service_main(int argc, char** argv)
{
	//指明服务可执行文件的类型- 如：表示该服务私有   SERVICE_WIN32_OWN_PROCESS;
	//如果可执行文件中只有一个单独的服务，就把这个成员设置成SERVICE_WIN32_OWN_PROCESS；
	//如果拥有多个服务的话，就设置成SERVICE_WIN32_SHARE_PROCESS。
	//除了这两个标志之外，如果你的服务需要和桌面发生交互（当然不推荐这样做），就要用“|”运算符附加上SERVICE_INTERACTIVE_PROCESS。
	//这个成员的值在服务的生存期内绝对不应该改变。
	ServiceStatus.dwServiceType = SERVICE_WIN32;

	//用于通知SCM此服务的现行状态   
	ServiceStatus.dwCurrentState = SERVICE_START_PENDING;//初始化服务，正在开始

														 //指明服务接受什么样的控制通知。
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE;

	//是允许服务报告错误的关键，如果希望服务去报告一个Win32错误代码（预定义在WinError.h中），它就设置dwWin32ExitCode为需要的代码。
	ServiceStatus.dwWin32ExitCode = 0;
	ServiceStatus.dwServiceSpecificExitCode = 0;

	//是一个服务用来报告它当前的事件进展情况的。当成员dwCurrentState被设置成SERVICE_START_PENDING的时候，应该把dwCheckPoint设成0，
	ServiceStatus.dwCheckPoint = 0;

	//dwWaitHint设成一个经过多次尝试后确定比较合适的超时毫秒数，这样服务才能高效运行。
	ServiceStatus.dwWaitHint = 0;

	//通过RegisterServiceCtrlHandler()与服务控制程序建立一个通信的协议。
	hServiceStatusHandle = RegisterServiceCtrlHandlerEx(_T(SERVICE_NAME), ServiceHandler,NULL);
	if (hServiceStatusHandle == 0)
	{
		DWORD nError = GetLastError();
	}

	//add your init code here
	//flog = fopen_s("d:\\test.txt", "w");
	fopen_s(&flog,"d:\\test.txt", "a");
	//add your service thread here
	HANDLE task_handle = CreateThread(NULL, NULL, srv_core_thread, NULL, NULL, NULL);
	if (task_handle == NULL)
	{
		fprintf(flog, "create srv_core_thread failed\n");
	}

	// Initialization complete - report running status 
	//RtlZeroMemory(&ServiceStatus, sizeof(SERVICE_STATUS));//结构体清空 不能启用

	ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWaitHint = 9000;

	SetServiceStatus(hServiceStatusHandle, &ServiceStatus);//报告服务运行状态
	if (ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
		//把服务要做的工作放到这里
	}

	HDEVNOTIFY hDevNotify;
	if (!DoRegisterDeviceInterface(GUID_DEVINTERFACE_USB_DEVICE, &hDevNotify))//GUID_DEVINTERFACE_USB_DEVICE  //GUID_DEVINTERFACE_VOLUME

	{
		ofstream f1("d:\\DoRegisterDeviceInterface.txt");
		if (f1)
		{
			f1 << "DoRegisterDeviceInterface  Faled!" << endl;
			f1.close();
		}
		return;
	}
	 
	if (!SetServiceStatus(hServiceStatusHandle, &ServiceStatus))
	{
		DWORD nError = GetLastError();
	}

}
//do not change main function
int main(int argc, const char *argv[])
{
	SERVICE_TABLE_ENTRY ServiceTable[2];

	ServiceTable[0].lpServiceName = _T(SERVICE_NAME);
	ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)service_main;

	//SERVICE_TABLE_ENTRY结构数组要求最后一个成员组都为NULL，我们称之为“哨兵”（所有值都为NULL），表示该服务表末尾。
	//一个服务启动后，马上调用StartServiceCtrlDispatcher()通知服务控制程序服务正在执行，并提供服务函数的地址。
	//StartServiceCtrlDispatcher()只需要一个至少有两SERVICE_TABLE_ENTRY结构的数组，它为每个服务启动一个线程，一直等到它们结束才返回。
	ServiceTable[1].lpServiceName = NULL;
	ServiceTable[1].lpServiceProc = NULL;
	// 启动服务的控制分派机线程
	StartServiceCtrlDispatcher(ServiceTable);
  

	return 0;
}



BOOL DoRegisterDeviceInterface(GUID InterfaceClassGuid,HDEVNOTIFY *hDevNotify)
{
	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;

	ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	NotificationFilter.dbcc_classguid = InterfaceClassGuid;//直接用这个也OK{ 0xA5DCBF10,0x6530,0x11D2,{ 0x90,0x1F,0x00,0xC0,0x4F,0xB9,0x51,0xED } };//InterfaceClassGuid;
	
																												 //GUID_DEVINTERFACE_USB_DEVICE
	//{0xA5DCBF10, 0x6530, 0x11D2, { 0x90,0x1F,0x00,0xC0,0x4F,0xB9,0x51,0xED }},
	//	//GUID_DEVINTERFACE_DISK
	//{ 0X53F56307,0XB6BF,0X11D0,{ 0X94,0XF2,0X00,0XA0,0XC9,0X1E,0XFB,0X8B } },
	//	//GUID_DEVINTERFACE_HID
	//{ 0X4D1E55B2,0XF16F,0X11CF,{ 0X88,0XCB,0X00,0X11,0X11,0X00,0X00,0X30 } },
	//	//GUID_NDIS_LAN_CLASS
	//{ 0XAD498944,0X762F,0X11D0,{ 0X8D,0XCB,0X00,0X00,0X4F,0XC3,0X35,0X8C } }
 

	//*hDevNotify = RegisterDeviceNotification(hServiceStatusHandle,&NotificationFilter,DEVICE_NOTIFY_SERVICE_HANDLE);
	*hDevNotify = RegisterDeviceNotification(hServiceStatusHandle, (PDEV_BROADCAST_HDR)&NotificationFilter, DEVICE_NOTIFY_SERVICE_HANDLE);//DEVICE_NOTIFY_WINDOW_HANDLE //DEVICE_NOTIFY_SERVICE_HANDLE
	if (!*hDevNotify)
	{
		printf("RegisterDeviceNotification failed: %d\n",
			GetLastError());
		return FALSE;
	}

	return TRUE;
}