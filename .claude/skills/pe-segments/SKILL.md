---
name: pe-segments
description: Windows PE (Portable Executable) segment/section referansı — IDA/x32dbg/x64dbg'de karşılaşılan .text, .rdata, .data, .rsrc gibi tüm segmentlerin ne işe yaradığı, karakteristikleri (RWX flagleri), hangi derleyici/packer/anti-cheat'ten geldiği. Tetikleyiciler: "segment", "section", ".text", ".rdata", ".data", ".rsrc", "PE section", "IDA segment", "packer section", "Themida section", "VMProtect section", "XIGNCODE section", ".vmp", "UPX", paket/protector segment karışıklığı.
---

# PE Segmentleri / Section'ları — Hızlı Referans

Windows PE formatında segmentler **karakteristik flag'leri** ile tanımlanır — isim sadece konvansiyon, asıl önemli olan RWX bitleri ve içerik tipidir. IDA'da bir segmente bakarken önce adına sonra flag'lerine bak.

## Section Karakteristik Bayrakları (IMAGE_SCN_*)

Her sectionu okuyarak ne tür içerik olduğunu anlarsın:

| Flag | Anlam |
|---|---|
| `MEM_READ` (R) | Okunabilir |
| `MEM_WRITE` (W) | Yazılabilir |
| `MEM_EXECUTE` (E) | Çalıştırılabilir kod |
| `CNT_CODE` | Section kod içeriyor |
| `CNT_INITIALIZED_DATA` | Başlatılmış veri |
| `CNT_UNINITIALIZED_DATA` | Başlatılmamış veri (BSS gibi) |
| `MEM_DISCARDABLE` | Bellek yüklendikten sonra atılabilir (örn: `.reloc`) |
| `MEM_SHARED` | Process'ler arası paylaşımlı |

**Hızlı tanı kuralı:**
- **RX** (Read + Execute) → kod
- **R** (salt oku) → sabitler, string, import tablosu
- **RW** (oku + yaz) → global değişkenler, heap init
- **RWX** → ⚠️ **şüpheli** — packer/protector/shellcode imzası

---

## 1. Standart MSVC / GCC Sectionları

Bu sectionları neredeyse her binary'de görürsün.

| Section | Karakteristik | İçerik | IDA'da ne görürsün |
|---|---|---|---|
| **`.text`** | R-X | Çalıştırılabilir kod (fonksiyonlar) | Disassembly — `push ebp / mov ebp, esp ...` |
| **`.data`** | RW- | Başlatılmış yazılabilir global veri | `int g_Counter = 5;` gibi değişkenler |
| **`.rdata`** | R-- | Salt-okunur veri: const, string literal, **import/export tablosu**, RTTI, vtable | String'ler (`"Hello"`), `dd offset func` listeleri |
| **`.bss`** | RW- | Başlatılmamış veri (raw size = 0, virtual size > 0) | Sıfırla başlatılır, disk'te yer kaplamaz |
| **`.idata`** | R-- veya RW- | Import Address Table (IAT) | Genelde MSVC'de `.rdata` içine gömülü, ayrı olunca `KERNEL32.DLL!CreateFileA` pointerları |
| **`.edata`** | R-- | Export table (sadece DLL'ler) | `GetProcAddress` hedefi fonksiyon isim/adres listesi |
| **`.rsrc`** | R-- | Resources — ikon, dialog, string table, manifest, version | IDA'da genelde hex dump; Resource Hacker ile okunur |
| **`.reloc`** | R-- (discardable) | Base relocation table — ASLR olursa adres düzeltmesi | DWORD array'i pointer düzeltme kayıtları |
| **`.tls`** | R-- | Thread Local Storage — `__declspec(thread)` değişkenler + TLS callbacks | `tls_callback` fonksiyonları çalıştırılır — ⚠️ anti-debug sık kullanır |
| **`.pdata`** | R-- | x64 exception handling (RUNTIME_FUNCTION table) | Sadece x64, try/catch unwind bilgisi |
| **`.xdata`** | R-- | x64 unwind info (`.pdata`'nın detayı) | x64 SEH metadata |

## 2. Derleyici/Runtime'a Özgü Sectionlar

### MSVC
| Section | İçerik |
|---|---|
| `.CRT` | C runtime initializer/terminator pointer listeleri (global constructor çağrıları) |
| `.textbss` | Debug build'lerde link-time code generation geçici alan |
| `.gfids` | Control Flow Guard function IDs |
| `.00cfg` | CFG metadata |
| `.didat` | Delay-load imports (lazy-loaded DLL'ler için ayrı IAT) |
| `.drectve` | Linker directives (obj dosyalarında, EXE'de genelde yok) |
| `.msvcjmc` | MSVC Just My Code debug helper |

### GCC / MinGW
| Section | İçerik |
|---|---|
| `.eh_frame` | Exception handling frame bilgisi (DWARF) |
| `.ctors` / `.dtors` | Global constructor/destructor pointer array'leri |
| `.init` / `.fini` | ELF'ten miras — MinGW'de genelde boş |
| `.CRT` | MSVC uyumlu CRT init (MinGW de kullanır) |

### Borland / Delphi
| Section | İçerik |
|---|---|
| `CODE` | `.text` yerine — Borland konvansiyonu |
| `DATA` | `.data` yerine |
| `BSS` | Zero-init data |
| `.itext` / `.idata` | Import tables |

### Debug Sectionları
| Section | İçerik |
|---|---|
| `.debug` / `.debug$S` / `.debug$T` | Debug symbol bilgisi (genelde sadece `.obj`, EXE'de `.pdb` ayrı) |
| `.stab` / `.stabstr` | Eski GCC debug format |

---

## 3. Packer / Protector Sectionları

⚠️ **Bu sectionları gördüğün anda binary paketlenmiş/korumalıdır. Statik analiz sınırlı olur, unpack gerekir.**

### UPX (açık kaynak packer)
| Section | Anlam |
|---|---|
| `UPX0` | Unpacked kodun gideceği boş alan (VirtualSize büyük, RawSize = 0) |
| `UPX1` | Sıkıştırılmış orijinal kod + unpacker stub |
| `UPX2` | Bazen import rebuild tablosu |

**Tespit:** `UPX!` magic string `UPX1` sonunda. `upx -d` ile unpack edilebilir.

### VMProtect
| Section | Anlam |
|---|---|
| `.vmp0` | VM bytecode / virtualized kod |
| `.vmp1` | VM interpreter + mutated original code |
| `.vmp2` | Ek veri / imports |

**Tespit:** `.vmp0/.vmp1` isimleri, çok büyük section'lar, yüksek entropy (>7.5).

### Themida / WinLicense (Oreans)
| Section | Anlam |
|---|---|
| `.themida` | Nadir — genelde **rastgele 5-8 karakterli isim** kullanır |
| Rastgele isimler (örn: `jmpR2Tp`, `b5WqXq2A`) | Themida'nın tipik imzası |
| RWX flagli section | Kod self-modification |

**Tespit:** Rastgele section isimleri + RWX + entropy > 7.9 + `Themida` string import'u veya mutex. Sizin KO.exe = **Themida** (memory'deki protector analizi notu).

### ASPack / ASProtect
| Section | Anlam |
|---|---|
| `.aspack` | ASPack unpacker stub |
| `.adata` | Auxiliary data |
| `.aspr` / `.aspr1` / `.aspr2` | ASProtect |

### PECompact
| Section | `pec1`, `pec2`, `.pec` |

### MPRESS
| Section | `.MPRESS1`, `.MPRESS2` |

### Enigma Protector
| Section | `.enigma1`, `.enigma2` veya rastgele |

### Obsidium
| Section | Rastgele isimler + RWX, entry point gizli |

### Armadillo / SoftwarePassport
| Section | `.text1`, `.data1` gibi sıra-numaralı isimler, yüksek section sayısı |

### EXECryptor
| Section | Rastgele isimler, virtualize edilmiş fonksiyonlar |

### PELock
| Section | `.pelock` veya rastgele, anti-debug yoğun |

---

## 4. Anti-Cheat Sectionları

### Wellbia XIGNCODE / XINGCODE (KO ve diğer Kore oyunlarında)
| Section | Anlam |
|---|---|
| `x3` / `X3` başlayan isimler | XIGNCODE modül yüklemesi imzası |
| Rastgele isimli RWX section | XIGNCODE dinamik kod — anti-debug & memory scan |
| `xigncode3` DLL'in kendi sectionları | `.xc_rt`, `.xc_data` benzeri (runtime) |

**Tespit:** Oyun process'inde `xigncode3/` klasörü + `xhunter1.sys` driver + process'te x3* modülleri.

### nProtect GameGuard (NCSOFT, bazı oyunlar)
| Section | `GG_*` prefixli veya rastgele RWX section, driver tabanlı |

### HackShield (AhnLab)
| Section | `HShield`, `.hs_*` rastgele |

### BattlEye / EasyAntiCheat
| Section | Genelde ring-0 driver'da; user-mode DLL'de özel section nadir |

### VMProtect-tabanlı Anti-Cheat
| Section | `.vmp*` görürsen anti-cheat VMProtect'le paketlenmiş |

---

## 5. Özel / Egzotik Sectionlar

| Section | Nereden gelir |
|---|---|
| `.ndata` | NSIS installer uninstaller data |
| `.ndata0/1` | NSIS compressed payload |
| `.rsrc1` / `.rsrc2` | Manuel eklenmiş resource (genelde icon changer) |
| `.petite` | Petite packer |
| `.Upack` / `.ByDwing` | Upack packer |
| `.yP` / `yoda` | yoda's Protector |
| `.MaskPE` | MaskPE packer |
| `.neolite` / `.neolit` | NeoLite packer |
| `.packed` | Generic packer isim |
| `.securom` | SecuROM DRM (oyunlar) |
| `.sforce3` / `.sforce4` | StarForce DRM |
| `.tsuarch` / `.tsustub` | TheSfxMaker |
| `.winapi` | Manual API hidden import section |
| `.ICE` | ICE packer |

---

## 6. IDA'da Segmentleri Nasıl Doğru Okursun

### Hızlı Workflow
1. **View → Segments** (`Shift+F7`) — tüm segment listesi, R/W/X flagleri yanında
2. Segment ismine çift tıkla → ilgili adrese atlarsın
3. **Strings pencersi** (`Shift+F12`) varsayılan olarak `.rdata`'ya bakar — string bulamıyorsan **"Setup"** deyip *All segments* aç
4. **Imports** (`Ctrl+I`) → IAT sadece `.idata` / `.rdata`'dakileri gösterir; eğer import yok gözüküyorsa **packer unpack'lemiyor demektir**

### Kafa Karıştıran Durumlar

**"Bu adres hangi segmentte?"** → IDA status bar'a bak, açık dosyada cursor üstündeki adres için segment ismi gösterilir.

**"Segment var ama içi boş"** → `Virtual Size > Raw Size` — genelde `.bss` veya unpacker'ın açacağı alan (UPX0 gibi). Runtime'da dolar, static dump'ta boş.

**"Fonksiyon `.text` dışında"** → İki olasılık:
  - Packer unpack'ledi ve kodu başka segmente taşıdı (örn: Themida)
  - Manuel yüklenen shellcode / inject edilmiş DLL

**"RWX gördüm"** → Legitimate binary'de nadirdir. Packer, protector, JIT engine (V8, .NET JIT) veya güvenlik sorunu işareti.

**"Section ismi rastgele görünüyor"** → Themida/VMProtect/Obsidium gibi protector var, IDA statik analizi çok sınırlı kalacak.

### Entropy Kontrolü
IDA'da entropy hesaplayan eklenti yoksa **Detect It Easy (DIE)** veya **CFF Explorer** kullan:
- Entropy < 6.0 → normal kod/veri
- Entropy 6.0-7.2 → sıkıştırılmış/şifrelenmiş olabilir
- Entropy > 7.5 → **muhtemelen paketli/şifreli**
- Entropy > 7.9 → **kesin paketli** (random noise seviyesi)

---

## 7. KnightOnline Özelinde Ne Göreceksin

Sizin KO.exe için memory'deki analiz notuna göre:
- **KO.exe = Themida paketli** → random section isimleri + RWX + entropy ~7.95 → statik disasm imkansız, runtime dump şart
- **REVOLTELAUNCHER = VMP/Wellbia packer** benzeri
- Dump alındıktan sonra göreceğin normal sectionlar (unpacked state'te):
  - `.text` (oyun kodu ~11MB) → `0x00400000` civarı
  - `.rdata` (string'ler, imports) → `.text` sonrasında
  - `.data` (global'ler — `KO_PTR_CHR` vs. burada)
  - `.rsrc` (icon, version info)
  - `.reloc`
  - Olası ek: XIGNCODE yüklenince runtime'da x3* sectionları eklenir

---

## 8. Kısa "Ne Görünce Ne Demek" Özet

| Görürsen | Anlamı |
|---|---|
| `.text`, `.rdata`, `.data`, `.rsrc`, `.reloc` | ✅ Normal binary, statik analiz yapılabilir |
| `UPX0/UPX1` | UPX packed — `upx -d` ile aç |
| `.vmp0/.vmp1` | VMProtect — unpack karmaşık, VM devirtualize gerek |
| Rastgele 5-8 karakter isim + RWX | Themida/Obsidium/EXECryptor — runtime dump şart |
| `x3` prefixli, xigncode3 yanında | XIGNCODE anti-cheat aktif |
| `.aspack`, `.pec1`, `.MPRESS1` | Klasik commercial packer — çoğu için auto-unpacker var |
| `.CRT`, `.tls`, `.gfids`, `.00cfg` | MSVC runtime — endişelenme, normal |
| `.eh_frame` | GCC binary |
| Sadece `CODE`, `DATA`, `BSS` | Borland/Delphi |
| `.ndata` | NSIS installer (inno setup değil) |
| 40+ section | Ya protector iş başında ya aşırı modüler derleme |

---

## Referans İçin Hızlı Komutlar

```cmd
:: Section listesi (VS Developer Prompt'tan)
dumpbin /headers binary.exe | findstr /i "name characteristics"

:: IDA yerine komut satırı
:: objdump (MinGW/WSL)
objdump -h binary.exe
```

**Python ile:**
```python
import pefile
pe = pefile.PE("binary.exe")
for section in pe.sections:
    print(f"{section.Name.decode().strip(chr(0))} "
          f"VA={hex(section.VirtualAddress)} "
          f"VS={hex(section.Misc_VirtualSize)} "
          f"RS={hex(section.SizeOfRawData)} "
          f"chars={hex(section.Characteristics)}")
```
