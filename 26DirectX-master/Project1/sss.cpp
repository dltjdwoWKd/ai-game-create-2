#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console") // 콘솔 창을 띄우면서 WinMain 진입점을 사용하도록 링커에 지시합니다.
#include <windows.h> // 윈도우 창 생성, 메시지 처리, 입력 등 Win32 API를 사용하기 위한 기본 헤더입니다.
#include <d3d11.h> // DirectX 11의 핵심 기능(Device, Context 등)을 사용하기 위한 헤더입니다.
#include <d3dcompiler.h> // 셰이더 코드를 런타임에 컴파일하기 위한 D3DCompiler 헤더입니다.
#include <iostream> // 콘솔창에 printf 등을 통해 문자열을 출력하기 위한 표준 입출력 헤더입니다.
#include <chrono> // 프레임 독립적 이동(DeltaTime)을 위한 고해상도 시간 측정 표준 라이브러리입니다.
#include <thread> // CPU 과부하 방지를 위해 스레드를 일시 정지(sleep)시키는 기능을 위한 헤더입니다.
#include <vector> // 가변 길이 배열인 vector를 사용하여 컴포넌트와 게임 오브젝트를 동적으로 관리하기 위한 헤더입니다.
#include <string> // 객체의 이름을 저장하고 관리하기 위한 C++ 표준 문자열 클래스 헤더입니다.

#pragma comment(lib, "d3d11.lib") // 컴파일 시 DirectX 11 라이브러리 파일을 링크하도록 지시합니다.
#pragma comment(lib, "dxgi.lib") // 하드웨어와 통신하고 화면을 전환(Swap)하는 DXGI 라이브러리를 링크합니다.
#pragma comment(lib, "d3dcompiler.lib") // 셰이더 컴파일을 수행하는 라이브러리를 링크합니다.

// --- [전역 변수: DirectX 11 리소스] ---
ID3D11Device* g_pd3dDevice = nullptr; // 그래픽 카드(GPU)를 추상화한 객체로, 버퍼나 텍스처 등의 리소스를 생성합니다.
ID3D11DeviceContext* g_pImmediateContext = nullptr; // 생성된 리소스를 파이프라인에 묶고 렌더링 명령(Draw)을 내리는 객체입니다.
IDXGISwapChain* g_pSwapChain = nullptr; // 완성된 화면(Back Buffer)을 모니터 화면(Front Buffer)으로 교체(Present)하는 역할을 합니다.
ID3D11RenderTargetView* g_pRenderTargetView = nullptr; // GPU가 그림을 그릴 대상(도화지)이 되는 메모리 뷰입니다.
ID3D11VertexShader* g_pVertexShader = nullptr; // 3D 공간의 정점(Vertex)을 2D 화면 좌표로 변환하는 정점 셰이더 객체입니다.
ID3D11PixelShader* g_pPixelShader = nullptr; // 화면에 그려질 각 픽셀의 최종 색상을 결정하는 픽셀 셰이더 객체입니다.
ID3D11InputLayout* g_pInputLayout = nullptr; // 정점 버퍼의 데이터 구조(위치, 색상 등)가 어떻게 구성되어 있는지 GPU에게 알려주는 레이아웃 객체입니다.
ID3D11Buffer* g_pVertexBuffer = nullptr; // 정점(Vertex) 데이터들을 GPU 메모리에 저장해두는 버퍼입니다.
ID3D11Buffer* g_pConstantBuffer = nullptr; // CPU에서 GPU의 셰이더로 프레임마다 변하는 값(위치, 색상)을 넘겨주기 위한 상수 버퍼입니다.

// 정점 구조체 (위치와 색상 정보를 가짐)
struct Vertex {
    float x, y, z; // 3D 공간상의 X, Y, Z 좌표입니다.
    float r, g, b, a; // 정점의 색상 정보(Red, Green, Blue, Alpha)입니다.
};

// 셰이더로 넘길 상수 버퍼 구조체 (HLSL 셰이더 구조와 메모리를 맞춰야 함, 16바이트 정렬 필수)
struct CBData {
    float offsetX, offsetY, pad1, pad2; // X축 이동량, Y축 이동량이며, pad1/pad2는 16바이트를 맞추기 위한 빈 공간(패딩)입니다.
    float r, g, b, a; // 셰이더에서 기존 색상에 곱해질 틴트(Tint) 컬러 값입니다.
};

// ==============================================================================
// [1단계: Component 기저 클래스]
// ==============================================================================
class Component {
public:
    class GameObject* pOwner = nullptr; // 이 컴포넌트를 소유하고 있는 게임 오브젝트의 포인터를 저장합니다.
    bool isStarted = false; // Start() 함수가 최초 1회 실행되었는지 확인하기 위한 플래그입니다.

    virtual void Start() = 0; // 컴포넌트가 처음 활성화될 때 단 한 번 호출되는 순수 가상 함수입니다. (반드시 재정의 필요)
    virtual void Input() {} // 사용자 입력을 처리하는 가상 함수입니다. (선택적 재정의)
    virtual void Update(float dt) = 0; // 프레임마다 로직을 처리하는 순수 가상 함수이며, DeltaTime을 인자로 받습니다.
    virtual void Render() {} // 화면에 자신을 그릴 때 호출되는 가상 함수입니다. (선택적 재정의)
    virtual ~Component() {} // 상속된 자식 클래스들이 안전하게 소멸될 수 있도록 가상 소멸자를 선언합니다.
};

// ==============================================================================
// [2단계: GameObject 클래스]
// ==============================================================================
class GameObject {
public:
    std::string name; // 게임 오브젝트의 식별용 이름(예: "Player1")을 저장합니다.
    float x, y; // 객체의 2D 공간상 X, Y 위치 좌표를 저장합니다. (요구사항: GameObject가 위치 정보를 가짐)
    std::vector<Component*> components; // 이 객체에 부착된 여러 컴포넌트들의 포인터를 관리하는 동적 배열입니다.

    GameObject(std::string n, float startX, float startY) { // 객체 생성 시 이름과 초기 좌표를 받는 생성자입니다.
        name = n; // 전달받은 이름을 멤버 변수에 저장합니다.
        x = startX; // 전달받은 초기 X 좌표를 저장합니다.
        y = startY; // 전달받은 초기 Y 좌표를 저장합니다.
    }

    ~GameObject() { // 객체가 파괴될 때 호출되는 소멸자입니다.
        for (int i = 0; i < (int)components.size(); i++) { // 자신이 가진 모든 컴포넌트를 순회합니다.
            delete components[i]; // 동적 할당된 컴포넌트들의 메모리를 안전하게 해제합니다.
        }
    }

    void AddComponent(Component* pComp) { // 외부에서 생성된 컴포넌트를 이 객체에 부착하는 함수입니다.
        pComp->pOwner = this; // 추가되는 컴포넌트의 주인을 '이 객체(this)'로 설정합니다.
        pComp->isStarted = false; // 부착된 컴포넌트의 Start()가 아직 실행되지 않았음을 표시합니다.
        components.push_back(pComp); // 컴포넌트 배열의 맨 끝에 해당 컴포넌트를 추가합니다.
    }
};

// ==============================================================================
// [3단계: 실제 구현할 기능 컴포넌트들]
// ==============================================================================

// 기능 1: 플레이어 조종 및 이동 (프레임 독립적 이동 구현)
class PlayerControl : public Component { // Component 클래스를 상속받아 이동 기능을 구현하는 클래스입니다.
public:
    int playerType; // 어떤 플레이어인지(0: Player1, 1: Player2) 구분하기 위한 변수입니다.
    float speed; // 객체의 초당 이동 속도를 저장하는 변수입니다.

    PlayerControl(int type) { // 생성 시 플레이어 타입을 결정합니다.
        playerType = type; // 전달받은 타입을 멤버 변수에 저장합니다.
        speed = 1.5f; // 화면 좌표계(NDC, -1.0 ~ 1.0) 기준으로 1초에 1.5만큼 이동하도록 속도를 설정합니다.
    }

    void Start() override { // 초기화 시 한 번 실행됩니다.
        printf("[%s] PlayerControl 준비 완료!\n", pOwner->name.c_str()); // 콘솔창에 누구의 이동 제어기가 시작되었는지 출력합니다.
    }

    void Update(float dt) override { // 매 프레임마다 위치를 갱신합니다. (DeltaTime 적용)
        // 요구사항: Position = Position + (Velocity * DeltaTime)
        if (playerType == 0) { // Player 1인 경우 방향키를 검사합니다.
            // GetAsyncKeyState의 최상위 비트(0x8000)를 검사하여 현재 키가 눌려있는지 비동기적으로 확인합니다.
            if (GetAsyncKeyState(VK_UP) & 0x8000)    pOwner->y += speed * dt; // 위 방향키: Y좌표를 속도*시간만큼 증가시킵니다.
            if (GetAsyncKeyState(VK_DOWN) & 0x8000)  pOwner->y -= speed * dt; // 아래 방향키: Y좌표를 속도*시간만큼 감소시킵니다.
            if (GetAsyncKeyState(VK_LEFT) & 0x8000)  pOwner->x -= speed * dt; // 왼쪽 방향키: X좌표를 감소시킵니다.
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) pOwner->x += speed * dt; // 오른쪽 방향키: X좌표를 증가시킵니다.
        }
        else if (playerType == 1) { // Player 2인 경우 W, A, S, D 키를 검사합니다.
            if (GetAsyncKeyState('W') & 0x8000) pOwner->y += speed * dt; // W키: Y좌표 증가
            if (GetAsyncKeyState('S') & 0x8000) pOwner->y -= speed * dt; // S키: Y좌표 감소
            if (GetAsyncKeyState('A') & 0x8000) pOwner->x -= speed * dt; // A키: X좌표 감소
            if (GetAsyncKeyState('D') & 0x8000) pOwner->x += speed * dt; // D키: X좌표 증가
        }
    }
};

// 기능 2: 렌더러 (상수 버퍼를 업데이트하여 객체의 현재 위치와 색상으로 그리기)
class TriangleRenderer : public Component { // 화면에 육망성을 그리는 렌더링 컴포넌트입니다.
public:
    float colorR, colorG, colorB; // 이 컴포넌트가 그릴 육망성의 고유 색상 값(RGB)을 저장합니다.

    TriangleRenderer(float r, float g, float b) : colorR(r), colorG(g), colorB(b) {} // 생성자에서 색상을 초기화 리스트로 받습니다.

    void Start() override { // 초기화 함수
        printf("[%s] StarRenderer 장착 완료!\n", pOwner->name.c_str()); // 렌더러가 붙었음을 콘솔에 출력합니다.
    }

    void Update(float dt) override { // Update 로직
        // 렌더링 전용 컴포넌트이므로 수치를 계산하는 Update 단계에서는 아무것도 하지 않습니다.
    }

    void Render() override {
        CBData cb;
        cb.offsetX = pOwner->x;
        cb.offsetY = pOwner->y;
        cb.pad1 = cb.pad2 = 0.0f;
        cb.r = colorR; cb.g = colorG; cb.b = colorB; cb.a = 1.0f;

        g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);
        g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

        // [수정됨] 정점 6개를 그려서 두 개의 정삼각형(육망성)을 출력합니다.
        g_pImmediateContext->Draw(6, 0);
    }
};

// ==============================================================================
// [4단계: GameWorld & GameLoop]
// ==============================================================================
class GameLoop { // 게임의 핵심 엔진 루프와 초기화를 담당하는 클래스입니다.
public:
    bool isRunning; // 게임 루프가 계속 돌아가야 하는지 판단하는 플래그입니다.
    bool isFullscreen; // 현재 전체화면 상태인지 여부를 저장합니다.
    std::vector<GameObject*> gameWorld; // 모든 게임 오브젝트를 담아두는 '바구니(World)' 역할을 하는 배열입니다.
    std::chrono::high_resolution_clock::time_point prevTime; // 이전 프레임의 시간을 기억하는 변수입니다.
    float deltaTime; // 이전 프레임과 현재 프레임 사이의 경과 시간(dt)입니다.

    void Initialize(HWND hWnd) { // 엔진 초기화 및 DirectX 리소스를 생성하는 함수입니다.
        isRunning = true; // 게임 루프 실행 플래그를 켭니다.
        isFullscreen = false; // 초기 상태는 창 모드입니다.
        deltaTime = 0.0f; // DeltaTime을 0으로 초기화합니다.
        prevTime = std::chrono::high_resolution_clock::now(); // 현재 시간을 측정하여 prevTime에 저장합니다.

        // --- DirectX 11 장치 및 스왑체인 생성 ---
        DXGI_SWAP_CHAIN_DESC sd = {}; // 스왑체인 설정을 담을 구조체를 0으로 초기화합니다.
        sd.BufferCount = 1; // 사용할 백 버퍼의 개수를 1개로 설정합니다 (더블 버퍼링).
        sd.BufferDesc.Width = 800; // 요구사항에 따라 창 너비를 800으로 고정합니다.
        sd.BufferDesc.Height = 600; // 요구사항에 따라 창 높이를 600으로 고정합니다.
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 버퍼 픽셀 포맷을 표준 32비트 색상으로 설정합니다.
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 이 버퍼를 렌더링 결과물 출력용으로 사용하겠다고 지정합니다.
        sd.OutputWindow = hWnd; // 렌더링 결과를 띄울 윈도우 핸들을 연결합니다.
        sd.SampleDesc.Count = 1; // 안티앨리어싱(MSAA)을 사용하지 않기 위해 샘플 수를 1로 설정합니다.
        sd.Windowed = TRUE; // 창 모드로 시작하도록 설정합니다.

        // 위 설정을 바탕으로 DX11 디바이스, 컨텍스트, 스왑체인을 한 번에 생성합니다.
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

        // --- 렌더 타겟 뷰 생성 ---
        ID3D11Texture2D* pBackBuffer = nullptr; // 백 버퍼의 텍스처 포인터를 받아올 변수입니다.
        g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer); // 스왑체인으로부터 실제 백 버퍼 메모리를 얻어옵니다.
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView); // 백 버퍼를 바탕으로 도화지(RTV)를 생성합니다.
        pBackBuffer->Release(); // RTV를 만들었으므로 텍스처 포인터는 메모리 누수 방지를 위해 해제(Release)합니다.

        // --- 셰이더 컴파일 (HLSL 코드를 문자열로 작성) ---
        const char* shaderSource = R"( 
            cbuffer TransformBuffer : register(b0) { // C++의 CBData와 매칭되는 상수 버퍼 (0번 슬롯 b0)
                float x, y, pad1, pad2; // X, Y 오프셋 및 패딩
                float4 tintColor; // 곱해질 틴트 컬러 (RGBA)
            };
            struct VS_IN { float3 pos : POSITION; float4 col : COLOR; }; // 정점 버퍼에서 들어오는 입력 데이터 구조체
            struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; }; // 정점 셰이더에서 픽셀 셰이더로 넘길 데이터 구조체
            
            PS_IN VS(VS_IN input) {  // 정점 셰이더 메인 함수
                PS_IN output; // 다음 단계로 넘길 출력 구조체 생성
                // 원래 정점 위치에 상수 버퍼로 받은 x, y 오프셋을 더해 객체를 이동시킵니다.
                output.pos = float4(input.pos.x + x, input.pos.y + y, input.pos.z, 1.0f); 
                // 원본 색상에 상수 버퍼로 받은 틴트 컬러를 곱해 색상을 입힙니다.
                output.col = input.col * tintColor; 
                return output; // 픽셀 셰이더로 데이터를 넘깁니다.
            }
            float4 PS(PS_IN input) : SV_Target { return input.col; } // 픽셀 셰이더: 전달받은 색상값을 그대로 픽셀에 칠합니다.
        )";

        ID3DBlob* vsBlob, * psBlob; // 컴파일된 셰이더 바이너리를 담을 블롭 객체 포인터입니다.
        // 문자열로 된 셰이더 소스 중 "VS" 함수를 정점 셰이더 4.0 버전으로 컴파일합니다.
        D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
        // 문자열로 된 셰이더 소스 중 "PS" 함수를 픽셀 셰이더 4.0 버전으로 컴파일합니다.
        D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);
        // 컴파일된 바이너리를 기반으로 GPU에 실제 정점 셰이더 객체를 생성합니다.
        g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
        // 컴파일된 바이너리를 기반으로 GPU에 실제 픽셀 셰이더 객체를 생성합니다.
        g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

        // 정점의 데이터가 어떻게 나열되어 있는지 레이아웃을 정의합니다.
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // 첫 12바이트는 3차원 위치 데이터
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // 다음 16바이트는 4차원 색상 데이터
        };
        // 정의한 레이아웃과 컴파일된 정점 셰이더 서명을 비교하여 인풋 레이아웃 객체를 생성합니다.
        g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
        vsBlob->Release(); psBlob->Release(); // 컴파일에 사용된 임시 블롭 메모리를 해제합니다.

        // --- [수정됨] 정점 버퍼 생성 (하얀색의 기본 육망성을 12개 정점으로 정의) ---
        // 삼각형 리스트 토폴로지를 사용하므로, 3개씩 짝을 지어 4개의 삼각형을 정의합니다.
        Vertex vertices[] = {
            // [첫 번째 삼각형: 위를 향하는 정삼각형]
            {  0.0f,    0.2f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f }, // 위쪽 꼭지점
            {  0.1732f,-0.1f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f }, // 우측 하단 (0.2 * cos(30))
            { -0.1732f,-0.1f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f }, // 좌측 하단

            // [두 번째 삼각형: 아래를 향하는 정삼각형]
            {  0.0f,   -0.2f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f }, // 아래쪽 꼭지점
            { -0.1732f, 0.1f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f }, // 좌측 상단
            {  0.1732f, 0.1f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f }, // 우측 상단
        };
        D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 }; // 정점 버퍼 생성 설정 구조체입니다.
        D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 }; // 초기 데이터로 배열의 포인터를 연결합니다.
        g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer); // GPU 메모리에 정점 버퍼를 최종 생성합니다.

        // --- 상수 버퍼 생성 ---
        D3D11_BUFFER_DESC cbd = { sizeof(CBData), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 }; // 상수 버퍼 생성 설정(크기는 CBData 만큼)
        g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer); // 초기 데이터 없이 상수 버퍼를 GPU에 생성합니다.
    }

    void ProcessSystemInput() { // 시스템 단의 입력(종료, 화면 전환)을 처리하는 함수입니다.
        // 요구사항: ESC 종료 처리
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { // ESC 키가 눌렸다면
            isRunning = false; // 루프 플래그를 꺼서 게임을 종료 상태로 만듭니다.
            PostQuitMessage(0); // 윈도우 OS에 프로그램 종료 메시지를 보냅니다.
        }
        // 요구사항: F키 전체화면 토글 (단발성 입력을 위해 0x0001 비트 검사)
        if (GetAsyncKeyState('F') & 0x0001) { // F 키가 방금 눌렸다면
            isFullscreen = !isFullscreen; // 플래그를 반전시킵니다.
            g_pSwapChain->SetFullscreenState(isFullscreen, nullptr); // 스왑체인에 지시하여 전체화면/창모드를 실제로 전환합니다.
        }
    }

    void Input() { // 엔진의 첫 번째 단계: 모든 입력을 받습니다.
        ProcessSystemInput(); // 먼저 시스템 입력을 처리합니다.
        for (auto obj : gameWorld) { // GameWorld 안의 모든 객체를 순회합니다.
            for (auto comp : obj->components) comp->Input(); // 객체 안의 모든 컴포넌트의 Input()을 호출합니다.
        }
    }

    void Update() { // 엔진의 두 번째 단계: 로직을 계산합니다.
        // 모든 객체의 Start() 함수가 최초 1회만 호출되도록 보장합니다.
        for (auto obj : gameWorld) {
            for (auto comp : obj->components) {
                if (!comp->isStarted) { // 아직 시작되지 않았다면
                    comp->Start(); // Start() 호출
                    comp->isStarted = true; // 플래그를 참으로 변경
                }
            }
        }
        // 본격적인 로직 업데이트 (모든 컴포넌트의 Update에 dt를 넘겨줌)
        for (auto obj : gameWorld) {
            for (auto comp : obj->components) comp->Update(deltaTime); // 이동 등의 연산 수행
        }
    }

    void Render() { // 엔진의 세 번째 단계: 화면에 결과물을 그립니다.
        // 1. 도화지 색상 지정 (배경색: 어두운 남색 계열)
        float clearColor[] = { 0.15f, 0.15f, 0.2f, 1.0f };
        // 렌더 타겟(도화지)을 지정한 배경색으로 깨끗하게 칠해 지웁니다.
        g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

        // 2. 화면에 그려질 영역(뷰포트)을 설정합니다. (0,0 부터 800x600 크기 전체 사용)
        D3D11_VIEWPORT vp = { 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f };
        g_pImmediateContext->RSSetViewports(1, &vp); // 파이프라인에 뷰포트 세팅 적용
        // 우리가 만든 도화지(RTV)를 최종 출력 대상 파이프라인에 묶습니다.
        g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

        // 3. 모델을 그리기 위한 공통 파이프라인 세팅 (어떤 정점을, 어떤 모양으로, 어떤 셰이더로 그릴 것인가)
        UINT stride = sizeof(Vertex), offset = 0; // 정점 하나당 크기와 시작 오프셋을 지정합니다.
        g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset); // 파이프라인에 정점 버퍼 묶기
        g_pImmediateContext->IASetInputLayout(g_pInputLayout); // 인풋 레이아웃 묶기
        g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 점 3개씩 이어서 삼각형으로 그리겠다고 선언
        g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0); // 파이프라인에 정점 셰이더 묶기
        g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0); // 파이프라인에 픽셀 셰이더 묶기

        // 4. 각 객체의 렌더링 함수 실행
        for (auto obj : gameWorld) { // 세계의 모든 객체를 돌면서
            for (auto comp : obj->components) comp->Render(); // 렌더 컴포넌트가 알아서 상수 버퍼를 채우고 Draw 하도록 지시
        }

        // 5. 백 버퍼에 다 그렸으므로 화면(프론트 버퍼)으로 결과물을 내보냅니다 (VSync On).
        g_pSwapChain->Present(1, 0);
    }

    void Run() { // 무한 게임 루프를 돌리는 핵심 함수입니다.
        MSG msg = { 0 }; // 윈도우 메시지를 받을 구조체입니다.
        // 요구사항: PeekMessage 기반의 Non-blocking 루프
        while (isRunning) { // isRunning이 참인 동안 계속 돕니다.
            // PeekMessage는 메시지가 없어도 프로그램이 멈추지 않고(Non-blocking) false를 반환합니다.
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) isRunning = false; // 종료 메시지면 루프 탈출
                TranslateMessage(&msg); // 키보드 입력 메시지 해석
                DispatchMessage(&msg); // 윈도우 프로시저(WndProc)로 메시지 전달
            }
            else { // 처리할 메시지가 없을 때 게임 로직을 실행합니다.
                // A. 시간 관리 (DeltaTime 계산)
                auto currentTime = std::chrono::high_resolution_clock::now(); // 현재 시간 기록
                std::chrono::duration<float> elapsed = currentTime - prevTime; // 현재 시간에서 이전 시간을 빼서 차이 구함
                deltaTime = elapsed.count(); // float 형태로 초 단위 DeltaTime 저장
                prevTime = currentTime; // 다음 프레임을 위해 현재 시간을 과거 시간으로 옮김

                // B. 엔진 파이프라인 실행
                Input(); // 입력 수집
                Update(); // 로직 계산
                Render(); // 화면 출력

                // CPU 코어 100% 점유율(과부하)을 막기 위해 최소 1ms 휴식시간을 부여합니다.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    ~GameLoop() { // 루프 종료 후 모든 자원을 해제하는 소멸자입니다.
        // 1. GameWorld의 객체 및 컴포넌트 메모리 해제 (C++ new 로 생성한 메모리 삭제)
        for (auto obj : gameWorld) delete obj;

        // 2. DX11 리소스 반납 (GPU 메모리 해제)
        if (g_pConstantBuffer) g_pConstantBuffer->Release(); // 상수 버퍼 반납
        if (g_pVertexBuffer) g_pVertexBuffer->Release(); // 정점 버퍼 반납
        if (g_pInputLayout) g_pInputLayout->Release(); // 인풋 레이아웃 반납
        if (g_pVertexShader) g_pVertexShader->Release(); // 정점 셰이더 반납
        if (g_pPixelShader) g_pPixelShader->Release(); // 픽셀 셰이더 반납
        if (g_pRenderTargetView) g_pRenderTargetView->Release(); // RTV 도화지 반납
        if (g_pSwapChain) { // 스왑체인 해제 전 처리
            g_pSwapChain->SetFullscreenState(FALSE, nullptr); // 전체화면 상태에서 강제 종료 시 오류 방지 위해 창모드 복귀
            g_pSwapChain->Release(); // 스왑체인 반납
        }
        if (g_pImmediateContext) g_pImmediateContext->Release(); // 디바이스 컨텍스트 반납
        if (g_pd3dDevice) g_pd3dDevice->Release(); // 디바이스 반납

        printf("안전하게 모든 자원을 해제했습니다.\n"); // 종료 완료 메시지
    }
};

// 윈도우 프로시저 (운영체제가 이 프로그램 창에 보내는 각종 메시지를 처리하는 함수)
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) { // 창의 X 버튼이 눌려서 파괴될 때
        PostQuitMessage(0); return 0; // 메시지 큐에 WM_QUIT을 넣고 종료 진행
    }
    return DefWindowProc(hWnd, message, wParam, lParam); // 그 외 메시지는 윈도우 기본 처리 함수로 넘김
}

// ==============================================================================
// [Main 함수 (엔트리 포인트)]
// ==============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 1. 윈도우 클래스 구조체를 설정하여 운영체제에 창의 틀을 등록합니다.
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc; // 메시지를 처리할 콜백 함수 지정
    wcex.hInstance = hInstance; // 프로그램의 인스턴스 핸들 지정
    wcex.lpszClassName = L"EngineWindowClass"; // 생성할 윈도우 클래스 이름 지정
    RegisterClassExW(&wcex); // 설정된 정보로 윈도우 클래스 등록

    // 2. 내부 해상도가 800x600이 되도록 테두리를 포함한 실제 창 크기를 계산합니다.
    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE); // 메뉴가 없는 기본 윈도우 스타일 기준 크기 보정

    // 3. 실제 윈도우 창을 생성합니다 (창 크기 조절 불가 옵션 적용).
    HWND hWnd = CreateWindowW(L"EngineWindowClass", L"Component DX11 Engine - ESC: Exit, F: Fullscreen",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,  // 창 스타일: 겹침, 제목표시줄, 시스템메뉴, 최소화버튼 (크기조절 불가)
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr); // 크기와 위치 지정
    ShowWindow(hWnd, nCmdShow); // 생성된 창을 화면에 보여줍니다.

    // 4. 게임 엔진 시스템 초기화
    GameLoop gLoop; // 스택 메모리에 루프 객체 생성
    gLoop.Initialize(hWnd); // 생성한 창 핸들을 넘겨주어 DX11 시스템 초기화

    // --- 객체 조립(Composition) 단계 ---

    // [Player 1] 요구사항에 맞춰 객체 조립 (우측 배치, 방향키 이동, 파란색 육망성)
    GameObject* p1 = new GameObject("Player1", 0.5f, 0.0f); // X: 0.5, Y: 0 지점에 Player1 생성
    p1->AddComponent(new PlayerControl(0)); // 타입 0(방향키 컨트롤) 이동 컴포넌트 부착
    p1->AddComponent(new TriangleRenderer(0.0f, 0.5f, 1.0f)); // 파란빛(R:0, G:0.5, B:1.0) 렌더 컴포넌트 부착
    gLoop.gameWorld.push_back(p1); // 게임 월드 바구니에 집어넣음

    // [Player 2] 요구사항에 맞춰 객체 조립 (좌측 배치, WASD 이동, 붉은색 육망성)
    GameObject* p2 = new GameObject("Player2", -0.5f, 0.0f); // X: -0.5, Y: 0 지점에 Player2 생성
    p2->AddComponent(new PlayerControl(1)); // 타입 1(WASD 컨트롤) 이동 컴포넌트 부착
    p2->AddComponent(new TriangleRenderer(1.0f, 0.2f, 0.2f)); // 붉은빛(R:1.0, G:0.2, B:0.2) 렌더 컴포넌트 부착
    gLoop.gameWorld.push_back(p2); // 게임 월드 바구니에 집어넣음

    // 5. 모든 세팅이 완료되었으므로 무한 게임 루프를 가동합니다.
    gLoop.Run();

    return 0; // 프로그램 정상 종료
}