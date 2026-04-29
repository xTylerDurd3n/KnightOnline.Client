---
name: pe-injection
description: Windows PE manuel injection + kod mutasyon motoru referansı — section mapping, base relocation, debug directory silme, kod mutasyonu (JMP cave), ASLR bypass (aynı VA iki proc), API stub inline kopyalama, NtCreateThreadEx. Tetikleyiciler: "pe injection", "manuel loader", "kod mutasyon", "relocation", "NtCreateThreadEx", "remote thread", "shellcode parametre", "API stub", "IAT bypass".
---

# PE Injection + Kod Mutasyon Motoru

`adapter.dll` içindeki `sub_100379B0` fonksiyonunun temiz C++ yeniden yazımı.  
Kaynak: IDA Pro pseudocode → derin analiz → reconstruct.

---

## Genel Akış

```
data.dat (XOR şifreli)
    │
    └─► XOR decrypt → ham PE buffer
                │
                └─► InjectMutated()
                        ├─ 1. PE parse (DOS/NT headers, sections)
                        ├─ 2. VirtualAllocEx (hedef proc + lokal staging)
                        ├─ 3. Section mapping (raw → virtual)
                        ├─ 4. Base relocation uygula + reloc tabloyu sil
                        ├─ 5. Debug directory sil
                        ├─ 6. ASLR bypass: aynı VA iki proc'da tahsis
                        ├─ 7. Fonksiyon mutasyonu (polimorfik JMP cave)
                        ├─ 8. Shellcode param bloğu (ntdll pointer'lar)
                        ├─ 9. API stub inline kopyalama (IAT hook bypass)
                        ├─ 10. WriteProcessMemory (4 adet)
                        └─ 11. NtCreateThreadEx / CreateRemoteThread
```

---

## Yardımcı Yapılar

```cpp
struct PeView {
    BYTE*                    base;
    IMAGE_DOS_HEADER*        dos;
    IMAGE_NT_HEADERS*        nt;
    IMAGE_FILE_HEADER*       file;
    IMAGE_OPTIONAL_HEADER*   opt;
    IMAGE_SECTION_HEADER*    sections;
    IMAGE_DATA_DIRECTORY&    relocs;   // DataDirectory[5] = BaseReloc
    IMAGE_DATA_DIRECTORY&    debug;    // DataDirectory[6] = Debug

    explicit PeView(void* buf)
        : base(static_cast<BYTE*>(buf))
        , dos(reinterpret_cast<IMAGE_DOS_HEADER*>(base))
        , nt(reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew))
        , file(&nt->FileHeader)
        , opt(&nt->OptionalHeader)
        , sections(IMAGE_FIRST_SECTION(nt))
        , relocs(opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC])
        , debug (opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG])
    {}
};

// Remote thread'e geçilen parametre bloğu (24 byte, 6 pointer)
struct LoaderParams {
    void*  remoteBase;               // hedef proc'daki PE base
    void*  shellcodeEnd;
    void*  pLdrGetProcedureAddress;
    void*  pLdrLoadDll;
    void*  pRtlInitUnicodeString;
    void*  pRtlInitAnsiString;
};

// Mutasyona girecek fonksiyon kaydı (a6 tablosu)
struct FunctionEntry {
    DWORD offset;   // PE içindeki RVA
    DWORD size;     // bayt cinsinden boyut
};
```

---

## Ana Fonksiyon

```cpp
int Injector::InjectMutated(
    HANDLE          hProcess,     // hedef: KnightOnLine.exe
    void*           peBuffer,     // data.dat'tan çözülmüş PE
    int             /*unused*/,
    std::wstring&   dllPath,      // yüklenecek DLL yolu
    std::vector<FunctionEntry>& functions  // mutasyona girecek fonksiyonlar
) {
    if (!peBuffer)    return 1;
    if (hProcess == INVALID_HANDLE_VALUE) return 3;

    srand(GetTickCount());

    // ── 1. PE parse ─────────────────────────────────────────────────────────
    PeView pe(peBuffer);
    DWORD  imageSize = pe.opt->SizeOfImage;

    // ── 2. Hedef process'te bellek ayır ─────────────────────────────────────
    //    Düzen: [PE imajı | param bloğu (24 byte) | shellcode | mutasyon alanı]
    size_t shellcodeSize = reinterpret_cast<size_t>(ShellcodeEnd)
                         - reinterpret_cast<size_t>(ShellcodeEntry);

    BYTE* remoteBase = static_cast<BYTE*>(
        VirtualAllocEx(hProcess, nullptr,
                       imageSize + 24 + shellcodeSize + 4096,
                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

    void* remoteParams    = remoteBase + imageSize;
    void* remoteShellcode = remoteBase + imageSize + 24;

    // ── 3. Yerel staging kopyası ─────────────────────────────────────────────
    BYTE* local = static_cast<BYTE*>(
        VirtualAlloc(nullptr, imageSize + 24 + shellcodeSize + 4096,
                     MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

    memset(local, 0, imageSize);

    // DOS + NT header kopyala
    memcpy(local,                    peBuffer, 0x40);
    memcpy(local + pe.dos->e_lfanew, pe.nt,   0xF8);

    // Section'ları dosya offsetinden sanal adrese map et
    for (WORD i = 0; i < pe.file->NumberOfSections; ++i) {
        auto& sec = pe.sections[i];
        if (sec.SizeOfRawData)
            memcpy(local + sec.VirtualAddress,
                   static_cast<BYTE*>(peBuffer) + sec.PointerToRawData,
                   sec.SizeOfRawData);
    }

    // ── 4. Base relocation uygula + tabloyu sil ──────────────────────────────
    if (pe.relocs.Size) {
        LONG_PTR delta = remoteBase - reinterpret_cast<BYTE*>(pe.opt->ImageBase);

        auto* block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                          local + pe.relocs.VirtualAddress);

        while (block->VirtualAddress) {
            WORD* entries = reinterpret_cast<WORD*>(block + 1);
            DWORD count   = (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;

            for (DWORD j = 0; j < count; ++j) {
                if (entries[j] >> 12 == IMAGE_REL_BASED_HIGHLOW) {
                    auto* target = reinterpret_cast<DWORD*>(
                        local + block->VirtualAddress + (entries[j] & 0xFFF));
                    *target += static_cast<DWORD>(delta);
                }
            }

            auto* next = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                             reinterpret_cast<BYTE*>(block) + block->SizeOfBlock);

            // Anti-forensics: reloc bloğunu random byte ile doldur
            for (DWORD k = 0; k < block->SizeOfBlock; ++k)
                reinterpret_cast<BYTE*>(block)[k] = rand() % 255;

            block = next;
        }
    }

    // ── 5. Debug directory sil ───────────────────────────────────────────────
    if (pe.debug.Size) {
        auto* dbg = reinterpret_cast<IMAGE_DEBUG_DIRECTORY*>(
                        local + pe.debug.VirtualAddress);
        DWORD count = pe.debug.Size / sizeof(IMAGE_DEBUG_DIRECTORY);

        for (DWORD i = 0; i < count; ++i) {
            memset(local + dbg[i].PointerToRawData, 0, dbg[i].SizeOfData);
            memset(&dbg[i], 0, sizeof(IMAGE_DEBUG_DIRECTORY));
        }

        pe.debug.VirtualAddress = 0;
        pe.debug.Size           = 0;
    }

    // ── 6. ASLR-farkında rastgele bellek tahsisi ─────────────────────────────
    std::vector<void*> freeRegions;
    ScanFreeRegions(GetCurrentProcess(), hProcess, 0x1000000, freeRegions);

    void* chosenAddr = freeRegions[rand() % freeRegions.size()];
    // 0x10000 (allocation granularity) sınırına hizala
    chosenAddr = reinterpret_cast<void*>(
        (reinterpret_cast<uintptr_t>(chosenAddr) + 0xFFFF) & ~0xFFFFull);

    // AYNI adresi her iki proceste tahsis et — JMP offset'leri geçerli kalır
    BYTE* remoteMutBuf = static_cast<BYTE*>(
        VirtualAllocEx(hProcess, chosenAddr,
                       0x1000000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    BYTE* localMutBuf  = static_cast<BYTE*>(
        VirtualAllocEx(GetCurrentProcess(), chosenAddr,
                       0x1000000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));

    if (remoteMutBuf != localMutBuf)
        return 5;

    // ── 7. Fonksiyon mutasyonu ───────────────────────────────────────────────
    BYTE*  writeCursor = localMutBuf;
    size_t remaining   = 0x1000000;

    for (auto& fn : functions) {
        BYTE* origRemote = remoteBase + fn.offset;
        BYTE* origLocal  = local      + fn.offset;

        // INT3 padding'i (0xCC) randomize et
        for (BYTE* p = origLocal + fn.size; *p == 0xCC; ++p)
            *p = rand() % 255;

        if (fn.size < 5)
            continue;

        MutationEngine engine;
        engine.Analyze(origRemote, origLocal, fn.size);

        DWORD written = 0;
        if (engine.Emit(writeCursor, remaining, &written)) {
            // Orijinal fonksiyonu random byte ile doldur
            for (DWORD i = 0; i < fn.size; ++i)
                origLocal[i] = rand() % 255;

            // JMP → mutated code
            origLocal[0] = 0xE9;
            *reinterpret_cast<DWORD*>(origLocal + 1) =
                static_cast<DWORD>(writeCursor - origRemote - 5);

            writeCursor += written;
            remaining   -= written;
        }
    }

    // ── 8. Shellcode parametre bloğu ─────────────────────────────────────────
    HMODULE ntdll = GetModuleHandleW(L"ntdll");

    LoaderParams params{};
    params.remoteBase               = remoteBase;
    params.shellcodeEnd             = static_cast<BYTE*>(remoteShellcode) + shellcodeSize;
    params.pLdrGetProcedureAddress  = GetProcAddress(ntdll, "LdrGetProcedureAddress");
    params.pLdrLoadDll              = GetProcAddress(ntdll, "LdrLoadDll");
    params.pRtlInitUnicodeString    = GetProcAddress(ntdll, "RtlInitUnicodeString");
    params.pRtlInitAnsiString       = GetProcAddress(ntdll, "RtlInitAnsiString");

    // ── 9. API stub inline kopyalama (IAT hook bypass) ───────────────────────
    //    ZwCreateSection, <func2>, ZwMapViewOfSection ilk 5 byte'ı shellcode'a gömülür.
    //    Şifreli string'ler runtime'da XOR ile çözülüyor (statik analizi engeller):
    //      "ZwCreateSection"    — key 0x06 (LOBYTE 1165057030)
    //      "ZwMapViewOfSection" — key 0x22 ('"', v33[0])
    //      "ntdll.dll"          — key 0x59 ('Y', v137[0])
    struct ApiStubBlock {
        BYTE zwCreateSection[5];
        BYTE func2[5];
        BYTE zwMapViewOfSection[5];
        WCHAR dllPath[1];   // değişken uzunluk
    };

    size_t stubSize = 15 + (dllPath.size() + 1) * sizeof(WCHAR);
    auto*  stub     = static_cast<ApiStubBlock*>(malloc(stubSize));

    auto copyProlog = [](BYTE* dst, HMODULE mod, const char* name) {
        FARPROC fn = GetProcAddress(mod, name);
        memcpy(dst, fn, 5);
    };

    copyProlog(stub->zwCreateSection,    ntdll, "ZwCreateSection");
    copyProlog(stub->func2,              ntdll, "<runtime_decoded>");
    copyProlog(stub->zwMapViewOfSection, ntdll, "ZwMapViewOfSection");
    wcscpy(stub->dllPath, dllPath.c_str());

    // ── 10. WriteProcessMemory ────────────────────────────────────────────────
    WriteProcessMemory(hProcess, remoteMutBuf,   localMutBuf, 0x1000000,      nullptr);
    WriteProcessMemory(hProcess, remoteBase,     local,       imageSize,       nullptr);
    WriteProcessMemory(hProcess, remoteParams,   &params,     sizeof(params),  nullptr);
    WriteProcessMemory(hProcess, remoteShellcode, ShellcodeEntry, shellcodeSize, nullptr);
    WriteProcessMemory(hProcess,
                       static_cast<BYTE*>(remoteShellcode) + shellcodeSize,
                       stub, stubSize, nullptr);

    // ── 11. Remote thread ─────────────────────────────────────────────────────
    //    NtCreateThreadEx tercih edilir — kernel'e doğrudan syscall, AV/AC
    //    hook'larını (XIGNCODE dahil) atlatır.
    typedef NTSTATUS(NTAPI* NtCreateThreadEx_t)(
        HANDLE*, ACCESS_MASK, void*, HANDLE,
        LPTHREAD_START_ROUTINE, void*,
        ULONG, ULONG_PTR, SIZE_T, SIZE_T, void*);

    auto pfnNtCreateThreadEx = reinterpret_cast<NtCreateThreadEx_t>(
        GetProcAddress(ntdll, "NtCreateThreadEx"));

    HANDLE hThread = nullptr;

    if (pfnNtCreateThreadEx) {
        pfnNtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, nullptr, hProcess,
                            reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteShellcode),
                            remoteParams, 0, 0, 0, 0, nullptr);
    } else {
        DWORD tid;
        hThread = CreateRemoteThread(hProcess, nullptr, 0,
                      reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteShellcode),
                      remoteParams, 0, &tid);
    }

    if (hThread) CloseHandle(hThread);

    free(stub);
    VirtualFree(local, 0, MEM_RELEASE);
    return 0;   // başarı
}
```

---

## Güvenlik Teknikleri Özeti

| Teknik | Nerede | Amaç |
|---|---|---|
| Reloc tabloyu random ile doldur | Faz 4 | Dump analizi engelle |
| Debug directory sıfırla | Faz 5 | Sembol/PDB tespiti engelle |
| INT3 padding randomize | Faz 7 | Bytecode pattern match bypass |
| Aynı VA iki proc'da tahsis | Faz 6 | ASLR etkisizleştir |
| Kod mutasyonu (JMP cave) | Faz 7 | XIGNCODE integrity check bypass |
| NtCreateThreadEx | Faz 11 | CreateRemoteThread tespit bypass |
| Runtime XOR string decrypt | Faz 9 | Static IAT string tarama bypass |
| API stub inline kopyalama | Faz 9 | IAT/EAT hook bypass |

## Şifreli String'ler (IDA'dan çözüldü)

| Değişken | Encrypted | Key | Decoded |
|---|---|---|---|
| `v124/v125` | `1165057030` + `"tcgrcUceroih"` | `0x06` | `ZwCreateSection` |
| `v33/v137` | `"\"xUoCRtKGUmDqGAVKML"` | `0x22` | `ZwMapViewOfSection` |
| `v137/v183` | `"Y7-=55w=55"` | `0x59` | `ntdll.dll` |

## IDA Pseudocode → Gerçek C++ Eşleşmesi

| IDA | C++ |
|---|---|
| `v193 = a3 + a3[15]` | `pe.nt = (NT_HEADERS*)(base + dos->e_lfanew)` |
| `v208[7]` | `pe.opt->ImageBase` |
| `v208[35]` | `pe.relocs.Size` |
| `v208[34]` | `pe.relocs.VirtualAddress` |
| `v197 += 10` (section loop) | `for (WORD i = 0; i < numSections; i++)` |
| 200+ yerel değişken | 5-6 struct field |
| `v211 = 4`, `v211 = 7`... | `return error_code` |
