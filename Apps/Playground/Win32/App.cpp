// App.cpp : Defines the entry point for the application.
//

#include "App.h"
#include <Windows.h>
#include <Windowsx.h>
#include <Shlwapi.h>
#include <filesystem>

#include <Shared/InputManager.h>

#include <Babylon/AppRuntime.h>
#include <Babylon/Graphics.h>
#include <Babylon/ScriptLoader.h>
#include <Babylon/Plugins/NativeCapture.h>
#include <Babylon/Plugins/NativeEngine.h>
#include <Babylon/Plugins/NativeOptimizations.h>
#include <Babylon/Plugins/ChromeDevTools.h>
#include <Babylon/Plugins/NativeXr.h>
#include <Babylon/Plugins/NativeCamera.h>
#include <Babylon/Polyfills/Console.h>
#include <Babylon/Polyfills/Window.h>
#include <Babylon/Polyfills/XMLHttpRequest.h>
#include <Babylon/Polyfills/Canvas.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                     // current instance
WCHAR szTitle[MAX_LOADSTRING];       // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name
std::unique_ptr<Babylon::AppRuntime> runtime{};
std::unique_ptr<Babylon::Graphics> graphics{};
std::unique_ptr<InputManager<Babylon::AppRuntime>::InputBuffer> inputBuffer{};
std::unique_ptr<Babylon::Plugins::ChromeDevTools> chromeDevTools{};
bool minimized{false};

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

namespace
{
    std::string GetUrlFromPath(const std::filesystem::path& path)
    {
        char url[1024];
        DWORD length = ARRAYSIZE(url);
        HRESULT hr = UrlCreateFromPathA(path.u8string().data(), url, &length, 0);
        if (FAILED(hr))
        {
            throw std::exception("Failed to create url from path", hr);
        }

        return {url};
    }

    std::vector<std::string> GetCommandLineArguments()
    {
        int argc;
        auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        std::vector<std::string> arguments{};
        arguments.reserve(argc);

        for (int idx = 1; idx < argc; idx++)
        {
            std::wstring hstr{argv[idx]};
            int bytesRequired = ::WideCharToMultiByte(CP_UTF8, 0, &hstr[0], static_cast<int>(hstr.size()), nullptr, 0, nullptr, nullptr);
            arguments.push_back(std::string(bytesRequired, 0));
            ::WideCharToMultiByte(CP_UTF8, 0, hstr.data(), static_cast<int>(hstr.size()), arguments.back().data(), bytesRequired, nullptr, nullptr);
        }

        LocalFree(argv);

        return arguments;
    }

    void Uninitialize()
    {
        if (graphics)
        {
            graphics->FinishRenderingCurrentFrame();
        }

        chromeDevTools.reset();
        inputBuffer.reset();
        runtime.reset();
        graphics.reset();
    }

    void RefreshBabylon(HWND hWnd)
    {
        Uninitialize();

        RECT rect;
        if (!GetClientRect(hWnd, &rect))
        {
            return;
        }

        auto width = static_cast<size_t>(rect.right - rect.left);
        auto height = static_cast<size_t>(rect.bottom - rect.top);

        Babylon::WindowConfiguration graphicsConfig{};
        graphicsConfig.WindowPtr = hWnd;
        graphicsConfig.Width = width;
        graphicsConfig.Height = height;

        graphics = Babylon::Graphics::CreateGraphics(graphicsConfig);
        graphics->StartRenderingCurrentFrame();

        runtime = std::make_unique<Babylon::AppRuntime>();
        inputBuffer = std::make_unique<InputManager<Babylon::AppRuntime>::InputBuffer>(*runtime);

        runtime->Dispatch([](Napi::Env env) {
            graphics->AddToJavaScript(env);

            Babylon::Polyfills::Console::Initialize(env, [](const char* message, auto) {
                OutputDebugStringA(message);
            });

            Babylon::Polyfills::Window::Initialize(env);

            Babylon::Polyfills::XMLHttpRequest::Initialize(env);
            Babylon::Polyfills::Canvas::Initialize(env);

            Babylon::Plugins::NativeEngine::Initialize(env);

            Babylon::Plugins::NativeOptimizations::Initialize(env);

            Babylon::Plugins::NativeCapture::Initialize(env);

            // Initialize Camera 
            Babylon::Plugins::Camera::Initialize(env);

            // Initialize NativeXr plugin.
            Babylon::Plugins::NativeXr::Initialize(env);

            InputManager<Babylon::AppRuntime>::Initialize(env, *inputBuffer);

            chromeDevTools = std::make_unique<Babylon::Plugins::ChromeDevTools>(Babylon::Plugins::ChromeDevTools::Initialize(env));
            if (chromeDevTools->SupportsInspector())
            {
                chromeDevTools->StartInspector(5643, "BabylonNative Playground");
            }
        });

        Babylon::ScriptLoader loader{*runtime};
        loader.Eval("document = {}", "");
        loader.LoadScript("app:///Scripts/ammo.js");
        // Commenting out recast.js for now because v8jsi is incompatible with asm.js.
        // loader.LoadScript("app:///Scripts/recast.js");
        loader.LoadScript("app:///Scripts/babylon.max.js");
        loader.LoadScript("app:///Scripts/babylonjs.loaders.js");
        loader.LoadScript("app:///Scripts/babylonjs.materials.js");
        loader.LoadScript("app:///Scripts/babylon.gui.js");
        loader.LoadScript("app:///Scripts/meshwriter.min.js");

        std::vector<std::string> scripts = GetCommandLineArguments();
        if (scripts.empty())
        {
            loader.LoadScript("app:///Scripts/experience.js");
        }
        else
        {
            for (const auto& script : scripts)
            {
                loader.LoadScript(GetUrlFromPath(script));
            }

            loader.LoadScript("app:///Scripts/playground_runner.js");
        }
    }

    void UpdateWindowSize(size_t width, size_t height)
    {
        graphics->UpdateSize(width, height);
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PLAYGROUNDWIN32, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PLAYGROUNDWIN32));

    MSG msg{};

    // Main message loop:
    while (msg.message != WM_QUIT)
    {
        BOOL result;

        if (minimized)
        {
            result = GetMessage(&msg, nullptr, 0, 0);
        }
        else
        {
            if (graphics)
            {
                graphics->FinishRenderingCurrentFrame();
                graphics->StartRenderingCurrentFrame();
            }

            result = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT;
        }

        if (result)
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PLAYGROUNDWIN32));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PLAYGROUNDWIN32);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    RefreshBabylon(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_SYSCOMMAND:
        {
            if ((wParam & 0xFFF0) == SC_MINIMIZE)
            {
                if (graphics)
                {
                    graphics->FinishRenderingCurrentFrame();
                }

                runtime->Suspend();

                minimized = true;
            }
            else if ((wParam & 0xFFF0) == SC_RESTORE)
            {
                if (minimized)
                {
                    runtime->Resume();

                    minimized = false;

                    if (graphics)
                    {
                        graphics->StartRenderingCurrentFrame();
                    }
                }
            }
            DefWindowProc(hWnd, message, wParam, lParam);
            break;
        }
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        }
        case WM_SIZE:
        {
            if (graphics)
            {
                auto width = static_cast<size_t>(LOWORD(lParam));
                auto height = static_cast<size_t>(HIWORD(lParam));
                UpdateWindowSize(width, height);
            }
            break;
        }
        case WM_DESTROY:
        {
            Uninitialize();
            PostQuitMessage(0);
            break;
        }
        case WM_KEYDOWN:
        {
            if (wParam == 'R')
            {
                RefreshBabylon(hWnd);
            }
            break;
        }
        case WM_MOUSEMOVE:
        {
            if (inputBuffer)
            {
                inputBuffer->SetPointerPosition(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            break;
        }
        case WM_LBUTTONDOWN:
        {
            SetCapture(hWnd);
            if (inputBuffer)
            {
                inputBuffer->SetPointerDown(true);
            }
            break;
        }
        case WM_LBUTTONUP:
        {
            if (inputBuffer)
            {
                inputBuffer->SetPointerDown(false);
            }
            ReleaseCapture();
            break;
        }
        default:
        {
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}
