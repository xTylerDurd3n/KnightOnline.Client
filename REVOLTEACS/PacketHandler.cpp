#include "pch.h"
#include "PacketHandler.h"
#include "Packet.h"

extern void RevLog(const char* fmt, ...);

// Static member tanimlari
tSend PacketHandler::s_oSend = nullptr;
tRecv PacketHandler::s_oRecv = nullptr;
DWORD PacketHandler::s_lastReturnAddress = 0;
uint16_t PacketHandler::s_clientVersion = 0;

// Global PacketHandler instance pointer (hkSend/hkRecv callback'inden erismek icin)
static PacketHandler* g_pPacketHandler = nullptr;

PacketHandler::PacketHandler()
{
}

PacketHandler::~PacketHandler()
{
    g_pPacketHandler = nullptr;
}

static bool VerifyHookTarget(DWORD addr, const char* name)
{
    if (addr == 0 || IsBadCodePtr((FARPROC)addr))
    {
        RevLog("hook: %s SKIP — adres gecersiz (0x%08X)", name, addr);
        return false;
    }
    BYTE b[4];
    memcpy(b, (void*)addr, 4);
    RevLog("hook: %s @ 0x%08X bytes=[%02X %02X %02X %02X]", name, addr, b[0], b[1], b[2], b[3]);
    // Gecerli fonksiyon prologu: push ebp (55), sub esp (83 EC), mov edi edi (8B FF), push edi (57)
    bool valid = (b[0] == 0x55 || b[0] == 0x83 || b[0] == 0x8B || b[0] == 0x57 || b[0] == 0x56 || b[0] == 0x53);
    if (!valid)
        RevLog("hook: %s UYARI — beklenmedik prologue, adres yanlis olabilir", name);
    return true;
}

void PacketHandler::InitSendHook()
{
    g_pPacketHandler = this;
    if (!VerifyHookTarget(KO_SND_FNC, "Send"))
        return;
    s_oSend = (tSend)DetourFunction((PBYTE)KO_SND_FNC, (PBYTE)hkSend);
    RevLog("hook: Send %s trampoline=%p", s_oSend ? "OK" : "FAILED", s_oSend);
}

void PacketHandler::InitRecvHook()
{
    g_pPacketHandler = this;
    if (KO_RECV_FNC == 0)
    {
        RevLog("hook: Recv SKIP (KO_RECV_FNC=0)");
        return;
    }
    if (!VerifyHookTarget(KO_RECV_FNC, "Recv"))
        return;
    s_oRecv = (tRecv)DetourFunction((PBYTE)KO_RECV_FNC, (PBYTE)hkRecv);
    RevLog("hook: Recv %s trampoline=%p", s_oRecv ? "OK" : "FAILED", s_oRecv);
}

void PacketHandler::Send(Packet* pkt)
{
    if (!pkt || !s_oSend)
        return;

    DWORD pktBase = *(DWORD*)KO_PTR_PKT;
    if (pktBase == 0)
        return;

    // Packet'in opcode + data'sini buffer'a yaz
    // Opcode ilk byte olarak eklenir, ardindan buffer icerigi
    size_t dataSize = pkt->size();
    size_t totalSize = 1 + dataSize; // opcode + data

    std::vector<BYTE> buf(totalSize);
    buf[0] = pkt->GetOpcode();

    if (dataSize > 0 && pkt->contents())
    {
        memcpy(&buf[1], pkt->contents(), dataSize);
    }

    s_oSend(pktBase, buf.data(), (int)totalSize);
}

int __fastcall PacketHandler::hkSend(DWORD thisPtr, DWORD edx, BYTE* pBuf, int iLen)
{
    s_lastReturnAddress = (DWORD)(uintptr_t)_ReturnAddress();

    if (pBuf && iLen > 0)
    {
        uint8 opcode = pBuf[0];

        // Version check (0x2B): client'in kendi version'ini kaydet
        if (opcode == 0x2B && iLen >= 3)
        {
            s_clientVersion = *(uint16_t*)(&pBuf[1]);
            RevLog("VER_SEND: client version=%u (0x%04X)", s_clientVersion, s_clientVersion);
        }

        // Hex dump (max 16 byte)
        char hex[64] = {};
        int dumpLen = iLen < 16 ? iLen : 16;
        for (int i = 0; i < dumpLen; i++)
            sprintf(hex + i * 3, "%02X ", pBuf[i]);
        RevLog("SEND op=0x%02X len=%d [%s]", opcode, iLen, hex);

        if (g_pPacketHandler && g_pPacketHandler->IsOpcodeBlocked(opcode))
        {
            RevLog("SEND BLOCKED op=0x%02X", opcode);
            return 0;
        }
    }

    return s_oSend(thisPtr, pBuf, iLen);
}

int __fastcall PacketHandler::hkRecv(DWORD thisPtr, DWORD edx, BYTE* pBuf, int iLen)
{
    // Version spoof: game process etmeden ONCE pBuf'i patcha
    if (pBuf && iLen >= 4 && pBuf[0] == 0x2B)
    {
        uint16_t serverVer = *(uint16_t*)(&pBuf[2]);
        RevLog("VER_RECV: server=%u client=%u", serverVer, s_clientVersion);
        if (s_clientVersion != 0 && serverVer != s_clientVersion)
        {
            *(uint16_t*)(&pBuf[2]) = s_clientVersion;
            RevLog("VER_SPOOFED: %u -> %u", serverVer, s_clientVersion);
        }
    }

    int result = s_oRecv(thisPtr, pBuf, iLen);

    if (pBuf && iLen > 0)
    {
        uint8 opcode = pBuf[0];
        char hex[64] = {};
        int dumpLen = iLen < 16 ? iLen : 16;
        for (int i = 0; i < dumpLen; i++)
            sprintf(hex + i * 3, "%02X ", pBuf[i]);
        RevLog("RECV op=0x%02X len=%d [%s]", opcode, iLen, hex);
    }

    return result;
}

// --- Opcode Filtreleme ---

void PacketHandler::AddBlockedOpcode(uint8 opcode)
{
    m_blockedOpcodes.insert(opcode);
}

void PacketHandler::RemoveBlockedOpcode(uint8 opcode)
{
    m_blockedOpcodes.erase(opcode);
}

bool PacketHandler::IsOpcodeBlocked(uint8 opcode) const
{
    return m_blockedOpcodes.find(opcode) != m_blockedOpcodes.end();
}

// --- Recv Handler Kayit ---

void PacketHandler::RegisterHandler(uint8 opcode, PacketHandlerFunc func)
{
    if (func)
    {
        m_recvHandlers[opcode] = func;
    }
}

void PacketHandler::UnregisterHandler(uint8 opcode)
{
    m_recvHandlers.erase(opcode);
}
