#include <windows.h>
#include <vector>
#include <string>
#include <chrono>
// DirectX 11을 사용하기 위한 필수 도구상자(헤더 파일)들입니다.
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX; // 수학 계산(좌표 등)을 쉽게 하려고 사용합니다.

/*
================================================================================
 [1단계: 컴포넌트 (기능 스티커) 기저 클래스]
 왜 필요한가요?: 모든 기능(이동, 그리기 등)이 공통으로 지켜야 할 '규칙'입니다.
 이 규칙이 있어야 나중에 '바구니(GameObject)'가 기능들을 한 번에 실행시킬 수 있습니다.
================================================================================
*/
class Component {
public:
    class GameObject* pOwner; // 이 기능이 어떤 물체에 붙어있는지 기억하는 변수입니다.
    bool isStarted;           // Start()가 딱 한 번만 실행되게 확인하는 도장입니다.

    // DirectX에서는 그림을 그리기 위해 '그래픽카드 연결 장치(Device, Context)'가 필요합니다.
    // 그래서 콘솔과 다르게 매개변수로 이 장치들을 전달받습니다.
    virtual void Start(ID3D11Device* device) = 0;             // 태어날 때 한 번 초기화
    virtual void OnInput() {}                                 // 키보드 확인
    virtual void OnUpdate(float dt) = 0;                      // 위치나 수치 계산
    virtual void OnRender(ID3D11DeviceContext* context) {}    // 화면에 그리기

    virtual ~Component() {}
};

/*
================================================================================
 [2단계: 게임 오브젝트 (빈 바구니)]
 왜 필요한가요?: 화면에 나타날 플레이어, 적, 배경 등을 담는 '빈 껍데기'입니다.
 여기에 위치(x, y) 정보를 주고, 필요한 기능(스티커)들을 붙여서 완성합니다.
================================================================================
*/
class GameObject {
public:
    std::string name;       // 물체의 이름 (예: "Player1")
    float x, y;             // 화면에서의 위치 좌표
    std::vector<Component*> components; // 이 물체에 붙은 기능(스티커)들의 목록입니다.

    // 물체가 태어날 때 이름과 시작 위치를 정해줍니다.
    GameObject(std::string n, float startX, float startY) {
        name = n;
        x = startX;
        y = startY;
    }

    // 물체가 죽을 때(삭제될 때) 자기가 가지고 있던 기능들도 같이 버립니다.(메모리 정리)
    ~GameObject() {
        for (int i = 0; i < (int)components.size(); i++) {
            delete components[i];
        }
    }

    // 빈 바구니에 새로운 기능을 착! 하고 붙여주는 함수입니다.
    void AddComponent(Component* pComp) {
        pComp->pOwner = this;      // "이 기능의 주인은 나(this)야!" 라고 알려줍니다.
        pComp->isStarted = false;  // 아직 시작 안 했다고 표시합니다.
        components.push_back(pComp); // 목록에 추가합니다.
    }
};

/*
================================================================================
 [3단계: 실제 구현할 독립적인 기능들]
 * 특징: 각 기능은 자기 할 일만 합니다. 이동은 이동만, 그리기는 그리기만 합니다.
         그래서 어떤 오브젝트에 붙이든 상관없이 독립적으로 잘 돌아갑니다.
================================================================================
*/

// 기능 1: 방향키나 WASD로 움직이게 해주는 조종 기능
class PlayerMove : public Component {
public:
    int keyUp, keyDown, keyLeft, keyRight; // 어떤 키를 누를지 저장하는 변수
    float speed;                           // 움직이는 속도

    // 기능을 만들 때, 어떤 키로 움직일지 미리 정해줍니다. (예: W,A,S,D 또는 방향키)
    PlayerMove(int u, int d, int l, int r) {
        keyUp = u; keyDown = d; keyLeft = l; keyRight = r;
    }

    void Start(ID3D11Device* device) override {
        speed = 300.0f; // 1초에 300픽셀만큼 움직이도록 속도를 정합니다.
    }

    void OnUpdate(float dt) override {
        // [입력 확인 및 좌표 계산]
        // GetAsyncKeyState는 해당 키가 눌렸는지 확인하는 윈도우 기능입니다.
        // dt(델타타임)를 곱해주는 이유: 똥컴이든 좋은 컴퓨터든 똑같은 속도로 움직이게 하기 위해서입니다!
        if (GetAsyncKeyState(keyUp) & 0x8000)    pOwner->y += speed * dt;
        if (GetAsyncKeyState(keyDown) & 0x8000)  pOwner->y -= speed * dt;
        if (GetAsyncKeyState(keyLeft) & 0x8000)  pOwner->x -= speed * dt;
        if (GetAsyncKeyState(keyRight) & 0x8000) pOwner->x += speed * dt;
    }
};

// 기능 2: 화면에 '육망성(별 모양)'을 그려주는 그래픽 기능
class StarRenderer : public Component {
public:
    float r, g, b;

    StarRenderer(float red, float green, float blue) {
        r = red; g = green; b = blue;
    }

    void Start(ID3D11Device* device) override {
        // (생략)
    }

    // ⭐ 이 부분을 추가해 주세요! (비어있어도 규칙을 지키기 위해 필요합니다)
    void OnUpdate(float dt) override {
        // 그리기는 위치 계산이 필요 없으므로 비워둡니다.
    }

    void OnRender(ID3D11DeviceContext* context) override {
        // (생략)
    }
};

// 기능 3: 시스템을 제어하는 기능 (전체화면, 종료)
class SystemControl : public Component {
public:
    IDXGISwapChain* pSwapChain; // 화면(창)을 관리하는 DirectX 부품입니다.
    bool isFullscreen;          // 지금 전체화면인지 아닌지 기억하는 변수
    bool isFPressed;            // F키를 꾹 누르고 있을 때 중복 실행을 막기 위한 도장

    SystemControl(IDXGISwapChain* swapChain) {
        pSwapChain = swapChain;
        isFullscreen = false;
        isFPressed = false;
    }

    void Start(ID3D11Device* device) override {}

    void OnUpdate(float dt) override {
        // 1. ESC 누르면 프로그램 끄기
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            PostQuitMessage(0); // 윈도우에게 "나 이제 꺼질래" 하고 알려줍니다.
        }

        // 2. F 누르면 창모드/전체화면 바꾸기 (토글)
        if (GetAsyncKeyState('F') & 0x8000) {
            if (isFPressed == false) { // 키를 막 눌렀을 때 딱 한 번만 작동하게 합니다.
                isFullscreen = !isFullscreen; // 상태를 반대로 뒤집습니다 (T->F, F->T)
                pSwapChain->SetFullscreenState(isFullscreen, nullptr); // 실제 화면 전환 명령
                isFPressed = true; // "눌렀다"고 도장을 찍어둡니다.
            }
        }
        else {
            // 손을 떼면 다시 누를 수 있게 도장을 지워줍니다.
            isFPressed = false;
        }
    }
};

/*
================================================================================
 [4단계: 메인 엔진 루프]
 콘솔의 int main()과 같습니다. 실제 게임이 계속 돌아가는 심장부입니다.
================================================================================
*/
void RunGameEngine(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapChain) {

    // [준비 단계] 게임 월드 바구니 만들기
    std::vector<GameObject*> gameWorld;

    // 1. 시스템 관리자 객체 조립 (위치값은 필요 없으니 0,0)
    GameObject* systemManager = new GameObject("System", 0.0f, 0.0f);
    systemManager->AddComponent(new SystemControl(swapChain));
    gameWorld.push_back(systemManager);

    // 2. 플레이어 1 조립 (육망성 모양 + 방향키 조작)
    GameObject* player1 = new GameObject("Player1", -100.0f, 0.0f); // 왼쪽에 배치
    player1->AddComponent(new PlayerMove(VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT)); // 방향키 기능 부착
    player1->AddComponent(new StarRenderer(1.0f, 0.0f, 0.0f)); // 빨간색 육망성 그리기 기능 부착
    gameWorld.push_back(player1);

    // 3. 플레이어 2 조립 (육망성 모양 + WASD 조작)
    GameObject* player2 = new GameObject("Player2", 100.0f, 0.0f); // 오른쪽에 배치
    player2->AddComponent(new PlayerMove('W', 'S', 'A', 'D')); // WASD 기능 부착
    player2->AddComponent(new StarRenderer(0.0f, 0.0f, 1.0f)); // 파란색 육망성 그리기 기능 부착
    gameWorld.push_back(player2);


    // [시간 측정 준비] 콘솔과 똑같이 고해상도 시계를 사용합니다.
    auto prevTime = std::chrono::high_resolution_clock::now();

    // [무한 루프 시작]
    bool isRunning = true;
    while (isRunning) {

        // 윈도우 메시지 처리 (X버튼 눌러서 끄는 등 운영체제 명령 처리)
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // A. 시간(DeltaTime) 계산: 이전 바퀴와 지금 바퀴 사이의 걸린 시간 구하기
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = currentTime - prevTime;
        float dt = elapsed.count();
        prevTime = currentTime;

        // B & C. 입력 및 업데이트 단계
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {

                // 처음 실행되는 기능이면 Start()를 한 번 불러서 초기화해줍니다.
                if (gameWorld[i]->components[j]->isStarted == false) {
                    gameWorld[i]->components[j]->Start(device);
                    gameWorld[i]->components[j]->isStarted = true;
                }

                gameWorld[i]->components[j]->OnInput();     // 키 눌렸나 확인
                gameWorld[i]->components[j]->OnUpdate(dt);  // 델타타임 곱해서 좌표 이동
            }
        }

        // D. 렌더링(그리기) 단계
        // 1. 화면을 파란색(배경색)으로 깨끗하게 지웁니다. (콘솔의 system("cls")와 같음)
        float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
        context->ClearRenderTargetView( /*타겟뷰*/ nullptr, clearColor);

        // 2. 물체들에게 각자 가진 '그리기 기능'을 실행하라고 명령합니다.
        for (int i = 0; i < (int)gameWorld.size(); i++) {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++) {
                gameWorld[i]->components[j]->OnRender(context);
            }
        }

        // 3. 다 그린 화면을 모니터에 "짠!" 하고 띄워줍니다.
        swapChain->Present(0, 0);
    }

    // [정리 단계] 게임이 끝나면 바구니를 비우고 메모리를 반환합니다.
    for (int i = 0; i < (int)gameWorld.size(); i++) {
        delete gameWorld[i];
    }
}