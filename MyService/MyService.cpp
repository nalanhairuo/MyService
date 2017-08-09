/***********************************************************************
* 版权所有 (C)2017,WQH
*
* 文件名称：service.cpp
* 内容摘要：windows服务程序框架
* 其它说明：测试是检测U盘插入与拔出的状态,将日志写入MyService.txt
* 当前版本：version1.0
* 作   者：WQH
* 完成日期：2017.0417
* //TODO //FIXME
2017.04.18 服务控制:install/uninstall/start/stop
***********************************************************************/
//       File Include Section
//***********************************************************************
#pragma warning(disable:4996)
#include <string.h>
#include <stdio.h>
#include <Windows.h>
#include <dbt.h>
#include <math.h>
#include <tchar.h>
//***********************************************************************
//       Macro Define Section
//***********************************************************************


//***********************************************************************
//       Global Variable Declare Section
//***********************************************************************

SERVICE_STATUS        g_ServiceStatus = { 0 };                  //服务状态
SERVICE_STATUS_HANDLE g_ServiceStatusHandle = NULL;             //服务状态句柄
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;//服务停止句柄

//***********************************************************************
//       File Static Variable Define Section
//***********************************************************************

LPTSTR  lpServiceName = _T("MyService");  // 服务名称
LPTSTR  lpDisplayName = _T("MyService");  // 显示名称
LPVOID  lpInfo = _T("U盘状态检测");       //服务描述

const int SLEEP_TIME = 5000;
LPCSTR LOGFILE = "C:\\MyService.txt";//日志文件

//***********************************************************************
//       Prototype Declare Section
//***********************************************************************


int WriteToLog(LPCTSTR str);                                          //log
void ServerReportEvent(LPTSTR szName, LPTSTR szFunction);             //写Windows日志
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);     //U盘检测回调函数


int InitService();                                                   //服务初始化
void RunService();                                                   //运行服务
VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv);             //服务主程序
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);                    //服务工作线程
VOID WINAPI ServiceControlHandler(DWORD ServiceControlRequest);      //服务控制
VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint); //服务状态和SCM交互


//服务控制

BOOL InstallService(LPCTSTR lpServiceName, LPCTSTR lpDisplayName, LPVOID lpInfo);  //安装服务
BOOL UninstallService(LPCTSTR lpServiceName);                                      //卸载服务
BOOL ControlServices(LPCTSTR lpServiceName, DWORD fdwControl);                     //控制服务
BOOL StartServices(LPCTSTR lpServiceName);                                         //开始服务
DWORD StopServices(LPCTSTR lpServiceName, BOOL fStopDependencies, DWORD dwTimeout);//停止服务


int main(int argc, char* argv[])
{
    printf("Service Control \n");
    printf("-install--------\n");
    printf("-uninstall------\n");
    printf("-start ---------\n");
    printf("-stop ----------\n");

    if (argc == 1)
    {
        RunService();
    }

    else
    {
        if (strcmp(argv[1], "-install") == 0)
        {
            if (InstallService(lpServiceName, lpDisplayName, lpInfo))
            {
                // MessageBox(NULL, _T("Services Installed Sucessfully"), "Alert", MB_OK);
            }
        }
        else if (strcmp(argv[1], "-uninstall") == 0)
        {
            if (UninstallService(lpServiceName))
            {
                // MessageBox(NULL, _T("Services UnInstalled Sucessfully"), "Alert", MB_OK);
            }
        }
        else if (strcmp(argv[1], "-stop") == 0)
        {
            if (ControlServices(lpServiceName, SERVICE_CONTROL_STOP))
            {
                //MessageBox(NULL, _T("Services Stopped Sucessfully"), "Alert", MB_OK);
            }
        }
        else if (strcmp(argv[1], "-start") == 0)
        {
            if (StartServices(lpServiceName))
            {
                //MessageBox(NULL, _T("Services Started Sucessfully"), "Alert", MB_OK);
            }
        }
    }

    return 0;
}


//写日志
int WriteToLog(LPCTSTR str)
{
    FILE* log;
    log = fopen(LOGFILE, "a+");
    // Opens for reading and appending;
    // creates the file first if it doesn’t exist.

    if (log == NULL)
    {

        return -1;
    }

    _ftprintf(log, _T("%s\n"), str);
    fclose(log);

    return 0;
}

//服务初始化
int InitService()
{
    //记录服务启动的时间
    TCHAR startTime[100] = { 0 };
    SYSTEMTIME systemTime;
    GetLocalTime(&systemTime);

    wsprintf(startTime, _T("Monitoring started at :%d年%d月%d日 %d时:%d分:%d秒")
             , systemTime.wYear, systemTime.wMonth, systemTime.wDay
             , systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

    int result = WriteToLog(startTime);

    return (result);
}

//运行服务
void RunService()
{
    SERVICE_TABLE_ENTRY dispatchTable[] =
    {
        { lpServiceName, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL },
    };

    if (!StartServiceCtrlDispatcher(dispatchTable))
    {
        ServerReportEvent(lpServiceName, _T("StartServiceCtrlDispatcher Failed!"));
    }

}

//U盘检测回调函数
LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DEVICECHANGE)
    {
        if ((DWORD)wp == DBT_DEVICEARRIVAL)
        {
            DEV_BROADCAST_VOLUME* p = (DEV_BROADCAST_VOLUME*)lp;

            if (p->dbcv_devicetype == DBT_DEVTYP_VOLUME)
            {
                int l = (int)(log(double(p->dbcv_unitmask)) / log(double(2)));
                TCHAR runningState[100] = { 0 };
                SYSTEMTIME systemTime;
                GetLocalTime(&systemTime);
                wsprintf(runningState, _T("%d年%d月%d日 %d时:%d分:%d秒 检测到%c盘插入\n"),
                         systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                         systemTime.wHour, systemTime.wMinute, systemTime.wSecond
                         , 'A' + l);

                int result = WriteToLog(runningState);
                // printf("检测到%c盘插入\n", 'A' + l);
            }
        }

        else if ((DWORD)wp == DBT_DEVICEREMOVECOMPLETE)
        {
            DEV_BROADCAST_VOLUME* p = (DEV_BROADCAST_VOLUME*)lp;

            if (p->dbcv_devicetype == DBT_DEVTYP_VOLUME)
            {
                int l = (int)(log(double(p->dbcv_unitmask)) / log(double(2)));
                // printf("检测到%c盘拔出\n", 'A' + l);
                TCHAR runningState[100] = { 0 };
                SYSTEMTIME systemTime;
                GetLocalTime(&systemTime);
                wsprintf(runningState, _T("%d年%d月%d日 %d时:%d分:%d秒 检测到%c盘拔出\n"),
                         systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                         systemTime.wHour, systemTime.wMinute, systemTime.wSecond
                         , 'A' + l);

                int result = WriteToLog(runningState);
            }
        }

        return TRUE;
    }

    else
    {
        return DefWindowProc(h, msg, wp, lp);
    }
}

//服务操作线程
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));

    wc.lpszClassName = _T("myusbmsg");
    wc.lpfnWndProc = WndProc;

    RegisterClass(&wc);
    HWND h = CreateWindow(_T("myusbmsg"), _T(""), 0, 0, 0, 0, 0,
                          0, 0, GetModuleHandle(0), 0);
    MSG msg;

    //  Periodically check if the service has been requested to stop
    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
    {
        /*
        * Perform main service function here
        */
        if (GetMessage(&msg, 0, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        //Simulate some work by sleeping
        Sleep(3000);
    }

    return ERROR_SUCCESS;
}

//服务控制
VOID WINAPI ServiceControlHandler(DWORD ServiceControlRequest)
{
    SYSTEMTIME systemTime;
    GetLocalTime(&systemTime);
    TCHAR stopTime[100] = { 0 };
    TCHAR shutDownTime[100] = { 0 };

    switch (ServiceControlRequest)
    {
        case SERVICE_CONTROL_PAUSE:
            g_ServiceStatus.dwCurrentState = SERVICE_PAUSED;
            wsprintf(stopTime, _T("Service paused at :%d年%d月%d日 %d时:%d分:%d秒"),
                     systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                     systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

            WriteToLog(stopTime);
            break;

        case SERVICE_CONTROL_CONTINUE:
            g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
            break;

        case SERVICE_CONTROL_STOP:
#if 0
            if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            {
                break;
            }

            /*
            * Perform tasks neccesary to stop the service here
            */

            g_ServiceStatus.dwControlsAccepted = 0;
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            g_ServiceStatus.dwWin32ExitCode = 0;
            g_ServiceStatus.dwCheckPoint = 4;

            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

            // This will signal the worker thread to start shutting down
            SetEvent(g_ServiceStopEvent);
            break;
#endif // 0
            g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            SetEvent(g_ServiceStopEvent);
            wsprintf(stopTime, _T("Service stopped at :%d年%d月%d日 %d时:%d分:%d秒"),
                     systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                     systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

            WriteToLog(stopTime);
            break;

        case SERVICE_CONTROL_SHUTDOWN:
            //ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            // Signal the service to stop.
            g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            SetEvent(g_ServiceStopEvent);
            wsprintf(shutDownTime, _T("Computer shutdown at :%d年%d月%d日 %d时:%d分:%d秒"),
                     systemTime.wYear, systemTime.wMonth, systemTime.wDay,
                     systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

            WriteToLog(shutDownTime);
            break;

        case SERVICE_CONTROL_INTERROGATE:
            // Fall through to send current status.
            break;

        default:
            break;
    }

    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
    //ReportSvcStatus(g_ServiceStatus.dwCurrentState, NO_ERROR, 0);
}

//服务主函数
VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv)
{

    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32;        //SERVICE_WIN32;//SERVICE_WIN32_OWN_PROCESS; //只有一个单独的服务
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;

    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;

    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;


    g_ServiceStatusHandle = RegisterServiceCtrlHandler(lpServiceName,
                            (LPHANDLER_FUNCTION)ServiceControlHandler);

    if (g_ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0)
    {
        // Registering Control Handler failed
        ServerReportEvent(lpServiceName, _T("RegisterServiceCtrlHandler Failed!"));
        return;
    }

    int error;
    // Initialize Service
    error = InitService();

    if (error)
    {
        // 初始化失败，终止服务
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = -1;
        // 退出 ServiceMain
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
        return;
    }

    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
    // Report initial status to the SCM
    //ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );


    // Perform service-specific initialization and work.// Create stop event to wait on later.
    g_ServiceStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (g_ServiceStopEvent == NULL)
    {
        //ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        if (SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus) == FALSE)
        {
            //OutputDebugString(_T("My Sample Service: ServiceMain: SetServiceStatus returned error"));
        }

        return;
    }

    // Tell the service controller we are started
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

#if 0
    // Report running status when initialization is complete.
    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // TO_DO: Perform work until service stops.
    ServerProgram(dwArgc, lpszArgv); //你需要的操作在此添加代码

    while (1)
    {
        // Check whether to stop the service.
        WaitForSingleObject(g_ServiceStopEvent, INFINITE);
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

#endif // 0

#if 0

    do
    {
        if (GetMessage(&msg, 0, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } while (WaitForSingleObject(g_ServiceStopEvent, 5000) == WAIT_TIMEOUT);

#endif


    // Start the thread that will perform the main task of the service
    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

    //OutputDebugString(_T("My Sample Service: ServiceMain: Waiting for Worker Thread to complete"));

    // Wait until our worker thread exits effectively signaling that the service needs to stop
    WaitForSingleObject(hThread, INFINITE);

    //OutputDebugString(_T("My Sample Service: ServiceMain: Worker Thread Stop Event signaled"));


    g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

    /*
    * Perform any cleanup tasks
    */
    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
}

//服务安装
BOOL InstallService(LPCTSTR lpServiceName, LPCTSTR lpDisplayName, LPVOID lpInfo)
{
    SC_HANDLE hSCManager;
    SC_HANDLE hSCService;
    //打开SCM,获取句柄然后允许创建服务
    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCManager == NULL)
    {
        return FALSE;
    }

    //获取当前程序执行路径
    TCHAR szModuleFileName[MAX_PATH];
    GetModuleFileName(NULL, szModuleFileName, MAX_PATH);

    hSCService = CreateService(hSCManager,
                               lpServiceName,
                               lpDisplayName,
                               SERVICE_ALL_ACCESS,
                               SERVICE_WIN32_SHARE_PROCESS,
                               SERVICE_AUTO_START,
                               SERVICE_ERROR_IGNORE,
                               szModuleFileName,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL);

    if (hSCManager != NULL)
    {
        ChangeServiceConfig2(hSCService, SERVICE_CONFIG_DESCRIPTION, &lpInfo);
        //启动服务
        StartService(hSCService, 0, NULL);
        printf("create service success!\n");
    }

    else
    {
        printf("Create service error!\n");
        return FALSE;
    }

    CloseServiceHandle(hSCService);
    CloseServiceHandle(hSCManager);

    return TRUE;
}

//服务卸载
BOOL UninstallService(LPCTSTR lpServiceName)
{
    SC_HANDLE hSCManager;
    SC_HANDLE hSCService;

    SERVICE_STATUS curStatus;
    SERVICE_STATUS ctrlstatus;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCManager == NULL)
    {
        return FALSE;
    }

    hSCService = OpenService(hSCManager, lpServiceName, SERVICE_ALL_ACCESS);

    if (hSCService == NULL)
    {
        return FALSE;
    }

    if (!QueryServiceStatus(hSCService, &curStatus))
    {
        return FALSE;
    }

    if (curStatus.dwCurrentState != SERVICE_STOPPED)
    {
        if (!ControlService(hSCService, SERVICE_CONTROL_STOP, &ctrlstatus))
        {
            printf("Stop Service failed:%d!\n", GetLastError());
            return FALSE;
        }
    }

    if (hSCService != NULL)
    {
        if (DeleteService(hSCService))
        {
            printf("Uninstall Service Success!\n");
        }
    }

    else
    {
        printf("Uninstall Service failed:%d!\n", GetLastError());
    }

    CloseServiceHandle(hSCService);
    CloseServiceHandle(hSCManager);

    return TRUE;
}

//服务开始
BOOL StartServices(LPCTSTR lpServiceName)
{
    SC_HANDLE hService;
    SERVICE_STATUS_PROCESS ssStatus;

    DWORD dwOldCheckPoint;
    DWORD dwStartTickCount;
    DWORD dwWaitTime;
    DWORD dwBytesNeeded;

    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCManager == NULL)
    {
        return FALSE;
    }

    // 打开服务
    hService = OpenService(
                   hSCManager,           // SCM database
                   lpServiceName,        // service name
                   SERVICE_ALL_ACCESS);

    if (hService == NULL)
    {
        return FALSE;
    }

    // 启动服务
    if (!StartService(
                hService,    // handle to service
                0,           // number of arguments
                NULL))       // no arguments
    {
        printf("Service start error (%u).\n", GetLastError());
        return FALSE;
    }

    else
    {
        printf("Service start pending.\n");
    }

    // 验证状态
    if (!QueryServiceStatusEx(
                hService,             // handle to service
                SC_STATUS_PROCESS_INFO, // info level
                (LPBYTE)&ssStatus,              // address of structure
                sizeof(SERVICE_STATUS_PROCESS), // size of structure
                &dwBytesNeeded))              // if buffer too small
    {
        return FALSE;
    }

    // tick count & checkpoint.
    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    // 查询状态，确定 PENDING 状态结束
    while (ssStatus.dwCurrentState == SERVICE_START_PENDING)
    {
        // 等待一段时间
        dwWaitTime = ssStatus.dwWaitHint / 10;

        if (dwWaitTime < 1000)
        {
            dwWaitTime = 1000;
        }

        else if (dwWaitTime > 10000)
        {
            dwWaitTime = 10000;
        }

        Sleep(dwWaitTime);

        // 再次查询
        if (!QueryServiceStatusEx(
                    hService,             // handle to service
                    SC_STATUS_PROCESS_INFO, // info level
                    (LPBYTE)&ssStatus,              // address of structure
                    sizeof(SERVICE_STATUS_PROCESS), // size of structure
                    &dwBytesNeeded))              // if buffer too small
        {
            break;
        }

        if (ssStatus.dwCheckPoint > dwOldCheckPoint)
        {
            // 进程创建中
            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }

        else
        {
            if (GetTickCount() - dwStartTickCount > ssStatus.dwWaitHint)
            {
                // WaitHint 时间到
                break;
            }
        }
    }

    // 关闭句柄
    CloseServiceHandle(hService);

    // 判断是否创建成功（状态由PENDING变为RUNNING）
    if (ssStatus.dwCurrentState == SERVICE_RUNNING)
    {
        printf("StartService SUCCESS.\n");
        return TRUE;
    }

    else
    {
        printf("\nService not started. \n");
        printf("  Current State: %d\n", ssStatus.dwCurrentState);
        printf("  Exit Code: %d\n", ssStatus.dwWin32ExitCode);
        printf("  Service Specific Exit Code: %d\n",
               ssStatus.dwServiceSpecificExitCode);
        printf("  Check Point: %d\n", ssStatus.dwCheckPoint);
        printf("  Wait Hint: %d\n", ssStatus.dwWaitHint);
        return FALSE;
    }
}

//服务控制
BOOL ControlServices(LPCTSTR lpServiceName, DWORD fdwControl)
{
    SC_HANDLE hService;
    SERVICE_STATUS curStatus;

    DWORD fdwAccess;
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCManager == NULL)
    {
        return FALSE;
    }

    // Access
    switch (fdwControl)
    {
        case SERVICE_CONTROL_STOP:
            fdwAccess = SERVICE_STOP;
            break;

        case SERVICE_CONTROL_PAUSE:
        case SERVICE_CONTROL_CONTINUE:
            fdwAccess = SERVICE_PAUSE_CONTINUE;
            break;

        case SERVICE_CONTROL_INTERROGATE:
            fdwAccess = SERVICE_INTERROGATE;
            break;

        default:
            fdwAccess = SERVICE_INTERROGATE;
    }

    // 打开服务
    hService = OpenService(
                   hSCManager,        // SCManager 句柄
                   lpServiceName,       // 服务名
                   fdwAccess);          // 存取权限

    if (hService == NULL)
    {
        printf("OpenService failed (%d)\n", GetLastError());
        return FALSE;
    }

    // 发送控制码
    if (!ControlService(
                hService,   // 服务的句柄
                fdwControl,   // 控制码
                &curStatus))  // 状态
    {
        printf("ControlService failed (%d)\n", GetLastError());
        return FALSE;
    }

    // 显示状态
    /*printf("\nStatus of Sample_Srv: \n");
    printf("  Service Type: 0x%x\n", ssStatus.dwServiceType);
    printf("  Current State: 0x%x\n", ssStatus.dwCurrentState);
    printf("  Controls Accepted: 0x%x\n",
           ssStatus.dwControlsAccepted);
    printf("  Exit Code: %d\n", ssStatus.dwWin32ExitCode);
    printf("  Service Specific Exit Code: %d\n",
           ssStatus.dwServiceSpecificExitCode);
    printf("  Check Point: %d\n", ssStatus.dwCheckPoint);
    printf("  Wait Hint: %d\n", ssStatus.dwWaitHint);*/
    CloseServiceHandle(hSCManager);//关闭服务句柄
    CloseServiceHandle(hService);
    return TRUE;
}

//停止服务
DWORD StopServices(LPCTSTR lpServiceName, BOOL fStopDependencies, DWORD dwTimeout)
{
    SERVICE_STATUS curStatus;
    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwStartTime = GetTickCount();
    DWORD dwBytesNeeded;

    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCManager == NULL)
    {
        return FALSE;
    }

    // 打开服务
    SC_HANDLE  hService = OpenService(
                              hSCManager,          // SCM 句柄
                              lpServiceName,          // 服务名
                              SERVICE_ALL_ACCESS);

    // 查询状态，确定是否已经停止
    if (!QueryServiceStatusEx(
                hService,
                SC_STATUS_PROCESS_INFO,
                (LPBYTE)&ssStatus,
                sizeof(SERVICE_STATUS_PROCESS),
                &dwBytesNeeded))
    {
        return GetLastError();
    }

    if (ssStatus.dwCurrentState == SERVICE_STOPPED)
    {
        return ERROR_SUCCESS;
    }

    // 如果是 STOP_PENDING 状态，则只需等待
    while (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
    {
        Sleep(ssStatus.dwWaitHint);

        // 循环查询，直到状态改变
        if (!QueryServiceStatusEx(
                    hService,
                    SC_STATUS_PROCESS_INFO,
                    (LPBYTE)&ssStatus,
                    sizeof(SERVICE_STATUS_PROCESS),
                    &dwBytesNeeded))
        {
            return GetLastError();
        }

        if (ssStatus.dwCurrentState == SERVICE_STOPPED)
        {
            return ERROR_SUCCESS;
        }

        if (GetTickCount() - dwStartTime > dwTimeout)
        {
            return ERROR_TIMEOUT;
        }
    }

    // 先结束依赖服务
    if (fStopDependencies)
    {
        DWORD i;
        DWORD dwBytesNeeded;
        DWORD dwCount;

        LPENUM_SERVICE_STATUS   lpDependencies = NULL;
        ENUM_SERVICE_STATUS     ess;
        SC_HANDLE               hDepService;

        // 使用 0 大小的 buf,获取buf的大小
        // 如果 EnumDependentServices 直接返回成功，说明没有依赖服务
        if (!EnumDependentServices(hService, SERVICE_ACTIVE,
                                   lpDependencies, 0, &dwBytesNeeded, &dwCount))
        {
            if (GetLastError() != ERROR_MORE_DATA)
            {
                return GetLastError();
            } // Unexpected error

            // 分配缓冲区存储依赖服务的数据
            lpDependencies = (LPENUM_SERVICE_STATUS)HeapAlloc(
                                 GetProcessHeap(), HEAP_ZERO_MEMORY, dwBytesNeeded);

            if (!lpDependencies)
            {
                return GetLastError();
            }

            __try
            {
                // 获得依赖服务
                if (!EnumDependentServices(hService, SERVICE_ACTIVE,
                                           lpDependencies, dwBytesNeeded, &dwBytesNeeded,
                                           &dwCount))
                {
                    return GetLastError();
                }

                for (i = 0; i < dwCount; i++)
                {
                    ess = *(lpDependencies + i);

                    // 打开服务
                    hDepService = OpenService(hSCManager, ess.lpServiceName,
                                              SERVICE_STOP | SERVICE_QUERY_STATUS);

                    if (!hDepService)
                    {
                        return GetLastError();
                    }

                    __try
                    {
                        // 结束服务
                        if (!ControlService(hDepService,
                                            SERVICE_CONTROL_STOP,
                                            &curStatus))
                        {
                            return GetLastError();
                        }

                        // 等待服务结束
                        while (curStatus.dwCurrentState != SERVICE_STOPPED)
                        {
                            Sleep(curStatus.dwWaitHint);

                            if (!QueryServiceStatusEx(
                                        hDepService,
                                        SC_STATUS_PROCESS_INFO,
                                        (LPBYTE)&ssStatus,
                                        sizeof(SERVICE_STATUS_PROCESS),
                                        &dwBytesNeeded))
                            {
                                return GetLastError();
                            }

                            if (curStatus.dwCurrentState == SERVICE_STOPPED)
                            {
                                break;
                            }

                            if (GetTickCount() - dwStartTime > dwTimeout)
                            {
                                return ERROR_TIMEOUT;
                            }
                        }

                    }

                    __finally
                    {
                        // 关闭服务
                        CloseServiceHandle(hDepService);

                    }
                }
            }

            __finally
            {
                // 释放内存
                HeapFree(GetProcessHeap(), 0, lpDependencies);
            }
        }
    }

    // 所有的依赖服务已经结束，结束指定服务
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &curStatus))
    {
        return GetLastError();
    }

    while (curStatus.dwCurrentState != SERVICE_STOPPED)
    {
        Sleep(curStatus.dwWaitHint);

        if (!QueryServiceStatusEx(
                    hService,
                    SC_STATUS_PROCESS_INFO,
                    (LPBYTE)&ssStatus,
                    sizeof(SERVICE_STATUS_PROCESS),
                    &dwBytesNeeded))
        {
            return GetLastError();
        }

        if (curStatus.dwCurrentState == SERVICE_STOPPED)
        {
            break;
        }

        if (GetTickCount() - dwStartTime > dwTimeout)
        {
            return ERROR_TIMEOUT;
        }
    }

    CloseServiceHandle(hSCManager);//关闭服务句柄
    CloseServiceHandle(hService);

    return ERROR_SUCCESS;
}


//写Windows日志
void ServerReportEvent(LPTSTR szName, LPTSTR szFunction)
{
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    unsigned int len = sizeof(szFunction);
    TCHAR *Buffer = new TCHAR[len];

    hEventSource = RegisterEventSource(NULL, szName);

    if (NULL != hEventSource)
    {
        //StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());
        _tcscpy(Buffer, szFunction);
        lpszStrings[0] = szName;
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,        // event log handle
                    EVENTLOG_ERROR_TYPE, // event type
                    0,                   // event category
                    666,           // event identifier
                    NULL,                // no security identifier
                    2,                   // size of lpszStrings array
                    0,                   // no binary data
                    lpszStrings,         // array of strings
                    NULL);               // no binary data
        DeregisterEventSource(hEventSource);
    }
}

//服务状态和SCM交互
VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    g_ServiceStatus.dwCurrentState = dwCurrentState;
    g_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
    g_ServiceStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
    {
        g_ServiceStatus.dwControlsAccepted = 0;
    }

    else
    {
        g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }

    if ((dwCurrentState == SERVICE_RUNNING) ||
            (dwCurrentState == SERVICE_STOPPED))
    {
        g_ServiceStatus.dwCheckPoint = 0;
    }

    else
    {
        g_ServiceStatus.dwCheckPoint = dwCheckPoint++;
    }

    // Report the status of the service to the SCM.
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
}
