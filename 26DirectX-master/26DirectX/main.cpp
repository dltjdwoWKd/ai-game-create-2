#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <math.h>

// 위치
float g_x = 400.0f;
float g_y = 300.0f;
float g_speed = 200.0f;

// 입력 상태
bool g_keyLeft = false;
bool g_keyRight = false;
bool g_keyUp = false;
bool g_keyDown = false;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_LEFT:  g_keyLeft = true; break;
        case VK_RIGHT: g_keyRight = true; break;
        case VK_UP:    g_keyUp = true; break;
        case VK_DOWN:  g_keyDown = true; break;
        }
        break;

    case WM_KEYUP:
        switch (wParam)
        {
        case VK_LEFT:  g_keyLeft = false; break;
        case VK_RIGHT: g_keyRight = false; break;
        case VK_UP:    g_keyUp = false; break;
        case VK_DOWN:  g_keyDown = false; break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 6망성
void DrawStar(HDC hdc, float cx, float cy)
{
    POINT tri1[3];
    POINT tri2[3];

    float size = 50.0f;

    for (int i = 0; i < 3; i++)
    {
        float angle = (90 + i * 120) * 3.14159f / 180.0f;
        tri1[i].x = (LONG)(cx + cosf(angle) * size);
        tri1[i].y = (LONG)(cy - sinf(angle) * size);
    }

    for (int i = 0; i < 3; i++)
    {
        float angle = (270 + i * 120) * 3.14159f / 180.0f;
        tri2[i].x = (LONG)(cx + cosf(angle) * size);
        tri2[i].y = (LONG)(cy - sinf(angle) * size);
    }

    Polygon(hdc, tri1, 3);
    Polygon(hdc, tri2, 3);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"GameWindow";

    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(
        L"GameWindow",
        L"GameLoop Star",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // 🔥 입력 포커스 강제
    SetFocus(hWnd);

    MSG msg = {};

    LARGE_INTEGER freq, prev, curr;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    // 🎮 Game Loop
    while (true)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // 시간
            QueryPerformanceCounter(&curr);
            float dt = (float)(curr.QuadPart - prev.QuadPart) / freq.QuadPart;
            prev = curr;

            // 이동
            if (g_keyLeft)  g_x -= g_speed * dt;
            if (g_keyRight) g_x += g_speed * dt;
            if (g_keyUp)    g_y -= g_speed * dt;
            if (g_keyDown)  g_y += g_speed * dt;

            // 🎨 더블 버퍼링 렌더링
            HDC hdc = GetDC(hWnd);
            HDC memDC = CreateCompatibleDC(hdc);

            RECT rc;
            GetClientRect(hWnd, &rc);

            HBITMAP bitmap = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, bitmap);

            // 배경
            FillRect(memDC, &rc, (HBRUSH)(COLOR_WINDOW + 1));

            // 별 그리기
            DrawStar(memDC, g_x, g_y);

            // 복사
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

            // 정리
            SelectObject(memDC, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memDC);
            ReleaseDC(hWnd, hdc);
        }
    }

    return (int)msg.wParam;
}