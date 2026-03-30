#include "pch.h"

// =============================================================================
// RenderSystem — DX9 EndScene Hook + Overlay Cizim Implementasyonu
// 25xx istemcisi DX9 kullanir.
// EndScene hook: gecici D3D9 device olusturup vtable'dan adres alinir.
// Cizim: DrawPrimitiveUP ile rect/line, GDI ile text.
// =============================================================================

// Static uye tanimlari
tEndScene RenderSystem::s_oEndScene = nullptr;
tPresent RenderSystem::s_oPresent = nullptr;
RenderSystem* RenderSystem::s_pInstance = nullptr;

RenderSystem g_RenderSystem;

RenderSystem::RenderSystem()
    : m_bInitialized(false)
    , m_bDX11Mode(false)
    , m_bDeviceLost(false)
    , m_pDX9Device(nullptr)
    , m_pSwapChain(nullptr)
    , m_pDX11Device(nullptr)
    , m_pDX11Context(nullptr)
{
    s_pInstance = this;
}

RenderSystem::~RenderSystem()
{
    s_pInstance = nullptr;
}

// =============================================================================
// Init — DX9/DX11 otomatik tespit ve hook kurulumu
// 25xx DX9 kullanir, DX11 modulu varsa ona gecer
// =============================================================================
bool RenderSystem::Init()
{

    // DX9/DX11 otomatik tespit
    HMODULE hDX11 = GetModuleHandleA("d3d11.dll");
    HMODULE hDX9 = GetModuleHandleA("d3d9.dll");

    // DX11 varsa DX11 hook'la
    if (hDX11 != nullptr)
    {
        m_bDX11Mode = true;

        // Gecici DX11 device + swap chain olustur
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = GetDesktopWindow();
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* pTempSwapChain = nullptr;
        ID3D11Device* pTempDevice = nullptr;
        ID3D11DeviceContext* pTempContext = nullptr;
        D3D_FEATURE_LEVEL featureLevel;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &sd, &pTempSwapChain, &pTempDevice, &featureLevel, &pTempContext);

        if (FAILED(hr) || !pTempSwapChain)
        {
            if (pTempContext) pTempContext->Release();
            if (pTempDevice) pTempDevice->Release();
            if (pTempSwapChain) pTempSwapChain->Release();
            goto try_dx9;
        }

        // vtable'dan Present adresini al
        DWORD* pVTable = *(DWORD**)pTempSwapChain;
        DWORD dwPresent = pVTable[DXGI_PRESENT_VTABLE_INDEX];

        pTempContext->Release();
        pTempDevice->Release();
        pTempSwapChain->Release();

        s_oPresent = (tPresent)DetourFunction((PBYTE)dwPresent, (PBYTE)hkPresent);
        if (s_oPresent)
        {
            m_bInitialized = true;
            return true;
        }
    }

try_dx9:
    if (hDX9 == nullptr)
    {
        return false;
    }

    m_bDX11Mode = false;
    m_bDX11Mode = false;

    // --- Gecici D3D9 device olustur, vtable'dan EndScene adresini al ---
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D)
    {
        return false;
    }

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = GetDesktopWindow();
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice9* pTempDevice = nullptr;
    HRESULT hr = pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        d3dpp.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
        &d3dpp,
        &pTempDevice
    );

    if (FAILED(hr) || !pTempDevice)
    {
        pD3D->Release();
        return false;
    }

    // vtable'dan EndScene adresini al (index 42)
    DWORD* pVTable = *(DWORD**)pTempDevice;
    DWORD dwEndScene = pVTable[D3D9_ENDSCENE_VTABLE_INDEX];


    // Gecici device'i serbest birak
    pTempDevice->Release();
    pD3D->Release();

    // EndScene'i hook'la
    s_oEndScene = (tEndScene)DetourFunction((PBYTE)dwEndScene, (PBYTE)hkEndScene);
    if (!s_oEndScene)
    {
        return false;
    }

    m_bInitialized = true;
    return true;
}

// =============================================================================
// IsDX11 — DX11 modunda mi?
// =============================================================================
bool RenderSystem::IsDX11() const
{
    return m_bDX11Mode;
}

// =============================================================================
// InitDX9 — Gercek oyun device'i ile DX9 kaynaklarini baslat
// hkEndScene icinden ilk frame'de cagirilir
// =============================================================================
void RenderSystem::InitDX9(LPDIRECT3DDEVICE9 pDevice)
{
    if (m_pDX9Device == pDevice)
        return;

    m_pDX9Device = pDevice;
    m_bDeviceLost = false;

    // Sadece ilk seferde logla
    static bool firstLog = true;
    if (firstLog)
    {
        firstLog = false;
    }
}

// =============================================================================
// hkEndScene — EndScene hook callback (static)
// Her frame'de oyun EndScene cagirdiginda burasi calisir
// =============================================================================
HRESULT __stdcall RenderSystem::hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    if (s_pInstance && !s_pInstance->m_bDeviceLost)
    {
        // Ilk cagri — device'i kaydet
        if (s_pInstance->m_pDX9Device != pDevice)
        {
            s_pInstance->InitDX9(pDevice);
        }

        // --- Overlay cizim buraya eklenir ---
        // Ornek: basit bir test metni ve dikdortgen
        // s_pInstance->DrawFilledRect(10, 10, 200, 25, 0x80000000);
        // s_pInstance->DrawText("REVOLTEACS Overlay", 15, 12, 0xFFFFFFFF);
    }

    // Orijinal EndScene'i cagir
    return s_oEndScene(pDevice);
}

// =============================================================================
// DrawText — GDI tabanli metin cizimi
// D3DXFont yerine GDI kullanilir (d3dx9 bagimliligi yok)
// =============================================================================
void RenderSystem::DrawText(const char* text, int x, int y, DWORD color)
{
    if (!m_pDX9Device || !text || m_bDeviceLost)
        return;

    // DX9 backbuffer'dan HDC al
    IDirect3DSurface9* pBackBuffer = nullptr;
    HRESULT hr = m_pDX9Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
    if (FAILED(hr) || !pBackBuffer)
        return;

    HDC hdc = nullptr;
    hr = pBackBuffer->GetDC(&hdc);
    if (SUCCEEDED(hr) && hdc)
    {
        DrawTextGDI(hdc, text, x, y, color);
        pBackBuffer->ReleaseDC(hdc);
    }
    pBackBuffer->Release();
}

// =============================================================================
// DrawTextGDI — GDI ile metin cizimi (dahili yardimci)
// =============================================================================
void RenderSystem::DrawTextGDI(HDC hdc, const char* text, int x, int y, DWORD color)
{
    // DWORD color: 0xAARRGGBB -> GDI RGB
    BYTE r = (BYTE)((color >> 16) & 0xFF);
    BYTE g = (BYTE)((color >> 8) & 0xFF);
    BYTE b = (BYTE)(color & 0xFF);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(r, g, b));

    HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    TextOutA(hdc, x, y, text, (int)strlen(text));
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

// =============================================================================
// DrawRect — Ici bos dikdortgen cizimi (4 cizgi)
// D3D9 DrawPrimitiveUP ile vertex tabanli
// =============================================================================
void RenderSystem::DrawRect(int x, int y, int w, int h, DWORD color)
{
    DrawLine(x, y, x + w, y, color);           // ust
    DrawLine(x, y + h, x + w, y + h, color);   // alt
    DrawLine(x, y, x, y + h, color);           // sol
    DrawLine(x + w, y, x + w, y + h, color);   // sag
}

// =============================================================================
// DrawFilledRect — Dolu dikdortgen cizimi
// D3D9 DrawPrimitiveUP ile 2 ucgen (triangle strip)
// =============================================================================
void RenderSystem::DrawFilledRect(int x, int y, int w, int h, DWORD color)
{
    if (!m_pDX9Device || m_bDeviceLost)
        return;

    RenderVertex vertices[4] = {
        { (float)x,       (float)y,       0.0f, 1.0f, color },
        { (float)(x + w), (float)y,       0.0f, 1.0f, color },
        { (float)x,       (float)(y + h), 0.0f, 1.0f, color },
        { (float)(x + w), (float)(y + h), 0.0f, 1.0f, color },
    };

    // Render state kaydet ve ayarla
    DWORD oldFVF, oldLighting, oldZEnable, oldAlphaBlend, oldSrcBlend, oldDestBlend;
    m_pDX9Device->GetFVF(&oldFVF);
    m_pDX9Device->GetRenderState(D3DRS_LIGHTING, &oldLighting);
    m_pDX9Device->GetRenderState(D3DRS_ZENABLE, &oldZEnable);
    m_pDX9Device->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
    m_pDX9Device->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
    m_pDX9Device->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);

    m_pDX9Device->SetFVF(RENDER_VERTEX_FVF);
    m_pDX9Device->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDX9Device->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDX9Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDX9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDX9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDX9Device->SetTexture(0, nullptr);

    m_pDX9Device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(RenderVertex));

    // Render state geri yukle
    m_pDX9Device->SetFVF(oldFVF);
    m_pDX9Device->SetRenderState(D3DRS_LIGHTING, oldLighting);
    m_pDX9Device->SetRenderState(D3DRS_ZENABLE, oldZEnable);
    m_pDX9Device->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
    m_pDX9Device->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
    m_pDX9Device->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);
}

// =============================================================================
// DrawLine — Cizgi cizimi
// D3D9 DrawPrimitiveUP ile line primitive
// =============================================================================
void RenderSystem::DrawLine(int x1, int y1, int x2, int y2, DWORD color, int thickness)
{
    if (!m_pDX9Device || m_bDeviceLost)
        return;

    RenderVertex vertices[2] = {
        { (float)x1, (float)y1, 0.0f, 1.0f, color },
        { (float)x2, (float)y2, 0.0f, 1.0f, color },
    };

    // Render state kaydet ve ayarla
    DWORD oldFVF, oldLighting, oldZEnable, oldAlphaBlend, oldSrcBlend, oldDestBlend;
    m_pDX9Device->GetFVF(&oldFVF);
    m_pDX9Device->GetRenderState(D3DRS_LIGHTING, &oldLighting);
    m_pDX9Device->GetRenderState(D3DRS_ZENABLE, &oldZEnable);
    m_pDX9Device->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
    m_pDX9Device->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
    m_pDX9Device->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);

    m_pDX9Device->SetFVF(RENDER_VERTEX_FVF);
    m_pDX9Device->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_pDX9Device->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_pDX9Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pDX9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_pDX9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_pDX9Device->SetTexture(0, nullptr);

    m_pDX9Device->DrawPrimitiveUP(D3DPT_LINELIST, 1, vertices, sizeof(RenderVertex));

    // Render state geri yukle
    m_pDX9Device->SetFVF(oldFVF);
    m_pDX9Device->SetRenderState(D3DRS_LIGHTING, oldLighting);
    m_pDX9Device->SetRenderState(D3DRS_ZENABLE, oldZEnable);
    m_pDX9Device->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
    m_pDX9Device->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
    m_pDX9Device->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);
}

// =============================================================================
// InitDX11 — DX11 stub (25xx DX9 kullanir)
// =============================================================================
// DX11 device'i Present hook'undan yakala
void RenderSystem::InitDX11Device(IDXGISwapChain* pSwapChain)
{
    if (m_pSwapChain == pSwapChain)
        return;

    m_pSwapChain = pSwapChain;

    // SwapChain'den device al
    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&m_pDX11Device)))
    {
        m_pDX11Device->GetImmediateContext(&m_pDX11Context);
        static bool firstLog = true;
        if (firstLog)
        {
            firstLog = false;
        }
    }
}

// DX11 Present hook callback
HRESULT __stdcall RenderSystem::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if (s_pInstance && !s_pInstance->m_bDeviceLost)
    {
        if (s_pInstance->m_pSwapChain != pSwapChain)
            s_pInstance->InitDX11Device(pSwapChain);

        // DX11 overlay cizim buraya eklenecek
    }

    return s_oPresent(pSwapChain, SyncInterval, Flags);
}

// =============================================================================
// DrawTexture — Texture cizim stub
// =============================================================================
void RenderSystem::DrawTexture(int x, int y, int w, int h)
{
    // TODO: Texture yukleme ve cizim implementasyonu
    // Asagidaki adimlar uygulanacak:
    //   1. D3DXCreateTextureFromFile ile texture yukle (veya ham veri)
    //   2. ID3DXSprite ile ciz veya DrawPrimitiveUP + texture coords
    //   3. Texture cache mekanizmasi ekle
    // Simdilik stub — ici bos dikdortgen cizer
    if (m_pDX9Device && !m_bDeviceLost)
    {
        DrawRect(x, y, w, h, 0xFFFF00FF); // Placeholder: magenta cerceve
    }
}

// =============================================================================
// OnDeviceLost — DX9 device kayboldu (minimize, alt-tab vb.)
// =============================================================================
void RenderSystem::OnDeviceLost()
{
    m_bDeviceLost = true;
}

// =============================================================================
// OnDeviceReset — DX9 device geri geldi
// =============================================================================
void RenderSystem::OnDeviceReset()
{
    m_bDeviceLost = false;
}
