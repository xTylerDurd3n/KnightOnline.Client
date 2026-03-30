#include "pch.h"
#include "UITaskbar.h"

// =============================================================================
// UITaskbar — Taskbar ReceiveMessage Hook Implementasyonu
// Referans: Pearl Guard 2369 UITaskbarMain.cpp / UITaskbarSub.cpp
// =============================================================================

// Global UIManager referansi (dllmain.cpp'de tanimli)
extern CUIManager g_UIManager;
extern PearlEngine* Engine;

// =============================================================================
// TaskbarMain — Static hook degiskenleri (file-scope, __asm icin)
// =============================================================================
static DWORD s_uiTaskbarMainVTable = 0;
static DWORD s_uiTaskbarMainOrigFunc = 0;

// =============================================================================
// TaskbarSub — Static hook degiskenleri
// =============================================================================
static DWORD s_uiTaskbarSubVTable = 0;
static DWORD s_uiTaskbarSubOrigFunc = 0;

// Forward declarations — hook fonksiyonlari constructor'dan once tanimlanmali
void __stdcall UITaskbarMainReceiveMessage_Hook(DWORD* pSender, uint32_t dwMsg);
void __stdcall UITaskbarSubReceiveMessage_Hook(DWORD* pSender, uint32_t dwMsg);

// =============================================================================
// CUITaskbarMain — Constructor
// =============================================================================
CUITaskbarMain::CUITaskbarMain()
    : m_dVTableAddr(0)
    , m_baseTaskBar(0), m_btn00Stand(0), m_btn01Sit(0), m_btn02Seek(0)
    , m_btn03Trade(0), m_btn04Skill(0), m_btn05Character(0), m_btn06Inventory(0)
    , m_baseMenu(0), m_btnMenu(0), m_btnRank(0)
{
    DWORD dlgBase = *(DWORD*)KO_PTR_DLG;
    if (dlgBase == 0)
    {
        return;
    }

    m_dVTableAddr = *(DWORD*)(dlgBase + KO_OFF_DLG_TASKBAR_MAIN);
    if (m_dVTableAddr == 0)
    {
        return;
    }


    ParseUIElements();
    InitReceiveMessage();
}

CUITaskbarMain::~CUITaskbarMain() {}

// =============================================================================
// CUITaskbarMain::ParseUIElements — GetChildByID ile buton pointer'larini coz
// =============================================================================
void CUITaskbarMain::ParseUIElements()
{
    if (m_dVTableAddr == 0) return;

    // base_TaskBar container
    m_baseTaskBar = g_UIManager.GetChildByID(m_dVTableAddr, "base_TaskBar");
    if (m_baseTaskBar)
    {
        m_btn00Stand     = g_UIManager.GetChildByID(m_baseTaskBar, "btn_00");
        m_btn01Sit       = g_UIManager.GetChildByID(m_baseTaskBar, "btn_01");
        m_btn02Seek      = g_UIManager.GetChildByID(m_baseTaskBar, "btn_02");
        m_btn03Trade     = g_UIManager.GetChildByID(m_baseTaskBar, "btn_03");
        m_btn04Skill     = g_UIManager.GetChildByID(m_baseTaskBar, "btn_04");
        m_btn05Character = g_UIManager.GetChildByID(m_baseTaskBar, "btn_05");
        m_btn06Inventory = g_UIManager.GetChildByID(m_baseTaskBar, "btn_06");
    }
    else
    {
    }

    // base_menu container
    m_baseMenu = g_UIManager.GetChildByID(m_dVTableAddr, "base_menu");
    if (m_baseMenu)
    {
        m_btnMenu = g_UIManager.GetChildByID(m_baseMenu, "btn_menu");
        m_btnRank = g_UIManager.GetChildByID(m_baseMenu, "btn_rank");
    }
    else
    {
    }

    // btn_powerup TaskbarMain'de de olabilir — engelle
    DWORD btnPowerupMain = g_UIManager.GetChildByID(m_dVTableAddr, "btn_powerup");
    if (btnPowerupMain != 0)
    {
        RegisterButtonHandler(btnPowerupMain, []() -> bool {
            return true;
        });
    }
}

// =============================================================================
// UITaskbarMainReceiveMessage_Hook — Global __stdcall hook fonksiyonu
// vTable pointer swap ile ReceiveMessage yerine cagirilir
// =============================================================================
void __stdcall UITaskbarMainReceiveMessage_Hook(DWORD* pSender, uint32_t dwMsg)
{
    // Ozel handler'lari kontrol et
    if (Engine && Engine->m_UITaskbarMain)
    {
        bool handled = Engine->m_UITaskbarMain->ReceiveMessage(pSender, dwMsg);
        if (handled)
            return; // Engellendi, orijinal cagirilmaz
    }

    // Orijinal fonksiyonu __thiscall olarak cagir
    __asm
    {
        MOV ECX, s_uiTaskbarMainVTable
        PUSH dwMsg
        PUSH pSender
        MOV EAX, s_uiTaskbarMainOrigFunc
        CALL EAX
    }
}

// =============================================================================
// CUITaskbarMain::InitReceiveMessage — vTable hook kurulumu
// =============================================================================
void CUITaskbarMain::InitReceiveMessage()
{
    if (m_dVTableAddr == 0) return;

    // GetRecvMessagePtr: (*(DWORD*)vTableAddr) + 0x7C (25xx ReceiveMessage offset)
    DWORD ptrMsg = (*(DWORD*)m_dVTableAddr) + 0x7C;
    if (ptrMsg == 0x7C) // vTable[0] sifir ise
    {
        return;
    }

    // Orijinal fonksiyon pointer'ini kaydet
    s_uiTaskbarMainVTable = m_dVTableAddr;
    s_uiTaskbarMainOrigFunc = *(DWORD*)ptrMsg;


    // VirtualProtect ile yazma izni al — vTable read-only olabilir
    DWORD oldProtect = 0;
    BOOL vpResult = VirtualProtect((LPVOID)ptrMsg, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
    if (!vpResult)
    {
    }

    // Hook fonksiyonuyla degistir
    *(DWORD*)ptrMsg = (DWORD)UITaskbarMainReceiveMessage_Hook;

    // Eski koruma geri yukle
    if (vpResult)
    {
        DWORD tmp;
        VirtualProtect((LPVOID)ptrMsg, sizeof(DWORD), oldProtect, &tmp);
    }

}

// =============================================================================
// CUITaskbarMain::ReceiveMessage — Handler dispatch
// =============================================================================
bool CUITaskbarMain::ReceiveMessage(DWORD* pSender, uint32_t dwMsg)
{
    if (!pSender)
        return false;

    DWORD senderAddr = (DWORD)pSender;

    if (dwMsg == UIMSG_BUTTON_CLICK)
    {
        char btnName[64] = {0};
        extern bool ReadStdString(DWORD base, DWORD offset, char* outBuf, int maxLen);
        if (ReadStdString(senderAddr, 0x054, btnName, 64))
        {

            // String bazli engelleme — btn_powerup her zaman engelle
            if (lstrcmpiA(btnName, "btn_powerup") == 0)
            {
                return true;
            }
        }
        else
        {
        }
    }

    // Adres bazli handler map (diger butonlar icin)
    auto it = m_handlers.find(senderAddr);
    if (it != m_handlers.end())
    {
        __try
        {
            return it->second();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    return false;
}

// =============================================================================
// CUITaskbarMain::RegisterButtonHandler
// =============================================================================
void CUITaskbarMain::RegisterButtonHandler(DWORD btnPtr, std::function<bool()> handler)
{
    if (btnPtr == 0)
    {
        return;
    }
    m_handlers[btnPtr] = handler;
}

// =============================================================================
// CUITaskbarSub — Constructor
// =============================================================================
CUITaskbarSub::CUITaskbarSub()
    : m_dVTableAddr(0)
    , m_btnPowerUPStore(0), m_btnHotkey(0), m_btnGlobalMap(0)
    , m_btnParty(0), m_btnExit(0)
{
    DWORD dlgBase = *(DWORD*)KO_PTR_DLG;
    if (dlgBase == 0)
    {
        return;
    }

    m_dVTableAddr = *(DWORD*)(dlgBase + KO_OFF_DLG_TASKBAR_SUB);
    if (m_dVTableAddr == 0)
    {
        return;
    }

    ParseUIElements();
    InitReceiveMessage();

    // PUS butonu handler'ini kaydet — tiklandiginda engelle
    if (m_btnPowerUPStore != 0)
    {
        RegisterButtonHandler(m_btnPowerUPStore, []() -> bool {
            return true; // Orijinal davranis engellendi
        });
    }
}

CUITaskbarSub::~CUITaskbarSub() {}

// =============================================================================
// CUITaskbarSub::ParseUIElements
// =============================================================================
void CUITaskbarSub::ParseUIElements()
{
    if (m_dVTableAddr == 0) return;

    // Once tum child'lari listele — PUS butonunun gercek ID'sini bulmak icin
    g_UIManager.DumpChildren(m_dVTableAddr);

    m_btnPowerUPStore = g_UIManager.GetChildByID(m_dVTableAddr, "btns_pus");
    // btns_pus bulunamazsa alternatif ID'leri dene
    if (m_btnPowerUPStore == 0)
        m_btnPowerUPStore = g_UIManager.GetChildByID(m_dVTableAddr, "btn_pus");
    if (m_btnPowerUPStore == 0)
        m_btnPowerUPStore = g_UIManager.GetChildByID(m_dVTableAddr, "btn_powerup");

    m_btnHotkey       = g_UIManager.GetChildByID(m_dVTableAddr, "btn_hotkey");
    m_btnGlobalMap    = g_UIManager.GetChildByID(m_dVTableAddr, "btn_globalmap");
    m_btnParty        = g_UIManager.GetChildByID(m_dVTableAddr, "btn_party");
    m_btnExit         = g_UIManager.GetChildByID(m_dVTableAddr, "btn_exit");

}

// =============================================================================
// UITaskbarSubReceiveMessage_Hook — Global __stdcall hook fonksiyonu
// =============================================================================
void __stdcall UITaskbarSubReceiveMessage_Hook(DWORD* pSender, uint32_t dwMsg)
{
    if (Engine && Engine->m_UITaskbarSub)
    {
        bool handled = Engine->m_UITaskbarSub->ReceiveMessage(pSender, dwMsg);
        if (handled)
            return;
    }

    __asm
    {
        MOV ECX, s_uiTaskbarSubVTable
        PUSH dwMsg
        PUSH pSender
        MOV EAX, s_uiTaskbarSubOrigFunc
        CALL EAX
    }
}

// =============================================================================
// CUITaskbarSub::InitReceiveMessage
// =============================================================================
void CUITaskbarSub::InitReceiveMessage()
{
    if (m_dVTableAddr == 0) return;

    DWORD ptrMsg = (*(DWORD*)m_dVTableAddr) + 0x7C;
    if (ptrMsg == 0x7C)
    {
        return;
    }

    s_uiTaskbarSubVTable = m_dVTableAddr;
    s_uiTaskbarSubOrigFunc = *(DWORD*)ptrMsg;


    // VirtualProtect ile yazma izni al
    DWORD oldProtect = 0;
    BOOL vpResult = VirtualProtect((LPVOID)ptrMsg, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
    if (!vpResult)
    {
    }

    *(DWORD*)ptrMsg = (DWORD)UITaskbarSubReceiveMessage_Hook;

    if (vpResult)
    {
        DWORD tmp;
        VirtualProtect((LPVOID)ptrMsg, sizeof(DWORD), oldProtect, &tmp);
    }

}

// =============================================================================
// CUITaskbarSub::ReceiveMessage — Handler dispatch
// =============================================================================
bool CUITaskbarSub::ReceiveMessage(DWORD* pSender, uint32_t dwMsg)
{
    if (!pSender)
        return false;

    DWORD senderAddr = (DWORD)pSender;

    if (dwMsg == UIMSG_BUTTON_CLICK)
    {
        char btnName[64] = {0};
        extern bool ReadStdString(DWORD base, DWORD offset, char* outBuf, int maxLen);
        if (ReadStdString(senderAddr, 0x054, btnName, 64))
        {

            // String bazli engelleme — btn_powerup her zaman engelle
            if (lstrcmpiA(btnName, "btn_powerup") == 0)
            {
                return true;
            }
        }
        else
        {
        }
    }

    // Adres bazli handler map (diger butonlar icin)
    auto it = m_handlers.find(senderAddr);
    if (it != m_handlers.end())
    {
        __try
        {
            return it->second();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    return false;
}

// =============================================================================
// CUITaskbarSub::RegisterButtonHandler
// =============================================================================
void CUITaskbarSub::RegisterButtonHandler(DWORD btnPtr, std::function<bool()> handler)
{
    if (btnPtr == 0)
    {
        return;
    }
    m_handlers[btnPtr] = handler;
}
