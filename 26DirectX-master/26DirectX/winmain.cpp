#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <chrono>
#include <thread>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

// --- [전역 객체] ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr; // 위치 전달용 상수 버퍼

// 상수 버퍼 구조체 (16바이트 정렬)
struct TransformBuffer {
    float offsetX;
    float offsetY;
    float padding[2];
};

// 셰이더 코드 (cbuffer 추가됨)
const char* g_szShaderCode = R"(
cbuffer TransformBuffer : register(b0) {
    float2 offset;
    float2 padding;
};

struct VS_OUTPUT { float4 Pos : SV_POSITION; float4 Col : COLOR; };

VS_OUTPUT VS(float3 pos : POSITION, float4 col : COLOR) {
    VS_OUTPUT output;
    output.Pos = float4(pos.x + offset.x, pos.y + offset.y, pos.z, 1.0f);
    output.Col = col;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_Target { return input.Col; }
)";

struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    const int width = 600;
    const int height = 600;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0, hInst, nullptr, nullptr, nullptr, nullptr, L"HexagramEngine", nullptr };
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, width, height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowW(L"HexagramEngine", L"DX11 DeltaTime Hexagram", WS_OVERLAPPEDWINDOW,
        100, 100, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

    // DX11 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // 셰이더 컴파일
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    D3DCompile(g_szShaderCode, strlen(g_szShaderCode), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(g_szShaderCode, strlen(g_szShaderCode), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pVertexLayout);
    vsBlob->Release(); psBlob->Release();

    // 상수 버퍼 생성 (DYNAMIC)
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.ByteWidth = sizeof(TransformBuffer);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    // 육망성 정점 데이터
    Vertex vertices[] = {
        {  0.0f,  0.6f, 0.0f,  0, 0, 1, 1 }, {  0.5f, -0.3f, 0.0f,  0, 0, 1, 1 }, { -0.5f, -0.3f, 0.0f,  0, 0, 1, 1 },
        {  0.0f, -0.6f, 0.0f,  1, 0, 0, 1 }, { -0.5f,  0.3f, 0.0f,  1, 0, 0, 1 }, {  0.5f,  0.3f, 0.0f,  1, 0, 0, 1 }
    };

    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);

    ShowWindow(hWnd, nCmdShow);
    MSG msg = { 0 };

    // --- [게임 루프 및 타이머 설정] ---
    auto prevTime = std::chrono::high_resolution_clock::now();
    float consoleUpdateTimer = 0.0f;

    // 45프레임 고정 설정
    const float targetFPS = 45.0f;
    const float targetFrameTime = 1.0f / targetFPS;

    // 오브젝트 상태 변수
    float playerX = 0.0f;
    float playerY = 0.0f;
    float moveSpeed = 1.5f;

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // 1. DeltaTime 계산 및 프레임 고정
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = currentTime - prevTime;
            float dt = elapsed.count();

            // 목표 프레임 시간보다 적게 지났다면 대기 (프레임 고정 핵심 로직)
            if (dt < targetFrameTime) {
                std::this_thread::yield();
                continue;
            }

            // DeltaTime 폭주 방지 (창 이동 등 렉 걸렸을 때 방어)
            if (dt > 0.1f) dt = 0.1f;

            prevTime = currentTime; // 업데이트 확정 시점에서 시간 갱신

            // 2. 입력 및 업데이트
            if (GetAsyncKeyState('W') & 0x8000) playerY += moveSpeed * dt;
            if (GetAsyncKeyState('S') & 0x8000) playerY -= moveSpeed * dt;
            if (GetAsyncKeyState('A') & 0x8000) playerX -= moveSpeed * dt;
            if (GetAsyncKeyState('D') & 0x8000) playerX += moveSpeed * dt;

            // 콘솔 출력 갱신 (0.1초 주기)
            consoleUpdateTimer += dt;
            if (consoleUpdateTimer >= 0.1f) {
                system("cls");
                printf("=== Engine Status ===\n");
                printf("FPS      : %.2f (Target: 45)\n", 1.0f / dt);
                printf("DeltaTime: %.4f s\n", dt);
                printf("Position : X(%.2f), Y(%.2f)\n", playerX, playerY);
                printf("Window   : 600 x 600 (Client)\n");
                printf("=====================\n");
                consoleUpdateTimer = 0.0f;
            }

            // 3. 렌더링
            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            D3D11_VIEWPORT vp = { 0, 0, (float)width, (float)height, 0, 1 };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            // CPU의 위치 데이터를 GPU 상수 버퍼로 복사
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            g_pImmediateContext->Map(g_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            TransformBuffer* pData = (TransformBuffer*)mappedResource.pData;
            pData->offsetX = playerX;
            pData->offsetY = playerY;
            g_pImmediateContext->Unmap(g_pConstantBuffer, 0);

            g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
            g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

            g_pImmediateContext->Draw(6, 0);
            g_pSwapChain->Present(0, 0);
        }
    }

    // 자원 해제
    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pVertexLayout) g_pVertexLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader)  g_pPixelShader->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();

    return 0;
}