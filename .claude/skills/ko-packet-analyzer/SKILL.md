---
name: ko-packet-analyzer
description: KnightOnline v2604 NETWORK SNIFF referansı — sadece runtime paket dinleme/intercept için. RE/offset analizi için geçersiz. AES-128 key/IV, decrypt hook noktası, plaintext intercept adresleri. Tetikleyiciler: "sniff", "paket dinle", "network traffic", "paket yakala", "AES decrypt hook", "XM550".
---

# KO Packet Analyzer — KnightOnline v2604
> **Kapsam:** Sadece **network sniffing / runtime paket intercept**. Reverse engineering veya offset analizi için bu skill'i kullanma — o veriler dump'a ve client versiyonuna göre değişir.

## Versiyon
**v2604** (client dump bazlı analiz, Themida korumalı binary)

---

## Keşfedilen Şifreleme Sistemi

### AES-128 Hardcoded Key
```
Key : XM550UVRL34CVSAY   (16 byte, ASCII)
Mode: AES-128
```
- `push 10h` (keySize = 16) + `push offset aXm550uvrl34cvs` olarak `sub_61AEB0`'a argüman geçiliyor
- Key sabit, binary'de `.rdata` segment yakınında string olarak duruyor

### IV (Initialization Vector)
- `unk_F9037C` adresinde — sabit mi, per-packet mı henüz doğrulanmadı
- İncelenmesi gerekiyor: her pakette değişiyorsa IV de intercept edilmeli

### AES Decrypt Fonksiyonu
| Sembol | Adres | Açıklama |
|--------|-------|----------|
| `sub_61AEB0` | `0x0061AEB0` | AES decrypt wrapper, AES-NI kullanıyor |
| `unk_F9037C` | `0x00F9037C` | Muhtemel IV |
| `dword_11863DC` | `0x011863DC` | Decrypt context / output buffer ptr |
| `aXm550uvrl34cvs` | `.rdata` | "XM550UVRL34CVSAY" key string |

**`sub_61AEB0` imzası (tahmini):**
```cpp
int sub_61AEB0(
    BYTE*  pInput,       // arg_0  — ciphertext
    DWORD  inSize,       // arg_4
    /* ... */
    BYTE*  pKey,         // arg_C  — "XM550UVRL34CVSAY"
    DWORD  keySize,      // arg_10 — 0x10 (zorunlu, aksi halde -21 döner)
    /* ... */
    BYTE*  pIV,          // unk_F9037C
    /* ... */
    BYTE*  pOutput,      // arg_24 — plaintext çıktı
    /* ... */
);
// Hata: -0x14 (input null), -21 / 0xFFFFFFEB (key null veya keySize != 16)
```

### Call Site (AES decrypt çağrısı)
```asm
push    10h                       ; keySize = 16
push    offset aXm550uvrl34cvs   ; "XM550UVRL34CVSAY"
push    eax                       ; input (ciphertext)
call    sub_61AEB0
mov     ebx, eax                  ; 0 = success
add     esp, 38h                  ; 14 arg temizliği
test    ebx, ebx
jnz     short loc_8350A5          ; hata path
```
Call site: `0x008350xx` civarı (`loc_8350A5`'ten önceki blok)

---

## String Referansları (.rdata)

| Adres | String | XREF |
|-------|--------|------|
| `0x00F84B30` | `"AES Decrypt SIZE in_size=%d:%d"` | `sub_61AEB0+3B8` |
| `0x00F84B50` | `"aAbBcCdDeFgGhHIjmMnprRStTuUvWwXxYyZz"` | custom encoding table |
| `0x00F84BAC` | `"ioctlsocket failed with error: %ld"` | `sub_61CE40+191` |
| `0x00F84BD0` | `"socket error"` | `sub_61CE40+1AC` |
| `0x00F84BE0` | `"[Invalid packet] : %02x"` | `sub_61E5D0+B6` (post-decrypt path) |
| `0x00F84BF8` | `"socket receive error! : %d"` | `sub_61DAC6`, `sub_61E2E0+275` |

---

## Socket / Recv Pipeline

| Fonksiyon | Adres | Rol |
|-----------|-------|-----|
| `sub_61CE40` | `0x0061CE40` | Socket recv / error handler |
| `sub_61E5D0` | `0x0061E5D0` | Post-decrypt packet işleme (invalid packet check burada) |
| `sub_61DAC6` | `0x0061DAC6` | Socket receive error |
| `sub_61E2E0` | `0x0061E2E0` | Socket receive error handler |
| `sub_61C8B0` | `0x0061C8B0` | `off_F84B7C` → vtable/fp slot |
| `sub_61CA90` | `0x0061CA90` | `off_F84B88`, `off_F84B9C` → vtable/fp slot |

---

## Hook Stratejisi — Plaintext Paket Intercept

### Yanlış yaklaşım
Raw `KO_RECV_FNC` hooklamak → ciphertext görürsün, opcode okunmaz.

### Doğru yaklaşım (3 seçenek)

**Seçenek A — `sub_61AEB0` çıkış hook'u:**
- `sub_61AEB0` döndükten hemen sonra (`add esp, 38h` sonrası) hook at
- `pOutput` buffer'ından plaintext oku
- `ebx == 0` kontrolü yap (başarılı decrypt)

**Seçenek B — `sub_61E5D0` entry hook'u:**
- `"[Invalid packet]"` referansı burada → fonksiyon entry'sinde paket zaten decrypted
- Daha temiz intercept noktası, şifreleme detayına gerek yok

**Seçenek C — Offline decrypt:**
- Key `XM550UVRL34CVSAY` bilindiğinden, raw ciphertext'i kendin decrypt edebilirsin
- IV doğrulanınca tam offline analiz mümkün

---

## Sıradaki Adımlar
1. `unk_F9037C` incele — sabit IV mi per-packet IV mi?
2. `sub_61AEB0` call site adresini kesinleştir (şu an `0x008350xx` tahmini)
3. `sub_61E5D0` entry'sine hook yaz → plaintext paket logla
4. `KO_SND_FNC` / `KO_RECV_FNC` adreslerini yeni client için bul (eski: Send=`0x006FC190`, Recv=`0x0082C7D0`)
