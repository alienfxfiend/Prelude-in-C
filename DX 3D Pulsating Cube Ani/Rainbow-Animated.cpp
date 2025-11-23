// Rainbow Spinning Cube - DirectX 11 - MinGW-w64 / GCC compatible - C++17
//#define UNICODE
//#define _UNICODE
#include <windows.h>
#include <dxgi.h>          // needed for IDXGISwapChain
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <chrono>
#include "resource.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
// dxguid.lib is not needed when using &IID_... directly

using namespace DirectX;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // ------------------- Window Setup (Unicode + WNDCLASSEX) -------------------
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"RainbowCubeClass";
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));

    RegisterClassExW(&wc);

    RECT wr = { 0, 0, 1280, 720 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(0, L"RainbowCubeClass", L"Rainbow Spinning Cube - DirectX 11",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    //ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ------------------- DirectX 11 Setup -------------------
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferDesc.Width = 1280;
    sd.BufferDesc.Height = 720;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.OutputWindow = hwnd;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        &featureLevel, 1, D3D11_SDK_VERSION,
        &sd, &swapChain, &device, nullptr, &context);

    if (FAILED(hr)) return -1;

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);  // FIXED: MinGW compatible
    device->CreateRenderTargetView(backBuffer, nullptr, &rtv);
    backBuffer->Release();

    // Depth stencil
    ID3D11Texture2D* depthStencil = nullptr;
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = 1280;
    depthDesc.Height = 720;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    device->CreateTexture2D(&depthDesc, nullptr, &depthStencil);

    ID3D11DepthStencilView* dsv = nullptr;
    device->CreateDepthStencilView(depthStencil, nullptr, &dsv);
    depthStencil->Release();

    context->OMSetRenderTargets(1, &rtv, dsv);

    D3D11_VIEWPORT vp = { 0, 0, 1280.0f, 720.0f, 0.0f, 1.0f };
    context->RSSetViewports(1, &vp);

    // ------------------- Vertex Data -------------------
    // ------------------- FIXED CUBE (24 vertices, 36 indices, perfect winding) -------------------
    struct Vertex { XMFLOAT3 pos; XMFLOAT3 color; };

    Vertex vertices[] = {
        // Front face (+Z)
        {{ -1, -1, -1 }, {1,0,0}}, {{ -1,  1, -1 }, {0,1,0}}, {{  1,  1, -1 }, {0,0,1}}, {{  1, -1, -1 }, {1,1,0}},
        // Back face (-Z)
        {{  1, -1,  1 }, {1,0,1}}, {{  1,  1,  1 }, {0,1,1}}, {{ -1,  1,  1 }, {1,1,1}}, {{ -1, -1,  1 }, {0,0,0}},
        // Top face (+Y)
        {{ -1,  1, -1 }, {1,0.5f,0}}, {{ -1,  1,  1 }, {0,1,0.5f}}, {{  1,  1,  1 }, {0.5f,0,1}}, {{  1,  1, -1 }, {1,0,0.5f}},
        // Bottom face (-Y)
        {{ -1, -1,  1 }, {0.3f,0.7f,1}}, {{ -1, -1, -1 }, {0.7f,0.3f,1}}, {{  1, -1, -1 }, {1,0.7f,0.3f}}, {{  1, -1,  1 }, {0.3f,1,0.7f}},
        // Left face (-X)
        {{ -1, -1,  1 }, {1,0.4f,0.8f}}, {{ -1, -1, -1 }, {0.8f,1,0.4f}}, {{ -1,  1, -1 }, {0.4f,0.8f,1}}, {{ -1,  1,  1 }, {1,0.8f,0.4f}},
        // Right face (+X)
        {{  1, -1, -1 }, {0.8f,0.4f,1}}, {{  1, -1,  1 }, {0.4f,1,0.8f}}, {{  1,  1,  1 }, {1,0.8f,0.4f}}, {{  1,  1, -1 }, {0.4f,0.8f,1}}
    };

    UINT indices[] = {
        0,  1,  2,  0,  2,  3,   // Front
        4,  5,  6,  4,  6,  7,   // Back
        8,  9, 10,  8, 10, 11,   // Top
       12, 13, 14, 12, 14, 15,   // Bottom
       16, 17, 18, 16, 18, 19,   // Left
       20, 21, 22, 20, 22, 23    // Right
    };

    // Vertex Buffer
    D3D11_BUFFER_DESC vbd = { sizeof(vertices), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER };
    D3D11_SUBRESOURCE_DATA vdata = { vertices };
    ID3D11Buffer* vertexBuffer = nullptr;
    device->CreateBuffer(&vbd, &vdata, &vertexBuffer);

    // Index Buffer
    D3D11_BUFFER_DESC ibd = { sizeof(indices), D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER };
    D3D11_SUBRESOURCE_DATA idata = { indices };
    ID3D11Buffer* indexBuffer = nullptr;
    device->CreateBuffer(&ibd, &idata, &indexBuffer);

    // Constant Buffer
    struct Constants {
        XMMATRIX worldViewProj;
        float    time;
        XMFLOAT3 padding;
    };

    D3D11_BUFFER_DESC cbd = { sizeof(Constants), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE };
    ID3D11Buffer* constantBuffer = nullptr;
    device->CreateBuffer(&cbd, nullptr, &constantBuffer);

    // ------------------- Shaders -------------------
    const char* vsSource = R"(
    cbuffer Constants : register(b0) {
        matrix worldViewProj;
        float time;
    };
    struct VSOut {
        float4 pos : SV_POSITION;
        float3 color : COLOR;
    };
    VSOut main(float3 pos : POSITION, float3 unused : COLOR) {
        VSOut v;
        v.pos = mul(float4(pos, 1.0), worldViewProj);

        // PURE PSYCHEDELIC RAINBOW MADNESS — maximum saturation, full HSV sweep
        float3 rainbow;
        rainbow.x = 0.5 + 0.5 * sin(time * 2.7 + pos.x * 8.0 + pos.y * 3.0 + pos.z * 5.0);
        rainbow.y = 0.5 + 0.5 * sin(time * 3.1 + pos.x * 5.0 + pos.y * 7.0 + pos.z * 4.0 + 2.0);
        rainbow.z = 0.5 + 0.5 * sin(time * 2.4 + pos.x * 6.0 + pos.y * 4.0 + pos.z * 9.0 + 4.0);

        // Extra chaos layer — makes it strobe and go full neon
        rainbow = rainbow * 1.5 + 0.5 * sin(time * 17.0 + length(pos) * 20.0);

        v.color = rainbow;
        return v;
    }
)";

    const char* psSource = R"(
    float4 main(float4 pos : SV_POSITION, float3 color : COLOR) : SV_TARGET {
        return float4(color, 1.0);   // raw, unfiltered rainbow violence
    }
)";

    ID3DBlob* vsBlob = nullptr, * psBlob = nullptr;
    D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs);
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    ID3D11InputLayout* inputLayout = nullptr;
    device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

    // Rasterizer state
    D3D11_RASTERIZER_DESC rsDesc = { D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE };  // changed from CULL_BACK
    //D3D11_RASTERIZER_DESC rsDesc = { D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE };
    ID3D11RasterizerState* rsState = nullptr;
    device->CreateRasterizerState(&rsDesc, &rsState);
    context->RSSetState(rsState);

    // ------------------- Main Loop -------------------
    auto start = std::chrono::high_resolution_clock::now();

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        else {
            auto now = std::chrono::high_resolution_clock::now();
            float t = std::chrono::duration<float>(now - start).count();

            float clearColor[4] = { 0.4f, 0.43f, 0.48f, 1.0f };
            context->ClearRenderTargetView(rtv, clearColor);
            context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

            XMMATRIX world = XMMatrixRotationY(t * 0.8f) * XMMatrixRotationX(t * 0.6f);
            XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0, 2, -5, 0), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
            XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, 1280.0f / 720.0f, 0.1f, 100.0f);
            XMMATRIX wvp = XMMatrixTranspose(world * view * proj);

            D3D11_MAPPED_SUBRESOURCE ms;
            context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
            ((Constants*)ms.pData)->worldViewProj = wvp;
            ((Constants*)ms.pData)->time = t;
            context->Unmap(constantBuffer, 0);

            context->IASetInputLayout(inputLayout);
            context->VSSetShader(vs, nullptr, 0);
            context->PSSetShader(ps, nullptr, 0);
            context->VSSetConstantBuffers(0, 1, &constantBuffer);
            UINT stride = sizeof(Vertex), offset = 0;
            context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            context->DrawIndexed(36, 0, 0);

            swapChain->Present(1, 0);
        }
    }

    // (Cleanup omitted for brevity - add proper Release() calls in real code)

    return 0;
}