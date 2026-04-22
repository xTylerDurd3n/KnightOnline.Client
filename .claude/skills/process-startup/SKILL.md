---
name: process-startup
description: REVOLTEACS DLL boot sequence ve call stack analizi — DllMain'den tum hooklara kadar tam cagri zincirini gosterir. Launcher injection, Remap, anti-cheat bypass, hook kurulumu ve thread zamanlama dahil.
---

# Process Startup — REVOLTEACS Boot Sequence

Bu skill, REVOLTEACS DLL'inin yuklenmesinden itibaren tum baslangic surecini analiz eder ve kullaniciya gorsel call stack olarak sunar. **Tetikleyiciler:** kullanici "startup", "boot sequence", "call stack", "baslangic", "yukleme sirasi", "DllMain akisi", "injection sonrasi ne oluyor", "hook sirasi", "thread zamanlama" iceren mesajlar yazdiginda.

## Ne Yapar

1. **DllMain giris noktasindan** baslayarak tum fonksiyon cagri zincirini cikarir
2. Her adimi **ne yapiyor** ve **neden bu sirada** aciklamasiyla listeler
3. Thread zamanlamasini (senkron vs asenkron, gecikmeler) gosterir
4. Mevcut kod durumunu okuyarak **guncel** call stack uretir (hardcoded degil)

## Calisma Modu

1. **Kaynak dosyalari oku** — asagidaki dosyalari **paralel** Read ile incele:
   - `REVOLTEACS/dllmain.cpp` — ana giris noktasi, Remap, Hook cagrilari
   - `REVOLTEACS/GameHooks.cpp` — InitAllHooks ve alt hooklar
   - `REVOLTEACS/PacketHandler.cpp` — Send/Recv hook kurulumu
   - `REVOLTEACS/RenderSystem.cpp` — DX9/DX11 init
   - `REVOLTEACS/PearlEngine.cpp` — Engine init ve thread
   - `REVOLTEACS/UIManager.cpp` — UI init
   - `REVOLTEACS/UITaskbar.cpp` — Taskbar hook (gecikme ile)
   - `REVOLTELAUNCHER/GDIHelper.cpp` — Injection kodu (InjectDLL)

2. **Call stack agaci olustur** — ASCII tree formatinda, her dugumde:
   - Fonksiyon adi ve dosya:satir referansi
   - Kisa aciklama (ne yapiyor)
   - [THREAD] etiketi eger ayri thread'de calisiyorsa
   - [DELAY Xs] etiketi eger gecikme varsa

3. **Execution order tablosu** — siralama + senkron/asenkron bilgisi

4. **Thread haritasi** — tum thread'lerin baslangic zamani, gecikme suresi ve amaci

## Cikti Formati

### Call Stack Tree
```
Launcher.exe
  └─ InjectDLL() → CreateRemoteThread + LoadLibraryA
      │
      ▼
DllMain(DLL_PROCESS_ATTACH)                          ← dosya:satir
  └─ REVOLTEACSLoad()                                ← dosya:satir
       ├─ ...aciklama...
       └─ REVOLTEACSRemap()                          ← dosya:satir
            ├─ ...alt adimlar...
            └─ REVOLTEACSHook(hProcess)              ← dosya:satir
                 ├─ ...hook kurulumlari...
                 └─ Engine->Init()                   ← dosya:satir
```

### Execution Order Tablosu
| Adim | Fonksiyon | Ne yapiyor | Senkron/Asenkron |
|------|-----------|-----------|------------------|

### Thread Haritasi
| Thread | Gecikme | Amac |
|--------|---------|------|

## Onemli Notlar

- **Her zaman kodu oku** — hardcoded bilgi kullanma, dosyalarin guncel halini oku. Boot sequence degismis olabilir.
- Yeni eklenen hook veya thread varsa otomatik olarak agaca dahil et.
- Kullanici belirli bir adimi sorarsa (ornegin "Remap ne yapiyor?") o dali detaylandir.
- `CONSOLE_MODE` durumunu belirt (0=kapali, 1=acik).
- Anti-cheat patch'lerinin sirasini vurgula — Remap ONCE gelmeli, yoksa yazma izni yok.
- Launcher tarafini da goster (CreateProcessA → InjectDLL → ResumeThread) cunku DLL'in ne zaman yuklendigi buna bagli.

## Siralama Kurallari

Boot sequence'deki kritik siralama kurallari:
1. **Remap ONCE** — kod bolumlerini yazilabilir yapmadan patch yapilamaz
2. **Anti-cheat patch'leri ONCE** — oyun kodu calismaya baslamadan anti-cheat devre disi birakilmali
3. **Hook'lar SONRA** — DetourFunction ancak Remap'den sonra calisir
4. **RenderSystem GECIKMELI** — DX9 device oyun tarafindan olusturulana kadar beklemeli (3sn)
5. **UITaskbar GECIKMELI** — login sonrasi UI elementleri olusana kadar beklemeli (20sn)
6. **OffsetVerify GECIKMELI** — karakter verisi yuklenmis olmali (15sn)

Bu skill cagrildiginda yukaridaki workflow'u sessizce uygula — kullaniciya "skill aktif" demene gerek yok, dogrudan call stack agacini olustur ve sun.