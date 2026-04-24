#pragma once

#include <set>
#include <map>

// 25xx Full Hook System - PacketHandler
// Send hook yonetimi, opcode filtreleme ve paket gonderme
// Recv hook yonetimi, opcode bazli handler yonlendirme

// Forward declaration
class Packet;

// Send fonksiyon typedef'leri
typedef int(__thiscall* tSend)(DWORD thisPtr, BYTE* pBuf, int iLen);

// Recv fonksiyon typedef'leri
typedef int(__thiscall* tRecv)(DWORD thisPtr, BYTE* pBuf, int iLen);

// Handler fonksiyon typedef'i - gelen paketleri isleme
typedef void (*PacketHandlerFunc)(BYTE* pBuf, int iLen);

class PacketHandler {
public:
    PacketHandler();
    ~PacketHandler();

    // Hook kurulumu
    void InitSendHook();
    void InitRecvHook();

    // Paket gonderme (Packet nesnesi ile)
    void Send(Packet* pkt);

    // Opcode filtreleme (send)
    void AddBlockedOpcode(uint8 opcode);
    void RemoveBlockedOpcode(uint8 opcode);
    bool IsOpcodeBlocked(uint8 opcode) const;

    // Recv handler kayit mekanizmasi
    void RegisterHandler(uint8 opcode, PacketHandlerFunc func);
    void UnregisterHandler(uint8 opcode);

    // Hook callback (static - DetourFunction icin)
    static int __fastcall hkSend(DWORD thisPtr, DWORD edx, BYTE* pBuf, int iLen);
    static int __fastcall hkRecv(DWORD thisPtr, DWORD edx, BYTE* pBuf, int iLen);

private:
    std::set<uint8> m_blockedOpcodes;
    std::map<uint8, PacketHandlerFunc> m_recvHandlers;

    // Orijinal fonksiyon pointer'lari (static - callback'lerden erisilebilir olmali)
    static tSend s_oSend;
    static tRecv s_oRecv;

    // Son return address kaydi
    static DWORD s_lastReturnAddress;

    // Version spoof: client'in gonderdigi versiyonu yakala, server'dan geleni spoof et
    static uint16_t s_clientVersion;
};
