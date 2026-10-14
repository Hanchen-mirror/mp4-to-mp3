#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

enum { ID_INPUT = 101, ID_OUTPUT, ID_OPEN_INPUT, ID_OPEN_OUTPUT, ID_START, ID_STATUS };
const UINT WM_CONVERT_DONE = WM_APP + 1;
HWND main_window = nullptr;
HWND input_edit = nullptr;
HWND output_edit = nullptr;
HWND start_button = nullptr;
HWND status_text = nullptr;
HFONT ui_font = nullptr;
std::wstring last_output;

void set_status(const wchar_t* text) {
    SetWindowTextW(status_text, text);
}

std::wstring get_text(HWND control) {
    int length = GetWindowTextLengthW(control);
    std::wstring result(length + 1, L'\0');
    GetWindowTextW(control, result.data(), length + 1);
    result.resize(length);
    return result;
}

std::wstring quote_arg(const std::wstring& arg) {
    std::wstring result = L"\"";
    int slashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++slashes;
        } else if (ch == L'"') {
            result.append(slashes * 2 + 1, L'\\');
            result += ch;
            slashes = 0;
        } else {
            result.append(slashes, L'\\');
            result += ch;
            slashes = 0;
        }
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}

std::wstring find_ffmpeg() {
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    fs::path local = fs::path(exe_path).parent_path() / L"ffmpeg.exe";
    if (fs::exists(local)) return local.wstring();

    DWORD size = SearchPathW(nullptr, L"ffmpeg.exe", nullptr, 0, nullptr, nullptr);
    if (size == 0) return L"";
    std::wstring path(size, L'\0');
    SearchPathW(nullptr, L"ffmpeg.exe", nullptr, size, path.data(), nullptr);
    path.resize(size - 1);
    return path;
}

void convert_file(std::wstring ffmpeg, std::wstring input, std::wstring output) {
    // -n 防止覆盖已有文件；-vn 只提取音频。
    std::wstring command = quote_arg(ffmpeg) + L" -hide_banner -loglevel error -nostdin -n -i "
        + quote_arg(input) + L" -vn -codec:a libmp3lame -q:a 2 " + quote_arg(output);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    BOOL started = CreateProcessW(ffmpeg.c_str(), command.data(), nullptr, nullptr, FALSE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    DWORD result = 1;
    if (started) {
        WaitForSingleObject(process.hProcess, INFINITE);
        GetExitCodeProcess(process.hProcess, &result);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    PostMessageW(main_window, WM_CONVERT_DONE, started && result == 0 ? 1 : 0, 0);
}

void start_conversion() {
    std::wstring input = get_text(input_edit);
    std::wstring output = get_text(output_edit);
    fs::path source(input);
    if (input.empty() || !fs::is_regular_file(source)) {
        MessageBoxW(main_window, L"请选择存在的 MP4 文件。", L"输入无效", MB_OK | MB_ICONWARNING);
        return;
    }
    if (_wcsicmp(source.extension().c_str(), L".mp4") != 0) {
        MessageBoxW(main_window, L"输入文件需要是 MP4。", L"输入无效", MB_OK | MB_ICONWARNING);
        return;
    }
    if (output.empty()) {
        fs::path target = source;
        target.replace_extension(L".mp3");
        output = target.wstring();
    } else {
        fs::path target(output);
        if (fs::is_directory(target)) target /= source.stem().wstring() + L".mp3";
        else if (target.extension().empty()) target += L".mp3";
        output = target.wstring();
    }
    fs::path target(output);
    if (_wcsicmp(target.extension().c_str(), L".mp3") != 0) {
        MessageBoxW(main_window, L"输出文件需要使用 .mp3 扩展名。", L"输出无效", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!fs::exists(target.parent_path()) || fs::exists(target)) {
        MessageBoxW(main_window, fs::exists(target) ? L"输出文件已存在，请换一个名称。" : L"输出文件夹不存在。",
                    L"输出无效", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring ffmpeg = find_ffmpeg();
    if (ffmpeg.empty()) {
        MessageBoxW(main_window, L"找不到 ffmpeg.exe。请把它放在本程序旁边，或加入 PATH。",
                    L"缺少转换工具", MB_OK | MB_ICONERROR);
        return;
    }
    last_output = output;
    EnableWindow(start_button, FALSE);
    set_status(L"正在转换，请稍候…");
    std::thread(convert_file, ffmpeg, input, output).detach();
}

void choose_file(bool input) {
    wchar_t path[32768] = L"";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = main_window;
    dialog.lpstrFile = path;
    dialog.nMaxFile = 32768;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | (input ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT);
    dialog.lpstrFilter = input ? L"MP4 文件\0*.mp4\0所有文件\0*.*\0" : L"MP3 文件\0*.mp3\0";
    dialog.lpstrDefExt = input ? L"mp4" : L"mp3";
    if (input ? GetOpenFileNameW(&dialog) : GetSaveFileNameW(&dialog)) {
        SetWindowTextW(input ? input_edit : output_edit, path);
    }
}

HWND add_control(const wchar_t* type, const wchar_t* title, DWORD style,
                 int x, int y, int width, int height, int id = 0) {
    HWND control = CreateWindowExW(0, type, title, WS_CHILD | WS_VISIBLE | style,
                                   x, y, width, height, main_window,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
    return control;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        main_window = window;
        ui_font = CreateFontW(-19, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                              0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei UI");
        add_control(L"STATIC", L"MP4 文件（可直接拖入窗口）", 0, 24, 22, 420, 27);
        input_edit = add_control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                                 24, 54, 390, 30, ID_INPUT);
        add_control(L"BUTTON", L"浏览…", BS_PUSHBUTTON, 426, 53, 92, 32, ID_OPEN_INPUT);
        add_control(L"STATIC", L"MP3 输出路径（留空则保存在 MP4 所在文件夹）", 0, 24, 108, 490, 27);
        output_edit = add_control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                                  24, 140, 390, 30, ID_OUTPUT);
        add_control(L"BUTTON", L"浏览…", BS_PUSHBUTTON, 426, 139, 92, 32, ID_OPEN_OUTPUT);
        start_button = add_control(L"BUTTON", L"开始转换", BS_DEFPUSHBUTTON,
                                   24, 194, 132, 40, ID_START);
        status_text = add_control(L"STATIC", L"等待选择 MP4 文件", 0, 174, 202, 344, 27, ID_STATUS);
        DragAcceptFiles(window, TRUE);
        return 0;
    }
    case WM_DROPFILES: {
        HDROP drop = reinterpret_cast<HDROP>(wparam);
        if (DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0) > 0) {
            UINT length = DragQueryFileW(drop, 0, nullptr, 0);
            std::wstring path(length + 1, L'\0');
            DragQueryFileW(drop, 0, path.data(), length + 1);
            path.resize(length);
            SetWindowTextW(input_edit, path.c_str());
            set_status(L"已选择 MP4 文件");
        }
        DragFinish(drop);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case ID_OPEN_INPUT: choose_file(true); return 0;
        case ID_OPEN_OUTPUT: choose_file(false); return 0;
        case ID_START: start_conversion(); return 0;
        }
        break;
    case WM_CONVERT_DONE:
        EnableWindow(start_button, TRUE);
        if (wparam) {
            set_status(L"转换完成");
            MessageBoxW(window, last_output.c_str(), L"MP3 已保存", MB_OK | MB_ICONINFORMATION);
        } else {
            set_status(L"转换失败");
            MessageBoxW(window, L"转换失败。请确认 MP4 含有音轨，且输出位置可写。",
                        L"转换失败", MB_OK | MB_ICONERROR);
        }
        return 0;
    case WM_DESTROY:
        DragAcceptFiles(window, FALSE);
        DeleteObject(ui_font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"Mp4ToMp3Window";
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&window_class);
    HWND window = CreateWindowExW(WS_EX_ACCEPTFILES, window_class.lpszClassName,
                                  L"MP4 转 MP3", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 555, 290, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    SendMessageW(window, WM_SETICON, ICON_SMALL,
                 reinterpret_cast<LPARAM>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                      16, 16, LR_DEFAULTCOLOR)));
    ShowWindow(window, show);
    UpdateWindow(window);
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
