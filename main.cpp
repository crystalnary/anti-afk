#include <windows.h>
#include <tlhelp32.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <imm.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

std::atomic<bool> running(true);
std::atomic<bool> user_active(false);
std::atomic<uint64_t> last_activity_time(0);

const int USER_INACTIVITY_WAIT = 3;
const int MAX_WAIT_TIME = 60;

struct Config {
    std::string process_name;
    int interval_seconds = 540;
    enum Action { SPACE, WASD, ZOOM } action = SPACE;
    bool user_safe = false;
};

struct EnumWindowsData {
    DWORD processId;
    std::vector<HWND>* windows;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<EnumWindowsData*>(lParam);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == data->processId && GetWindowTextLength(hwnd) > 0 && IsWindowVisible(hwnd)) {
        data->windows->push_back(hwnd);
    }
    return TRUE;
}

std::vector<HWND> GetWindowsForProcess(DWORD processId) {
    std::vector<HWND> windows;
    EnumWindowsData data{processId, &windows};
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return windows;
}

std::vector<HWND> find_process_windows(const std::string& process_name) {
    std::vector<HWND> windows;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return windows;
    
    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(snapshot, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, process_name.c_str()) == 0) {
                auto procWindows = GetWindowsForProcess(pe.th32ProcessID);
                windows.insert(windows.end(), procWindows.begin(), procWindows.end());
            }
        } while (Process32Next(snapshot, &pe));
    }
    
    CloseHandle(snapshot);
    return windows;
}

void monitor_user_activity() {
    while (running) {
        LASTINPUTINFO lii = {0};
        lii.cbSize = sizeof(LASTINPUTINFO);
        
        if (GetLastInputInfo(&lii)) {
            DWORD currentTime = GetTickCount();
            DWORD idleTimeMs = currentTime - lii.dwTime;
            
            last_activity_time = lii.dwTime;
            
            user_active = (idleTimeMs < USER_INACTIVITY_WAIT * 1000);
        }
        
        Sleep(100);
    }
}

void send_key(BYTE vk) {
    keybd_event(vk, static_cast<BYTE>(MapVirtualKey(vk, 0)), 0, 0);
    Sleep(15);
    keybd_event(vk, static_cast<BYTE>(MapVirtualKey(vk, 0)), KEYEVENTF_KEYUP, 0);
    Sleep(15);
}

void perform_action(Config::Action action) {
    switch (action) {
        case Config::SPACE:
            send_key(VK_SPACE);
            break;
        case Config::WASD:
            send_key('W');
            send_key('A');
            send_key('S');
            send_key('D');
            break;
        case Config::ZOOM:
            send_key('I');
            send_key('O');
            break;
    }
}

void RestoreForegroundWindow(HWND prevWnd) {
    if (!prevWnd || !IsWindowVisible(prevWnd) || IsIconic(prevWnd)) return;
    
    ShowWindow(prevWnd, SW_SHOW);
    SetWindowPos(prevWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    
    DWORD currentThreadId = GetCurrentThreadId();
    DWORD prevThreadId = GetWindowThreadProcessId(prevWnd, NULL);
    
    AttachThreadInput(currentThreadId, prevThreadId, TRUE);
    BringWindowToTop(prevWnd);
    SetForegroundWindow(prevWnd);
    AttachThreadInput(currentThreadId, prevThreadId, FALSE);
    
    SetWindowPos(prevWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void DisableIMEForWindow(HWND hwnd) {
    if (!hwnd) return;
    
    HIMC hImc = ImmGetContext(hwnd);
    if (hImc) {
        ImmSetOpenStatus(hImc, FALSE);
        ImmReleaseContext(hwnd, hImc);
    }
}

void antiafk_loop(const Config& config) {
    auto logger = spdlog::get("console");
    
    logger->info("Anti-AFK started. Press Ctrl+C to stop.");
    logger->info("Process: {}", config.process_name);
    logger->info("Interval: {} seconds", config.interval_seconds);
    logger->info("Action: {}", config.action == Config::SPACE ? "Space" : 
                               config.action == Config::WASD ? "WASD" : "Zoom");
    if (config.user_safe) {
        logger->info("User-Safe Mode: ON (waits for inactivity)");
    }
    
    while (running) {
        auto windows = find_process_windows(config.process_name);
        
        if (windows.empty()) {
            logger->warn("No {} windows found, waiting...", config.process_name);
        } else {
            if (config.user_safe) {
                uint64_t start_wait_time = GetTickCount64();
                bool warned = false;
                
                while (true) {
                    uint64_t current_time = GetTickCount64();
                    uint64_t elapsed_seconds = (current_time - start_wait_time) / 1000;
                    
                    if (!user_active.load()) {
                        if (warned) {
                            logger->info("User inactive, performing action now");
                        }
                        break;
                    }
                    
                    if (elapsed_seconds >= MAX_WAIT_TIME) {
                        logger->warn("Max wait time reached, forcing action");
                        break;
                    }
                    
                    if (!warned && elapsed_seconds > 5) {
                        logger->info("Waiting for user inactivity...");
                        warned = true;
                    }
                    
                    Sleep(500);
                    
                    if (!running) return;
                }
            }
            
            HWND previousWindow = GetForegroundWindow();
            BlockInput(TRUE);
            {
                for (HWND window : windows) {
                    if (!IsWindow(window)) continue;
                    
                    bool wasMinimized = IsIconic(window);
                    if (wasMinimized) {
                        ShowWindow(window, SW_RESTORE);
                    }
                    
                    SetForegroundWindow(window);
                    Sleep(100);
                    
                    DisableIMEForWindow(window);
                    
                    for (int i = 0; i < 3; i++) {
                        perform_action(config.action);
                    }
                    
                    if (wasMinimized) {
                        ShowWindow(window, SW_MINIMIZE);
                    }
                }
                
                RestoreForegroundWindow(previousWindow);
                logger->info("Action performed on {} window(s)", windows.size());
            }
            BlockInput(FALSE);
        }
        
        for (int i = 0; i < config.interval_seconds && running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        spdlog::get("console")->info("Shutting down...");
        running = false;
        return TRUE;
    }
    return FALSE;
}

void show_help() {
    auto console = spdlog::get("console");
    console->info("Usage: antiafk <process_name> [options]");
    console->info("");
    console->info("Arguments:");
    console->info("  process_name              Target process name (required)");
    console->info("");
    console->info("Options:");
    console->info("  -i, --interval <seconds>  Set interval (default: 540)");
    console->info("  -a, --action <type>       Set action: space/wasd/zoom (default: space)");
    console->info("  -s, --safe                Enable User-Safe mode (waits for inactivity)");
    console->info("  -h, --help                Show this help");
    console->info("");
    console->info("Examples:");
    console->info("  antiafk notepad.exe");
    console->info("  antiafk game.exe -i 300");
    console->info("  antiafk \"My Game.exe\" -a wasd -i 60");
    console->info("  antiafk RobloxPlayerBeta.exe -s");
}

int main(int argc, char* argv[]) {
    auto console = spdlog::stdout_color_mt("console");
    console->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    
    if (argc < 2) {
        console->error("No process name specified");
        show_help();
        return 1;
    }
    
    if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
        show_help();
        return 0;
    }
    
    Config config;
    config.process_name = argv[1];
    
    if (config.process_name.find(".exe") == std::string::npos) {
        config.process_name += ".exe";
    }
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_help();
            return 0;
        }
        else if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            config.interval_seconds = std::stoi(argv[++i]);
        }
        else if ((arg == "-a" || arg == "--action") && i + 1 < argc) {
            std::string action = argv[++i];
            std::transform(action.begin(), action.end(), action.begin(), ::tolower);
            if (action == "wasd") config.action = Config::WASD;
            else if (action == "zoom") config.action = Config::ZOOM;
            else config.action = Config::SPACE;
        }
        else if (arg == "-s" || arg == "--safe") {
            config.user_safe = true;
        }
    }
    
    SetConsoleCtrlHandler(console_handler, TRUE);
    
    console->info("Anti-AFK");
    console->info("========");
    
    std::thread activity_thread;
    if (config.user_safe) {
        activity_thread = std::thread(monitor_user_activity);
    }
    
    auto windows = find_process_windows(config.process_name);
    if (windows.empty()) {
        console->warn("No {} windows found. Start the process and the program will detect it.", config.process_name);
    } else {
        console->info("Found {} {} window(s)", windows.size(), config.process_name);
    }
    
    antiafk_loop(config);
    
    if (activity_thread.joinable()) {
        activity_thread.join();
    }
    
    return 0;
}
