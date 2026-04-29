---
name: userassist-antiforensics
description: Windows UserAssist registry anti-forensics tekniği — cheat/loader yazılımlarının kendini nasıl sakladığı, Procmon tespiti, C++ enumerate/temizleme kodu, karşı önlemler. Tetikleyiciler: "UserAssist", "RegDeleteValue", "RegEnumValue", "anti-forensics", "iz silme", "loader gizleme", "registry temizleme", "cheat saklama".
---

# UserAssist Anti-Forensics

## Bulgu (2026-04-26)

**loader.exe (PID: 20064)** Procmon'da sürekli şu işlemleri yapıyor:
```
RegOpenKey   → HKCU\...\UserAssist\{GUID}\Count
RegEnumValue → tüm program kayıtlarını listele   (NO MORE ENTRIES gelene kadar)
RegDeleteValue → kayıtları tek tek sil
RegCloseKey
RegSetInfoKey → HandleTags: 0x0  (handle metadata, veri silmez)
```

**Sonuç:** loader.exe kendi execution izini UserAssist'ten sürekli temizliyor — klasik cheat/loader anti-forensics tekniği.

---

## UserAssist Nedir?

`HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\UserAssist\{GUID}\Count`

Windows Shell'in program çalıştırma geçmişi. Explorer, `ShellExecuteEx` ile başlatılan her programı buraya yazar:
- Kaç kez çalıştırıldığı (`runCount`)
- Son çalıştırma zamanı (`lastRun` FILETIME)
- Program adı — **ROT13 encode** edilmiş

`Count` bir sayı değil, **subkey'in adı** (Microsoft'un verdiği).

### Bilinen GUID'ler (Win7+)
| GUID | İçerik |
|------|--------|
| `{CEBFF5CD-ACE2-4F4F-9178-9926F41749EA}` | EXE çalıştırma geçmişi |
| `{F4E57C4B-2036-45F0-A9AB-443BCFE33D9F}` | Shortcut (.lnk) geçmişi |

---

## Neden Yapıyor — Olası Senaryolar

### Senaryo 1 — Anti-forensics (en olası)
```
loader.exe çalışır
  → ShellExecuteEx ile KnightOnline.exe başlatır
  → Windows Explorer UserAssist'e loader.exe kaydı yazar
  → loader.exe kendi kaydını hemen siler
  → Adli incelemede "bu program hiç çalışmamış" görünür
```

### Senaryo 2 — XIGNCODE whitelist kontrolü
```
XIGNCODE başlar
  → UserAssist'i tarar
  → Whitelist dışı exe (loader, cheat DLL injector) görürse engel
  → loader bunu önlemek için XIGNCODE'dan önce siler
```

### Senaryo 3 — Explorer tetikliyor (loader değil)
```
loader ShellExecuteEx kullanır
  → Explorer (farklı process) UserAssist'i günceller
  → Procmon bunu loader'a attribute edebilir
  → Stack trace ile gerçek sorumlu belirlenmeli
```

**Stack trace kontrolü:** Procmon'da ilgili satır → sağ tık → **Stack** sekmesi → hangi fonksiyondan geldiği görünür.

---

## Data Layout (Win10)

```c
#pragma pack(push, 1)
struct UserAssistEntry {
    DWORD version;      // 0
    DWORD unknown;      // 0
    DWORD runCount;     // kaç kez çalıştırıldı
    DWORD focusCount;   // aktif pencere odak sayısı
    DWORD focusTime_ms; // toplam odak süresi (ms)
    FILETIME lastRun;   // son çalıştırma zamanı
};
#pragma pack(pop)
```

Key isimleri ROT13 ile encode — `HRZR_CNGU:P:\...` gibi görünür, decode edilince gerçek path çıkar.

---

## ROT13 Decode

```cpp
std::wstring rot13(const std::wstring& s) {
    std::wstring out = s;
    for (wchar_t& c : out) {
        if      (c >= L'A' && c <= L'M') c += 13;
        else if (c >= L'N' && c <= L'Z') c -= 13;
        else if (c >= L'a' && c <= L'm') c += 13;
        else if (c >= L'n' && c <= L'z') c -= 13;
    }
    return out;
}
```

---

## C++ — Enumerate & Temizle

```cpp
#include <windows.h>
#include <string>
#include <vector>

std::wstring rot13(const std::wstring& s) {
    std::wstring out = s;
    for (wchar_t& c : out) {
        if      (c >= L'A' && c <= L'M') c += 13;
        else if (c >= L'N' && c <= L'Z') c -= 13;
        else if (c >= L'a' && c <= L'm') c += 13;
        else if (c >= L'n' && c <= L'z') c -= 13;
    }
    return out;
}

#pragma pack(push, 1)
struct UserAssistEntry {
    DWORD version, unknown, runCount, focusCount, focusTime_ms;
    FILETIME lastRun;
};
#pragma pack(pop)

std::wstring FileTimeToStr(const FILETIME& ft) {
    SYSTEMTIME st; FileTimeToSystemTime(&ft, &st);
    wchar_t buf[64];
    swprintf(buf, 64, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return buf;
}

void EnumUserAssist(bool deleteEntries = false) {
    const wchar_t* guids[] = {
        L"{CEBFF5CD-ACE2-4F4F-9178-9926F41749EA}",
        L"{F4E57C4B-2036-45F0-A9AB-443BCFE33D9F}",
        nullptr
    };

    for (int i = 0; guids[i]; i++) {
        wchar_t keyPath[256];
        swprintf(keyPath, 256,
            L"Software\\Microsoft\\Windows\\CurrentVersion"
            L"\\Explorer\\UserAssist\\%s\\Count", guids[i]);

        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath,
                          0, KEY_READ | KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
            continue;

        DWORD index = 0;
        wchar_t name[512]; BYTE data[128];
        DWORD nameLen, dataLen, type;
        std::vector<std::wstring> toDelete;

        while (true) {
            nameLen = 512; dataLen = sizeof(data);
            LONG r = RegEnumValueW(hKey, index++, name, &nameLen,
                                   nullptr, &type, data, &dataLen);
            if (r == ERROR_NO_MORE_ITEMS) break;
            if (r != ERROR_SUCCESS) continue;

            std::wstring decoded = rot13(name);
            if (dataLen >= sizeof(UserAssistEntry)) {
                auto* e = reinterpret_cast<UserAssistEntry*>(data);
                wprintf(L"[%u x] %s — %s\n",
                    e->runCount, decoded.c_str(),
                    FileTimeToStr(e->lastRun).c_str());
            }
            if (deleteEntries) toDelete.push_back(name);
        }

        if (deleteEntries)
            for (auto& n : toDelete)
                RegDeleteValueW(hKey, n.c_str());

        RegCloseKey(hKey);
    }
}

// Kullanım:
// EnumUserAssist(false);  → sadece listele
// EnumUserAssist(true);   → listele + sil
```

---

## Procmon Filtresi — Tespiti Doğrulama

```
Process Name   is      loader.exe
Path           contains UserAssist
Operation      is      RegDeleteValue
→ Result: SUCCESS satırları → aktif iz silme
```

Ayrıca:
```
Operation  is  RegSetInfoKey
Detail     contains HandleTags
→ Bu veri silmez, sadece handle metadata güncellemesi
```

---

## Karşı Önlemler

| Teknik | Açıklama |
|--------|----------|
| **Registry audit** | `auditpol /set /subcategory:"Registry" /success:enable` → Event 4663 logla |
| **Procmon boot log** | Procmon'u boot'ta başlat, loader'dan önce kayıt yakala |
| **Shadow copy** | UserAssist'in silinmeden önceki halini VSS ile yakala |
| **ETW tracing** | `Microsoft-Windows-Registry` ETW provider ile real-time izle |
| **Sticky değer** | Reg key'e DACL ile WriteKey iznini kaldır — silme başarısız olur |

---

## REVOLTEACS Bağlamı

Bu bulgu XIGNCODE bypass stratejisini etkiliyor:
- XIGNCODE muhtemelen UserAssist'i tarayarak hangi araçların (x32dbg, Cheat Engine, inject araçları) çalıştığını kontrol ediyor
- loader.exe XIGNCODE'dan önce bu kayıtları temizleyerek tespiti önlüyor
- REVOLTEACS DLL'i inject eden launcher aynı tekniği uygulamalı
