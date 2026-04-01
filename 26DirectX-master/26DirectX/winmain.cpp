#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

// [필수] 라이브러리 및 콘솔 서브시스템 링커 설정 (누락 복구)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

// --- [전역 DX11 객체] ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;

struct TransformBuffer {
    float offsetX;
    float offsetY;
    float padding[2];
};

struct Vertex { float x, y, z; float r, g, b, a; };

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

// ================================================================================
// [엔진 코어 아키텍처]
// ================================================================================

class GameObject; // 전방 선언 (Component가 GameObject를 참조하기 위해 필수)

class Component {
public:
    GameObject* pOwner = nullptr;
    bool isStarted = false;

    virtual void Start() {}
    virtual void OnInput() {}
    virtual void OnUpdate(float dt) {}
    virtual void OnRender() {}
    virtual ~Component() {}
};

class GameObject {
public:
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    std::vector<Component*> components;

    GameObject(std::string n) : name(n) {}
    ~GameObject() {
        for (auto comp : components) delete comp;
    }

    void AddComponent(Component* pComp) {
        pComp->pOwner = this;
        pComp->isStarted = false;
        components.push_back(pComp);
    }
};

class GameWorld {
public:
    std::vector<GameObject*> objects;

    void AddObject(GameObject* obj) { objects.push_back(obj); }

    void ProcessInput() {
        for (auto obj : objects)
            for (auto comp : obj->components)
                comp->OnInput();
    }

    void Update(float dt) {
        for (auto obj : objects) {
            for (auto comp : obj->components) {
                if (!comp->isStarted) {
                    comp->Start();
                    comp->isStarted = true;
                }
                comp->OnUpdate(dt);
            }
        }
    }

    void Render() {
        for (auto obj : objects)
            for (auto comp : obj->components)
                comp->OnRender();
    }

    ~GameWorld() {
        for (auto obj : objects) delete obj;
    }
};

// ================================================================================
// [기능 컴포넌트]
// ================================================================================

class PlayerControl : public Component {
public:
    float speed = 1.5f;
    bool m_Up = false, m_Down = false, m_Left = false, m_Right = false;

    void Start() override {
        printf("[%s] 플레이어 조종 컴포넌트 활성화!\n", pOwner->name.c_str());
    }

    void OnInput() override {
        m_Up = (GetAsyncKeyState('W') & 0x8000);
        m_Down = (GetAsyncKeyState('S') & 0x8000);
        m_Left = (GetAsyncKeyState('A') & 0x8000);
        m_Right = (GetAsyncKeyState('D') & 0x8000);
    }

    void OnUpdate(float dt) override {
        if (m_Up)    pOwner->y += speed * dt;
        if (m_Down)  pOwner->y -= speed * dt;
        if (m_Left)  pOwner->x -= speed * dt;
        if (m_Right) pOwner->x += speed * dt;
    }
};

class HexagramRenderer : public Component {
public:
    void OnRender() override {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        g_pImmediateContext->Map(g_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        TransformBuffer* pData = (TransformBuffer*)mappedResource.pData;
        pData->offsetX = pOwner->x;
        pData->offsetY = pOwner->y;
        g_pImmediateContext->Unmap(g_pConstantBuffer, 0);

        g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
        UINT stride = sizeof(Vertex), offset = 0;
        g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
        g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

        g_pImmediateContext->Draw(6, 0);
    }
};

// ================================================================================
// [메인 함수 및 초기화]
// ================================================================================

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void InitDirectX(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1; sd.BufferDesc.Width = 600; sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    ID3DBlob* vsBlob = nullptr; ID3DBlob* psBlob = nullptr;
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

    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.ByteWidth = sizeof(TransformBuffer);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    Vertex vertices[] = {
        {  0.0f,  0.6f, 0.0f,  0, 0, 1, 1 }, {  0.5f, -0.3f, 0.0f,  0, 0, 1, 1 }, { -0.5f, -0.3f, 0.0f,  0, 0, 1, 1 },
        {  0.0f, -0.6f, 0.0f,  1, 0, 0, 1 }, { -0.5f,  0.3f, 0.0f,  1, 0, 0, 1 }, {  0.5f,  0.3f, 0.0f,  1, 0, 0, 1 }
    };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // 기존 설정에 맞춰 WNDCLASSEXW 및 L""(유니코드 문자열)로 원상 복구
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0, hInst, nullptr, nullptr, nullptr, nullptr, L"HexagramEngine", nullptr };
    RegisterClassExW(&wc);
    HWND hWnd = CreateWindowW(L"HexagramEngine", L"OOP DX11 Engine", WS_OVERLAPPEDWINDOW, 100, 100, 600, 600, nullptr, nullptr, hInst, nullptr);

    InitDirectX(hWnd);
    ShowWindow(hWnd, nCmdShow);

    // [엔진 객체 조립]
    GameWorld world;
    GameObject* player = new GameObject("Player1");
    player->AddComponent(new PlayerControl());
    player->AddComponent(new HexagramRenderer());
    world.AddObject(player);

    MSG msg = { 0 };
    auto prevTime = std::chrono::high_resolution_clock::now();

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            auto currentTime = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(currentTime - prevTime).count();
            prevTime = currentTime;

            world.ProcessInput();
            world.Update(dt);

            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            D3D11_VIEWPORT vp = { 0, 0, 600, 600, 0, 1 };
            g_pImmediateContext->RSSetViewports(1, &vp);
            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            world.Render();

            g_pSwapChain->Present(0, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // 메모리 누수 방지 리소스 해제
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