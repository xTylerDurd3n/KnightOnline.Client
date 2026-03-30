#include "pch.h"
#include "UIManager.h"

// Static member tanimlari
tGetChild CUIManager::s_oGetChild = nullptr;

CUIManager::CUIManager() {}
CUIManager::~CUIManager() {}

// Init — GetChildByID hook'unu kurar
// Orijinal GetChildByID fonksiyon adresi (hook'lanmamis)
static DWORD s_origGetChildByID = KO_GET_CHILD_BY_ID_FUNC;
static bool s_bGetChildHooked = false;

void CUIManager::Init()
{
    // GetChildByID hook'unu KURMA — 0x005E6E5A fonksiyon ortasi, trampoline bozuluyor
    // Loglama icin hook gerekmiyor, GetChildByID_Hook loglamayi zaten devre disi birakti
    // Dogrudan orijinal fonksiyonu cagiriyoruz
    s_oGetChild = nullptr;
    s_bGetChildHooked = false;
}

// GetChildByID_Hook — hook callback (loglama)
void __stdcall CUIManager::GetChildByID_Hook(const std::string& szString, DWORD nUnknown)
{
    DWORD thisPtr;
    __asm
    {
        MOV thisPtr, ECX
        pushad
        pushfd
    }
    // Sadece taskbar ile ilgili ID'leri logla
    if (szString.find("pus") != std::string::npos ||
        szString.find("PUS") != std::string::npos ||
        szString.find("Pus") != std::string::npos ||
        szString.find("btn_") != std::string::npos ||
        szString.find("base_") != std::string::npos ||
        szString.find("hotkey") != std::string::npos ||
        szString.find("exit") != std::string::npos ||
        szString.find("globalmap") != std::string::npos)
    {
    }
    __asm
    {
        popfd
        popad
        PUSH ECX
        PUSH nUnknown
        PUSH DWORD PTR[szString]
        CALL s_oGetChild
    }
}

// =============================================================================
// GetChildByID — Kendi implementasyonumuz (oyunun bozuk wrapper'i yerine)
// N3UIBase nesnesinin m_Children (std::list) listesini iterate ederek
// m_szID string'i esleseni dondurur.
//
// Yaklasim: Runtime'da offset'leri probe ederek buluyoruz.
// Bilinen: Nesne+0x020 = m_szFileName (UIF dosya adi, CN3BaseFileAccess)
// m_szID ve m_Children offset'leri probe ile tespit edilecek.
// =============================================================================

// MSVC x86 std::list<T*> node yapisi:
// struct _List_node { _List_node* _Next; _List_node* _Prev; T* _Myval; }
// std::list: { _List_node* _Myhead; size_t _Mysize; }
// _Myhead sentinel node'dur — ilk gercek eleman _Myhead->_Next

// Guvenli bellek okuma yardimcisi
static bool SafeRead4(DWORD addr, DWORD& out)
{
    if (addr < 0x10000 || addr > 0x7FFFFFFF) return false;
    if (IsBadReadPtr((VOID*)addr, 4)) return false;
    out = *(DWORD*)addr;
    return true;
}

// Bir DWORD'un gecerli heap/stack pointer olup olmadigini kontrol et
static bool IsValidPtr(DWORD val)
{
    return (val > 0x00100000 && val < 0x7FFFFFFF && !IsBadReadPtr((VOID*)val, 4));
}

// Bir offset'teki std::string'in icerigini oku (MSVC x86 std::string layout)
// MSVC std::string (x86): SSO buffer veya heap pointer
// Layout A (VS2008+): +0x00 = union { char buf[16]; char* ptr; }  +0x10 = size  +0x14 = capacity
// Layout B (bazi eski): +0x04 = union { char buf[16]; char* ptr; }  +0x14 = size  +0x18 = capacity
// Eger capacity >= 16 ise heap pointer kullanilir, degilse SSO buffer
static int s_strLayout = -1; // -1=bilinmiyor, 0=Layout A, 1=Layout B

static bool ReadStdStringWithLayout(DWORD strAddr, int layout, char* outBuf, int maxLen)
{
    int bufOff = (layout == 0) ? 0x00 : 0x04;
    int sizeOff = (layout == 0) ? 0x10 : 0x14;
    int capOff = (layout == 0) ? 0x14 : 0x18;
    int totalSize = (layout == 0) ? 0x18 : 0x1C;

    if (IsBadReadPtr((VOID*)strAddr, totalSize)) return false;

    DWORD size = *(DWORD*)(strAddr + sizeOff);
    DWORD capacity = *(DWORD*)(strAddr + capOff);

    if (size == 0 || size > 256 || capacity < size) return false;

    const char* src = nullptr;
    if (capacity < 16)
    {
        // SSO — string dogrudan buffer'da
        src = (const char*)(strAddr + bufOff);
    }
    else
    {
        // Heap allocated
        DWORD heapPtr = *(DWORD*)(strAddr + bufOff);
        if (!IsValidPtr(heapPtr)) return false;
        if (IsBadReadPtr((VOID*)heapPtr, size)) return false;
        src = (const char*)heapPtr;
    }

    int copyLen = (int)size < (maxLen - 1) ? (int)size : (maxLen - 1);
    memcpy(outBuf, src, copyLen);
    outBuf[copyLen] = 0;

    // ASCII kontrol
    for (int i = 0; i < copyLen; i++)
    {
        if (outBuf[i] < 0x20 || outBuf[i] > 0x7E)
            return false;
    }
    return true;
}

bool ReadStdString(DWORD base, DWORD offset, char* outBuf, int maxLen)
{
    DWORD strAddr = base + offset;

    // Layout zaten biliniyor
    if (s_strLayout >= 0)
        return ReadStdStringWithLayout(strAddr, s_strLayout, outBuf, maxLen);

    // Her iki layout'u da dene
    if (ReadStdStringWithLayout(strAddr, 0, outBuf, maxLen))
    {
        s_strLayout = 0;
        return true;
    }
    if (ReadStdStringWithLayout(strAddr, 1, outBuf, maxLen))
    {
        s_strLayout = 1;
        return true;
    }
    return false;
}

// =============================================================================
// Offset probe — m_szID ve m_Children offset'lerini runtime'da bul
// Bir kez calisir, sonuclari cache'ler
// =============================================================================
static int s_offID = -1;        // m_szID offset (std::string)
static int s_offChildren = -1;  // m_Children offset (std::list head ptr)
static int s_offChildSize = -1; // m_Children size offset
static bool s_bProbed = false;
static bool s_bProbeOK = false;

static void ProbeOffsets(DWORD uiObject)
{
    if (s_bProbed) return;
    s_bProbed = true;


    // Bilinen: +0x020 = m_szFileName (UIF dosya adi)
    // m_szID bundan SONRA olmali (CN3UIBase, CN3BaseFileAccess'ten sonra)
    // Ama once m_szFileName'in gercekten +0x020'de olup olmadigini dogrulayalim
    char testBuf[64] = {0};
    if (ReadStdString(uiObject, 0x020, testBuf, 64))
    {
    }

    // m_szID'yi bul — "base_" veya "btn_" gibi UI ID'leri icermeli
    // Genellikle m_szFileName'den sonra, +0x038 ile +0x080 arasi
    // Ama once tum std::string offset'lerini tarayalim
    for (int off = 0; off < 0x100; off += 4)
    {
        char buf[64] = {0};
        if (ReadStdString(uiObject, off, buf, 64))
        {
        }
    }

    // m_Children (std::list) taramasi
    // std::list MSVC x86: { _Myhead (ptr to sentinel node), _Mysize }
    // Sentinel node: { _Next, _Prev, _Myval(unused) }
    // _Next = ilk gercek eleman, _Prev = son gercek eleman
    // Bos liste: _Next == _Prev == _Myhead (sentinel kendine isaret eder)
    for (int off = 0; off < 0x100; off += 4)
    {
        DWORD headPtr = 0;
        if (!SafeRead4(uiObject + off, headPtr)) continue;
        if (!IsValidPtr(headPtr)) continue;

        // Sentinel node'u oku
        DWORD nextPtr = 0, prevPtr = 0;
        if (!SafeRead4(headPtr, nextPtr)) continue;      // _Next
        if (!SafeRead4(headPtr + 4, prevPtr)) continue;   // _Prev

        if (!IsValidPtr(nextPtr) || !IsValidPtr(prevPtr)) continue;

        // Size degerini kontrol et (hemen sonraki DWORD)
        DWORD sizeVal = 0;
        if (!SafeRead4(uiObject + off + 4, sizeVal)) continue;

        // Makul child sayisi: 0-100
        if (sizeVal > 100) continue;

        // Bos liste kontrolu: sentinel kendine isaret eder
        if (sizeVal == 0 && nextPtr == headPtr && prevPtr == headPtr)
        {
            continue;
        }

        // Dolu liste: ilk elemanin _Myval'i gecerli pointer mi?
        if (sizeVal > 0 && nextPtr != headPtr)
        {
            DWORD firstVal = 0;
            if (SafeRead4(nextPtr + 8, firstVal) && IsValidPtr(firstVal))
            {
                // Bu child'in m_szID'sini okumaya calis
                // Eger m_szID offset'ini henuz bilmiyorsak, child uzerinde de string taramasi yap
                if (s_offID < 0)
                {
                    for (int idOff = 0; idOff < 0x080; idOff += 4)
                    {
                        char idBuf[64] = {0};
                        if (ReadStdString(firstVal, idOff, idBuf, 64))
                        {
                            // UI ID'leri genellikle "btn_", "base_", "txt_", "edit_" ile baslar
                            if (strstr(idBuf, "btn_") || strstr(idBuf, "base_") ||
                                strstr(idBuf, "txt_") || strstr(idBuf, "edit_") ||
                                strstr(idBuf, "img_") || strstr(idBuf, "btns_") ||
                                strstr(idBuf, "str_") || strstr(idBuf, "list_") ||
                                strstr(idBuf, "progress_") || strstr(idBuf, "scroll_"))
                            {
                                if (s_offID < 0)
                                    s_offID = idOff;
                            }
                        }
                    }
                }

                if (s_offChildren < 0)
                {
                    s_offChildren = off;
                    s_offChildSize = off + 4;
                }
            }
        }
    }

    if (s_offID >= 0 && s_offChildren >= 0)
    {
        s_bProbeOK = true;
    }
}

// =============================================================================
// GetChildByID — Kendi implementasyonumuz
// =============================================================================
DWORD CUIManager::GetChildByID(DWORD pParent, const std::string& szID)
{
    if (pParent == 0 || szID.empty()) return 0;
    if (IsBadReadPtr((VOID*)pParent, 0x100)) return 0;

    // Ilk cagri — offset'leri probe et
    if (!s_bProbed)
    {
        ProbeOffsets(pParent);
    }

    if (!s_bProbeOK)
        return 0;

    // m_Children listesini iterate et
    DWORD headPtr = 0;
    if (!SafeRead4(pParent + s_offChildren, headPtr)) return 0;
    if (!IsValidPtr(headPtr)) return 0;

    DWORD sizeVal = 0;
    SafeRead4(pParent + s_offChildSize, sizeVal);

    // Sentinel node'dan baslayarak iterate et
    DWORD currentNode = 0;
    if (!SafeRead4(headPtr, currentNode)) return 0; // _Next = ilk gercek eleman

    int count = 0;
    int maxIter = (sizeVal > 0 && sizeVal < 200) ? (int)sizeVal : 200;

    while (currentNode != headPtr && count < maxIter)
    {
        // Node'dan child pointer'i oku: node+0x08 = _Myval
        DWORD childPtr = 0;
        if (!SafeRead4(currentNode + 8, childPtr)) break;
        if (!IsValidPtr(childPtr)) break;

        // Child'in m_szID'sini oku
        char childID[64] = {0};
        if (ReadStdString(childPtr, s_offID, childID, 64))
        {
            if (lstrcmpiA(childID, szID.c_str()) == 0)
            {
                return childPtr;
            }
        }

        // Sonraki node
        DWORD nextNode = 0;
        if (!SafeRead4(currentNode, nextNode)) break; // node+0x00 = _Next
        currentNode = nextNode;
        count++;
    }

    return 0;
}

// =============================================================================
// DumpChildren — Bir UI nesnesinin tum child'larini konsola yaz
// =============================================================================
void CUIManager::DumpChildren(DWORD pParent)
{
    if (pParent == 0 || !s_bProbeOK) return;
    if (IsBadReadPtr((VOID*)pParent, 0x100)) return;

    DWORD headPtr = 0;
    if (!SafeRead4(pParent + s_offChildren, headPtr)) return;
    if (!IsValidPtr(headPtr)) return;

    DWORD sizeVal = 0;
    SafeRead4(pParent + s_offChildSize, sizeVal);


    DWORD currentNode = 0;
    if (!SafeRead4(headPtr, currentNode)) return;

    int count = 0;
    int maxIter = (sizeVal > 0 && sizeVal < 200) ? (int)sizeVal : 200;

    while (currentNode != headPtr && count < maxIter)
    {
        DWORD childPtr = 0;
        if (!SafeRead4(currentNode + 8, childPtr)) break;
        if (!IsValidPtr(childPtr)) break;

        char childID[64] = {0};
        char childFile[64] = {0};
        ReadStdString(childPtr, s_offID, childID, 64);
        ReadStdString(childPtr, 0x020, childFile, 64);


        DWORD nextNode = 0;
        if (!SafeRead4(currentNode, nextNode)) break;
        currentNode = nextNode;
        count++;
    }
}


// SetVisible
void CUIManager::SetVisible(DWORD pElement, bool bVisible)
{
    if (pElement == 0) return;
    // TODO: KO_SET_VISIBLE_FUNC adresi 25xx icin kesfedilmedi
}

// IsVisible
bool CUIManager::IsVisible(DWORD pElement)
{
    if (pElement == 0) return false;
    uint8 visible = *(uint8*)(pElement + KO_OFF_UI_VISIBLE);
    return (visible != 0);
}

// SetString
void CUIManager::SetString(DWORD pElement, const std::string& str)
{
    if (pElement == 0) return;
    // TODO: KO_SET_STRING_FUNC adresi 25xx icin kesfedilmedi
}

// GetString
std::string CUIManager::GetString(DWORD pElement)
{
    if (pElement == 0) return "";
    char* pStr = (char*)(pElement + KO_OFF_UI_EDIT_VALUE);
    if (pStr != nullptr) return std::string(pStr);
    return "";
}

// SetState
void CUIManager::SetState(DWORD pElement, DWORD dwState)
{
    if (pElement == 0) return;
    // TODO: 25xx icin state offset'i dogrulanacak
}

// Pozisyon ve Boyut
void CUIManager::SetUIPos(DWORD pElement, POINT pt) { if (pElement == 0) return; }
POINT CUIManager::GetUiPos(DWORD pElement) { POINT pt = {0,0}; return pt; }
LONG CUIManager::GetUiWidth(DWORD pElement) { return 0; }
LONG CUIManager::GetUiHeight(DWORD pElement) { return 0; }

// Region
void CUIManager::SetUiRegion(DWORD pElement, RECT rc) { if (pElement == 0) return; }
RECT CUIManager::GetUiRegion(DWORD pElement) { RECT rc = {0,0,0,0}; return rc; }

// Liste
void CUIManager::AddListString(DWORD pElement, const std::string& str, DWORD dwColor) {}
void CUIManager::ClearListString(DWORD pElement) {}
