#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "resource.h"  // Add this with your other includes

using namespace DirectX;

// Link required libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

//int GetSystemMetrics(int nIndex);

RECT GetDesktopResolution()
{
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);
    return desktop;
}

// Global declarations
IDXGISwapChain* swapchain;
ID3D11Device* dev;
ID3D11DeviceContext* devcon;
ID3D11RenderTargetView* backbuffer;
ID3D11VertexShader* pVS;
ID3D11PixelShader* pPS;
ID3D11Buffer* pVBuffer;
ID3D11Buffer* pIBuffer;
ID3D11Buffer* pCBuffer;
ID3D11InputLayout* pLayout;
ID3D11DepthStencilView* depthStencilView;
ID3D11Texture2D* depthStencilBuffer;
ID3D11RasterizerState* rasterState;
HWND hwnd;  // Declare hwnd as a global variable

// Vertex structure
struct VERTEX {
    XMFLOAT3 Position;
    XMFLOAT4 Color;
};

// Constant buffer structure
struct ConstantBuffer {
    XMMATRIX mWorld;
    XMMATRIX mView;
    XMMATRIX mProjection;
};

// Vertex Shader
const char* vsSource = R"(
cbuffer ConstantBuffer : register(b0) {
    matrix World;
    matrix View;
    matrix Projection;
};

struct VS_INPUT {
    float3 Position : POSITION;
    float4 Color : COLOR;
};

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float4 Color : COLOR;
};

PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    float4 pos = float4(input.Position, 1.0f);
    
    // Transform the position
    pos = mul(pos, World);
    pos = mul(pos, View);
    pos = mul(pos, Projection);
    
    output.Position = pos;
    output.Color = input.Color;
    return output;
})";

// Pixel Shader
const char* psSource = R"(
struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(PS_INPUT input) : SV_Target {
    return input.Color;
})";

// Helper function for error checking
void CheckError(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        MessageBoxA(NULL, message, "Error", MB_OK);
        exit(1);
    }
}

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void InitD3D(HWND hWnd);
void CleanD3D(void);
void RenderFrame(void);
void InitGraphics(void);
void InitPipeline(void);

// Indices for the cube (12 triangles x 3 indices)
DWORD indices[] = {
    0, 1, 2, 0, 2, 3, // Front face
    4, 6, 5, 4, 7, 6, // Back face
    0, 4, 1, 1, 4, 5, // Left face
    2, 7, 3, 2, 6, 7, // Right face
    0, 3, 4, 3, 7, 4, // Top face
    1, 5, 2, 2, 5, 6  // Bottom face
};

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"WindowClass";
    RegisterClassEx(&wc);

    // Define the size of your window
    const int WindowWidth = 800;
    const int WindowHeight = 600;

    // Get the desktop resolution
    RECT desktop = GetDesktopResolution();

    // Calculate the position for a centered window
    int posX = (desktop.right - WindowWidth) / 2;
    int posY = (desktop.bottom - WindowHeight) / 2;

    // Create the window with the calculated position
    HWND hWnd = CreateWindowEx(
        0,
        L"WindowClass",
        L"DirectX Rotating Cube",
        WS_OVERLAPPEDWINDOW,
        posX, posY,  // Positioned at the center
        WindowWidth, WindowHeight,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    InitD3D(hWnd);

    MSG msg = { 0 };
    while (TRUE)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                break;
        }
        RenderFrame();
    }

    CleanD3D();
    return msg.wParam;
}

// Initialize Direct3D
void InitD3D(HWND hWnd)
{
    // Create swap chain
    DXGI_SWAP_CHAIN_DESC scd = { 0 };
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = 800;
    scd.BufferDesc.Height = 600;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 4;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        NULL, 0, D3D11_SDK_VERSION, &scd, &swapchain, &dev, NULL, &devcon);
    CheckError(hr, "Failed to create device and swapchain");

    // Create render target view
    ID3D11Texture2D* pBackBuffer;
    hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    CheckError(hr, "Failed to get back buffer");

    hr = dev->CreateRenderTargetView(pBackBuffer, NULL, &backbuffer);
    CheckError(hr, "Failed to create render target view");
    pBackBuffer->Release();

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = 800;
    descDepth.Height = 600;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 4;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = dev->CreateTexture2D(&descDepth, NULL, &depthStencilBuffer);
    CheckError(hr, "Failed to create depth stencil buffer");

    // Create depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

    hr = dev->CreateDepthStencilView(depthStencilBuffer, &descDSV, &depthStencilView);
    CheckError(hr, "Failed to create depth stencil view");

    devcon->OMSetRenderTargets(1, &backbuffer, depthStencilView);

    // Create depth stencil state
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState* depthStencilState;
    hr = dev->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
    CheckError(hr, "Failed to create depth stencil state");
    devcon->OMSetDepthStencilState(depthStencilState, 1);
    depthStencilState->Release();

    // Create rasterizer state
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.AntialiasedLineEnable = TRUE;
    rasterDesc.CullMode = D3D11_CULL_BACK;
    rasterDesc.DepthClipEnable = TRUE;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.MultisampleEnable = TRUE;
    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.DepthBias = 0;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.SlopeScaledDepthBias = 0.0f;

    hr = dev->CreateRasterizerState(&rasterDesc, &rasterState);
    CheckError(hr, "Failed to create rasterizer state");
    devcon->RSSetState(rasterState);

    // Set the viewport
    D3D11_VIEWPORT viewport = { 0 };
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = 800;
    viewport.Height = 600;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    devcon->RSSetViewports(1, &viewport);

    InitPipeline();
    InitGraphics();
}

// Initialize graphics
void InitGraphics() {
    // Define the cube vertices with more distinct positions and colors
    VERTEX vertices[] = {
        {XMFLOAT3(-0.5f, 0.5f, -0.5f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)}, // Front top left
        {XMFLOAT3(0.5f, 0.5f, -0.5f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f)},  // Front top right
        {XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)},   // Back top right
        {XMFLOAT3(-0.5f, 0.5f, 0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)},  // Back top left
        {XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f)}, // Front bottom left
        {XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)},  // Front bottom right
        {XMFLOAT3(0.5f, -0.5f, 0.5f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f)},   // Back bottom right
        {XMFLOAT3(-0.5f, -0.5f, 0.5f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)}   // Back bottom left
    };

    // Create vertex buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    HRESULT hr = dev->CreateBuffer(&bd, &initData, &pVBuffer);
    CheckError(hr, "Failed to create vertex buffer");

    // Create index buffer
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = indices;

    hr = dev->CreateBuffer(&ibd, &indexData, &pIBuffer);
    CheckError(hr, "Failed to create index buffer");

    // Create constant buffer
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    hr = dev->CreateBuffer(&cbd, nullptr, &pCBuffer);
    CheckError(hr, "Failed to create constant buffer");
}

// Initialize pipeline
void InitPipeline() {
    ID3DBlob* VS, * PS, * error;

    // Compile shaders
    HRESULT hr = D3DCompile(vsSource, strlen(vsSource), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &VS, &error);
    if (FAILED(hr)) {
        if (error) {
            MessageBoxA(NULL, (char*)error->GetBufferPointer(), "Error", MB_OK);
            error->Release();
        }
        exit(1);
    }

    hr = D3DCompile(psSource, strlen(psSource), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &PS, &error);
    if (FAILED(hr)) {
        if (error) {
            MessageBoxA(NULL, (char*)error->GetBufferPointer(), "Error", MB_OK);
            error->Release();
        }
        exit(1);
    }

    // Create shader objects
    hr = dev->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &pVS);
    CheckError(hr, "Failed to create vertex shader");

    hr = dev->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &pPS);
    CheckError(hr, "Failed to create pixel shader");

    // Set shaders
    devcon->VSSetShader(pVS, 0, 0);
    devcon->PSSetShader(pPS, 0, 0);

    // Create input layout
    // Create input layout
    D3D11_INPUT_ELEMENT_DESC ied[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    hr = dev->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &pLayout);
    CheckError(hr, "Failed to create input layout");
    devcon->IASetInputLayout(pLayout);

    VS->Release();
    PS->Release();
}

// Render frame
void RenderFrame() {
    static float angle = 0.0f;
    angle += 0.01f;

    // Clear render target and depth buffer
    //float color[4] = { 0.0f, 0.2f, 0.4f, 1.0f }; // Dark blue background
    /*
    // Clear render target and depth buffer
    float color[4] = { 0.0f, 0.2f, 0.4f, 1.0f }; // Dark blue background
    devcon->ClearRenderTargetView(backbuffer, color);
    devcon->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    */
    /*
    // Clear render target and depth buffer to white
    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // White background
    devcon->ClearRenderTargetView(backbuffer, color);
    devcon->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    */
    
    // Clear render target and depth buffer to white
    float color[4] = { 0.0f, 0.2f, 0.4f, 1.0f }; // Dark blue background
    devcon->ClearRenderTargetView(backbuffer, color);
    devcon->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    
    devcon->ClearRenderTargetView(backbuffer, color);
    devcon->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Update constant buffer
    ConstantBuffer cb;
    XMMATRIX world = XMMatrixRotationY(angle);
    XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
    XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(Eye, At, Up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 800.0f / 600.0f, 0.01f, 100.0f);

    cb.mWorld = XMMatrixTranspose(world);
    cb.mView = XMMatrixTranspose(view);
    cb.mProjection = XMMatrixTranspose(projection);

    devcon->UpdateSubresource(pCBuffer, 0, nullptr, &cb, 0, 0);
    devcon->VSSetConstantBuffers(0, 1, &pCBuffer);

    // Set vertex buffer
    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    devcon->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
    devcon->IASetIndexBuffer(pIBuffer, DXGI_FORMAT_R32_UINT, 0);
    devcon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw the cube
    devcon->DrawIndexed(36, 0, 0);

    // Present the frame
    swapchain->Present(0, 0);
}

// Clean up D3D objects
void CleanD3D() {
    if (rasterState) rasterState->Release();
    if (depthStencilView) depthStencilView->Release();
    if (depthStencilBuffer) depthStencilBuffer->Release();
    if (backbuffer) backbuffer->Release();
    if (swapchain) swapchain->Release();
    if (pCBuffer) pCBuffer->Release();
    if (pIBuffer) pIBuffer->Release();
    if (pVBuffer) pVBuffer->Release();
    if (pLayout) pLayout->Release();
    if (pVS) pVS->Release();
    if (pPS) pPS->Release();
    if (devcon) devcon->Release();
    if (dev) dev->Release();
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        else if (wParam == VK_F1) {
            MessageBox(hwnd, TEXT("DirectX Spinning Multi-colored Cube (Useful for studying DX Hooks) Programmed in C++ Win32 API (472 lines of code) by Entisoft Software (c) Evans Thorpemorton"), TEXT("Information"), MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}