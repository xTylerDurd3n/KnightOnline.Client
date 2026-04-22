---
name: new-client-port
description: Yeni KnightOnline client versiyonuna REVOLTEACS portlama workflow'u — injection, XIGNCODE dispatcher bypass, Themida remap sorunu, adres kayması tanısı ve patch stratejisi. Tetikleyiciler: "yeni client", "port", "adres kaydı", "inject olmuyor", "client açılmıyor", "bypass çalışmıyor".
---

# REVOLTEACS — Yeni Client Port Workflow

Bu skill, REVOLTEACS'ı yeni bir KnightOnline client versiyonuna taşırken izlenecek adımları ve öğrenilen dersleri belgeler.

---

## 1. Injection Altyapısı

### CREATE_SUSPENDED Zorunluluğu

Launcher'da `CreateProcessA` flags parametresi `0` ise Themida stub anında çalışmaya başlar ve anti-injection kontrollerini aktif eder. DLL inject etmeden önce process'in suspend edilmesi gerekir:

```cpp
// YANLIŞ (eski kod):
CreateProcessA(NULL, cmd, ..., 0, ..., &pi);          // flags=0, anında çalışır

// DOĞRU:
CreateProcessA(NULL, cmd, ..., CREATE_SUSPENDED, ..., &pi);  // dondurulmuş başlar
InjectDLL(pi.dwProcessId, dllPath);                          // Themida başlamadan inject
ResumeThread(pi.hThread);                                    // şimdi başlat
```

### InjectDLL Return Değeri Yanıltıcı

`WaitForSingleObject` bitmesi inject'in başarılı olduğu anlamına gelmez. `GetExitCodeThread` ile LoadLibraryA'nın gerçekten DLL'i yükleyip yüklemediği doğrulanmalı:

```cpp
WaitForSingleObject(hThread, INFINITE);
DWORD loadResult = 0;
GetExitCodeThread(hThread, &loadResult);
if (loadResult == 0) { /* LoadLibraryA failed */ }
```

### DLL Path — cwd Değil, Exe Dizini

`GetCurrentDirectory()` değişebilir. DLL path'i her zaman Launcher.exe'nin kendi dizinine göre hesaplanmalı:

```cpp
char launcherDir[MAX_PATH];
GetModuleFileNameA(NULL, launcherDir, MAX_PATH);
// last slash'e kadar kes + "REVOLTEACS.dll"
```

---

## 2. Section Remap — Themida Yeni Versiyonunda Crash

### Sorun

`Remap::PatchSection` (ZwCreateSection + ZwMapViewOfSection) ile `.text` section'ları `PAGE_EXECUTE_READWRITE` yapılıyordu. Yeni Themida versiyonu unpack bittikten sonra kendi code section'larının page permission'larını kontrol ediyor:

```
Themida: ".text sayfam RWX mi?" → EVET → TerminateProcess  (yeni versiyon)
Themida: ".text sayfam RWX mi?" → EVET → devam              (eski versiyon)
```

**Belirti:** Log'da "remap: all done" + "hook: all done" görünür ama process ~115-230ms sonra ölür. Watchdog heartbeat hiç gelmez (10 saniyelik interval'e ulaşmadan ölüyor).

### Çözüm

Section remap kaldırıldı. Byte patch gereksinimleri için `WriteProcessMemory` yeterli — OS kernel seviyesinde page protection'ı geçici bypass eder, XIGNCODE izleyemez:

```cpp
BYTE patch[] = { 0xEB };
WriteProcessMemory(hProcess, (LPVOID)0x00XXXXXX, &patch, sizeof(patch), NULL);
```

Inline hook trampoline gibi "sürekli RWX" gerektiren durumlar için alternatif: `VirtualProtect` → yaz → `VirtualProtect` geri al. XIGNCODE bunu yakalayabilir; yaklarsa `VirtualProtectEx` + kısa sleep denenebilir.

---

## 3. XIGNCODE Dispatcher Bypass — NopHandler Stub

### Keşif Yöntemi

IDA'da pattern search: `75 ?? E8 ?? ?? ?? ?? 85 C0` (jne + call + test).  
Daha hedefli: `FF 15 ?? ?? F6 00` (indirect call ds:[F6xxxx] — XIGNCODE SDK pointer bölgesi).

### dword_F661D0 — Ana Dispatcher Slot'u (yeni client v25XX)

Yeni client'ta `.rdata:0x00F661D0` XIGNCODE SDK function pointer'ı. **21 yerden çağrılıyor** (xref TO ile doğrulandı). Tek slot override = tüm check'ler bypass.

**Dump zamanı değeri:** `0x7624E370` (xldr.dll veya XignCode.dll — modül henüz doğrulanmadı).

**Calling convention:** `__stdcall` 1 arg (0x0050BAA9 call site'ından görüldü).

```cpp
// NopHandler stub — 5 byte, stack-allocated, executable bölgede olmalı
static unsigned char g_NopHandler[] = {
    0x33, 0xC0,        // xor eax, eax   (return 0 = check passed)
    0xC2, 0x04, 0x00   // ret 4           (__stdcall 1 arg cleanup)
};
```

### Watchdog Thread — Timing Sorunu

XIGNCODE SDK init, `dword_F661D0`'a gerçek check fonksiyon adresini yazıyor. DllMain başında bu slot genellikle 0. Sabit bir kez yazmak yerine watchdog thread ile periyodik override:

```cpp
DWORD WINAPI XignDispatcherWatchdog(LPVOID)
{
    const DWORD XIGN_SLOT = 0x00F661D0;
    while (true) {
        __try {
            void** slot = (void**)XIGN_SLOT;
            if (*slot != (void*)g_NopHandler)
                *slot = (void*)g_NopHandler;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        Sleep(100);
    }
    return 0;
}
```

**Dikkat:** Slot .rdata'da (RO) → section remap olmadan write Access Violation fırlatır, `__except` yakalar, sessizce başarısız olur. Watchdog'un gerçekten yazabilmesi için ya remap (crash riski) ya da o sayfaya `VirtualProtect(RW)` gerekebilir. **Şu an test edilmedi.**

---

## 4. Adres Kayması Tanısı

### v2524 Patch'lerinin Yeni Client'ta Çalışmaması

Hardcoded adreslere yazılan patch'ler (`*(uint8_t*)0x004F35DF = 0x25` gibi) yeni client'ta farklı koda denk gelir. Yanlış adrese `WriteProcessMemory` → kod bozulur → Themida unpack sırasında crash.

**Belirti:** Log'da "remap: all done" görünür ama Themida resume sonrası hemen crash.

**Teşhis:** IDA'da o adrese git, beklenen instruction var mı? (`jne` bekliyorsan `mov al` görüyorsan adres kaydı var.)

**Çözüm:** Eski adreslere dokunma, comment out et. Yeni adresleri IDA'da pattern search ile bul.

### Güvenli Pattern (v2524 XIGNCODE check imzası)
```
75 ?? E8 ?? ?? ?? ?? 85 C0   (false positive riski var — instruction boundary'e dikkat)
E8 ?? ?? ?? ?? 85 C0 74      (daha güvenli: call + test + jz)
FF 15 ?? ?? F6 00            (en hedefli: indirect call to XIGNCODE SDK region)
```

---

## 5. Diagnostic Logging — RevLog

Crash noktasını bulmak için dosya + DebugString logger:

```cpp
static void RevLog(const char* fmt, ...) {
    // C:\REVOLTEACS.log + OutputDebugStringA
    // Her yazımdan sonra fflush + fclose (crash'te kaybolmaz)
}
```

**Kullanım:** Her kritik adımın öncesine ve sonrasına ekle. Son log satırı = crash noktası.

---

## 6. Build Pipeline (VSCode)

| Adım | Detay |
|---|---|
| `REVOLTEACS.vcxproj` OutDir | `$(MSBuildProjectDirectory)\..\REVOLTELAUNCHER\Win32\Release\` |
| Launch config preLaunchTask | `Build ALL (DLL + Launcher Release)` — her ikisini sırayla build eder |
| DLL path (GDIHelper.cpp) | `GetModuleFileNameA` tabanlı (cwd değil, exe dizini) |
| Debug MessageBox | InjectDLL öncesi path + exists check — eski client test ederken kaldır |

---

## 7. Açık Görevler (Sonraki Adımlar)

1. **NopHandler write doğrulaması**: .rdata'daki `dword_F661D0` RO sayfa. Watchdog write Access Violation alıyor mu? Log'da "slot changed" görünüyorsa write başarılı (section remap henüz kaldırılmamış halden kalma). Section remap kaldırıldıktan sonra watchdog'un gerçekten yazıp yazmadığı test edilmeli.

2. **Hook adreslerini yeni client'ta bul**: GameHooks (EndGame, Tick), PacketHandler (Send 0x006FC190, Recv 0x0082C7D0) adresleri kaymış. IDA'da pattern + xref ile bulunacak.

3. **RandomYazdir adreslerini güncelle**: `0x01161AF4` vb. eski adreslere random değer yazılıyor. Yeni client'ta bu anti-cheat obfuscation adreslerini bul veya bu yaklaşımı terk et.

4. **`0x7624E370` modül doğrulaması**: x32dbg ile oyun çalışırken Memory Map → xldr.dll mi, xigncode.dll mi?

5. **PEB module unlink**: REVOLTEACS.dll module listesinde görünür — runtime XIGNCODE scan riski. Detect edilirse uygula.
