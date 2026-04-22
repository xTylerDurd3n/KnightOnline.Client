# REVOLTEACS - KnightOnline v25xx Client Mod Framework

## Project Overview

REVOLTEACS is a DLL injection framework for the KnightOnline v2524-v25xx game client. It provides memory patching, function hooking, packet interception, UI manipulation, and DirectX overlay rendering. The DLL is loaded into the game process and hooks into core game functions at runtime.

**Solution:** `REVOLTEACS.sln` (Visual Studio 2022, v143 toolset)
**Output:** DynamicLibrary (DLL)
**Target:** Windows x86/x64

---"

## Project Structure

```
KnightOnline Client/
├── REVOLTEACS/                      # Main DLL project
│   ├── DetourAPI/3.0/               # Detours library (include + lib)
│   │   ├── include/detours.h
│   │   └── lib/detours.lib
│   ├── framework.h                  # ALL game offsets & pointers
│   ├── pch.h                        # Precompiled header (master include)
│   ├── types.h                      # Type aliases (uint8, int32, etc.)
│   ├── dllmain.cpp                  # DLL entry + Remap + boot sequence
│   ├── PearlEngine.h/.cpp           # Central engine coordinator
│   ├── PlayerBase.h/.cpp            # Character data reader (40+ fields)
│   ├── PacketHandler.h/.cpp         # Send/Recv hook + opcode filtering
│   ├── Packet.h / ByteBuffer.h      # Binary packet serialization
│   ├── GameHooks.h/.cpp             # EndGame, Tick, game-level hooks
│   ├── RenderSystem.h/.cpp          # DX9/DX11 EndScene overlay
│   ├── UIManager.h/.cpp             # GetChildByID + UI element probing
│   ├── UIFramework.h/.cpp           # Custom UI elements (Panel, Button, Label, etc.)
│   ├── UITaskbar.h/.cpp             # Taskbar vTable ReceiveMessage hooks
│   ├── UIChatBar.h/.cpp             # Chat UI (stub)
│   ├── UIInventory.h/.cpp           # Inventory UI (stub)
│   ├── UILogin.h/.cpp               # Login UI (stub)
│   ├── Remap.h                      # NT syscall memory section remapping
│   ├── SkCrypter.h                  # Compile-time XOR string encryption
│   ├── xorstr.h                     # Runtime XOR string obfuscation
│   ├── crc32.h/.cpp                 # CRC32 checksum
│   ├── md5.h/.cpp                   # MD5 hashing
│   └── REVOLTEACS.vcxproj           # Project file
├── OffsetScanner/                   # Utility DLL for offset discovery
│   ├── OffsetScanner.cpp
│   └── OffsetScanner.vcxproj
├── REVOLTELAUNCHER/                 # Game launcher + DLL injector
│   ├── Launcher.cpp                 # Main launcher UI + window management
│   ├── Launcher.sln                 # Launcher solution file
│   ├── Launcher.vcxproj             # Launcher project file
│   ├── LauncherEngine.cpp           # Patch/update engine + Windows Defender exclusions
│   ├── LauncherEngine.h
│   ├── GDIHelper.cpp                # GDI rendering + DLL injection (InjectDLL + CreateRemoteThread)
│   ├── GDIHelper.h
│   ├── APISocket.cpp/.h             # Socket communication
│   ├── Packet.h / ByteBuffer.h      # Packet serialization
│   ├── JvCryption.cpp/.h            # Encryption
│   ├── hdr.cpp/.h                   # Header utilities
│   ├── CRC.cpp/.h / crc32.h         # CRC checksums
│   ├── MD5.h / sha.cpp/.hpp         # Hashing
│   ├── Remap.h                      # NT syscall memory remapping
│   ├── xorstr.h                     # Runtime XOR string obfuscation
│   ├── curl/                        # libcurl headers
│   ├── ftpclient/                   # FTP client for patching
│   └── zlib/                        # Compression library
├── Release/                         # Build output + game resources
├── REVOLTEACS.sln                   # Solution file
├── .gitignore
└── clean.bat                        # Cleans build artifacts
```

---

## Architecture & Boot Sequence

```
Launcher.exe (REVOLTELAUNCHER)
  ├─ CreateProcessA("KnightOnLine.exe")     (game process)
  ├─ InjectDLL(pid, "REVOLTEACS.dll")       (CreateRemoteThread + LoadLibraryA)
  ├─ Sleep(2000)
  └─ ResumeThread()                          (game starts with DLL loaded)

DllMain (DLL_PROCESS_ATTACH)
  └─ REVOLTEACSLoad()
       ├─ AllocConsole() (if CONSOLE_MODE=1)
       └─ REVOLTEACSRemap()
            ├─ RemapArrayInsert() x3 memory regions
            ├─ REVOLTEACSRemapProcess(hProcess)
            │   ├─ Remap::PatchSection() x3  (PAGE_EXECUTE_READWRITE)
            │   ├─ XINGCODE/XIGNCODE byte patches
            │   └─ Anti-cheat obfuscation threads (x7)
            └─ REVOLTEACSHook(hProcess)
                 ├─ CGameHooks::InitAllHooks()   (EndGame + Tick hooks)
                 ├─ CUIManager::Init()
                 ├─ PacketHandler::InitSendHook() + InitRecvHook()
                 ├─ RenderSystem::Init()          (3sec delay thread)
                 └─ PearlEngine::Init()           (engine thread)
```

---

## Core Systems

### PearlEngine (Central Coordinator)
- Owns all subsystem instances (PlayerBase, UIManager, PacketHandler, RenderSystem)
- Engine loop runs in separate thread (~100ms tick)
- `Update()` calls `PlayerBase::UpdateFromMemory()` each frame
- `Send(Packet*)` routes packets through PacketHandler
- Taskbar init delayed 20sec (post-login)

### PlayerBase (Character Data)
- Reads 40+ fields from `KO_PTR_CHR` base pointer + offsets
- Class helpers: `isWarrior()`, `isRogue()`, `isMage()`, `isPriest()`, `isKurian()`
- Weight values are x10 (divide by 10 for actual)
- Logs character status every 10 seconds

### PacketHandler (Send/Recv Interception)
- **Send hook** at `0x006FC190` (__thiscall via __fastcall trick)
- **Recv hook** at `0x0082C7D0` (__thiscall)
- Opcode blocking via `AddBlockedOpcode()` / `RemoveBlockedOpcode()`
- Handler registry by opcode via `RegisterHandler()` / `UnregisterHandler()`
- Packet class extends ByteBuffer with stream operators (`<<` / `>>`)

### RenderSystem (DX9/DX11 Overlay)
- Auto-detects DX version (v25xx uses DX9)
- DX9: hooks EndScene (vtable index 42) via DetourFunction
- DX11: hooks Present (vtable index 8), falls back to DX9
- Drawing: `DrawRect()`, `DrawFilledRect()`, `DrawLine()`, `DrawText()` (GDI)
- Creates temp device to extract vtable addresses

### GameHooks
- **EndGame** (0x00E76BD9): naked hook, TerminateProcess
- **Tick** (0x006CE830): naked hook, ~30ms, speed multiplier

### UIManager
- Probes `std::list<N3UIBase*>` children from UI elements
- Handles MSVC string SSO vs heap layout detection
- Safe memory reads with bounds checking

### UITaskbar
- Hooks vTable+0x7C (ReceiveMessage) on main/sub taskbars
- Button handlers via `RegisterButtonHandler()`
- Delayed 20sec init (post-login)

### Remap System (Memory Patching)
- NT syscalls: `ZwCreateSection`, `ZwMapViewOfSection`, `ZwUnmapViewOfSection`
- Remaps game memory sections with PAGE_EXECUTE_READWRITE
- Three regions: 0x00400000 (11.25MB), 0x00F30000 (1.5MB), 0x01060000 (64KB)

---

## Key Game Offsets (framework.h)

### Core Pointers
| Pointer | Address | Description |
|---------|---------|-------------|
| KO_PTR_CHR | 0x01092964 | Character base |
| KO_PTR_DLG | 0x01092A14 | Dialog manager |
| KO_PTR_PKT | 0x01092A2C | CAPISocket |
| KO_FLDB | 0x01092958 | Field database |

### Core Functions
| Function | Address | Description |
|----------|---------|-------------|
| KO_SND_FNC | 0x006FC190 | Send packet |
| KO_RECV_FNC | 0x0082C7D0 | Recv handler |
| KO_GAME_TICK | 0x006CE830 | Game tick |
| KO_CALL_END_GAME | 0x0079A5D0 | EndGame call |
| KO_FNC_END_GAME | 0x00E76BD9 | EndGame func |
| KO_GET_CHILD_BY_ID_FUNC | 0x005E6E5A | UI child lookup |

### Player Offsets (from KO_PTR_CHR base)
| Offset | Type | Field |
|--------|------|-------|
| 0x690 | uint16 | ID |
| 0x694 | char* | Name |
| 0x6A4 | uint16 | Class |
| 0x6B4 | uint8 | Nation (1=Karus, 2=ElMorad) |
| 0x6B8 | uint8 | Race |
| 0x6C0 | uint8 | Level |
| 0x6C4 | int32 | MaxHP |
| 0x6C8 | int32 | HP |
| 0x BBC | int32 | MaxMP |
| 0xBC0 | int32 | MP |
| 0x3D0 | float | X position |
| 0x3D4 | float | Y position |
| 0x3D8 | float | Z position |
| 0xBCC | uint32 | Gold |
| 0xBE0 | uint32 | Nation Points |
| 0xBD8 | uint64 | Experience |
| 0xBD0 | uint64 | Max Experience |
| 0xC1C | uint32 | Attack |
| 0xC24 | uint32 | Defence |
| 0x650 | int16 | Target ID |
| 0x140 | uint8 | Move state |
| 0x17C | float | Yaw angle |
| 0xBF0 | uint32 | Weight (x10) |
| 0xBE8 | uint32 | MaxWeight (x10) |
| 0xC60 | uint8 | Zone |
| 0x6EC | uint32 | Knights/Clan ID |
| 0xFF4 | uint32 | Rebirth |

### Stats Offsets
| Offset | Field |
|--------|-------|
| 0xBF4 | STR |
| 0xBFC | HP stat |
| 0xC04 | DEX |
| 0xC0C | INT |
| 0xC14 | MP stat |

### Resistances
| Offset | Field |
|--------|-------|
| 0xC2C | Fire |
| 0xC34 | Ice |
| 0xC3C | Lightning |
| 0xC44 | Magic |
| 0xC4C | Curse |
| 0xC54 | Poison |

### Class Values
```
Warrior:  1 (base), 5 (Blade), 6 (Berserker)
Rogue:    2 (base), 7 (Assassin), 8 (Archer)
Mage:     3 (base), 9 (Fire), 10 (Ice)
Priest:   4 (base), 11 (Heal), 12 (Buffer)
Kurian:   13 (Porutu), 14 (Divine), 15 (Shadow)
```

---

## Hooking Pattern (DetourAPI)

```cpp
// Install hook
OriginalFunc = (OriginalFuncType)DetourFunction((PBYTE)targetAddress, (PBYTE)hookFunction);

// Hook callback calls original
return OriginalFunc(args...);
```

For vTable hooks (UITaskbar):
```cpp
DWORD* vTable = *(DWORD**)uiElement;
oReceiveMessage = vTable[0x7C / 4];   // save original
vTable[0x7C / 4] = (DWORD)hkReceiveMessage;  // replace
```

---

## Threading Model

| Thread | Delay | Purpose |
|--------|-------|---------|
| DllMain | 0s | Boot sequence |
| EngineMain | 0s | Core loop (~100ms) |
| RenderSystem | 3s | DX9 device init |
| UITaskbar | 20s | Post-login taskbar hooks |
| OffsetVerify | 15s | Offset validation |
| RandomYazdir x7 | 0s | Anti-cheat memory obfuscation (10s interval) |

---

## Build Configuration

- **Toolset:** MSVC v143 (VS 2022)
- **Precompiled Header:** pch.h
- **Preprocessor:** `REVOLTEACS_EXPORTS`, `_CRT_SECURE_NO_WARNINGS`, `PERK_COUNT=13`
- **Libraries:** detours.lib, d3d9.lib, d3d11.lib, dxgi.lib, iphlpapi.lib, Psapi.lib, user32.lib, Ws2_32.lib, Mpr.lib
- **Release:** /Gy, /Oi, /OPT:ICF, /OPT:REF, safe exception handlers disabled

---

## String Encryption

- **SkCrypter** (`skCryptDec("str")`): compile-time XOR, key from `__TIME__`
- **xorstr** (`xorstr("str")`): runtime XOR, Park-Miller PRNG seed from compile time

---

## Conventions

- Turkish comments throughout codebase
- `g_` prefix for globals (g_PacketHandler, g_UIManager, g_GameHooks, g_RenderSystem)
- `m_` prefix for member variables
- `s_` prefix for static members
- `hk` prefix for hook functions (hkSend, hkRecv, hkEndScene, hkPresent)
- `KO_` prefix for all game offsets and pointers
- `C` prefix for classes (CPlayerBase, CUIManager, CGameHooks)
- Engine singleton accessed via global `Engine` pointer

---

## Stub/TODO Features

- Camera zoom hook (address unknown for v25xx)
- Object loop hooks (player/mob iteration)
- Mouse hook
- UI ReceiveMessage hooks for: MiniMenu, Login, ChatBar
- DX11 rendering (Present hook exists, no draw code)
- UIChatBar, UIInventory, UILogin (all stubs)
- DrawTexture (placeholder only)

---

## Tech Stack Cheatsheet

### C++ & Windows API
| Concept | Usage in Project | Key API |
|---------|-----------------|---------|
| DLL Injection | Entry point for all hooks | `DllMain`, `DLL_PROCESS_ATTACH` |
| Process Memory | Read/write game memory | `ReadProcessMemory`, `WriteProcessMemory`, `OpenProcess` |
| Virtual Memory | Change page protection | `VirtualProtect`, `ZwCreateSection`, `ZwMapViewOfSection` |
| Threading | Concurrent subsystems | `CreateThread`, `LPTHREAD_START_ROUTINE` |
| Console I/O | Debug output | `AllocConsole`, `freopen("CONOUT$")` |
| Module Loading | Dynamic DLL loading | `LoadLibraryA`, `GetModuleHandleA` |
| Process Control | Anti-cheat/exit | `TerminateProcess`, `ExitProcess`, `FreeLibrary` |

### DirectX 9
| Concept | Usage | Details |
|---------|-------|---------|
| Device Creation | Temp device for vtable | `Direct3DCreate9` + `CreateDevice` |
| EndScene Hook | Overlay rendering | vtable index 42, `DetourFunction` |
| DrawPrimitiveUP | Rectangles, lines | `D3DPT_TRIANGLESTRIP`, `D3DPT_LINELIST` |
| Vertex Format | `D3DFVF_XYZRHW \| D3DFVF_DIFFUSE` | Transformed + colored vertices |
| Render States | Alpha blending setup | `D3DRS_ALPHABLENDENABLE`, `D3DBLEND_SRCALPHA` |
| GDI Fallback | Text rendering | `GetBackBuffer` -> `GetDC` -> `TextOutA` |
| Device Lost | Alt-tab handling | `OnDeviceLost()` / `OnDeviceReset()` |

### DirectX 11
| Concept | Usage | Details |
|---------|-------|---------|
| Device+SwapChain | Temp for vtable | `D3D11CreateDeviceAndSwapChain` |
| Present Hook | Frame callback | vtable index 8 on `IDXGISwapChain` |
| Fallback | If DX11 fails | Falls back to DX9 path |

### DetourAPI v3.0
| Function | Purpose |
|----------|---------|
| `DetourFunction(target, hook)` | Install inline hook, returns trampoline |
| `DetourRemove(trampoline, hook)` | Remove hook |
| Trampoline | 32-byte stub that jumps to original code |

### Hooking Techniques
| Type | Example | How |
|------|---------|-----|
| Inline (Detour) | Send, Recv, EndScene | `DetourFunction()` patches first bytes |
| vTable | UITaskbar ReceiveMessage | Overwrite function pointer at vTable+0x7C |
| Naked (__declspec) | EndGame, Tick | Raw assembly hook, manual stack management |
| Byte Patch | XIGNCODE bypass | Direct memory write (`*(uint8_t*)addr = val`) |
| Section Remap | Main code sections | NT syscall to remap with new protection |

### Calling Conventions
| Convention | Used For | Register Usage |
|------------|----------|----------------|
| `__thiscall` | Game class methods (Send, Recv) | `this` in ECX |
| `__fastcall` | Hook trick for __thiscall | ECX=this, EDX=unused |
| `__stdcall` | DX9 EndScene, DX11 Present | Stack-based, callee cleans |
| `__cdecl` | Standard C functions | Stack-based, caller cleans |
| `naked` | EndGame, Tick hooks | No prologue/epilogue |

### Memory Layout (MSVC x86)
| Structure | Layout |
|-----------|--------|
| vTable | First DWORD of object = pointer to function table |
| std::string (SSO) | If len < 16: inline buffer; else: heap pointer |
| std::list | Doubly-linked nodes, each with _Next/_Prev + value |
| DWORD* | 4 bytes on x86, pointer dereference for vtable |

### Packet Protocol
```cpp
// Create packet
Packet pkt(OPCODE);
pkt << (uint32)value1 << (uint16)value2 << (uint8)value3;

// Send
Engine->Send(&pkt);

// Read received
uint32 val; pkt >> val;
```

### Common Patterns
```cpp
// Read game pointer chain
DWORD chrBase = *(DWORD*)KO_PTR_CHR;
uint16_t hp = *(uint16_t*)(chrBase + KO_OFF_HP);

// Safe pointer read
if (IsBadReadPtr((void*)addr, 4)) return;

// Hook installation
oOriginal = (tOriginal)DetourFunction((PBYTE)addr, (PBYTE)hkFunc);

// Hook callback
int __fastcall hkSend(DWORD thisPtr, DWORD /*edx*/, BYTE* pBuf, int iLen) {
    // custom logic...
    return oSend(thisPtr, pBuf, iLen);  // call original
}
```

### Key Windows NT Syscalls (Remap.h)
| Syscall | Purpose |
|---------|---------|
| `ZwCreateSection` | Create named/unnamed memory section |
| `ZwMapViewOfSection` | Map section into process address space |
| `ZwUnmapViewOfSection` | Unmap existing memory view |
| `ZwClose` | Close NT handle |

### Preprocessor Guards
| Define | Purpose |
|--------|---------|
| `CONSOLE_MODE 1` | Enable/disable debug console |
| `REVOLTEACS_EXPORTS` | DLL export marker |
| `PERK_COUNT 13` | Number of skill perks |
| `_CRT_SECURE_NO_WARNINGS` | Suppress secure CRT warnings |
