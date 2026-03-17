#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <stdio.h>

struct GameContext
{
    int playerPos;
    bool isRunning;
};

// 입력 처리 (Enter 필요 없음)
void ProcessInput(GameContext* ctx)
{
    if (GetAsyncKeyState('A') & 0x8000)
    {
        ctx->playerPos--;
        Sleep(120);
    }

    if (GetAsyncKeyState('D') & 0x8000)
    {
        ctx->playerPos++;
        Sleep(120);
    }

    if (GetAsyncKeyState('Q') & 0x8000)
    {
        ctx->isRunning = false;
    }
}

// 게임 상태 업데이트
void Update(GameContext* ctx)
{
    if (ctx->playerPos < 0)
        ctx->playerPos = 0;

    if (ctx->playerPos > 10)
        ctx->playerPos = 10;
}

// 화면 출력
void Render(GameContext* ctx)
{
    system("cls");   // 화면 초기화

    printf("========= GAME SCREEN =========\n");
    printf("Press A / D to Move | Q to Quit\n\n");

    printf("Player Position : %d\n", ctx->playerPos);
    printf("[");

    for (int i = 0; i <= 10; i++)
    {
        if (i == ctx->playerPos)
            printf("P");
        else
            printf("_");
    }

    printf("]\n");
    printf("================================\n");
}

// 윈도우 메시지 처리
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

// 프로그램 시작
int APIENTRY WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = L"DXWindow";

    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(
        L"DXWindow",
        L"DirectX Learning Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    ShowWindow(hWnd, nCmdShow);

    GameContext game;
    game.playerPos = 5;
    game.isRunning = true;

    MSG msg = {};

    // 게임 루프
    while (game.isRunning)
    {
        // 윈도우 메시지 처리
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ProcessInput(&game);
        Update(&game);
        Render(&game);

        Sleep(16); // 약 60FPS
    }

    return 0;