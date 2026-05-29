#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <thread>
#include <atomic>

// --- Offsets from dump.cs ---
static constexpr uint32_t OFF_GameFacade_CurrentMatchGame = 0x8;
static constexpr uint32_t OFF_MatchGame_m_ReplicationEntitis = 0xC0;
static constexpr uint32_t OFF_Entity_m_CachedTransform = 0x58;
static constexpr uint32_t OFF_AttackableEntity_IsDead = 0x7C;
static constexpr uint32_t OFF_Player_OriginalNickName = 0x440;
static constexpr uint32_t OFF_Player_IsClientBot = 0x448;
static constexpr uint32_t DICT_OFF_ENTRIES = 0x18;
static constexpr uint32_t DICT_OFF_COUNT = 0x20;
static constexpr uint32_t ARRAY_DATA_OFFSET = 0x18;
static constexpr uint32_t ENTRY_SIZE = 16;
static constexpr uint32_t ENTRY_VAL_OFF = 8;
static constexpr uint32_t RVA_Camera_get_main = 0x9BFC5F0;
static constexpr uint32_t RVA_Camera_WorldToScreenPoint = 0x9C0AD80;
static constexpr uint32_t RVA_Entity_get_Position = 0x7801394;

// --- Entity data ---
struct EspEntry {
    float screenX, screenY;
    float distance;
    bool isDead, isBot, onScreen;
    char name[64];
};

// --- Globals ---
static std::vector<EspEntry> g_entities;
static std::atomic<bool> g_running{true};
static int g_screenW = 1920;
static int g_screenH = 1080;
static HWND g_hwnd = nullptr;
static HWND g_bluestacks_hwnd = nullptr;

// --- ADB helper ---
static std::string adb_exec(const char* cmd) {
    char full_cmd[1024];
    snprintf(full_cmd, sizeof(full_cmd), "adb %s", cmd);
    FILE* pipe = _popen(full_cmd, "rb");
    if (!pipe) return "";
    std::string result;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) {
        result.append(buf, n);
    }
    _pclose(pipe);
    return result;
}

static std::string adb_shell(const char* cmd) {
    char full_cmd[1024];
    snprintf(full_cmd, sizeof(full_cmd), "adb shell %s", cmd);
    FILE* pipe = _popen(full_cmd, "r");
    if (!pipe) return "";
    std::string result;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    _pclose(pipe);
    return result;
}

// --- Read game memory via ADB ---
static std::vector<uint8_t> read_memory(int pid, uint64_t address, size_t size) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "exec-out su -c \"dd if=/proc/%d/mem bs=1 skip=%llu count=%llu 2>/dev/null\"",
        pid, (unsigned long long)address, (unsigned long long)size);
    std::string raw = adb_exec(cmd);
    return std::vector<uint8_t>(raw.begin(), raw.end());
}

static uint64_t find_il2cpp_base(int pid) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "shell su -c \"grep 'r-xp.*libil2cpp' /proc/%d/maps | head -1\"", pid);
    std::string line = adb_shell(cmd + 7); // skip "shell "
    // Actually use adb shell directly
    FILE* pipe = _popen("adb shell su -c \"grep 'r-xp.*libil2cpp' /proc/PID/maps | head -1\"", "r");

    // Retry with correct pid
    char cmd2[512];
    snprintf(cmd2, sizeof(cmd2), "adb shell \"su -c \\\"grep 'r-xp.*libil2cpp' /proc/%d/maps | head -1\\\"\"", pid);
    std::string output;
    char buf[256];
    FILE* p = _popen(cmd2, "r");
    if (p) {
        while (fgets(buf, sizeof(buf), p)) output += buf;
        _pclose(p);
    }

    // Parse "START-END rw-p ... /path/to/libil2cpp.so"
    uint64_t start = 0;
    if (!output.empty()) {
        sscanf(output.c_str(), "%lx-", &start);
    }
    return start;
}

static int get_pid() {
    std::string pid_str = adb_shell("\"pidof com.dts.freefireth\"");
    // Clean up string
    while (!pid_str.empty() && (pid_str.back() == '\n' || pid_str.back() == '\r' || pid_str.back() == ' '))
        pid_str.pop_back();
    return atoi(pid_str.c_str());
}

// --- Read a pointer from game memory ---
static uint64_t read_ptr(int pid, uint64_t addr) {
    auto data = read_memory(pid, addr, 8);
    if (data.size() < 8) return 0;
    uint64_t val = 0;
    memcpy(&val, data.data(), 8);
    return val;
}

// --- Read bytes from game memory ---
static uint32_t read_u32(int pid, uint64_t addr) {
    auto data = read_memory(pid, addr, 4);
    if (data.size() < 4) return 0;
    uint32_t val = 0;
    memcpy(&val, data.data(), 4);
    return val;
}

static float read_f32(int pid, uint64_t addr) {
    auto data = read_memory(pid, addr, 4);
    if (data.size() < 4) return 0;
    float val = 0;
    memcpy(&val, data.data(), 4);
    return val;
}

static bool read_bool(int pid, uint64_t addr) {
    auto data = read_memory(pid, addr, 1);
    if (data.empty()) return false;
    return data[0] != 0;
}

static std::string read_string(int pid, uint64_t strPtr) {
    if (!strPtr) return "";
    uint32_t len = read_u32(pid, strPtr + 0x10);
    if (len <= 0 || len > 128) return "";
    auto data = read_memory(pid, strPtr + 0x14, len * 2);
    std::string result;
    for (uint32_t i = 0; i < len && i * 2 + 1 < data.size(); i++) {
        char c = (char)data[i * 2];
        if (c >= 32 && c < 127) result += c;
        else result += '?';
    }
    return result;
}

// --- WorldToScreen via RVA ---
static bool world_to_screen(int pid, uint64_t il2cpp_base, float wx, float wy, float wz,
                             float& sx, float& sy, float& dist) {
    // Call Camera.get_main
    uint64_t get_main_addr = il2cpp_base + RVA_Camera_get_main;
    uint64_t w2s_addr = il2cpp_base + RVA_Camera_WorldToScreenPoint;

    // These are function pointers in the game's code
    // We can't call them directly from PC via ADB
    // For now, just use a simple 2D projection fallback
    // TODO: implement proper W2S math from camera matrix

    sx = g_screenW / 2.0f;
    sy = g_screenH / 2.0f;
    dist = 0;
    return false;
}

// --- Main ESP data thread ---
static void esp_thread() {
    Sleep(15000); // Wait for game to load

    int pid = get_pid();
    if (pid <= 0) {
        return;
    }

    uint64_t il2cpp_base = find_il2cpp_base(pid);
    if (!il2cpp_base) {
        return;
    }

    while (g_running) {
        g_entities.clear();

        // TODO: Read entity data from game memory
        // This requires finding GameFacade static fields which needs IL2CPP metadata parsing
        // For now, read from the Zygisk module's dump file via ADB
        std::string data = adb_shell("\"cat /sdcard/ff_entities.dat 2>/dev/null\"");

        if (!data.empty()) {
            const char* p = data.c_str();
            int count = 0;
            sscanf(p, "%d", &count);
            // Skip first line
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;

            for (int i = 0; i < count && i < 200; i++) {
                EspEntry e{};
                float x, y, d;
                int dead, bot;
                char name[64] = {};
                int parsed = sscanf(p, "%f,%f,%f,%d,%d,%63[^\n]", &x, &y, &d, &dead, &bot, name);
                if (parsed >= 5) {
                    e.screenX = x;
                    e.screenY = y;
                    e.distance = d;
                    e.isDead = dead != 0;
                    e.isBot = bot != 0;
                    e.onScreen = true;
                    strncpy(e.name, name, sizeof(e.name) - 1);
                    g_entities.push_back(e);
                }
                while (*p && *p != '\n') p++;
                if (*p == '\n') p++;
            }
        }

        // Find Bluestacks window and position overlay
        if (!g_bluestacks_hwnd) {
            g_bluestacks_hwnd = FindWindowA("BlueStacksApp", nullptr);
            if (!g_bluestacks_hwnd) g_bluestacks_hwnd = FindWindowA(nullptr, "BlueStacks");
        }

        if (g_bluestacks_hwnd && g_hwnd) {
            RECT rc;
            GetWindowRect(g_bluestacks_hwnd, &rc);
            g_screenW = rc.right - rc.left;
            g_screenH = rc.bottom - rc.top;
            SetWindowPos(g_hwnd, HWND_TOPMOST, rc.left, rc.top, g_screenW, g_screenH,
                         SWP_NOACTIVATE);
        }

        InvalidateRect(g_hwnd, nullptr, FALSE);
        Sleep(500); // Update every 500ms
    }
}

// --- Window procedure ---
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            g_running = false;
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Double buffer
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, g_screenW, g_screenH);
            SelectObject(memDC, memBmp);

            // Clear
            RECT rc = {0, 0, g_screenW, g_screenH};
            FillRect(memDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            SetBkMode(memDC, TRANSPARENT);

            for (const auto& e : g_entities) {
                if (!e.onScreen) continue;

                HPEN pen;
                if (e.isDead) {
                    pen = CreatePen(PS_SOLID, 2, RGB(128, 128, 128));
                } else if (e.isBot) {
                    pen = CreatePen(PS_SOLID, 2, RGB(255, 200, 0));
                } else {
                    pen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
                }
                SelectObject(memDC, pen);

                // Line from bottom center to player
                MoveToEx(memDC, g_screenW / 2, g_screenH, nullptr);
                LineTo(memDC, (int)e.screenX, (int)e.screenY);

                // Box around player
                float boxSize = 30.0f;
                if (e.distance > 1.0f) {
                    boxSize = 10000.0f / e.distance;
                    if (boxSize < 15) boxSize = 15;
                    if (boxSize > 150) boxSize = 150;
                }
                Rectangle(memDC,
                    (int)(e.screenX - boxSize / 2), (int)(e.screenY - boxSize),
                    (int)(e.screenX + boxSize / 2), (int)(e.screenY + boxSize / 2));

                // Dead X mark
                if (e.isDead) {
                    HPEN redPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
                    SelectObject(memDC, redPen);
                    int sz = 8;
                    MoveToEx(memDC, (int)(e.screenX - sz), (int)(e.screenY - sz), nullptr);
                    LineTo(memDC, (int)(e.screenX + sz), (int)(e.screenY + sz));
                    MoveToEx(memDC, (int)(e.screenX + sz), (int)(e.screenY - sz), nullptr);
                    LineTo(memDC, (int)(e.screenX - sz), (int)(e.screenY + sz));
                    DeleteObject(redPen);
                }

                // Name + distance text
                char text[128];
                snprintf(text, sizeof(text), "%s [%.0fm]", e.name, e.distance);
                SetTextColor(memDC, e.isDead ? RGB(128, 128, 128) :
                                       e.isBot ? RGB(255, 200, 0) :
                                       RGB(0, 255, 0));
                TextOutA(memDC, (int)e.screenX - 30, (int)(e.screenY - boxSize - 15),
                         text, (int)strlen(text));

                DeleteObject(pen);
            }

            // Copy to screen
            BitBlt(hdc, 0, 0, g_screenW, g_screenH, memDC, 0, 0, SRCCOPY);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// --- Entry point ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Register window class
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "FFESPOverlay";
    RegisterClassExA(&wc);

    // Find Bluestacks window
    g_bluestacks_hwnd = FindWindowA("BlueStacksApp", nullptr);
    if (!g_bluestacks_hwnd) g_bluestacks_hwnd = FindWindowA(nullptr, "BlueStacks");

    int x = 0, y = 0, w = 1920, h = 1080;
    if (g_bluestacks_hwnd) {
        RECT rc;
        GetWindowRect(g_bluestacks_hwnd, &rc);
        x = rc.left; y = rc.top;
        w = rc.right - rc.left; h = rc.bottom - rc.top;
    }
    g_screenW = w; g_screenH = h;

    // Create overlay window (transparent, click-through, topmost)
    g_hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        "FFESPOverlay", "FF ESP",
        WS_POPUP | WS_VISIBLE,
        x, y, w, h,
        nullptr, nullptr, hInstance, nullptr
    );

    // Make fully transparent (draw our own content)
    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    // Start ESP data thread
    std::thread t(esp_thread);
    t.detach();

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_running = false;
    return 0;
}
