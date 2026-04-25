// dllmain.cpp
// Injection noktasi: REVOLTELAUNCHER/GDIHelper.cpp InjectDLL()
//   CreateProcessA("KnightOnLine.exe") -> CreateRemoteThread(LoadLibraryA, "REVOLTEACS.dll")
//   -> Sleep(2000) -> ResumeThread()
//   -> DllMain(DLL_PROCESS_ATTACH) -> REVOLTEACSLoad() -> REVOLTEACSRemap()
#include "pch.h"

PacketHandler g_PacketHandler;
CUIManager g_UIManager;
CGameHooks g_GameHooks;

// ============================================================================
// RevLog — Console + C:\REVOLTEACS.log + OutputDebugStringA
// ============================================================================
static CRITICAL_SECTION g_LogCs;
static BOOL g_LogInit = FALSE;
static HANDLE g_hConsole = INVALID_HANDLE_VALUE;



static void InitConsole()
{
	AllocConsole();
	SetConsoleTitleA("REVOLTEACS Debug");
	g_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// console buffer buyut — kayip olmasin
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(g_hConsole, &csbi))
	{
		COORD size = {csbi.dwSize.X, 3000};
		SetConsoleScreenBufferSize(g_hConsole, size);
	}
}

void RevLog(const char *fmt, ...)
{
	if (!g_LogInit)
	{
		InitializeCriticalSection(&g_LogCs);
		g_LogInit = TRUE;
	}
	EnterCriticalSection(&g_LogCs);

	char msg[1024];
	SYSTEMTIME st;
	GetLocalTime(&st);
	int hdr = sprintf_s(msg, sizeof(msg),
						"[%02d:%02d:%02d.%03d T=%05lu] ",
						st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
						GetCurrentThreadId());

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg + hdr, sizeof(msg) - hdr - 2, fmt, ap);
	va_end(ap);
	strcat_s(msg, sizeof(msg), "\n");

	// Console
	if (g_hConsole != INVALID_HANDLE_VALUE)
	{
		DWORD written;
		WriteConsoleA(g_hConsole, msg, (DWORD)strlen(msg), &written, NULL);
	}

	// Dosya
	FILE *fp = nullptr;
	if (fopen_s(&fp, "C:\\REVOLTEACS.log", "a") == 0 && fp)
	{
		fputs(msg, fp);
		fflush(fp);
		fclose(fp);
	}

	OutputDebugStringA(msg);
	LeaveCriticalSection(&g_LogCs);
}

static void LogLoadedModules(const char *tag)
{
	HMODULE hMods[256];
	DWORD cbNeeded;
	if (!EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded))
	{
		RevLog("modules[%s]: EnumProcessModules failed err=%lu", tag, GetLastError());
		return;
	}
	DWORD count = cbNeeded / sizeof(HMODULE);
	RevLog("modules[%s]: %lu DLL yuklendi", tag, count);
	for (DWORD i = 0; i < count; i++)
	{
		char name[MAX_PATH];
		if (GetModuleFileNameExA(GetCurrentProcess(), hMods[i], name, MAX_PATH))
			RevLog("  [%02lu] base=%p  %s", i, hMods[i], name);
	}
}

// ============================================================================
// REVOLTEACSRemapProcess — Themida unpack bittikten SONRA cagrilmali
// Oyunun 3 kod bolgesini PAGE_EXECUTE_READWRITE olarak remap eder:
//   - XIGNCODE'un sayfa koruma taramalari bypass edilir
//   - ApplyPatches sonraki WritePatch cagrilerinde VirtualProtect gerekmez
//   - ZwUnmapViewOfSection + ZwMapViewOfSection: yeni anonymous section, orijinal baytlar kopyalanir
//
// Bolgeler (CLAUDE.md'den):
//   0x00400000 - 11.25MB  — ana kod + data section
//   0x00F30000 -  1.5MB   — XIGNCODE code region
//   0x01060000 -   64KB   — ek stub region
// ============================================================================
static void REVOLTEACSRemapProcess(HANDLE hProcess)
{
	BYTE *imageBase = (BYTE *)GetModuleHandleA(NULL);
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)imageBase;
	PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)(imageBase + dos->e_lfanew);
	SIZE_T imageSize      = nt->OptionalHeader.SizeOfImage;

	RevLog("remap: KO.exe @ %p size=0x%X", imageBase, (DWORD)imageSize);

	// 1. Copy buffer — VirtualAlloc sifirlar, NOACCESS sayfalar sifir kalir
	BYTE *copyBuf = (BYTE *)VirtualAlloc(NULL, imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!copyBuf) { RevLog("remap: VirtualAlloc failed"); return; }

	// 2. Sayfa sayfa oku — NOACCESS / uncommitted sayfaları atla, offsetlerini kaydet
	DWORD naccessPages[8192];
	DWORD naccessCount = 0;
	DWORD readOk = 0, readSkip = 0;

	for (SIZE_T off = 0; off < imageSize; off += 0x1000)
	{
		BYTE *page = imageBase + off;
		MEMORY_BASIC_INFORMATION mbi = {};
		VirtualQueryEx(hProcess, page, &mbi, sizeof(mbi));

		bool accessible = (mbi.State == MEM_COMMIT) &&
						  (mbi.Protect != PAGE_NOACCESS) &&
						  (mbi.Protect != 0);

		if (!accessible)
		{
			if (naccessCount < 8192) naccessPages[naccessCount++] = (DWORD)off;
			readSkip++;
			continue;
		}

		ReadProcessMemory(hProcess, page, copyBuf + off, 0x1000, NULL);
		readOk++;
	}
	RevLog("remap: read — %lu OK, %lu NOACCESS/skipped", readOk, readSkip);

	// 3. ZwCreateSection
	HANDLE hSection = NULL;
	LARGE_INTEGER secSize = {}; secSize.QuadPart = (LONGLONG)imageSize;
	NTSTATUS r1 = ZwCreateSection(&hSection, SECTION_ALL_ACCESS, NULL, &secSize, PAGE_EXECUTE_READWRITE, SEC_COMMIT, NULL);
	if (r1) { RevLog("remap: ZwCreateSection FAILED 0x%08lX", r1); VirtualFree(copyBuf, 0, MEM_RELEASE); return; }

	// 4. ZwUnmapViewOfSection — buradan sonra image kaybolur
	NTSTATUS r2 = ZwUnmapViewOfSection(hProcess, imageBase);
	if (r2) { RevLog("remap: ZwUnmapViewOfSection FAILED 0x%08lX", r2); CloseHandle(hSection); VirtualFree(copyBuf, 0, MEM_RELEASE); return; }

	// 5. ZwMapViewOfSection — ayni adrese yeni RWX section
	PVOID viewBase = imageBase;
	LARGE_INTEGER secOff = {}; SIZE_T viewSize = 0;
	NTSTATUS r3 = ZwMapViewOfSection(hSection, hProcess, &viewBase, 0, imageSize, &secOff, &viewSize, ViewUnmap, 0, PAGE_EXECUTE_READWRITE);
	if (r3) { RevLog("remap: ZwMapViewOfSection FAILED 0x%08lX — IMAGE LOST", r3); CloseHandle(hSection); VirtualFree(copyBuf, 0, MEM_RELEASE); return; }

	// 6. Icerigi geri yaz
	SIZE_T written = 0;
	WriteProcessMemory(hProcess, viewBase, copyBuf, viewSize, &written);
	CloseHandle(hSection);
	VirtualFree(copyBuf, 0, MEM_RELEASE);

	// 7. NOACCESS sayfalarini geri koru — Themida VEH'i bu sayfalara bagimli
	for (DWORD i = 0; i < naccessCount; i++)
	{
		DWORD oldProt;
		VirtualProtect((BYTE *)viewBase + naccessPages[i], 0x1000, PAGE_NOACCESS, &oldProt);
	}

	RevLog("remap: KO.exe OK — %lu RWX, %lu NOACCESS restored, written=0x%X",
		   readOk, naccessCount, (DWORD)written);
}



static void HookNtQSI();

// ============================================================================
// ============================================================================
// WritePatch — adrese byte dizisi yazar, VirtualProtect ile RWX yapar
// ============================================================================
// expected: patch oncesi bu adreste olmasi gereken byte'lar (NULL = kontrol etme)
static void WritePatch(DWORD addr, const BYTE *bytes, SIZE_T len,
					   const BYTE *expected = nullptr, SIZE_T expectedLen = 0)
{
	uintptr_t gameBase = (uintptr_t)GetModuleHandleA(NULL);
	uintptr_t rva = addr - gameBase;

	// 1. Pre-patch: beklenen byte'lar varsa kontrol et
	if (expected && expectedLen > 0)
	{
		char got[64] = {}, exp[64] = {};
		bool match = true;
		for (SIZE_T i = 0; i < expectedLen; i++)
		{
			BYTE cur = *(BYTE *)(addr + i);
			sprintf(got + strlen(got), "%02X ", cur);
			sprintf(exp + strlen(exp), "%02X ", expected[i]);
			if (cur != expected[i])
				match = false;
		}
		if (!match)
		{
			RevLog("patch: WRONG LOCATION @ IDA:0x%p — expected [%s] got [%s], SKIPPED",
				   (void *)(0x400000 + rva), exp, got);
			return;
		}
	}

	DWORD oldProt;
	if (!VirtualProtect((LPVOID)addr, len, PAGE_EXECUTE_READWRITE, &oldProt))
	{
		RevLog("patch: VirtualProtect failed @ IDA:0x%p (err=%lu)", (void *)(0x400000 + rva), GetLastError());
		return;
	}
	memcpy((void *)addr, bytes, len);
	VirtualProtect((LPVOID)addr, len, oldProt, &oldProt);

	// 2. Post-patch: yazdıklarımızı readback ile doğrula
	char written[64] = {}, readback[64] = {};
	bool ok = true;
	for (SIZE_T i = 0; i < len; i++)
	{
		BYTE cur = *(BYTE *)(addr + i);
		sprintf(written + strlen(written), "%02X ", bytes[i]);
		sprintf(readback + strlen(readback), "%02X ", cur);
		if (cur != bytes[i])
			ok = false;
	}
	if (ok)
		RevLog("patch: OK @ VA:0x%p (IDA:0x%p) [%s]", (void *)addr, (void *)(0x400000 + rva), written);
	else
		RevLog("patch: READBACK MISMATCH @ IDA:0x%p — wrote [%s] but got [%s]",
			   (void *)(0x400000 + rva), written, readback);
}

// ============================================================================
// ApplyPatches — Themida unpack bittikten sonra uygulanan tum memory patch'ler
//
// sub_E73C73 içinde loc_E73CD5 (0x00E73CD5) adresinden başlayan __try bloğu
// sub_E9263B'yi (XIGNCODE init) çağırıyor.
//
// Patch: 0x00E73CD5'e JMP 0x00E73D20 yaz → init çağrısını atla, başarı path'ine git.
//
// Offset hesabı:
//   Hedef (0x00E73D20) - (Patch adresi (0x00E73CD5) + 5) = 0x46
//   CPU: IP = 0x00E73CD5 + 5 = 0x00E73CDA, sonra +0x46 = 0x00E73D20
// ============================================================================
static void ApplyPatches()
{
	// Patch 2: XIGNCODE CRC32 scanner bypass — jnz NOP, scanner her zaman "OK" doner
	BYTE patch2[] = {0x90, 0x90};
	WritePatch(0x005753C6, patch2, sizeof(patch2));
	RevLog("xigncode: Patch 2 (CRC scanner NOP @ 0x005753C6) aktif");

	// Patch 7: xign_init_sub_50A400 entry → JMP loc_50A673 (xor eax,eax; retn 8)
	// Offset: 0x50A673 - (0x50A400 + 5) = 0x26E
	BYTE patch7_expected[] = { 0x83, 0xEC, 0x5C, 0x8B, 0x44 };
	BYTE patch7[] = { 0xE9, 0x6E, 0x02, 0x00, 0x00 };
	WritePatch(0x0050A400, patch7, sizeof(patch7), patch7_expected, sizeof(patch7_expected));
	RevLog("xigncode: Patch 7 (JMP @ xign_init_sub_50A400 -> loc_50A673) aktif");
}

// ============================================================================
// XIGNCODE Dispatcher Bypass
//
// XIGN_SLOT_DISPATCHER   (0x00F661D0)  — ana SDK dispatcher slot'u.
// XIGN_SLOT_TBL_VALIDATE (0x00F6654C)  — TBL integrity validation slot'u.
// Adresler framework.h'ta tanimli (VA, ASLR yok).
//
// XIGNCODE SDK init bu slotlara gercek check fn adresini yaziyor.
// Watchdog her 100ms'de ikisini de kontrol edip NopHandler'a geri yazar.
//
// NopHandler: mov eax, 1; ret 4  (__stdcall 1-arg)
//   eax=1 → TBL validation cagrilari non-zero bekliyor → gecer.
//   Protection check call-site'lari icin ayrica 0x0050BAD5 byte patch gerekiyor
//   (jz -> jmp, 0x74 -> 0xEB) — Themida unpack bitmeden yapilmamali.
// ============================================================================

// --- F661D0: Ana dispatcher hook ---
// Orijinal HICBIR ZAMAN cagrilmaz — orijinal cagrilirsa XIGNCODE scan yapip kill eder.
// Direkt return 1, caller + arg loglanir.
typedef int(__stdcall *tXignCheck)(DWORD arg);
static tXignCheck oXignCheck = nullptr;

static int __stdcall hkXignCheck(DWORD arg)
{
	static int callCount = 0;
	if (callCount < 50)
	{
		void *retAddr = _ReturnAddress();

		char argDump[64] = "(unreadable)";
		if (!IsBadReadPtr((void*)arg, 16))
			sprintf_s(argDump, sizeof(argDump), "%08X %08X %08X %08X",
				*(DWORD*)(arg+0), *(DWORD*)(arg+4),
				*(DWORD*)(arg+8), *(DWORD*)(arg+12));

		RevLog("xign: F661D0 [%d] caller=%p arg=0x%08lX dump=[%s]",
			callCount, retAddr, arg, argDump);
		callCount++;
	}
	return 1;
}

DWORD WINAPI XignDispatcherWatchdog(LPVOID)
{
	RevLog("watchdog: started, waiting for F661D0 slot (Unpack Event)...");

	void *lastAddressInSlot = nullptr;
	bool patchesApplied = false;
	DWORD tick = 0;

	while (true)
	{
		__try
		{
			// 1. Slotun icindeki adresi kontrol et
			void **pSlot = (void **)XIGN_SLOT_DISPATCHER;
			void *curFuncAddr = *pSlot;

			// 2. UNPACK GERÇEKLEŞTİ Mİ?
			// Slot dolduysa (nullptr degilse) ve henüz biz müdahale etmediysek
			if (curFuncAddr != nullptr && !patchesApplied)
			{
				RevLog("watchdog: UNPACK DETECTED! F661D0 contains: %p", curFuncAddr);
				// LogLoadedModules("unpack");

				// --- A) SECTION REMAP — Themida bitti, artik guvenli ---
				// DEVRE DISI: watchdog tetiklendiginde oyun zaten caliyor,
				// ZwUnmapViewOfSection aktif thread'leri crash ettiriyor.
				// WritePatch/DetourFunction kendi VirtualProtect'ini kullaniyor.
				// HANDLE hSelfProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
				// if (hSelfProc)
				// {
				// 	REVOLTEACSRemapProcess(hSelfProc);
				// 	CloseHandle(hSelfProc);
				// }
				// else
				// {
				// 	RevLog("watchdog: OpenProcess failed for remap (err=%lu)", GetLastError());
				// }

				// --- B) DETOUR HOOK: F661D0 (ana dispatcher) ---
				oXignCheck = (tXignCheck)DetourFunction((PBYTE)curFuncAddr, (PBYTE)hkXignCheck);
				RevLog("hook: F661D0 dispatcher @ %p -> hkXignCheck, trampoline=%p", curFuncAddr, oXignCheck);

				// --- C) XIGNCODE PATCH ---
				ApplyPatches();
				patchesApplied = true;

				// --- D) HANDLE SCAN GİZLEME ---
				HookNtQSI();

				// --- E) GAME HOOKS ---
				g_GameHooks.InitAllHooks(GetCurrentProcess());

				// --- F) PACKET HOOKS — aktif etmek icin yorumu kaldir ---
				// CreateThread(NULL, 0, [](LPVOID) -> DWORD {
				// 	Sleep(5000);
				// 	g_PacketHandler.InitSendHook();
				// 	g_PacketHandler.InitRecvHook();
				// 	RevLog("hook: Send/Recv aktif (SND=0x%08X RECV=0x%08X)", KO_SND_FNC, KO_RECV_FNC);
				// 	return 0;
				// }, NULL, 0, NULL);

				BYTE check[6];
				memcpy(check, (void *)0x00459B64, 6);
				RevLog("Verify Patch 3 at 0x00459B64: %02X %02X %02X %02X %02X %02X",
					   check[0], check[1], check[2], check[3], check[4], check[5]);
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			// Bellek sayfasi henüz hazir degilse (PAGE_NOACCESS gibi) buraya duser.
			// Sessizce beklemeye devam ediyoruz.
		}

		// if ((++tick % 100) == 0)
		// 	RevLog("watchdog: alive, still waiting/monitoring...");
		++tick;

		Sleep(1); // 100ms bekleme (Themida'yi uyandirmamak icin ideal süre)
	}
	return 0;
}

// ============================================================================
// NtQuerySystemInformation hook — dış araç handle scan gizleme
//
// XIGNCODE periyodik olarak SystemHandleInformation (cls=16) çağırır.
// Tüm sistem handle'larını listeler, game process'ine dışarıdan açılmış
// handle'ları (CE, x32dbg vb.) tespit eder ve kill eder.
//
// Hook: sonuçlardan game process'ine ait dış handle'ları filtrele.
// ============================================================================
typedef struct {
	USHORT UniqueProcessId;
	USHORT CreatorBackTraceIndex;
	UCHAR  ObjectTypeIndex;
	UCHAR  HandleAttributes;
	USHORT HandleValue;
	PVOID  Object;
	ULONG  GrantedAccess;
} SYS_HANDLE_ENTRY;

typedef struct {
	ULONG          NumberOfHandles;
	SYS_HANDLE_ENTRY Handles[1];
} SYS_HANDLE_INFO;

typedef NTSTATUS(NTAPI *tNtQSI)(ULONG, PVOID, ULONG, PULONG);
static tNtQSI oNtQSI = nullptr;
static PVOID  s_GameProcessObject = nullptr;

static NTSTATUS NTAPI hkNtQSI(ULONG cls, PVOID buf, ULONG len, PULONG retLen)
{
	NTSTATUS st = oNtQSI(cls, buf, len, retLen);

	static int callCount = 0;
	if (cls == 16 && callCount < 10)
	{
		RevLog("NtQSI: SystemHandleInformation cagrisi [%d] caller=%p st=0x%08lX",
			callCount, _ReturnAddress(), (ULONG)st);
		callCount++;
	}

	if (st != 0 || cls != 16 || !buf)
		return st;

	SYS_HANDLE_INFO *info = (SYS_HANDLE_INFO *)buf;
	DWORD myPid = GetCurrentProcessId();

	// İlk çağrıda game process'in kernel object pointer'ını bul
	if (!s_GameProcessObject)
	{
		HANDLE hSelf = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, myPid);
		if (hSelf)
		{
			USHORT selfVal = (USHORT)(ULONG_PTR)hSelf;
			for (ULONG i = 0; i < info->NumberOfHandles; i++)
			{
				if (info->Handles[i].UniqueProcessId == (USHORT)myPid &&
					info->Handles[i].HandleValue == selfVal)
				{
					s_GameProcessObject = info->Handles[i].Object;
					RevLog("NtQSI: game process kernel object = %p", s_GameProcessObject);
					break;
				}
			}
			CloseHandle(hSelf);
		}
	}

	if (!s_GameProcessObject)
		return st;

	// Game process'e dışarıdan açılmış handle'ları filtrele
	ULONG writeIdx = 0, removed = 0;
	for (ULONG i = 0; i < info->NumberOfHandles; i++)
	{
		bool external   = (info->Handles[i].UniqueProcessId != (USHORT)myPid);
		bool pointsToUs = (info->Handles[i].Object == s_GameProcessObject);

		if (external && pointsToUs)
		{
			RevLog("NtQSI: FILTERED external handle — pid=%u handle=0x%X access=0x%08lX",
				info->Handles[i].UniqueProcessId,
				info->Handles[i].HandleValue,
				info->Handles[i].GrantedAccess);
			removed++;
			continue;
		}
		info->Handles[writeIdx++] = info->Handles[i];
	}

	if (removed > 0)
	{
		info->NumberOfHandles = writeIdx;
		RevLog("NtQSI: %lu dis handle gizlendi", removed);
	}

	return st;
}

static void HookNtQSI()
{
	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	if (!hNtdll) return;

	oNtQSI = (tNtQSI)GetProcAddress(hNtdll, "NtQuerySystemInformation");
	if (oNtQSI)
		oNtQSI = (tNtQSI)DetourFunction((PBYTE)oNtQSI, (PBYTE)hkNtQSI);

	RevLog("hook: NtQuerySystemInformation hooked");
}



// ============================================================================
// CreateWindowExA/W hook — XIGNCODE pencere tespiti
//
// XIGNCODE her seferinde random caption urettigi icin string search calismaz.
// Tum window olusumlarini loglayip class + caption'i tespit ediyoruz.
// Tespit sonrasi: NULL dondurup pencereyi engelleyecegiz.
// ============================================================================
typedef HWND(WINAPI *tCreateWindowExA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef HWND(WINAPI *tCreateWindowExW)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);

static tCreateWindowExA oCreateWindowExA = nullptr;
static tCreateWindowExW oCreateWindowExW = nullptr;

// XIGNCODE pencere tespiti icin kullanilan kriterler:
//   1. Class adi — log'dan ogrenince buraya yazilacak (TODO)
//   2. Caption uzunlugu — XIGNCODE random 40+ karakter uretir, oyun pencereleri oyle yapmaz
static bool IsXigncodeWindow(LPCSTR lpClass, LPCSTR lpCaption)
{
	// Kriter 1: class adi eslesmesi (log'dan ogrenince aktif edilecek)
	// if (lpClass && strcmp(lpClass, "BURAYA_CLASS_ADI") == 0) return true;

	// Kriter 2: caption 32+ karakter ve sadece alfanumerik → XIGNCODE random string
	if (lpCaption)
	{
		size_t len = strlen(lpCaption);
		if (len >= 32)
		{
			bool allAlnum = true;
			for (size_t i = 0; i < len; i++)
				if (!isalnum((unsigned char)lpCaption[i]))
				{
					allAlnum = false;
					break;
				}
			if (allAlnum)
				return true;
		}
	}
	return false;
}

static HWND WINAPI hkCreateWindowExA(
	DWORD dwExStyle, LPCSTR lpClass, LPCSTR lpCaption,
	DWORD dwStyle, int X, int Y, int W, int H,
	HWND hParent, HMENU hMenu, HINSTANCE hInst, LPVOID lpParam)
{
	RevLog("CreateWindowExA: class='%s' caption='%s' style=0x%08lX exstyle=0x%08lX",
		   lpClass ? lpClass : "(null)",
		   lpCaption ? lpCaption : "(null)",
		   dwStyle, dwExStyle);

	if (IsXigncodeWindow(lpClass, lpCaption))
	{
		RevLog("xigncode: window BLOCKED (class='%s')", lpClass ? lpClass : "(null)");
		return NULL;
	}

	return oCreateWindowExA(dwExStyle, lpClass, lpCaption,
							dwStyle, X, Y, W, H, hParent, hMenu, hInst, lpParam);
}

static HWND WINAPI hkCreateWindowExW(
	DWORD dwExStyle, LPCWSTR lpClass, LPCWSTR lpCaption,
	DWORD dwStyle, int X, int Y, int W, int H,
	HWND hParent, HMENU hMenu, HINSTANCE hInst, LPVOID lpParam)
{
	char classA[128] = "(null)";
	char captionA[256] = "(null)";
	if (lpClass)
		WideCharToMultiByte(CP_ACP, 0, lpClass, -1, classA, sizeof(classA), NULL, NULL);
	if (lpCaption)
		WideCharToMultiByte(CP_ACP, 0, lpCaption, -1, captionA, sizeof(captionA), NULL, NULL);

	RevLog("CreateWindowExW: class='%s' caption='%s' style=0x%08lX exstyle=0x%08lX",
		   classA, captionA, dwStyle, dwExStyle);

	if (IsXigncodeWindow(classA, captionA))
	{
		RevLog("xigncode: window BLOCKED (class='%s')", classA);
		return NULL;
	}

	return oCreateWindowExW(dwExStyle, lpClass, lpCaption,
							dwStyle, X, Y, W, H, hParent, hMenu, hInst, lpParam);
}

// ============================================================================
// ShellExecuteA/W + ShellExecuteExA/W hook — xxd-0.xem kim baslatıyor?
// Sadece log, bloklama yok. Caller adresi = patch yapilacak yer.
// ============================================================================
typedef HINSTANCE(WINAPI *tShellExA)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);
typedef HINSTANCE(WINAPI *tShellExW)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
typedef BOOL(WINAPI *tShellExExA)(SHELLEXECUTEINFOA*);
typedef BOOL(WINAPI *tShellExExW)(SHELLEXECUTEINFOW*);

static tShellExA  oShellExA  = nullptr;
static tShellExW  oShellExW  = nullptr;
static tShellExExA oShellExExA = nullptr;
static tShellExExW oShellExExW = nullptr;

static HINSTANCE WINAPI hkShellExA(HWND h, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir, INT show)
{
	RevLog("ShellExecuteA: file='%s' params='%s' caller=%p", file?file:"(null)", params?params:"(null)", _ReturnAddress());
	return oShellExA(h, op, file, params, dir, show);
}
static HINSTANCE WINAPI hkShellExW(HWND h, LPCWSTR op, LPCWSTR file, LPCWSTR params, LPCWSTR dir, INT show)
{
	char fileA[512]="(null)", paramsA[256]="(null)";
	if (file)   WideCharToMultiByte(CP_ACP,0,file,-1,fileA,sizeof(fileA),NULL,NULL);
	if (params) WideCharToMultiByte(CP_ACP,0,params,-1,paramsA,sizeof(paramsA),NULL,NULL);
	RevLog("ShellExecuteW: file='%s' params='%s' caller=%p", fileA, paramsA, _ReturnAddress());
	return oShellExW(h, op, file, params, dir, show);
}
static BOOL WINAPI hkShellExExA(SHELLEXECUTEINFOA* pei)
{
	RevLog("ShellExecuteExA: file='%s' params='%s' caller=%p",
		pei&&pei->lpFile?pei->lpFile:"(null)", pei&&pei->lpParameters?pei->lpParameters:"(null)", _ReturnAddress());
	return oShellExExA(pei);
}
static BOOL WINAPI hkShellExExW(SHELLEXECUTEINFOW* pei)
{
	char fileA[512]="(null)", paramsA[256]="(null)";
	if (pei && pei->lpFile)       WideCharToMultiByte(CP_ACP,0,pei->lpFile,-1,fileA,sizeof(fileA),NULL,NULL);
	if (pei && pei->lpParameters) WideCharToMultiByte(CP_ACP,0,pei->lpParameters,-1,paramsA,sizeof(paramsA),NULL,NULL);
	RevLog("ShellExecuteExW: file='%s' params='%s' caller=%p", fileA, paramsA, _ReturnAddress());
	return oShellExExW(pei);
}

static void HookShellExecute()
{
	HMODULE hShell = LoadLibraryA("shell32.dll");
	if (!hShell) { RevLog("ShellExecute hook: shell32 yok"); return; }
	oShellExA   = (tShellExA)  GetProcAddress(hShell, "ShellExecuteA");
	oShellExW   = (tShellExW)  GetProcAddress(hShell, "ShellExecuteW");
	oShellExExA = (tShellExExA)GetProcAddress(hShell, "ShellExecuteExA");
	oShellExExW = (tShellExExW)GetProcAddress(hShell, "ShellExecuteExW");
	if (oShellExA)   oShellExA   = (tShellExA)  DetourFunction((PBYTE)oShellExA,   (PBYTE)hkShellExA);
	if (oShellExW)   oShellExW   = (tShellExW)  DetourFunction((PBYTE)oShellExW,   (PBYTE)hkShellExW);
	if (oShellExExA) oShellExExA = (tShellExExA)DetourFunction((PBYTE)oShellExExA, (PBYTE)hkShellExExA);
	if (oShellExExW) oShellExExW = (tShellExExW)DetourFunction((PBYTE)oShellExExW, (PBYTE)hkShellExExW);
	RevLog("hook: ShellExecuteA/W + ShellExecuteExA/W hooked (log only)");
}

static void HookCreateWindow()
{
	HMODULE hUser32 = GetModuleHandleA("user32.dll");
	if (!hUser32)
	{
		RevLog("hook: user32.dll not found");
		return;
	}

	oCreateWindowExA = (tCreateWindowExA)GetProcAddress(hUser32, "CreateWindowExA");
	oCreateWindowExW = (tCreateWindowExW)GetProcAddress(hUser32, "CreateWindowExW");

	if (oCreateWindowExA)
		oCreateWindowExA = (tCreateWindowExA)DetourFunction((PBYTE)oCreateWindowExA, (PBYTE)hkCreateWindowExA);
	if (oCreateWindowExW)
		oCreateWindowExW = (tCreateWindowExW)DetourFunction((PBYTE)oCreateWindowExW, (PBYTE)hkCreateWindowExW);

	RevLog("hook: CreateWindowExA/W hooked");
}

// ============================================================================
// MessageBoxA/W hook — bos dialog tespiti
// Oyunun actigi dialog'larin text + tip bilgisini logla
// ============================================================================
typedef int(WINAPI *tMessageBoxA)(HWND, LPCSTR, LPCSTR, UINT);
typedef int(WINAPI *tMessageBoxW)(HWND, LPCWSTR, LPCWSTR, UINT);

static tMessageBoxA oMessageBoxA = nullptr;
static tMessageBoxW oMessageBoxW = nullptr;

// Bos text + bos caption → launcher check dialog → otomatik cevap don, gostermeden gec
static int AutoReply(UINT uType)
{
	if (uType & MB_YESNO)
		return IDYES;
	if (uType & MB_OKCANCEL)
		return IDOK;
	return IDOK;
}

static bool IsEmptyDialog(LPCSTR lpText, LPCSTR lpCaption)
{
	bool textEmpty = (!lpText || lpText[0] == '\0');
	bool captionEmpty = (!lpCaption || lpCaption[0] == '\0');
	return textEmpty && captionEmpty;
}

static int WINAPI hkMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	RevLog("MessageBoxA: caption='%s' text='%s' type=0x%X",
		   lpCaption ? lpCaption : "(null)",
		   lpText ? lpText : "(null)",
		   uType);

	void *retAddr = _ReturnAddress();

	// Detaylı loglama
	RevLog("MessageBoxA yakalandi!");
	RevLog("Cagiran Adres: %p", retAddr);
	RevLog("Baslik: %s | Metin: %s", lpCaption ? lpCaption : "null", lpText ? lpText : "null");

	if (IsEmptyDialog(lpText, lpCaption))
	{
		int reply = AutoReply(uType);
		// Race condition fix: watchdog'un tum patch'leri uygulamasi icin bekle.
		// Aninda donersek T=05728 0x00E11900 icinde kosarken patch[4] henuz hazir olmaz.
		// 2s bekleyince watchdog (<100ms toplam) tamamlanmis olur.
		RevLog("MessageBoxA: BLOCKED (empty dialog) -> sleeping 2s for patch window...");
		Sleep(2000);
		RevLog("MessageBoxA: BLOCKED -> reply=%d", reply);
		return reply;
	}

	return oMessageBoxA(hWnd, lpText, lpCaption, uType);
}

static int WINAPI hkMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
	char textA[512] = "(null)";
	char captionA[256] = "(null)";
	if (lpText)
		WideCharToMultiByte(CP_ACP, 0, lpText, -1, textA, sizeof(textA), NULL, NULL);
	if (lpCaption)
		WideCharToMultiByte(CP_ACP, 0, lpCaption, -1, captionA, sizeof(captionA), NULL, NULL);

	RevLog("MessageBoxW: caption='%s' text='%s' type=0x%X", captionA, textA, uType);

	if (IsEmptyDialog(lpText ? textA : nullptr, lpCaption ? captionA : nullptr))
	{
		int reply = AutoReply(uType);
		RevLog("MessageBoxW: BLOCKED (empty dialog) -> reply=%d", reply);
		return reply;
	}

	return oMessageBoxW(hWnd, lpText, lpCaption, uType);
}

static void HookMessageBox()
{
	HMODULE hUser32 = GetModuleHandleA("user32.dll");
	if (!hUser32)
		return;

	oMessageBoxA = (tMessageBoxA)GetProcAddress(hUser32, "MessageBoxA");
	oMessageBoxW = (tMessageBoxW)GetProcAddress(hUser32, "MessageBoxW");

	if (oMessageBoxA)
		oMessageBoxA = (tMessageBoxA)DetourFunction((PBYTE)oMessageBoxA, (PBYTE)hkMessageBoxA);
	if (oMessageBoxW)
		oMessageBoxW = (tMessageBoxW)DetourFunction((PBYTE)oMessageBoxW, (PBYTE)hkMessageBoxW);

	RevLog("hook: MessageBoxA/W hooked");
}

// ============================================================================
// ExitProcess / TerminateProcess / VEH — Kim kapatıyor tespiti
//
// ExitProcess    : normal cikis (ExitProcess(0) veya return from main)
// TerminateProcess : zorla kapatma (XIGNCODE, anti-cheat vs.)
// VEH            : unhandled exception / crash
// ============================================================================
typedef VOID(WINAPI *tExitProcess)(UINT);
typedef BOOL(WINAPI *tTerminateProcess)(HANDLE, UINT);
typedef NTSTATUS(NTAPI *tNtTerminateProcess)(HANDLE, NTSTATUS);
typedef NTSTATUS(NTAPI *tNtTerminateThread)(HANDLE, NTSTATUS);
typedef VOID(NTAPI *tRtlExitUserProcess)(NTSTATUS);
typedef NTSTATUS(NTAPI *tNtRaiseHardError)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);

static tExitProcess oExitProcess = nullptr;
static tTerminateProcess oTerminateProcess = nullptr;
static tNtTerminateProcess oNtTerminateProcess = nullptr;
static tNtTerminateThread oNtTerminateThread = nullptr;
static tRtlExitUserProcess oRtlExitUserProcess = nullptr;
static tNtRaiseHardError oNtRaiseHardError = nullptr;

// Caller adresini oku (hook icinde esp+4 = return address)
static DWORD GetReturnAddr()
{
	DWORD ret = 0;
	__try
	{
		ret = ((DWORD *)_AddressOfReturnAddress())[0];
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
	return ret;
}

static void PauseConsole(const char *reason, bool forceWait = false)
{
	if (g_hConsole == INVALID_HANDLE_VALUE)
		return;

	char buf[256];
	// "forceWait" true ise ENTER bekler, false ise sadece yazıp geçer
	if (forceWait)
	{
		sprintf_s(buf, "\n>>> %s -- devam etmek icin ENTER'a bas <<<\n", reason);
	}
	else
	{
		sprintf_s(buf, "\n>>> %s -- (Loglandi, devam ediliyor...) <<<\n", reason);
	}

	DWORD w;
	WriteConsoleA(g_hConsole, buf, (DWORD)strlen(buf), &w, NULL);

	// Eğer forceWait false ise, ENTER beklemeden hemen geri dön
	if (!forceWait)
	{
		return;
	}

	// YALNIZCA forceWait true olduğunda (Crash anı gibi) burası çalışır
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	FlushConsoleInputBuffer(hIn);
	SetConsoleMode(hIn, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
	char ch;
	DWORD rd;
	ReadConsoleA(hIn, &ch, 1, &rd, NULL);
}

static VOID WINAPI hkExitProcess(UINT uExitCode)
{
	RevLog("EXIT: ExitProcess(code=%u) T=%lu", uExitCode, GetCurrentThreadId());
	// XIGNCODE fake-crash exit kodunu block'la
	if (uExitCode == 0xC0000005)
	{
		RevLog("EXIT: ExitProcess BLOCKED (XIGNCODE self-kill)");
		return;
	}
	oExitProcess(uExitCode);
}

static BOOL WINAPI hkTerminateProcess(HANDLE hProc, UINT uExitCode)
{
	DWORD pid = GetProcessId(hProc);
	bool isSelf = (hProc == (HANDLE)-1 || pid == GetCurrentProcessId());
	RevLog("EXIT: TerminateProcess(pid=%lu code=0x%08X) T=%lu self=%d caller=0x%08lX",
		   pid, uExitCode, GetCurrentThreadId(), isSelf ? 1 : 0, GetReturnAddr());

	if (isSelf && uExitCode == 0xC0000005)
	{
		RevLog("EXIT: TerminateProcess BLOCKED (XIGNCODE self-kill 0xC0000005)");
		return TRUE;
	}
	return oTerminateProcess(hProc, uExitCode);
}

static NTSTATUS NTAPI hkNtTerminateProcess(HANDLE hProc, NTSTATUS exitStatus)
{
	DWORD pid = GetProcessId(hProc);
	bool isSelf = (hProc == (HANDLE)-1 || pid == GetCurrentProcessId());

	RevLog("EXIT: NtTerminateProcess(pid=%lu status=0x%08lX) T=%lu self=%d",
		   pid, (ULONG)exitStatus, GetCurrentThreadId(), isSelf ? 1 : 0);

	// XIGNCODE self-kill: 0xC0000005 (fake AV) ile kendi process'ini oldurmeye calisiyor
	// Blokla — normal exit kodlari (0, 0xC000013A vb.) gecsin
	if (isSelf && exitStatus == (NTSTATUS)0xC0000005)
	{
		RevLog("EXIT: NtTerminateProcess BLOCKED (XIGNCODE self-kill)");
		return STATUS_SUCCESS;
	}

	return oNtTerminateProcess(hProc, exitStatus);
}

static NTSTATUS NTAPI hkNtTerminateThread(HANDLE hThread, NTSTATUS exitStatus)
{
	DWORD tid = GetThreadId(hThread);
	DWORD myTid = GetCurrentThreadId();
	bool isSelf = (hThread == (HANDLE)-1 || tid == myTid);

	RevLog("EXIT: NtTerminateThread(tid=%lu status=0x%08lX) T=%lu self=%d caller=0x%08lX",
		   tid, (ULONG)exitStatus, myTid, isSelf ? 1 : 0, GetReturnAddr());

	if (exitStatus == (NTSTATUS)0xC0000005)
	{
		RevLog("EXIT: NtTerminateThread BLOCKED (XIGNCODE kill 0xC0000005)");
		return STATUS_SUCCESS;
	}
	return oNtTerminateThread(hThread, exitStatus);
}

static VOID NTAPI hkRtlExitUserProcess(NTSTATUS status)
{
	RevLog("EXIT: RtlExitUserProcess(status=0x%08lX) T=%lu caller=0x%08lX",
		   (ULONG)status, GetCurrentThreadId(), GetReturnAddr());
	if (status == (NTSTATUS)0xC0000005)
	{
		RevLog("EXIT: RtlExitUserProcess BLOCKED (XIGNCODE self-kill 0xC0000005)");
		return;
	}
	oRtlExitUserProcess(status);
}

static NTSTATUS NTAPI hkNtRaiseHardError(NTSTATUS errStatus, ULONG numParams, ULONG unicodeStrMask,
										  PULONG_PTR params, ULONG responseOption, PULONG response)
{
	RevLog("EXIT: NtRaiseHardError(status=0x%08lX resp=%lu) T=%lu caller=0x%08lX",
		   (ULONG)errStatus, responseOption, GetCurrentThreadId(), GetReturnAddr());
	// OptionAbortRetryIgnore (6) veya OptionOk (0) ile hard error = process kill
	if (responseOption >= 5)
	{
		RevLog("EXIT: NtRaiseHardError BLOCKED (responseOption=%lu)", responseOption);
		if (response) *response = 1; // IDOK
		return STATUS_SUCCESS;
	}
	return oNtRaiseHardError(errStatus, numParams, unicodeStrMask, params, responseOption, response);
}

static LONG WINAPI VehCrashHandler(EXCEPTION_POINTERS *ep)
{
	DWORD code = ep->ExceptionRecord->ExceptionCode;

	// Anti-cheat / Themida kasıtlı trap exception'ları — kesinlikle dokunma
	// Bunları handle edersek XIGNCODE/Themida'nın kendi SEH zinciri kırılır
	switch (code)
	{
	case 0xC0000096: // Privileged instruction — Themida VM trap
	case 0x80000003: // INT3 breakpoint — debugger/AC detection
	case 0x80000004: // Single step — debugger detection
	case 0xC000001D: // Illegal instruction — Themida VM opcode
	case 0xC0000374: // Heap corruption — runtime check, bize ait degil
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// Sadece ACCESS_VIOLATION logla, digerleri gecir
	if (code != 0xC0000005)
		return EXCEPTION_CONTINUE_SEARCH;

	PEXCEPTION_RECORD er = ep->ExceptionRecord;
	PCONTEXT ctx = ep->ContextRecord;
	DWORD eip = ctx->Eip;

	// Sistem DLL araligindaki AV'leri gecir (XIGNCODE, ntdll, user32 vs. 0x7xxxxxxx)
	// Bunlar genelde XIGNCODE'un kendi icindeki kasitli hatalar
	if (eip >= 0x70000000)
		return EXCEPTION_CONTINUE_SEARCH;

	// Game address range (0x00400000 - 0x01200000) veya inject DLL'lerimiz
	RevLog("========= CRASH: ACCESS VIOLATION =========");
	RevLog("EIP: %08X | Fault: %p | Type: %s",
		   eip,
		   (void *)er->ExceptionInformation[1],
		   er->ExceptionInformation[0] == 0 ? "READ" : (er->ExceptionInformation[0] == 1 ? "WRITE" : "DEP"));
	RevLog("EAX=%08X EBX=%08X ECX=%08X EDX=%08X", ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx);
	RevLog("ESI=%08X EDI=%08X EBP=%08X ESP=%08X", ctx->Esi, ctx->Edi, ctx->Ebp, ctx->Esp);

	unsigned char opc[10];
	if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)eip, opc, sizeof(opc), NULL))
	{
		char hex[64] = {};
		for (int i = 0; i < 10; i++)
			sprintf(hex + strlen(hex), "%02X ", opc[i]);
		RevLog("Opcodes: %s", hex);
	}

	HMODULE hMod;
	char modName[MAX_PATH];
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)er->ExceptionAddress, &hMod))
		GetModuleFileNameA(hMod, modName, MAX_PATH);
	else
		strcpy_s(modName, "UNKNOWN");
	RevLog("Module: %s", modName);

	// Call stack: ESP'den return address'leri oku — crashin gercek cagiran kodunu bul
	uintptr_t gameBase = (uintptr_t)GetModuleHandleA(NULL);
	RevLog("--- CALL STACK (ESP) ---");
	DWORD *pStack = (DWORD *)ctx->Esp;
	for (int i = 0; i < 16; i++)
	{
		DWORD val;
		if (!ReadProcessMemory(GetCurrentProcess(), pStack + i, &val, sizeof(val), NULL))
			break;
		// Sadece game exe araligindaki adresleri logla (0x00400000 - 0x01200000)
		if (val >= 0x00400000 && val <= 0x01200000)
		{
			uintptr_t ida = 0x400000 + (val - gameBase);
			RevLog("  [ESP+%02X] 0x%08lX (IDA:0x%08lX)", i * 4, val, ida);
		}
	}
	RevLog("===========================================");

	// VEH icinde ASLA bloklama yapma — dosyaya yazildi, geciyoruz
	return EXCEPTION_CONTINUE_SEARCH;
}

static void HookExit()
{
	HMODULE hK32 = GetModuleHandleA("kernel32.dll");
	if (!hK32)
		return;

	oExitProcess = (tExitProcess)GetProcAddress(hK32, "ExitProcess");
	oTerminateProcess = (tTerminateProcess)GetProcAddress(hK32, "TerminateProcess");

	if (oExitProcess)
		oExitProcess = (tExitProcess)DetourFunction((PBYTE)oExitProcess, (PBYTE)hkExitProcess);
	if (oTerminateProcess)
		oTerminateProcess = (tTerminateProcess)DetourFunction((PBYTE)oTerminateProcess, (PBYTE)hkTerminateProcess);

	// NtTerminateProcess / NtTerminateThread — ntdll direkt cagrisi
	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	if (hNtdll)
	{
		oNtTerminateProcess = (tNtTerminateProcess)GetProcAddress(hNtdll, "NtTerminateProcess");
		if (oNtTerminateProcess)
			oNtTerminateProcess = (tNtTerminateProcess)DetourFunction((PBYTE)oNtTerminateProcess, (PBYTE)hkNtTerminateProcess);

		oNtTerminateThread = (tNtTerminateThread)GetProcAddress(hNtdll, "NtTerminateThread");
		if (oNtTerminateThread)
			oNtTerminateThread = (tNtTerminateThread)DetourFunction((PBYTE)oNtTerminateThread, (PBYTE)hkNtTerminateThread);

		oRtlExitUserProcess = (tRtlExitUserProcess)GetProcAddress(hNtdll, "RtlExitUserProcess");
		if (oRtlExitUserProcess)
			oRtlExitUserProcess = (tRtlExitUserProcess)DetourFunction((PBYTE)oRtlExitUserProcess, (PBYTE)hkRtlExitUserProcess);

		oNtRaiseHardError = (tNtRaiseHardError)GetProcAddress(hNtdll, "NtRaiseHardError");
		if (oNtRaiseHardError)
			oNtRaiseHardError = (tNtRaiseHardError)DetourFunction((PBYTE)oNtRaiseHardError, (PBYTE)hkNtRaiseHardError);
	}

	// VEH: sadece patch sonrasi crash tespiti icin, Themida trap'leri filtreli
	AddVectoredExceptionHandler(0, VehCrashHandler);

	RevLog("hook: ExitProcess/TerminateProcess/Nt+RtlExit/NtTermThread/NtRaiseHardError/VEH hooked");
}

// ============================================================================
// Hook kurulum noktasi
// TODO: Yeni client adresleri dogrulaninca asagidaki hook'lar tek tek acilacak.
// ============================================================================
void REVOLTEACSHook(HANDLE hProcess)
{
	// g_GameHooks.InitAllHooks(hProcess);  // EndGame 0x00E76BD9, Tick 0x006CE830
	// g_UIManager.Init();
	// RenderSystem::Init();
	// Engine = new PearlEngine(); Engine->Init();

	// HookCreateWindow();  // gecici devre disi — saf injection testi
	// HookMessageBox();
	// HookExit();

	// Send/Recv hook — gecici devre disi (version spoof debug)
	// CreateThread(NULL, 0, [](LPVOID) -> DWORD {
	// 	Sleep(5000);
	// 	g_PacketHandler.InitSendHook();
	// 	g_PacketHandler.InitRecvHook();
	// 	RevLog("hook: Send/Recv hooks aktif (KO_SND_FNC=0x006FC190, KO_RECV_FNC=0x0082C7D0)");
	// 	return 0;
	// }, NULL, 0, NULL);
}

// ============================================================================
// Anti-AntiDebug
// PEB flag temizle + NtQueryInformationProcess / IsDebuggerPresent hookla
// ============================================================================
typedef NTSTATUS(NTAPI *tNtQIP)(HANDLE, UINT, PVOID, ULONG, PULONG);
typedef BOOL(WINAPI *tIsDbgPresent)();

static tNtQIP oNtQIP = nullptr;
static tIsDbgPresent oIsDbgPresent = nullptr;

static NTSTATUS NTAPI hkNtQIP(HANDLE hProc, UINT cls, PVOID info, ULONG len, PULONG retLen)
{
	NTSTATUS st = oNtQIP(hProc, cls, info, len, retLen);
	if (st >= 0)
	{
		if (cls == 7 || cls == 0x1E) // ProcessDebugPort / ProcessDebugObjectHandle
			*(DWORD *)info = 0;
	}
	return st;
}

static BOOL WINAPI hkIsDbgPresent() { return FALSE; }

static void AntiAntiDebug()
{
	// 1. PEB.BeingDebugged = 0
	// BYTE* peb = (BYTE*)__readfsdword(0x30);
	// peb[2] = 0;

	// 2. PEB.NtGlobalFlag heap debug bitleri temizle
	// *(DWORD*)(peb + 0x68) &= ~0x70u;

	// 3. NtQueryInformationProcess hook — ProcessDebugPort / ProcessDebugObjectHandle
	// HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	// if (hNtdll) {
	//     oNtQIP = (tNtQIP)GetProcAddress(hNtdll, "NtQueryInformationProcess");
	//     if (oNtQIP)
	//         oNtQIP = (tNtQIP)DetourFunction((PBYTE)oNtQIP, (PBYTE)hkNtQIP);
	// }

	// 4. IsDebuggerPresent hook
	// HMODULE hK32 = GetModuleHandleA("kernel32.dll");
	// if (hK32) {
	//     oIsDbgPresent = (tIsDbgPresent)GetProcAddress(hK32, "IsDebuggerPresent");
	//     if (oIsDbgPresent)
	//         oIsDbgPresent = (tIsDbgPresent)DetourFunction((PBYTE)oIsDbgPresent, (PBYTE)hkIsDbgPresent);
	// }

	// RevLog("antidebug: all writes disabled (debug-observe mode)");
}



void REVOLTEACSRemap()
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
	if (!hProcess)
	{
		RevLog("remap: OpenProcess failed");
		return;
	}

	// AntiAntiDebug();

	HANDLE hWatchdog = CreateThread(NULL, 0, XignDispatcherWatchdog, NULL, 0, NULL);
	if (hWatchdog)
	{
		RevLog("remap: watchdog started");
		CloseHandle(hWatchdog);
	}

	// REVOLTEACSHook(hProcess);
	CloseHandle(hProcess);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		// 1. Process ID'yi alıyoruz
		DWORD pid = GetCurrentProcessId();

		InitConsole();
		RevLog("REVOLTEACS loaded — PID=%lu", GetCurrentProcessId());
		// LogLoadedModules("DllMain");
		REVOLTEACSRemap();
	}
	return TRUE;
}
