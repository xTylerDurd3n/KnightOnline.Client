---
name: afkbot_patch
description: KnightOnline afkbot RE ve patch workflow'u — loader.exe/adapter.dll/data.dat zinciri, XOR decrypt, integrity check bypass, PE1/PE2 extraction, re-encrypt. Tetikleyiciler: "afkbot", "adapter.dll", "data.dat", "pe1_extracted", "integrity patch", "xor decrypt", "afkbot crack".
---

# AFKBot — RE & Patch Workflow

## Dosya Yapısı

```
C:\Users\MDORA\Desktop\RE\afkbot\
├── loader.exe           ← ana launcher, adapter.dll'i runtime'da yükler
├── adapter.dll          ← DLL loader zinciri (analiz edildi)
├── adapter.dll.i64      ← IDA database
├── data.dat             ← XOR şifreli, PE1+PE2 gömülü
├── data_decrypted.bin   ← full decrypted blob (header+PE1+PE2+footer)
├── decrypt.py           ← XOR decrypt + PE extraction scripti
├── pe1_extracted.exe    ← PE1 standalone (IDA analizi için)
├── pe1_extracted.exe.i64
├── pe2_extracted.dll    ← PE2 standalone
├── minning/             ← bot config klasörleri
├── resource/
├── routes/
└── settings/
```

---

## data.dat Şifreleme

**Algoritma:** Single-byte XOR  
**Key:** `0xF2`  
**Bulma yöntemi:** Frekans analizi — PE dosyalarında `0x00` en sık byte, şifreli dosyada en baskın byte = key

```python
KEY = 0xF2
decrypted = bytes([b ^ KEY for b in encrypted])
# Re-encrypt için aynı işlem (XOR simetrik)
encrypted = bytes([b ^ KEY for b in decrypted])
```

---

## data_decrypted.bin Yapısı

| Offset | İçerik | Boyut |
|--------|---------|-------|
| `0x000000` | header/config | ~980 KB |
| `0x0EF42F` | **PE1** — asıl bot motoru (VMProtect .vlizer) | 8.31 MB |
| `0x093F201` | **PE2** — helper DLL | 0.29 MB |
| `0x0988201` | footer/index tablosu | ~36 KB |

```python
PE1_OFF  = 0x0EF42F
PE1_SIZE = 0x84FDD2
PE2_OFF  = 0x093F201
PE2_SIZE = 0x49000
```

---

## adapter.dll Yükleme Zinciri

```
loader.exe
  └─ adapter.dll (runtime injection)
       └─ DllMain
            └─ IntegrityThread (CreateThread → hSelfModule geçer)
                 ├─ ComputeMemoryHash   → adapter.dll bellek hash
                 ├─ ReadFileSegments    → adapter.dll disk'ten oku
                 ├─ ComputeFileHash     → adapter.dll disk hash
                 ├─ VerifyIntegrity     ← PATCH NOKTASI
                 │    true  → InitMainPayload
                 │    false → FreeLibraryAndExitThread (sessiz kapanır)
                 └─ InitMainPayload
                      ├─ data.dat parse et (XOR decrypt yok, parse direkt)
                      ├─ "1" key entry → PE1 → VirtualAlloc(RWX) → memmove
                      └─ \1.dat yükle (opsiyonel, yoksa skip)
```

---

## Fonksiyon Haritası (adapter.dll)

| Adres | İsim | Açıklama |
|-------|------|----------|
| `DllMain` | DllMain | Sadece DLL_PROCESS_ATTACH, thread başlatır |
| `StartAddress` | IntegrityThread | Self-integrity check thread |
| `sub_10001160` | ComputeMemoryHash | Bellekteki DLL hash'i, v5/v6 output |
| `sub_10001B20` | ReadFileSegments | DLL'yi diskten 6 chunk'a böler |
| `sub_10001380` | ComputeFileHash | 6 chunk'tan hash hesaplar |
| `sub_10001ED0` | **VerifyIntegrity** | İki hash karşılaştırır, **PATCH NOKTASI** |
| `sub_10001520` | InitMainPayload | data.dat parse, PE1 yükle |
| `sub_10001AC0` | FreeHashContext | Destructor |
| `sub_10002C00` | LoadSecondaryPayload | 7-adım PE loader zinciri |
| `sub_10001C50` | WStringConstruct | MSVC wstring oluştur |
| `sub_10001A40` | ConcatPath | Path birleştir |
| `sub_10002250` | ParseDataFile | data.dat linked list parse |
| `sub_10002FA4` | CustomFree | Custom allocator free |

---

## Integrity Check Bypass

### VerifyIntegrity (`sub_10001ED0`) Analizi
- `__fastcall` — a1=ECX (memory hash), a2=EDX (disk hash)
- İki MSVC std::string nesnesini WORD WORD karşılaştırır
- Eşleşirse `1` (true), değilse `0` döner
- Stack'te arg yok → `ret` yeterli, `ret N` gerekmez

### Patch
```
Adres: 0x10001ED0

Eski bytes:   8B C2 56   (mov eax,edx / push esi)
Yeni bytes:   B0 01 C3   (mov al,1 / ret)
```

**IDA'da uygulama:**
```
1. G → 0x10001ED0
2. Edit → Patch program → Change byte(s)
3. 8B C2 56 → B0 01 C3
4. Edit → Patch program → Apply patches to input file
```

**Graph view açılmaz** — tek blok, dallanma yok, normaldir.  
**Kırmızı dead code** — ret sonrası ulaşılamaz kod, normaldir.

---

## Test Senaryosu

```
adapter.dll patch → loader.exe çalıştır

Açılırsa:
  → Tek integrity check adapter.dll içindeydi ✓
  → loader.exe adapter.dll'e güveniyor
  → data.dat serbestçe değiştirilebilir (check yok)

Açılmazsa:
  → loader.exe de adapter.dll hash'ini kontrol ediyor
  → loader.exe içinde aynı pattern'i ara
```

**Sonuç:** Patch sonrası açıldı — tek katmanlı integrity check.

---

## data.dat Modification Workflow

```
1. pe1_extracted.exe'de patch yap (IDA ile)
2. Patch'li PE1'i data_decrypted.bin'e yaz (offset 0x0EF42F)
3. data_decrypted.bin → XOR 0xF2 → yeni data.dat
```

```python
# Patch'li PE1'i data_decrypted.bin'e yaz
with open("data_decrypted.bin", "rb") as f:
    blob = bytearray(f.read())

with open("pe1_patched.exe", "rb") as f:
    pe1 = f.read()

blob[0x0EF42F : 0x0EF42F + len(pe1)] = pe1

with open("data_decrypted_patched.bin", "wb") as f:
    f.write(blob)

# Re-encrypt
with open("data_decrypted_patched.bin", "rb") as f:
    dec = f.read()

enc = bytes([b ^ 0xF2 for b in dec])

with open("data.dat", "wb") as f:
    f.write(enc)
```

---

## IDA'da Binary File Açma

data_decrypted.bin gibi raw blob'lar için:
```
File → Open → Binary file seç
Processor: x86
Load address: manuel gir (PE1 için 0x10000000)
Sections/imports otomatik gelmez, manuel analiz gerekir
```
PE olarak açmak (pe1_extracted.exe) çok daha pratik — IDA her şeyi otomatik tanır.

---

## VMProtect Notu

PE1 (`pe1_extracted.exe`) VMProtect `.vlizer` section içeriyor:
- Disk'te bytecode → memory'de de bytecode (orijinal x86 hiç olmadı)
- VM Engine uygulamanın içine gömülü, ayrı process değil
- IDA attach etsen de sadece VM dispatcher/handler görürsün
- Virtualize **edilmemiş** fonksiyonlar normal x86 olarak analiz edilebilir
- VTIL/VMAttack ile bytecode → IR lift mümkün ama emek ister
- Pratik alternatif: API hook, network intercept, davranış analizi
