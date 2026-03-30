#include "pch.h"
#include "PacketHandler.h"
#include "Packet.h"

// Static member tanimlari
tSend PacketHandler::s_oSend = nullptr;
tRecv PacketHandler::s_oRecv = nullptr;
DWORD PacketHandler::s_lastReturnAddress = 0;

// Global PacketHandler instance pointer (hkSend/hkRecv callback'inden erismek icin)
static PacketHandler* g_pPacketHandler = nullptr;

PacketHandler::PacketHandler()
{
}

PacketHandler::~PacketHandler()
{
    g_pPacketHandler = nullptr;
}

void PacketHandler::InitSendHook()
{
    g_pPacketHandler = this;

    s_oSend = (tSend)DetourFunction((PBYTE)KO_SND_FNC, (PBYTE)hkSend);

    if (s_oSend)
    {
    }
    else
    {
    }
}

void PacketHandler::InitRecvHook()
{
    g_pPacketHandler = this;

    if (KO_RECV_FNC == 0)
    {
        return;
    }

    // Game server recv handler - SEH prolog fonksiyon
    // __thiscall degil, DetourFunction ile hook'la
    s_oRecv = (tRecv)DetourFunction((PBYTE)KO_RECV_FNC, (PBYTE)hkRecv);

    if (s_oRecv)
    {
    }
    else
    {
    }
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
    // Return address kaydet
    DWORD retAddr = 0;
    __asm
    {
        mov eax, [ebp + 4]
        mov retAddr, eax
    }
    s_lastReturnAddress = retAddr;

    if (pBuf && iLen > 0)
    {
        uint8 opcode = pBuf[0];

        // Opcode/uzunluk loglama

        // Engellenen opcode kontrolu
        if (g_pPacketHandler && g_pPacketHandler->IsOpcodeBlocked(opcode))
        {
            return 0;
        }
    }

    return s_oSend(thisPtr, pBuf, iLen);
}

// Recv hook - game server recv handler
// Bu fonksiyon __thiscall, ECX = CAPISocket this pointer
// Parametreler bilinmiyor, sadece this pointer'i yakaliyoruz
int __fastcall PacketHandler::hkRecv(DWORD thisPtr, DWORD edx, BYTE* pBuf, int iLen)
{
    // Orijinal fonksiyonu cagir
    int result = s_oRecv(thisPtr, pBuf, iLen);
    
    // Basarili donusten sonra logla (crash onleme)
    // Not: Bu fonksiyonun parametreleri farkli olabilir
    // Sadece cagirildigini logluyoruz
    static int recvCount = 0;
    recvCount++;
    if (recvCount <= 5) {
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
