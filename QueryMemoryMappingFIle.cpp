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
    // �n��Service Control Handler
    Service_Status_Handle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, ServiceCtrlHandlerEx, SERVICE_NAME);

    if (Service_Status_Handle == NULL) return;
    // �]�w���檬�A
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
    if (argc >= 2)   //�P�_�O�_�s�b�Ѽ�
    {
        size_t origsize = strlen(argv[2]) + 1;
        const size_t newsize = 100;
        size_t convertedChars = 0;
        wchar_t wcstring[newsize];
        mbstowcs_s(&convertedChars, wcstring, origsize, argv[2], _TRUNCATE);
        wcscpy_s(SERVICE_NAME, sizeof(SERVICE_NAME), wcstring);

        // ���o�A�ȵ{���ҰʰѼ�
        const char* param = argv[1];
        //----- �w�˪A��
        if (!_stricmp(param, "/install"))
        {
            InstallService();
            return 0;
        }
        //----- �����A��
        if (!_stricmp(param, "/uninstall"))
        {
            RemoveService();
            return 0;
        }
        //----- �ҰʪA��
        if (!_stricmp(param, "/start"))
        {
            StartService();
            return 0;
        }
        //----- ����A��
        if (!_stricmp(param, "/stop"))
        {
            StopService();
            return 0;
        }
        //----- ����A��
        if (!_stricmp(param, "/run"))
        {
            RunService();
            return 0;
        }
    }
    return 0;
}

//---------- �w�˪A�� ----------//
void InstallService(void)
{
    wchar_t depends[64] = L"ProfSvc";
    int tails = (int)wcslen(depends);
    depends[tails + 2] = depends[tails];
    depends[tails + 3] = depends[tails + 1];
    depends[tails] = NULL;
    depends[tails + 1] = NULL;



    // �A�ȵ{�����|
    wchar_t wszSvcPath[MAX_PATH];
    //----- ���o�A�ȵ{�����|
    if (GetModuleFileName(NULL, wszSvcPath, MAX_PATH))
    {
        wcscat_s(wszSvcPath, MAX_PATH, L" /run ");
        wcscat_s(wszSvcPath, MAX_PATH, SERVICE_NAME);
        //----- �ҰʪA�ȱ���޲z��
        SC_HANDLE schSCMan = OpenSCManager(
            NULL,                   // �q���W��
            NULL,                   // ��Ʈw�W��
            SC_MANAGER_ALL_ACCESS   // �s���v��
        );
        if (schSCMan == NULL)
            MessageBoxW(NULL, L"�Ұ� SCM ����", L"����", MB_OK);
        else
        {
            //----- �w�˪A��
            SC_HANDLE schSvc = CreateService(
                schSCMan,                   // SCM �� HANDLE
                SERVICE_NAME,               // �A�Ȫ��w�˦W��
                SERVICE_NAME,               // �A�Ȫ���ܦW��
                SC_MANAGER_ALL_ACCESS,      // �s���v��
                SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,    // �A�����O
                SERVICE_AUTO_START,         // �Ұ�����
                SERVICE_ERROR_IGNORE,       // ���~�B�z����
                wszSvcPath,                 // �A�ȵ{�����|
                L"ProfSvc_Group",           // �A�ȸ��J�s��
                NULL,                       // �A�ȸ��J�s�հѼ�
                depends,                // �A�Ȭ̩ۨ�
                NULL,                       // ����A�Ȫ��b��
                NULL                        // ����A�Ȫ��K�X
            );
            if (schSvc == NULL)
                MessageBoxW(NULL, L"�w�˥���", L"����", MB_OK);
            else
            {
                //MessageBoxW(NULL, L"�w�˦��\", L"����", MB_OK);
                printf("Install Service success!\n");
                //-------------
                //  �ҰʪA��
                //-------------
            }
            CloseServiceHandle(schSvc);
        }
        CloseServiceHandle(schSCMan);
    }
    else
        MessageBoxW(NULL, L"���o����", L"����", MB_OK);
}

//------- Remove service -------//
void RemoveService(void)
{
    //----- �ҰʪA�ȱ���޲z��
    SC_HANDLE schSCMan = OpenSCManager(
        NULL,                   // �q���W��
        NULL,                   // ��Ʈw�W��
        SC_MANAGER_ALL_ACCESS   // �s���v��
    );
    if (schSCMan == NULL)
        MessageBoxW(NULL, L"�Ұ� SCM ����", L"����", MB_OK);
    else
    {
        //----- �}�ҪA��
        SC_HANDLE schSvc = OpenServiceW(
            schSCMan,               // SCM �� HANDLE
            SERVICE_NAME,           // �A�ȦW��
            SC_MANAGER_ALL_ACCESS   // �s���v��
        );
        if (schSvc == NULL)
            MessageBoxW(NULL, L"�}�ҥ���", L"����", MB_OK);
        else
        {
            //----- �����A��
            if (DeleteService(schSvc))
            {
                //MessageBoxW(NULL, L"�������\", L"����", MB_OK);
                printf("Uninstall Service success!\n");
            }
            else
                MessageBoxW(NULL, L"��������", L"����", MB_OK);
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
    //status.dwCurrentState�K�O�ثeService�{�������A��
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