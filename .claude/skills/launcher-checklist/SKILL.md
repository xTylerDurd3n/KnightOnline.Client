---
name: launcher-checklist
description: REVOLTELAUNCHER uygulama checklist'i — faz faz sıralı görev listesi. Mevcut kod durumu, eksikler, bağımlılıklar. Tetikleyiciler: "launcher checklist", "launcher ne eklenecek", "faz", "launcher todo", "launcher sıra".
---

# REVOLTELAUNCHER — Uygulama Checklist

> Sıra kritik. Execution order'a göre dizildi.
> Kod referansı: `REVOLTELAUNCHER/GDIHelper.cpp`

---

## FAZA 1 — Launcher başlar başlamaz (oyun açılmadan önce)

- [ ] **UserAssist temizle** *(YENİ — 2026-04-26 bulgusu)*
  - `HKCU\...\UserAssist\{CEBFF5CD-ACE2-4F4F-9178-9926F41749EA}\Count`
  - `HKCU\...\UserAssist\{F4E57C4B-2036-45F0-A9AB-443BCFE33D9F}\Count`
  - Her iki GUID altındaki tüm value'ları `RegDeleteValue` ile sil
  - Launcher'ın kendi adını da temizle
  - C++ kodu: `userassist-antiforensics` skill'inde, `EnumUserAssist(true)` çağrısı yeterli

- [ ] **Windows Defender exclusion doğrula**
  - `LauncherEngine.cpp`'de var mı kontrol et
  - `gameDir` klasörünü exclusion listesine ekle
  - DLL inject başarısız oluyorsa ilk bakılacak yer burası

---

## FAZA 2 — Game process başlatma

- [x] **`CreateProcessA` + `CREATE_SUSPENDED`** *(ZATEN VAR — GDIHelper.cpp:399)*
- [x] **Launcher PID'ini argüman geç** *(ZATEN VAR — cmdLine'da `%lu` ile)*
- [ ] **Launcher dependency check doğrula** *(açık araştırma)*
  - KO.exe başlarken launcher PID'inin hayatta olup olmadığını kontrol ediyor
  - Şu an kendi PID'imizi geçiyoruz ama kontrol mekanizması tam anlaşılmadı
  - Procmon'da `OpenProcess` / `IsProcessAlive` tarzı call görünüyor mu bak

---

## FAZA 3 — Suspended process'e cold patch (inject öncesi)

- [ ] **XIGNCODE dispatcher patch** *(suspended process'e WriteProcessMemory)*
  - `dword_F661D0` adresine NopHandler stub adresi yaz
  - 21 xref noktasının hepsi bu slot üzerinden geçiyor — tek patch yeterli
  - Timing kritik: inject'ten önce, resume'dan önce
  - Adres: keşif devam ediyor (yeni client'ta `0x0050BAA9` / `0x0050BAD5`)

- [ ] **Launcher dependency patch** *(adres keşif bekliyor)*
  - KO.exe'nin launcher check yaptığı adres bulunursa NOP/JMP patch
  - Adres henüz bilinmiyor — Procmon + IDA ile bulunacak

---

## FAZA 4 — DLL inject

- [x] **`InjectDLL` (CreateRemoteThread + LoadLibraryA)** *(ZATEN VAR — GDIHelper.cpp:408)*
- [ ] **Inject doğrulama iyileştir**
  - `loadResult == 0` kontrolü var ama hata mesajı generic
  - Başarılı inject sonrası `EnumProcessModules` ile DLL'in gerçekten yüklendiğini teyit et

---

## FAZA 5 — Resume & post-launch

- [x] **`ResumeThread`** *(ZATEN VAR — GDIHelper.cpp:421)*
- [ ] **PEB module unlink** *(REVOLTEACS DllMain içinde yapılacak, launcher değil)*
  - `REVOLTEACS.dll` PEB Ldr listesinden çıkarılacak
  - XIGNCODE runtime module scan'ine karşı
  - Ertelendi — detection gözlemlenirse uygula

---

## FAZA 6 — Background watchdog (launcher kapanmadan önce)

- [ ] **UserAssist watch loop** *(YENİ)*
  - Launcher şu an `PostQuitMessage` + `TerminateProcess` ile hemen kapanıyor (GDIHelper.cpp:427)
  - Sürekli temizleme gerekiyorsa launcher'ın arka planda kalması lazım
  - **Karar gerekiyor:** Launcher kapansın mı, yoksa tray'de gizli kalsın mı?

- [ ] **XIGNCODE watchdog thread** *(açık araştırma)*
  - Dispatcher patch'i XIGNCODE kendi kendine geri yazabilir mi?
  - Geri yazıyorsa watchdog thread loop ile tekrar patch at
  - Önce gözlemle, sonra uygula

---

## Bağımlılık Haritası

```
Faza 1 → Faza 2 → Faza 3 ──────→ Faza 4 → Faza 5 → Faza 6
   ↑                 ↑
UserAssist       XIGNCODE patch adresi
temizleme        (keşif devam ediyor)
hazır
```

---

## Mevcut Durum Özeti

| Faz | Durum | Bloker |
|-----|-------|--------|
| Faza 1 — UserAssist temizle | Bekliyor | Yok — kod hazır |
| Faza 1 — Defender exclusion | Doğrula | — |
| Faza 2 — CreateSuspended | ✅ Tamamlandı | — |
| Faza 2 — Dependency check | Araştırılıyor | Adres bilinmiyor |
| Faza 3 — XIGNCODE dispatcher | Araştırılıyor | Adres keşif devam |
| Faza 3 — Launcher dep patch | Araştırılıyor | Adres bilinmiyor |
| Faza 4 — InjectDLL | ✅ Tamamlandı | — |
| Faza 4 — Inject doğrulama | Bekliyor | Yok |
| Faza 5 — ResumeThread | ✅ Tamamlandı | — |
| Faza 5 — PEB unlink | Ertelendi | Detection gözlemle |
| Faza 6 — UserAssist loop | Karar bekliyor | Launcher lifecycle |
| Faza 6 — XIGNCODE watchdog | Araştırılıyor | Önce gözlemle |

---

## İlgili Skill'ler

- `userassist-antiforensics` — Faza 1 ve Faza 6 için C++ kodu
- `re-research` — Faza 3 adres keşfi için
- `new-client-port` — XIGNCODE dispatcher adresleri
