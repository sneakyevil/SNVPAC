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

static CNvAPI g_NvAPI;
static int g_NvHandle = -1;

struct AppConfiguration_t
{
    std::string m_Name;
    uint32_t m_Hash = 0;
    
    int m_Vibrance = 0;
};
static AppConfiguration_t g_AppConfigDefault;
static std::vector<AppConfiguration_t> g_AppConfigs;

namespace Utils
{
    bool Sort_Alphabet(std::string m_Left, std::string m_Right)
    {
        std::transform(m_Left.begin(), m_Left.end(), m_Left.begin(), tolower);
        std::transform(m_Right.begin(), m_Right.end(), m_Right.begin(), tolower);
        return m_Right > m_Left;
    }

    std::string GetProcessNameByPID(DWORD p_PID)
    {
        HANDLE _Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (_Snapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 _ProcessEntry;
            _ProcessEntry.dwSize = sizeof(PROCESSENTRY32);

            if (Process32First(_Snapshot, &_ProcessEntry))
            {
                while (Process32Next(_Snapshot, &_ProcessEntry))
                {
                    if (_ProcessEntry.th32ProcessID == p_PID)
                    {
                        CloseHandle(_Snapshot);
                        return _ProcessEntry.szExeFile;
                    }
                }
            }

            CloseHandle(_Snapshot);
        }

        return "";
    }

    std::vector<std::string> GetListOfProcesses()
    {
        std::vector<std::string> _List;

        HANDLE _Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (_Snapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 _ProcessEntry; 
            _ProcessEntry.dwSize = sizeof(PROCESSENTRY32);

            if (Process32First(_Snapshot, &_ProcessEntry))
            {
                while (Process32Next(_Snapshot, &_ProcessEntry))
                {
                    if (GetCurrentProcessId() == _ProcessEntry.th32ProcessID || !strstr(_ProcessEntry.szExeFile, ".exe")) {
                        continue;
                    }

                    bool _AlreadyExist = false;
                    {
                        for (std::string& m_Check : _List)
                        {
                            if (strstr(m_Check.c_str(), _ProcessEntry.szExeFile))
                            {
                                _AlreadyExist = true;
                                break;
                            }
                        }
                    }
                    if (!_AlreadyExist) {
                        _List.emplace_back(_ProcessEntry.szExeFile);
                    }
                }
            }

            CloseHandle(_Snapshot);
        }

        return _List;
    }

    uint32_t JOAAT(const char* p_String) 
    {
        uint32_t _Hash = 0;

        while (*p_String)
        {
            _Hash += *p_String;
            _Hash += (_Hash << 10);
            _Hash ^= (_Hash >> 6);
            ++p_String;
        }

        _Hash += (_Hash << 3);
        _Hash ^= (_Hash >> 11);
        _Hash += (_Hash << 15);

        return _Hash;
    }
}

namespace Configuration
{
    void Add(AppConfiguration_t& m_AppConfig)
    {
        g_AppConfigs.emplace_back(m_AppConfig);
        std::sort(g_AppConfigs.begin(), g_AppConfigs.end(), [](const AppConfiguration_t& m_Left, const AppConfiguration_t& m_Right) -> bool { return Utils::Sort_Alphabet(m_Left.m_Name, m_Right.m_Name); });
    }

    void Set(AppConfiguration_t* m_AppConfig)
    {
        static AppConfiguration_t m_LastConfig;
        if (m_LastConfig.m_Vibrance != m_AppConfig->m_Vibrance)
        {
            m_LastConfig.m_Vibrance = m_AppConfig->m_Vibrance;
            g_NvAPI.SetDVCLevel(g_NvHandle, 0, static_cast<int>(m_AppConfig->m_Vibrance * 1.26));
        }
    }

    const char* m_FileName = "config.dat";
    std::string GetFilePath()
    {
        std::string m_Path(MAX_PATH, '\0');
        m_Path.resize(GetModuleFileNameA(0, &m_Path[0], m_Path.size()));

        size_t m_Index = m_Path.find_last_of("\\/");
        m_Path.erase(m_Index, m_Path.size() - m_Index);
        return (m_Path + "\\" + m_FileName);
    }

    void SaveFile()
    {
        FILE* m_File;
        fopen_s(&m_File, GetFilePath().c_str(), "w");
        if (m_File)
        {
            for (AppConfiguration_t& _AppConfig : g_AppConfigs)
            {
                fwrite(&_AppConfig.m_Name[0], sizeof(char), _AppConfig.m_Name.size() + 1, m_File);
                fwrite(&_AppConfig.m_Vibrance, sizeof(int), 1, m_File);
            }

            fclose(m_File);
        }
    }

    void LoadFile()
    {
        FILE* m_File;
        fopen_s(&m_File, GetFilePath().c_str(), "r");
        if (m_File)
        {
            fseek(m_File, 0, SEEK_END);
            long m_FileSize = ftell(m_File);
            uint8_t* m_RawFile = new uint8_t[m_FileSize];

            fseek(m_File, 0, SEEK_SET);
            fread(m_RawFile, sizeof(uint8_t), m_FileSize, m_File);

            fclose(m_File);

            int m_Offset = 0;
            while (1)
            {
                if (m_Offset >= m_FileSize) break;

                AppConfiguration_t m_AppConfig;
                m_AppConfig.m_Name = reinterpret_cast<const char*>(&m_RawFile[m_Offset]);
                m_AppConfig.m_Hash = Utils::JOAAT(&m_AppConfig.m_Name[0]);

                m_Offset += (m_AppConfig.m_Name.size() + 1);

                memcpy(&m_AppConfig.m_Vibrance, &m_RawFile[m_Offset], sizeof(int));

                m_Offset += sizeof(int);

                Add(m_AppConfig);
            }

            delete[] m_RawFile;
        }
    }

    std::vector<std::string> GetFormatted()
    {
        std::vector<std::string> _List;

        int _Count = 1;
        for (AppConfiguration_t& _AppConfig : g_AppConfigs)
        {
            std::string _App = std::to_string(_Count) + ". ";
            _App += _AppConfig.m_Name + " ";
            _App += "[V: " + std::to_string(_AppConfig.m_Vibrance) + "]";

            _List.emplace_back(_App);
            ++_Count;
        }

        return _List;
    }
}

namespace Program
{
    // Set process to idle when console hidden so we don't rape too much cpu while gaming...
    void ToggleIdlePriority(bool p_Enable)
    { 
        SetPriorityClass(GetCurrentProcess(), (p_Enable ? IDLE_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS));
    }

    // Set affinity to last core so we don't rape too much core with this crap...
    void SetAffinity()
    {
        SYSTEM_INFO _SystemInfo;
        GetSystemInfo(&_SystemInfo);

        if (_SystemInfo.dwNumberOfProcessors)
        {
            DWORD _LastCoreMask(1 << (_SystemInfo.dwNumberOfProcessors - 1));
            SetProcessAffinityMask(GetCurrentProcess(), _LastCoreMask);
        }
    }

    static const char* s_RunRegPath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
    bool IsRegisteredOnStartup()
    {
        bool _IsRegistered = false;
        HKEY _RegKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, s_RunRegPath, 0, KEY_READ, &_RegKey) == ERROR_SUCCESS)
        {
            DWORD _Dummy[2];

            _IsRegistered = (RegQueryValueExA(_RegKey, APP_NAME, 0, &_Dummy[0], 0, &_Dummy[1]) == ERROR_SUCCESS);

            RegCloseKey(_RegKey);
        }

        return _IsRegistered;
    }

    void ToggleOnStartup()
    {
        bool _Registered = IsRegisteredOnStartup();

        HKEY _RegKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, s_RunRegPath, 0, KEY_ALL_ACCESS, &_RegKey) != ERROR_SUCCESS) {
            return;
        }

        if (_Registered) {
            RegDeleteValueA(_RegKey, APP_NAME);
        }
        else
        {
            char _FilePath[MAX_PATH];
            GetModuleFileNameA(0, _FilePath, MAX_PATH);
            RegSetValueExA(_RegKey, APP_NAME, 0, REG_SZ, reinterpret_cast<BYTE*>(_FilePath), strlen(_FilePath));
        }

        RegCloseKey(_RegKey);
    }

    void Info()
    {
        Console::Clear();
        Console::Print(Console::m_DefaultColor, '!', "SNVPAC made by "); Console::Print(CLR_BRED, "sneakyevil\n");
        Console::Print(Console::m_DefaultColor, '!', "Startup: ");

        if (IsRegisteredOnStartup()) {
            Console::Print(CLR_BGREEN, "Yes\n");
        }
        else {
            Console::Print(CLR_BRED, "No\n");
        }

        Console::Print(Console::m_DefaultColor, "\n");
    }

    int Listbox(std::string p_Text, std::vector<std::string>* p_List, int p_DeltaShow, std::string p_Prefix)
    {
        int _Index = 0;
        bool _Update = true;
        int _Size = static_cast<int>(p_List->size());

        while (1)
        {
            if (_Update)
            {
                if (0 > p_DeltaShow || p_DeltaShow >= _Size) {
                    p_DeltaShow = _Size - 1;
                }

                if (0 > _Index) {
                    _Index = _Size - 1;
                }
                else if (_Index >= _Size) {
                    _Index = 0;
                }

                _Update = false;

                Info();
                Console::Print(Console::m_DefaultColor, '!', p_Text);
                Console::DrawListbox(_Index, p_List->data(), _Size, p_DeltaShow, p_Prefix);
            }

            switch (_getch())
            {
                case 8: 
                    return -1;
                case 13: 
                    return _Index;
                case 72:
                {
                    _Index--;
                    _Update = true;
                }
                break;
                case 80:
                {
                    _Index++;
                    _Update = true;
                }
                break;
            }
        }
    }

    int Error(std::string p_Error)
    {
        Info();
        Console::Print(CLR_BRED, '!', p_Error);
        int _Dummy = getchar();
        return EXIT_FAILURE;
    }

    int GetVibranceValue(int m_Delimer)
    {
        std::vector<std::string> _Array;
        for (int i = 0; static_cast<int>(50 / m_Delimer) >= i; ++i) {
            _Array.emplace_back(std::to_string(i * m_Delimer));
        }

        int _Index = Program::Listbox("Select Vibrance:\n", &_Array, -1, "\t");
        if (_Index == -1) {
            return -1;
        }

        return atoi(&_Array[_Index][0]);
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

    void __stdcall WinEvent(HWINEVENTHOOK p_Hook, DWORD p_Event, HWND p_Window, LONG p_ID, LONG p_ChildID, DWORD p_Thread, DWORD p_Time)
    {
        static HWND s_LastWindow = 0;
        if (p_Window == s_LastWindow) {
            return;
        }
        
        s_LastWindow = p_Window;

        uint32_t _ProcessHash = 0;
        {
            DWORD _PID;
            if (GetWindowThreadProcessId(p_Window, &_PID))
            {
                std::string m_ProcessName = Utils::GetProcessNameByPID(_PID);
                _ProcessHash = Utils::JOAAT(&m_ProcessName[0]);
            }
        }

        for (AppConfiguration_t& _AppConfig : g_AppConfigs)
        {
            if (_AppConfig.m_Hash != _ProcessHash) {
                continue;
            }

            Configuration::Set(&_AppConfig);
            return;
        }

        Configuration::Set(&g_AppConfigDefault);
    }

    DWORD __stdcall Thread(void* p_Reserved)
    {
        RegisterHotKey(0, 1, APP_CONSOLE_HOTKEY_MOD, APP_CONSOLE_HOTKEY_KEY);
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, 0, WinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);

        int _MsgRet = 0;
        MSG _Msg;
        while ((_MsgRet = GetMessageA(&_Msg, 0, 0, 0)) != 0)
        { 
            if (_MsgRet == -1) {
                continue;
            }

            switch (_Msg.message)
            {
                case WM_HOTKEY:
                {
                    switch (_Msg.wParam)
                    {
                        case 1: 
                            ConsoleHotkey(); break;
                    }
                }
                break;
            }

            TranslateMessage(&_Msg);
            DispatchMessageA(&_Msg);
        }

        return 0;
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

    if (g_NvAPI.Loaded())
    {
        g_NvAPI.EnumDisplayHandle(0, &g_NvHandle);

        NvDVCInfo_t _DVCInfo;
        g_NvAPI.GetDVCInfo(g_NvHandle, 0, &_DVCInfo);

        g_AppConfigDefault.m_Vibrance = _DVCInfo.m_CurLevel;
    }
    else
    {
        MessageBoxA(0, "Couldn't initialize Nvidia API. Nvidia driver not installed?", APP_NAME, MB_OK | MB_ICONERROR);
        return 0;
    }

    MessageHandler::ConsoleHotkey(); // Auto-Hide on launch
    Program::SetAffinity();
    CreateThread(0, 0, MessageHandler::Thread, 0, 0, 0);

    Configuration::LoadFile();

    while (1)
    {
        static std::vector<std::string> s_MainMenu = { "Add Application", "Edit Application", "Toggle Startup" };

        switch (Program::Listbox("Select Option:\n", &s_MainMenu, -1, "\t"))
        {
            case 0:
            {
                std::vector<std::string> _Processes = Utils::GetListOfProcesses();
                std::sort(_Processes.begin(), _Processes.end(), Utils::Sort_Alphabet);

                int _ProcessIndex = Program::Listbox("Select Process:\n", &_Processes, 5, "\t");
                if (_ProcessIndex == -1) {
                    break;
                }

                AppConfiguration_t _AppConfig;
                {
                    _AppConfig.m_Name = _Processes[_ProcessIndex];
                    _AppConfig.m_Hash = Utils::JOAAT(&_AppConfig.m_Name[0]);
                    _AppConfig.m_Vibrance = Program::GetVibranceValue(5);
                }

                if (_AppConfig.m_Vibrance == -1) {
                    break;
                }

                Configuration::Add(_AppConfig);
                Configuration::SaveFile();
            }
            break;
            case 1:
            {
                if (g_AppConfigs.empty())
                {
                    Program::Error("You need to first add some application before you can edit one.");
                    break;
                }

                std::vector<std::string> _AppConfigs = Configuration::GetFormatted();

                int _AppIndex = Program::Listbox("Select Application:\n", &_AppConfigs, 5, "\t");
                if (_AppIndex == -1) {
                    break;
                }

                int m_NewVibrance = Program::GetVibranceValue(5);
                if (m_NewVibrance == -1) {
                    continue;
                }

                if (m_NewVibrance == 0) {
                    g_AppConfigs.erase(g_AppConfigs.begin() + _AppIndex);
                }
                else {
                    g_AppConfigs[_AppIndex].m_Vibrance = m_NewVibrance;
                }

                Configuration::SaveFile();
            }
            break;
            case 2: 
                Program::ToggleOnStartup(); break;
        }
    }

    return 0;
}