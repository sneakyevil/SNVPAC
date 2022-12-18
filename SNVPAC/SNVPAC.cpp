#include <algorithm>
#include <iostream>
#include <conio.h>
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>

#include "Console.hpp"
#include "NvAPI.hpp"

#define APP_NAME                        "SNVPAC"
#define APP_CONSOLE_HOTKEY_KEY          VK_F2
#define APP_CONSOLE_HOTKEY_MOD          (MOD_ALT | MOD_NOREPEAT)

static CNvAPI m_gNvAPI;
static int m_gNvHandle = -1;

struct AppConfiguration_t
{
    std::string m_Name;
    
    int m_Vibrance = 0;
};
static AppConfiguration_t m_gAppConfigDefault;
static std::vector<AppConfiguration_t> m_gAppConfigs;

namespace Utils
{
    bool Sort_Alphabet(std::string m_Left, std::string m_Right)
    {
        std::transform(m_Left.begin(), m_Left.end(), m_Left.begin(), tolower);
        std::transform(m_Right.begin(), m_Right.end(), m_Right.begin(), tolower);
        return m_Right > m_Left;
    }

    void AddConfiguration(AppConfiguration_t& m_AppConfig)
    {
        m_gAppConfigs.emplace_back(m_AppConfig);
        std::sort(m_gAppConfigs.begin(), m_gAppConfigs.end(), [](const AppConfiguration_t& m_Left, const AppConfiguration_t& m_Right) -> bool { return Sort_Alphabet(m_Left.m_Name, m_Right.m_Name); });
    }

    void SetConfiguration(AppConfiguration_t* m_AppConfig)
    {
        static AppConfiguration_t m_LastSettings;
        if (m_LastSettings.m_Vibrance != m_AppConfig->m_Vibrance)
        {
            m_LastSettings.m_Vibrance = m_AppConfig->m_Vibrance;
            m_gNvAPI.SetDVCLevel(m_gNvHandle, 0, static_cast<int>(m_AppConfig->m_Vibrance * 1.26));
        }
    }

    const char* m_ConfigurationFile = "config.dat";
    void SaveConfiguration()
    {
        FILE* m_File;
        fopen_s(&m_File, m_ConfigurationFile, "w");
        if (m_File)
        {
            for (AppConfiguration_t& m_AppConfig : m_gAppConfigs)
            {
                fwrite(&m_AppConfig.m_Name[0], sizeof(char), m_AppConfig.m_Name.size() + 1, m_File);
                fwrite(&m_AppConfig.m_Vibrance, sizeof(int), 1, m_File);
            }

            fclose(m_File);
        }
    }

    void LoadConfiguration()
    {
        FILE* m_File;
        fopen_s(&m_File, m_ConfigurationFile, "r");
        if (m_File)
        {
            fseek(m_File, 0, SEEK_END);
            long m_FileSize     = ftell(m_File);
            uint8_t* m_RawFile  = new uint8_t[m_FileSize];

            fseek(m_File, 0, SEEK_SET);
            fread(m_RawFile, sizeof(uint8_t), m_FileSize, m_File);

            fclose(m_File);

            int m_Offset = 0;
            while (1)
            {
                if (m_Offset >= m_FileSize) break;

                AppConfiguration_t m_AppConfig;
                m_AppConfig.m_Name = reinterpret_cast<const char*>(&m_RawFile[m_Offset]);

                m_Offset += (m_AppConfig.m_Name.size() + 1);

                memcpy(&m_AppConfig.m_Vibrance, &m_RawFile[m_Offset], sizeof(int));

                m_Offset += sizeof(int);

                AddConfiguration(m_AppConfig);
            }

            delete m_RawFile;
        }
    }

    std::vector<std::string> GetConfigurationFormatted()
    {
        std::vector<std::string> m_List;

        int m_iCount = 1;
        for (AppConfiguration_t& m_AppConfig : m_gAppConfigs)
        {
            std::string m_App = std::to_string(m_iCount) + ". ";
            m_App += m_AppConfig.m_Name + " ";
            m_App += "[V: " + std::to_string(m_AppConfig.m_Vibrance) + "]";

            m_List.emplace_back(m_App);
            ++m_iCount;
        }

        return m_List;
    }

    std::string GetProcessByID(DWORD m_ProcessID)
    {
        HANDLE m_Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (m_Snapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 m_Process;
            m_Process.dwSize = sizeof(PROCESSENTRY32);

            if (Process32First(m_Snapshot, &m_Process))
            {
                while (Process32Next(m_Snapshot, &m_Process))
                {
                    if (m_Process.th32ProcessID == m_ProcessID)
                    {
                        CloseHandle(m_Snapshot);
                        return m_Process.szExeFile;
                    }
                }
            }

            CloseHandle(m_Snapshot);
        }

        return "";
    }

    std::vector<std::string> GetListOfProcesses()
    {
        std::vector<std::string> m_List;

        HANDLE m_Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (m_Snapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 m_Process; 
            m_Process.dwSize = sizeof(PROCESSENTRY32);

            if (Process32First(m_Snapshot, &m_Process))
            {
                while (Process32Next(m_Snapshot, &m_Process))
                {
                    if (GetCurrentProcessId() == m_Process.th32ProcessID) continue;
                    if (!strstr(m_Process.szExeFile, ".exe")) continue;

                    bool m_AlreadyExist = false;
                    for (std::string& m_Check : m_List)
                    {
                        if (strstr(m_Check.c_str(), m_Process.szExeFile))
                        {
                            m_AlreadyExist = true;
                            break;
                        }
                    }
                    if (m_AlreadyExist) continue;

                    m_List.emplace_back(m_Process.szExeFile);
                }
            }

            CloseHandle(m_Snapshot);
        }

        return m_List;
    }
}

namespace Program
{
    // Set process to idle when console hidden so we don't rape too much cpu while gaming...
    void ToggleIdlePriority(bool m_Background)
    { SetPriorityClass(GetCurrentProcess(), (m_Background ? IDLE_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS)); }

    // Set affinity to last core so we don't rape too much core with this crap...
    void SetAffinity()
    {
        SYSTEM_INFO m_SystemInfo;
        GetSystemInfo(&m_SystemInfo);

        if (m_SystemInfo.dwNumberOfProcessors > 1)
        {
            DWORD m_LastCoreMask(1 << (m_SystemInfo.dwNumberOfProcessors - 1));
            SetProcessAffinityMask(GetCurrentProcess(), m_LastCoreMask);
        }
    }

    const char* m_RunRegPath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    bool IsRegisteredOnStartup()
    {
        bool m_Return = false;
        HKEY m_Registry = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, m_RunRegPath, 0, KEY_READ, &m_Registry) == ERROR_SUCCESS)
        {
            DWORD m_Dummy[2];
            m_Return = RegQueryValueExA(m_Registry, APP_NAME, 0, &m_Dummy[0], 0, &m_Dummy[1]) == ERROR_SUCCESS;
            RegCloseKey(m_Registry);
        }

        return m_Return;
    }

    void ToggleOnStartup()
    {
        bool m_Registered = IsRegisteredOnStartup();

        HKEY m_Registry = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, m_RunRegPath, 0, KEY_ALL_ACCESS, &m_Registry) != ERROR_SUCCESS)
            return;


        if (m_Registered) RegDeleteValueA(m_Registry, APP_NAME);
        else
        {
            char m_FilePath[MAX_PATH];
            GetModuleFileNameA(0, m_FilePath, MAX_PATH);
            RegSetValueExA(m_Registry, APP_NAME, 0, REG_SZ, reinterpret_cast<BYTE*>(m_FilePath), strlen(m_FilePath));
        }

        RegCloseKey(m_Registry);
    }

    void Info()
    {
        Console::Clear();
        Console::Print(Console::m_DefaultColor, '!', "SNVPAC made by "); Console::Print(CLR_BRED, "sneakyevil\n");
        Console::Print(Console::m_DefaultColor, '!', "Startup: ");

        if (IsRegisteredOnStartup())
            Console::Print(CLR_BGREEN, "Yes\n");
        else
            Console::Print(CLR_BRED, "No\n");

        Console::Print(Console::m_DefaultColor, "\n");
    }

    int Listbox(std::string m_Text, std::vector<std::string>* m_List, int m_DeltaShow, std::string m_Prefix)
    {
        int m_Index = 0;
        bool m_Update = true;
        int m_Size = static_cast<int>(m_List->size());

        while (1)
        {
            if (m_Update)
            {
                if (0 > m_DeltaShow || m_DeltaShow >= m_Size) m_DeltaShow = m_Size - 1;

                if (0 > m_Index) m_Index = m_Size - 1;
                else if (m_Index >= m_Size) m_Index = 0;

                m_Update = false;

                Info();
                Console::Print(Console::m_DefaultColor, '!', m_Text);
                Console::DrawListbox(m_Index, m_List->data(), m_Size, m_DeltaShow, m_Prefix);
            }

            switch (_getch())
            {
                case 8: return -1;
                case 13: return m_Index;
                case 72:
                {
                    m_Index--;
                    m_Update = true;
                }
                break;
                case 80:
                {
                    m_Index++;
                    m_Update = true;
                }
                break;
            }
        }
    }

    void WaitForInput() { int m_Dummy = getchar(); }

    int Error(std::string m_Error)
    {
        Info();
        Console::Print(CLR_BRED, '!', m_Error);
        WaitForInput();
        return EXIT_FAILURE;
    }

    int GetVibranceValue(int m_Delimer)
    {
        std::vector<std::string> m_Array;
        for (int i = 0; static_cast<int>(50 / m_Delimer) >= i; ++i)
            m_Array.emplace_back(std::to_string(i * m_Delimer));

        int m_Index = Program::Listbox("Select Vibrance:\n", &m_Array, -1, "\t");
        if (m_Index == -1)
            return -1;

        return atoi(&m_Array[m_Index][0]);
    }
}

namespace MessageHandler
{
    void ConsoleHotkey()
    {
        if (IsWindowVisible(Console::m_Window))
        {
            ShowWindow(Console::m_Window, SW_HIDE);

           Program::ToggleIdlePriority(true);
        }
        else
        {
            ShowWindow(Console::m_Window, SW_RESTORE);
            SetForegroundWindow(Console::m_Window);
            SetFocus(Console::m_Window);
            SetActiveWindow(Console::m_Window);

            Program::ToggleIdlePriority(false);
        }
    }

    void __stdcall Event(HWINEVENTHOOK m_Hook, DWORD m_Event, HWND m_Window, LONG m_ID, LONG m_ChildID, DWORD m_Thread, DWORD m_Time)
    {
        static HWND m_LastWindow = 0;
        if (m_Window == m_LastWindow) return;
        else m_LastWindow = m_Window;

        DWORD m_ProcessID = 0x0;
        GetWindowThreadProcessId(m_Window, &m_ProcessID);

        std::string m_ProcessName = Utils::GetProcessByID(m_ProcessID);

        for (AppConfiguration_t& m_AppConfig : m_gAppConfigs)
        {
            if (m_AppConfig.m_Name.size() != m_ProcessName.size()) continue;
            if (m_AppConfig.m_Name != m_ProcessName) continue;

            Utils::SetConfiguration(&m_AppConfig);
            return;
        }

        Utils::SetConfiguration(&m_gAppConfigDefault);
    }

    DWORD __stdcall Thread(void* m_Reserved)
    {
        RegisterHotKey(0, 1, APP_CONSOLE_HOTKEY_MOD, APP_CONSOLE_HOTKEY_KEY);
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, 0, Event, 0, 0, WINEVENT_OUTOFCONTEXT);

        int m_MsgRet = 0;
        MSG m_Msg;
        while ((m_MsgRet = GetMessageA(&m_Msg, 0, 0, 0)) != 0)
        { 
            if (m_MsgRet == -1) continue;

            switch (m_Msg.message)
            {
                case WM_HOTKEY:
                {
                    switch (m_Msg.wParam)
                    {
                        case 1: ConsoleHotkey(); break;
                    }
                }
                break;
            }

            TranslateMessage(&m_Msg);
            DispatchMessageA(&m_Msg);
        }

        return 0x0;
    }
}

int main()
{
    Console::Initialize("Simple NVIDIA Vibrance Per-app Configuration");
    SetConsoleOutputCP(437);

    CreateEventA(0, 0, 0, "SNVPAC_sneakyevil");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxA(0, "There is already another instance of this application running!\nPress: ALT + F2 to restore console window for it.", APP_NAME, MB_OK | MB_ICONERROR);
        return 0;
    }

    if (m_gNvAPI.Loaded())
    {
        m_gNvAPI.EnumDisplayHandle(0, &m_gNvHandle);

        NvDVCInfo_t m_DVCInfo;
        m_gNvAPI.GetDVCInfo(m_gNvHandle, 0, &m_DVCInfo);

        m_gAppConfigDefault.m_Vibrance = m_DVCInfo.m_CurLevel;
    }
    else
    {
        MessageBoxA(0, "Couldn't initialize Nvidia API. Nvidia driver not installed?", APP_NAME, MB_OK | MB_ICONERROR);
        return 0;
    }

    MessageHandler::ConsoleHotkey(); // Auto-Hide on launch
    Program::SetAffinity();
    CreateThread(0, 0, MessageHandler::Thread, 0, 0, 0);

    Utils::LoadConfiguration();

    while (1)
    {
        static std::vector<std::string> m_MainMenu = { "Add Application", "Edit Application", "Toggle Startup" };

        switch (Program::Listbox("Select Option:\n", &m_MainMenu, -1, "\t"))
        {
            case 0:
            {
                std::vector<std::string> m_Processes = Utils::GetListOfProcesses();
                std::sort(m_Processes.begin(), m_Processes.end(), Utils::Sort_Alphabet);
                int m_ProcessIndex = Program::Listbox("Select Process:\n", &m_Processes, 5, "\t");
                if (m_ProcessIndex == -1)
                    break;

                AppConfiguration_t m_AppConfig;
                m_AppConfig.m_Name = m_Processes[m_ProcessIndex];
                m_AppConfig.m_Vibrance = Program::GetVibranceValue(5);

                if (m_AppConfig.m_Vibrance == -1)
                    break;

                Utils::AddConfiguration(m_AppConfig);
                Utils::SaveConfiguration();
            }
            break;
            case 1:
            {
                if (m_gAppConfigs.empty())
                {
                    Program::Error("You need to first add some application before you can edit one.");
                    break;
                }

                std::vector<std::string> m_AppConfigs = Utils::GetConfigurationFormatted();
                int m_AppIndex = Program::Listbox("Select Application:\n", &m_AppConfigs, 5, "\t");
                if (m_AppIndex == -1)
                    break;

                int m_NewVibrance = Program::GetVibranceValue(5);
                if (m_NewVibrance != -1)
                {
                    if (m_NewVibrance == 0)
                        m_gAppConfigs.erase(m_gAppConfigs.begin() + m_AppIndex);
                    else
                        m_gAppConfigs[m_AppIndex].m_Vibrance = m_NewVibrance;
                    Utils::SaveConfiguration();
                }
            }
            break;
            case 2: Program::ToggleOnStartup(); break;
        }
    }

    return 0;
}