#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

using Microsoft::WRL::ComPtr;

namespace {
constexpr UINT kWidth = 320;
constexpr UINT kHeight = 240;
constexpr UINT kFramesPerSecond = 30;

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string jsonString(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
            else out << static_cast<char>(c);
        }
    }
    out << '"';
    return out.str();
}

std::wstring userObjectName(HANDLE object) {
    DWORD needed = 0;
    GetUserObjectInformationW(object, UOI_NAME, nullptr, 0, &needed);
    if (needed < sizeof(wchar_t)) return {};
    std::vector<wchar_t> buffer(needed / sizeof(wchar_t) + 1, L'\0');
    if (!GetUserObjectInformationW(object, UOI_NAME, buffer.data(), needed, &needed)) return {};
    return buffer.data();
}

bool inputDesktopName(std::wstring& name, DWORD& error) {
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desktop) {
        error = GetLastError();
        return false;
    }
    name = userObjectName(desktop);
    const DWORD nameError = name.empty() ? GetLastError() : ERROR_SUCCESS;
    CloseDesktop(desktop);
    if (name.empty()) {
        error = nameError ? nameError : ERROR_INVALID_DATA;
        return false;
    }
    error = ERROR_SUCCESS;
    return true;
}

std::wstring quoteArgument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    size_t slashes = 0;
    for (const wchar_t c : value) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'\"') {
            quoted.append(slashes * 2 + 1, L'\\');
            quoted.push_back(L'\"');
            slashes = 0;
            continue;
        }
        quoted.append(slashes, L'\\');
        slashes = 0;
        quoted.push_back(c);
    }
    quoted.append(slashes * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

struct ChildResult {
    std::wstring desktop;
    std::string adapter;
    bool hardwareAdapter{};
    unsigned seconds{};
    unsigned frameAttempts{};
    unsigned presentOk{};
    unsigned presentOccluded{};
    unsigned presentOtherSuccess{};
    unsigned presentFailures{};
    unsigned stagingReadbackMapsCompleted{};
    bool gpuEventQueriedAfterOccludedPresent{};
    bool gpuEventReadyAfterOccludedPresent{};
    unsigned gpuEventPolls{};
    HRESULT gpuEventLastGetData{S_FALSE};
    std::array<std::array<unsigned, 4>, 2> readbacks{};
    std::array<bool, 2> readbackPassed{};
    std::string failureStage;
    HRESULT failureHr{S_OK};
    DWORD failureWin32{};
};

std::string rgbaJson(const std::array<unsigned, 4>& rgba) {
    std::ostringstream out;
    out << '[' << rgba[0] << ',' << rgba[1] << ',' << rgba[2] << ',' << rgba[3] << ']';
    return out.str();
}

bool closeChannel(unsigned actual, unsigned expected) {
    return actual + 2 >= expected && actual <= expected + 2;
}

LRESULT CALLBACK probeWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(window, message, wParam, lParam);
}

std::string childJson(const ChildResult& r, bool passed) {
    const auto hrHex = [](HRESULT hr) {
        std::ostringstream out;
        out << "0x" << std::hex << std::uppercase << static_cast<uint32_t>(hr);
        return out.str();
    };
    std::ostringstream out;
    out << "{\"mode\":\"private-win32-desktop-d3d11\""
        << ",\"privateDesktop\":" << jsonString(utf8(r.desktop))
        << ",\"adapter\":" << jsonString(r.adapter)
        << ",\"hardwareAdapter\":" << (r.hardwareAdapter ? "true" : "false")
        << ",\"secondsRequested\":" << r.seconds
        << ",\"frameAttempts\":" << r.frameAttempts
        << ",\"presentSOk\":" << r.presentOk
        << ",\"presentOccluded\":" << r.presentOccluded
        << ",\"presentOtherSuccess\":" << r.presentOtherSuccess
        << ",\"presentFailures\":" << r.presentFailures
        << ",\"stagingReadbackMapsCompleted\":" << r.stagingReadbackMapsCompleted
        << ",\"gpuEventQueriedAfterOccludedPresent\":" << (r.gpuEventQueriedAfterOccludedPresent ? "true" : "false")
        << ",\"gpuEventReadyAfterOccludedPresent\":" << (r.gpuEventReadyAfterOccludedPresent ? "true" : "false")
        << ",\"gpuEventPolls\":" << r.gpuEventPolls
        << ",\"gpuEventLastGetDataHresult\":" << jsonString(hrHex(r.gpuEventLastGetData))
        << ",\"readbackRGBA\":[" << rgbaJson(r.readbacks[0]) << ',' << rgbaJson(r.readbacks[1]) << ']'
        << ",\"readbackPassed\":[" << (r.readbackPassed[0] ? "true" : "false") << ',' << (r.readbackPassed[1] ? "true" : "false") << ']'
        << ",\"d3dProgressPassed\":" << (passed ? "true" : "false")
        << ",\"failureStage\":" << (r.failureStage.empty() ? "null" : jsonString(r.failureStage))
        << ",\"failureHresult\":" << jsonString(hrHex(r.failureHr))
        << ",\"failureWin32\":" << r.failureWin32
        << '}';
    return out.str();
}

bool writeReport(const std::wstring& path, const std::string& json) {
    std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    file.put('\n');
    return static_cast<bool>(file);
}

int runChild(const std::wstring& reportPath, const std::wstring& desktopName, unsigned seconds) {
    ChildResult result;
    result.desktop = desktopName;
    result.seconds = seconds;
    auto fail = [&](const char* stage, HRESULT hr = E_FAIL, DWORD win32 = ERROR_SUCCESS) {
        result.failureStage = stage;
        result.failureHr = hr;
        result.failureWin32 = win32;
        writeReport(reportPath, childJson(result, false));
        return 1;
    };

    HDESK threadDesktop = GetThreadDesktop(GetCurrentThreadId());
    if (!threadDesktop || userObjectName(threadDesktop) != desktopName) return fail("thread-desktop", E_FAIL, GetLastError());

    constexpr wchar_t className[] = L"MGS5VRBackgroundD3DProbeWindow";
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = probeWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = className;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return fail("register-window-class", E_FAIL, GetLastError());

    RECT bounds{0, 0, static_cast<LONG>(kWidth), static_cast<LONG>(kHeight)};
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExW(0, className, L"MGS5VR private desktop D3D probe",
        WS_OVERLAPPEDWINDOW, 16, 16, bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, windowClass.hInstance, nullptr);
    if (!window) return fail("create-window", E_FAIL, GetLastError());

    // This window is shown only inside the newly created, inactive Win32 desktop.
    ShowWindow(window, SW_SHOWNOACTIVATE);
    UpdateWindow(window);

    DXGI_SWAP_CHAIN_DESC swapDesc{};
    swapDesc.BufferDesc.Width = kWidth;
    swapDesc.BufferDesc.Height = kHeight;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.OutputWindow = window;
    swapDesc.Windowed = TRUE;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL requested[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel{};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, requested, ARRAYSIZE(requested), D3D11_SDK_VERSION,
        &swapDesc, &swapChain, &device, &featureLevel, &context);
    if (hr == E_INVALIDARG) {
        const D3D_FEATURE_LEVEL fallback[] = { D3D_FEATURE_LEVEL_11_0 };
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, fallback, ARRAYSIZE(fallback), D3D11_SDK_VERSION,
            &swapDesc, &swapChain, &device, &featureLevel, &context);
    }
    if (FAILED(hr)) {
        DestroyWindow(window);
        return fail("create-hardware-d3d11-swapchain", hr);
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIAdapter1> adapter1;
    DXGI_ADAPTER_DESC1 adapterDesc{};
    if (FAILED(device.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) ||
        FAILED(adapter.As(&adapter1)) || FAILED(adapter1->GetDesc1(&adapterDesc))) {
        swapChain.Reset();
        context.Reset();
        device.Reset();
        DestroyWindow(window);
        return fail("read-gpu-adapter", E_FAIL);
    }
    result.adapter = utf8(adapterDesc.Description);
    result.hardwareAdapter = (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
    if (!result.hardwareAdapter) {
        swapChain.Reset();
        context.Reset();
        device.Reset();
        DestroyWindow(window);
        return fail("software-adapter-rejected", E_FAIL);
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        swapChain.Reset();
        context.Reset();
        device.Reset();
        DestroyWindow(window);
        return fail("get-swapchain-buffer", hr);
    }
    D3D11_TEXTURE2D_DESC backDesc{};
    backBuffer->GetDesc(&backDesc);
    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = 1;
    stagingDesc.Height = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = backDesc.Format;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    hr = device->CreateTexture2D(&stagingDesc, nullptr, &staging);
    if (FAILED(hr)) {
        swapChain.Reset();
        context.Reset();
        device.Reset();
        DestroyWindow(window);
        return fail("create-readback-texture", hr);
    }
    ComPtr<ID3D11RenderTargetView> target;
    hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, &target);
    if (FAILED(hr)) {
        swapChain.Reset();
        context.Reset();
        device.Reset();
        DestroyWindow(window);
        return fail("create-render-target", hr);
    }

    const float colors[2][4] = {
        { 0.125f, 0.25f, 0.5f, 1.0f },
        { 0.75f, 0.125f, 0.375f, 1.0f }
    };
    const unsigned expected[2][4] = {
        { 32, 64, 128, 255 },
        { 191, 32, 96, 255 }
    };
    const auto captureSample = [&](unsigned sample) -> HRESULT {
        context->ClearRenderTargetView(target.Get(), colors[sample]);
        context->OMSetRenderTargets(0, nullptr, nullptr);
        const UINT x = backDesc.Width / 2;
        const UINT y = backDesc.Height / 2;
        D3D11_BOX box{ x, y, 0, x + 1, y + 1, 1 };
        context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, backBuffer.Get(), 0, &box);
        context->Flush();

        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT mapHr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(mapHr)) return mapHr;
        ++result.stagingReadbackMapsCompleted;
        const auto* bytes = static_cast<const uint8_t*>(mapped.pData);
        if (backDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || backDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            result.readbacks[sample] = { bytes[2], bytes[1], bytes[0], bytes[3] };
        } else {
            result.readbacks[sample] = { bytes[0], bytes[1], bytes[2], bytes[3] };
        }
        context->Unmap(staging.Get(), 0);
        result.readbackPassed[sample] = closeChannel(result.readbacks[sample][0], expected[sample][0]) &&
            closeChannel(result.readbacks[sample][1], expected[sample][1]) &&
            closeChannel(result.readbacks[sample][2], expected[sample][2]) &&
            closeChannel(result.readbacks[sample][3], expected[sample][3]);
        return S_OK;
    };
    hr = captureSample(0);
    if (FAILED(hr)) {
        target.Reset(); staging.Reset(); backBuffer.Reset(); swapChain.Reset(); context.Reset(); device.Reset(); DestroyWindow(window);
        return fail("readback-color-a", hr);
    }
    const HRESULT firstPresent = swapChain->Present(0, 0);
    if (firstPresent == DXGI_STATUS_OCCLUDED) ++result.presentOccluded;
    else if (firstPresent == S_OK) ++result.presentOk;
    else if (SUCCEEDED(firstPresent)) ++result.presentOtherSuccess;
    else ++result.presentFailures;

    const auto duration = std::chrono::seconds(seconds);
    const auto deadline = std::chrono::steady_clock::now() + duration;
    auto nextFrame = std::chrono::steady_clock::now();
    const float movingColors[2][4] = {
        { 0.20f, 0.31f, 0.42f, 1.0f },
        { 0.73f, 0.41f, 0.16f, 1.0f }
    };
    while (std::chrono::steady_clock::now() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) break;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        const auto colorIndex = result.frameAttempts % 2;
        context->ClearRenderTargetView(target.Get(), movingColors[colorIndex]);
        const HRESULT present = swapChain->Present(0, 0);
        ++result.frameAttempts;
        if (present == DXGI_STATUS_OCCLUDED) ++result.presentOccluded;
        else if (present == S_OK) ++result.presentOk;
        else if (SUCCEEDED(present)) ++result.presentOtherSuccess;
        else {
            ++result.presentFailures;
            result.failureStage = "present";
            result.failureHr = present;
            break;
        }
        if (!result.gpuEventQueriedAfterOccludedPresent && present == DXGI_STATUS_OCCLUDED) {
            result.gpuEventQueriedAfterOccludedPresent = true;
            D3D11_QUERY_DESC queryDesc{ D3D11_QUERY_EVENT, 0 };
            ComPtr<ID3D11Query> eventQuery;
            result.gpuEventLastGetData = device->CreateQuery(&queryDesc, &eventQuery);
            if (SUCCEEDED(result.gpuEventLastGetData)) {
                context->End(eventQuery.Get());
                context->Flush();
                const auto queryDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                while (std::chrono::steady_clock::now() < queryDeadline) {
                    BOOL complete = FALSE;
                    result.gpuEventLastGetData = context->GetData(eventQuery.Get(), &complete, sizeof(complete), D3D11_ASYNC_GETDATA_DONOTFLUSH);
                    ++result.gpuEventPolls;
                    if (result.gpuEventLastGetData == S_OK && complete) {
                        result.gpuEventReadyAfterOccludedPresent = true;
                        break;
                    }
                    if (FAILED(result.gpuEventLastGetData)) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }
        nextFrame += std::chrono::milliseconds(1000 / kFramesPerSecond);
        std::this_thread::sleep_until(nextFrame);
    }

    hr = captureSample(1);
    if (FAILED(hr)) {
        target.Reset(); staging.Reset(); backBuffer.Reset(); swapChain.Reset(); context.Reset(); device.Reset(); DestroyWindow(window);
        return fail("readback-color-b", hr);
    }
    const HRESULT lastPresent = swapChain->Present(0, 0);
    if (lastPresent == DXGI_STATUS_OCCLUDED) ++result.presentOccluded;
    else if (lastPresent == S_OK) ++result.presentOk;
    else if (SUCCEEDED(lastPresent)) ++result.presentOtherSuccess;
    else ++result.presentFailures;

    target.Reset();
    staging.Reset();
    backBuffer.Reset();
    swapChain.Reset();
    context.Reset();
    device.Reset();
    DestroyWindow(window);
    UnregisterClassW(className, windowClass.hInstance);

    const bool passed = result.hardwareAdapter && result.stagingReadbackMapsCompleted == 2 &&
        result.readbackPassed[0] && result.readbackPassed[1] && result.frameAttempts >= kFramesPerSecond;
    if (!passed && result.failureStage.empty()) result.failureStage = "d3d-progress-criteria";
    const std::string json = childJson(result, passed);
    if (!writeReport(reportPath, json)) return 2;
    return passed ? 0 : 1;
}

int runParent(unsigned seconds, const std::wstring& reportPath) {
    std::wstring activeBefore;
    DWORD inputError = ERROR_SUCCESS;
    if (!inputDesktopName(activeBefore, inputError)) {
        std::cerr << "Could not read the active input desktop; no probe was launched. Win32=" << inputError << '\n';
        return 2;
    }

    const std::wstring desktopName = L"MGS5VR-D3D-Probe-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(GetTickCount64());
    HDESK privateDesktop = CreateDesktopW(desktopName.c_str(), nullptr, nullptr, 0,
        DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS, nullptr);
    if (!privateDesktop) {
        std::cerr << "Could not create the private Win32 desktop; no child was launched. Win32=" << GetLastError() << '\n';
        return 2;
    }

    wchar_t executablePath[MAX_PATH]{};
    const DWORD pathLength = GetModuleFileNameW(nullptr, executablePath, ARRAYSIZE(executablePath));
    if (!pathLength || pathLength >= ARRAYSIZE(executablePath)) {
        const DWORD error = GetLastError();
        CloseDesktop(privateDesktop);
        std::cerr << "Could not resolve the probe executable path. Win32=" << error << '\n';
        return 2;
    }

    std::wstring desktopPath = L"WinSta0\\" + desktopName;
    std::wstring command = quoteArgument(executablePath) + L" --probe-child " + quoteArgument(reportPath) +
        L" " + quoteArgument(desktopName) + L" " + std::to_wstring(seconds);
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.lpDesktop = desktopPath.data();
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(executablePath, mutableCommand.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &startup, &process);
    if (!created) {
        const DWORD error = GetLastError();
        CloseDesktop(privateDesktop);
        std::cerr << "Could not start the probe on its private desktop. Win32=" << error << '\n';
        return 2;
    }

    const DWORD firstWait = WaitForSingleObject(process.hProcess, seconds > 2 ? 1000U : 500U);
    std::wstring activeDuring;
    DWORD duringError = ERROR_SUCCESS;
    const bool duringRead = inputDesktopName(activeDuring, duringError);
    DWORD wait = firstWait;
    if (wait == WAIT_TIMEOUT) wait = WaitForSingleObject(process.hProcess, (seconds + 5U) * 1000U);
    bool timedOut = wait == WAIT_TIMEOUT;
    if (timedOut) {
        TerminateProcess(process.hProcess, ERROR_TIMEOUT);
        WaitForSingleObject(process.hProcess, 5000);
    }
    DWORD exitCode = ERROR_GEN_FAILURE;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    std::wstring activeAfter;
    DWORD afterError = ERROR_SUCCESS;
    const bool afterRead = inputDesktopName(activeAfter, afterError);
    CloseDesktop(privateDesktop);

    std::ifstream input(std::filesystem::path(reportPath), std::ios::binary);
    std::string childReport((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (!childReport.empty() && childReport.back() == '\n') childReport.pop_back();
    const bool desktopUnchanged = duringRead && afterRead && activeBefore == activeDuring && activeBefore == activeAfter;
    const bool childPassed = exitCode == 0 && !childReport.empty();
    std::ostringstream out;
    out << "{\"probe\":\"private-win32-desktop-d3d11\""
        << ",\"activeInputDesktopBefore\":" << jsonString(utf8(activeBefore))
        << ",\"activeInputDesktopDuring\":" << (duringRead ? jsonString(utf8(activeDuring)) : "null")
        << ",\"activeInputDesktopAfter\":" << (afterRead ? jsonString(utf8(activeAfter)) : "null")
        << ",\"activeDesktopUnchanged\":" << (desktopUnchanged ? "true" : "false")
        << ",\"duringDesktopReadError\":" << (duringRead ? 0 : duringError)
        << ",\"afterDesktopReadError\":" << (afterRead ? 0 : afterError)
        << ",\"childExitCode\":" << exitCode
        << ",\"timedOut\":" << (timedOut ? "true" : "false")
        << ",\"d3dProgressPassed\":" << (childPassed ? "true" : "false")
        << ",\"child\":" << (childReport.empty() ? "null" : childReport)
        << ",\"result\":" << jsonString(desktopUnchanged && childPassed ? "pass" : "inconclusive")
        << '}';
    std::cout << out.str() << '\n';
    return desktopUnchanged && childPassed ? 0 : 1;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 5 && wcscmp(argv[1], L"--probe-child") == 0) {
        const unsigned seconds = static_cast<unsigned>(_wtoi(argv[4]));
        return runChild(argv[2], argv[3], seconds);
    }
    if (argc == 4 && wcscmp(argv[1], L"--run") == 0) {
        const unsigned seconds = static_cast<unsigned>(_wtoi(argv[2]));
        if (seconds < 3 || seconds > 15) {
            std::cerr << "Duration must be between 3 and 15 seconds.\n";
            return 2;
        }
        return runParent(seconds, argv[3]);
    }
    std::wcerr << L"Usage: background-desktop-probe.exe --run <3..15 seconds> <report path>\n";
    return 2;
}
