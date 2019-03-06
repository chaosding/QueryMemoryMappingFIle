#include <stdio.h>
#include <wchar.h>
#include <windows.h>
#include <string.h>


wchar_t SERVICE_NAME[128];
SERVICE_STATUS          Service_Status;
SERVICE_STATUS_HANDLE   Service_Status_Handle;
HANDLE                  ghSvcStopEvent = NULL;
HANDLE                  FileMappingHandle = INVALID_HANDLE_VALUE;



void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

int QueryMemoryFile(void);

void logs(char *strings, ...);
void logs(wchar_t *strings, ...);

void InstallService(void);
void RemoveService(void);
void StartService(void);
void StopService(void);
void RunService(void);

wchar_t envAPP_PATH[MAX_PATH];
wchar_t LogPath[MAX_PATH];

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    // 登錄Service Control Handler
    Service_Status_Handle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, ServiceCtrlHandlerEx, SERVICE_NAME);

    if (Service_Status_Handle == NULL) return;
    // 設定執行狀態
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
    // Perform service-specific initialization and work.

    ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }

    // Report running status when initialization is complete.
    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    if (!SetServiceStatus(Service_Status_Handle, &Service_Status))
    {
        return;
    }

    //initalize parameter
    SecureZeroMemory(&envAPP_PATH, sizeof(envAPP_PATH));
    GetEnvironmentVariable(L"APP_PATH", envAPP_PATH, sizeof(wchar_t) * MAX_PATH);
    GetShortPathName(envAPP_PATH, envAPP_PATH, _countof(envAPP_PATH));

    SecureZeroMemory(&LogPath, sizeof(LogPath));
    wcscpy_s(LogPath, sizeof(LogPath), envAPP_PATH);
    wcscat_s(LogPath, sizeof(LogPath), L"\\logs\\QueryMemoryMappedFile.log");

    for (int i = 0; i < 10 && QueryMemoryFile();i++) Sleep(1000);

}

DWORD WINAPI ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    switch (dwControl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        break;
    default:
        break;
    }
    return NO_ERROR;
}


int QueryMemoryFile(void)
{
    int MemSize = 1024;
    //FileMappingHandle = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, MemSize, L"Global\\MemTest");
    FileMappingHandle=OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, L"Global\\MemTest");
    if (FileMappingHandle == 0 || FileMappingHandle == INVALID_HANDLE_VALUE)
    {
        logs("OpenFileMapping function fail.");
        return 1;
    }
    else
    {
        LPTSTR lpMapAddr = (LPTSTR)MapViewOfFile(FileMappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (lpMapAddr == NULL)
        {
            logs("Can not get the memoryfile address.");
            return 1;
        }
        else
        {            
            logs("OpenFileMapping function success. Memory address: 0x%08x",(unsigned long long)lpMapAddr);
        }
        UnmapViewOfFile(lpMapAddr);        
        return 0;
    }
}



VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.
    Service_Status.dwServiceType = SERVICE_WIN32;
    Service_Status.dwCurrentState = dwCurrentState;
    Service_Status.dwWin32ExitCode = dwWin32ExitCode;
    Service_Status.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        Service_Status.dwControlsAccepted = 0;
    else
        Service_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_PRESHUTDOWN;

    if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
        Service_Status.dwCheckPoint = 0;
    else
        Service_Status.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(Service_Status_Handle, &Service_Status);
}


void logs(char *strings, ...)
{

    if (wcscmp(LogPath, L"") != 0)
    {
        char strbuffer[4012];
        va_list args;
        va_start(args, strings);
        vsprintf_s(strbuffer, strings, args);
        va_end(args);

        SYSTEMTIME st, lt;

        GetSystemTime(&st);
        GetLocalTime(&lt);

        char record[512];
        ZeroMemory(&record, 512);

        sprintf_s(record, 512, "[%04d-%02d-%02d %02d:%02d:%02d]\t%s\r\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, strbuffer);

        HANDLE hdstFile;

        hdstFile = CreateFile(LogPath, FILE_SHARE_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, NULL, NULL);
        if (hdstFile != INVALID_HANDLE_VALUE)
        {
            LARGE_INTEGER lic = { 0 };
            SetFilePointerEx(hdstFile, lic, &lic, FILE_END);

            DWORD dwCopyReadBytes = 0;
            DWORD dwWriteBytes = (DWORD)strlen(record);
            WriteFile(hdstFile, record, dwWriteBytes, &dwCopyReadBytes, NULL);

        }
        FlushFileBuffers(hdstFile);
        CloseHandle(hdstFile);
    }

}


void logs(wchar_t *format, ...)
{
    wchar_t strbuffer[4012];
    va_list args;
    va_start(args, format);
    vswprintf_s(strbuffer, format, args);
    va_end(args);

    size_t   i;
    char record[4096];
    ZeroMemory(&record, 4096);
    wcstombs_s(&i, record, strbuffer, wcslen(strbuffer));
    logs(record);
}

int main(int argc, char* argv[])
{
    if (argc >= 2)   //判斷是否存在參數
    {
        size_t origsize = strlen(argv[2]) + 1;
        const size_t newsize = 100;
        size_t convertedChars = 0;
        wchar_t wcstring[newsize];
        mbstowcs_s(&convertedChars, wcstring, origsize, argv[2], _TRUNCATE);
        wcscpy_s(SERVICE_NAME, sizeof(SERVICE_NAME), wcstring);

        // 取得服務程式啟動參數
        const char* param = argv[1];
        //----- 安裝服務
        if (!_stricmp(param, "/install"))
        {
            InstallService();
            return 0;
        }
        //----- 移除服務
        if (!_stricmp(param, "/uninstall"))
        {
            RemoveService();
            return 0;
        }
        //----- 啟動服務
        if (!_stricmp(param, "/start"))
        {
            StartService();
            return 0;
        }
        //----- 停止服務
        if (!_stricmp(param, "/stop"))
        {
            StopService();
            return 0;
        }
        //----- 執行服務
        if (!_stricmp(param, "/run"))
        {
            RunService();
            return 0;
        }
    }
    return 0;
}

//---------- 安裝服務 ----------//
void InstallService(void)
{
    wchar_t depends[64] = L"ProfSvc";
    int tails = (int)wcslen(depends);
    depends[tails + 2] = depends[tails];
    depends[tails + 3] = depends[tails + 1];
    depends[tails] = NULL;
    depends[tails + 1] = NULL;



    // 服務程式路徑
    wchar_t wszSvcPath[MAX_PATH];
    //----- 取得服務程式路徑
    if (GetModuleFileName(NULL, wszSvcPath, MAX_PATH))
    {
        wcscat_s(wszSvcPath, MAX_PATH, L" /run ");
        wcscat_s(wszSvcPath, MAX_PATH, SERVICE_NAME);
        //----- 啟動服務控制管理員
        SC_HANDLE schSCMan = OpenSCManager(
            NULL,                   // 電腦名稱
            NULL,                   // 資料庫名稱
            SC_MANAGER_ALL_ACCESS   // 存取權限
        );
        if (schSCMan == NULL)
            MessageBoxW(NULL, L"啟動 SCM 失敗", L"提示", MB_OK);
        else
        {
            //----- 安裝服務
            SC_HANDLE schSvc = CreateService(
                schSCMan,                   // SCM 的 HANDLE
                SERVICE_NAME,               // 服務的安裝名稱
                SERVICE_NAME,               // 服務的顯示名稱
                SC_MANAGER_ALL_ACCESS,      // 存取權限
                SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,    // 服務類別
                SERVICE_AUTO_START,         // 啟動類型
                SERVICE_ERROR_IGNORE,       // 錯誤處理類型
                wszSvcPath,                 // 服務程式路徑
                L"ProfSvc_Group",           // 服務載入群組
                NULL,                       // 服務載入群組參數
                depends,                // 服務相依性
                NULL,                       // 執行服務的帳號
                NULL                        // 執行服務的密碼
            );
            if (schSvc == NULL)
                MessageBoxW(NULL, L"安裝失敗", L"提示", MB_OK);
            else
            {
                //MessageBoxW(NULL, L"安裝成功", L"提示", MB_OK);
                printf("Install Service success!\n");
                //-------------
                //  啟動服務
                //-------------
            }
            CloseServiceHandle(schSvc);
        }
        CloseServiceHandle(schSCMan);
    }
    else
        MessageBoxW(NULL, L"取得失敗", L"提示", MB_OK);
}

//------- Remove service -------//
void RemoveService(void)
{
    //----- 啟動服務控制管理員
    SC_HANDLE schSCMan = OpenSCManager(
        NULL,                   // 電腦名稱
        NULL,                   // 資料庫名稱
        SC_MANAGER_ALL_ACCESS   // 存取權限
    );
    if (schSCMan == NULL)
        MessageBoxW(NULL, L"啟動 SCM 失敗", L"提示", MB_OK);
    else
    {
        //----- 開啟服務
        SC_HANDLE schSvc = OpenServiceW(
            schSCMan,               // SCM 的 HANDLE
            SERVICE_NAME,           // 服務名稱
            SC_MANAGER_ALL_ACCESS   // 存取權限
        );
        if (schSvc == NULL)
            MessageBoxW(NULL, L"開啟失敗", L"提示", MB_OK);
        else
        {
            //----- 移除服務
            if (DeleteService(schSvc))
            {
                //MessageBoxW(NULL, L"移除成功", L"提示", MB_OK);
                printf("Uninstall Service success!\n");
            }
            else
                MessageBoxW(NULL, L"移除失敗", L"提示", MB_OK);
        }
        CloseServiceHandle(schSvc);
    }
    CloseServiceHandle(schSCMan);
}

//------- Start service --------//
void StartService(void)
{
    SC_HANDLE scm_handle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm_handle == NULL)
    {
        // error
    }
    SC_HANDLE sv_handle = OpenService(scm_handle, SERVICE_NAME, SC_MANAGER_ALL_ACCESS);
    if (sv_handle == NULL)
    {
    }
    if (!StartService(sv_handle, 0, NULL))
    {
    }
    CloseServiceHandle(sv_handle);
    CloseServiceHandle(scm_handle);

}

//------- Stop service ---------//
void StopService(void)
{
    SC_HANDLE scm_handle = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scm_handle == NULL)
    {
        // error
    }

    SC_HANDLE sv_handle = OpenService(scm_handle, SERVICE_NAME, SC_MANAGER_ALL_ACCESS);
    if (sv_handle == NULL)
    {

    }
    SERVICE_STATUS status;
    if (!ControlService(sv_handle, SERVICE_CONTROL_STOP, &status))
    {
        printf("Stop fail %x\n", GetLastError());
    }
    //status.dwCurrentState便是目前Service程式的狀態值
    CloseServiceHandle(sv_handle);
    CloseServiceHandle(scm_handle);

    //MessageBoxW(NULL, L"Service Stop", L"Message", MB_OK);
    printf("Service Stop!\n");
}

//------- Run service ----------//
void RunService(void)
{
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.

    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
    }
}