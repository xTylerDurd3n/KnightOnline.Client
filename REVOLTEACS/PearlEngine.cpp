#include "pch.h"
#include "PearlEngine.h"

// =============================================================================
// PearlEngine — Merkezi Engine Sinifi Implementasyonu
// =============================================================================

// Global Engine pointer
PearlEngine* Engine = nullptr;

// Mevcut global instance'lar (dllmain.cpp ve diger .cpp dosyalarinda tanimli)
extern PacketHandler g_PacketHandler;
extern CUIManager    g_UIManager;
extern CGameHooks    g_GameHooks;
extern RenderSystem  g_RenderSystem;
extern UIFramework   g_UIFramework;

// =============================================================================
// Constructor / Destructor
// =============================================================================

PearlEngine::PearlEngine()
    : m_PlayerBase(nullptr)
    , m_UiMgr(nullptr)
    , m_PacketHandler(nullptr)
    , m_RenderSystem(nullptr)
    , m_UIFramework(nullptr)
    , m_GameHooks(nullptr)
    , m_UITaskbarMain(nullptr)
    , m_UITaskbarSub(nullptr)
    , m_bInitialized(false)
    , m_hEngineThread(NULL)
{
}

PearlEngine::~PearlEngine()
{
    // PlayerBase engine tarafindan olusturulur, temizle
    if (m_PlayerBase)
    {
        delete m_PlayerBase;
        m_PlayerBase = nullptr;
    }

    // Diger moduller global instance — silme
    m_UiMgr = nullptr;
    m_PacketHandler = nullptr;
    m_RenderSystem = nullptr;
    m_UIFramework = nullptr;
    m_GameHooks = nullptr;

    // Taskbar modulleri engine tarafindan olusturulur, temizle
    if (m_UITaskbarMain)
    {
        delete m_UITaskbarMain;
        m_UITaskbarMain = nullptr;
    }
    if (m_UITaskbarSub)
    {
        delete m_UITaskbarSub;
        m_UITaskbarSub = nullptr;
    }

    if (m_hEngineThread)
    {
        CloseHandle(m_hEngineThread);
        m_hEngineThread = NULL;
    }
}

// =============================================================================
// Init — Tum modulleri baslat ve engine thread'ini calistir
// =============================================================================

void PearlEngine::Init()
{
    if (m_bInitialized)
        return;


    // Modul pointer'larini mevcut global instance'lara bagla
    m_PacketHandler = &g_PacketHandler;
    m_UiMgr         = &g_UIManager;
    m_GameHooks      = &g_GameHooks;
    m_RenderSystem   = &g_RenderSystem;
    m_UIFramework    = &g_UIFramework;

    // PlayerBase — yeni instance olustur (global yok)
    m_PlayerBase = new CPlayerBase();

    m_bInitialized = true;

    // UITaskbar — oyuna giris sonrasi baslatilacak (ayri thread'de)
    // Not: Taskbar UI elemanlari oyuna giris yapildiktan sonra yuklenir
    // Bu yuzden 20sn bekleyip sonra baslatiriz
    CreateThread(NULL, 0, [](LPVOID lpParam) -> DWORD {
        PearlEngine* engine = reinterpret_cast<PearlEngine*>(lpParam);
        Sleep(20000);

        DWORD dlgBase = *(DWORD*)KO_PTR_DLG;
        if (dlgBase == 0)
        {
            Sleep(30000);
        }

        engine->m_UITaskbarMain = new CUITaskbarMain();
        engine->m_UITaskbarSub = new CUITaskbarSub();

        if (engine->m_UITaskbarMain->m_dVTableAddr && engine->m_UITaskbarSub->m_dVTableAddr)
        

        return 0;
    }, (LPVOID)this, 0, NULL);

    // Engine ana dongusunu ayri thread'de baslat
    m_hEngineThread = CreateThread(NULL, 0, EngineMain, (LPVOID)this, 0, NULL);
    if (m_hEngineThread)
    {
    }
    else
    {
    }
}

// =============================================================================
// Update — Her dongu cagrisinda calisir
// =============================================================================

void PearlEngine::Update()
{
    // PlayerBase bellekten guncelle
    if (m_PlayerBase)
    {
        m_PlayerBase->UpdateFromMemory();

        // Her 10 saniyede bir durum logla
        static int updateCount = 0;
        updateCount++;
        if (updateCount % 100 == 1 && m_PlayerBase->m_iLevel > 0) // 100 * 100ms = 10sn
        {
                m_PlayerBase->m_strCharacterName.c_str(),
                m_PlayerBase->m_iLevel,
                m_PlayerBase->m_iHp, m_PlayerBase->m_iMaxHp,
                m_PlayerBase->m_iMp, m_PlayerBase->m_iMaxMp,
                m_PlayerBase->m_fX, m_PlayerBase->m_fZ,
                m_PlayerBase->m_iGold;
        }
    }
}

// =============================================================================
// Send — PacketHandler uzerinden paket gonder
// =============================================================================

void PearlEngine::Send(Packet* pkt)
{
    if (m_PacketHandler && pkt)
    {
        m_PacketHandler->Send(pkt);
    }
}

// =============================================================================
// GetTarget — Mevcut hedef ID'sini dondur
// =============================================================================

int16 PearlEngine::GetTarget()
{
    if (m_PlayerBase)
    {
        return m_PlayerBase->m_iTargetID;
    }
    return -1;
}

// =============================================================================
// GetNation — Oyuncunun ulusunu dondur
// =============================================================================

uint8 PearlEngine::GetNation()
{
    if (m_PlayerBase)
    {
        return m_PlayerBase->m_iNation;
    }
    return 0;
}

// =============================================================================
// GetRecvMessagePtr — vTable'dan ReceiveMessage pointer adresi dondur
// 25xx: vTable+0x07C (PG2369'da 0x70 idi)
// =============================================================================

DWORD PearlEngine::GetRecvMessagePtr(DWORD vTableAddr)
{
    if (vTableAddr == 0)
        return 0;
    return (*(DWORD*)vTableAddr) + 0x7C;  // 25xx: ReceiveMessage = vTable+0x07C
}

// =============================================================================
// EngineMain — Engine ana dongusu (ayri thread)
// Her 100ms'de Update() cagrilir
// =============================================================================

DWORD WINAPI PearlEngine::EngineMain(LPVOID lpParam)
{
    PearlEngine* engine = reinterpret_cast<PearlEngine*>(lpParam);
    if (!engine)
        return 0;


    while (true)
    {
        engine->Update();
        Sleep(100); // 100ms aralik
    }

    return 0;
}
