---
name: re-research
description: KnightOnline v25xx için reverse engineering araştırma workflow'u — yeni offset/fonksiyon keşfi, hook analizi, packet protokol çözümleme, anti-cheat (XIGNCODE/XINGCODE) bypass araştırması. Bulguları otomatik olarak memory'ye yazar ve framework.h ile senkronize tutar.
---

# RE Research — KnightOnline v25xx

Bu skill, REVOLTEACS projesinde reverse engineering çalışması yaparken kullanılacak ortak workflow'u tanımlar. **Tetikleyiciler:** kullanıcı "RE", "tersine mühendislik", "offset bul/doğrula", "hook çöz/analiz", "packet analiz", "opcode", "XIGNCODE", "XINGCODE", "anti-cheat", "IDA", "x32dbg/x64dbg", "Ghidra", "disassembly", "naked hook", "vtable", veya `framework.h` güncellemesi içeren mesajlar yazdığında.

## Çalışma Modu

1. **Soruyu netleştir** — hedef ne? (yeni adres bulmak / mevcut hook'u derinleştirmek / packet yapısı çıkarmak / anti-cheat baypas etmek). Belirsizse `AskUserQuestion` ile en fazla 2 soru sor.
2. **Mevcut zemini oku** — ilgili dosyaları (`framework.h`, ilgili hook dosyası, `dllmain.cpp` Remap bloğu) önce **paralel** Read ile incele. Tahmin etme, kaynağa bak.
3. **Hipotezi yaz** — kullanıcıya 2-3 cümlede ne aradığını ve yöntemini söyle.
4. **Doğrula** — adres/byte/opcode bulgularını mümkün olduğunda iki ayrı yoldan teyit et (örn. çağrı imzası + sabit referansı, hex dump + xref).
5. **Belge + memory** — bulguyu uygun yere yaz (kod yorumu **değil**, ya `framework.h` makro tanımı ya `RE_NOTES.md` benzeri kullanıcı talep eden dosya). **Otomatik memory kaydı** — aşağıdaki kurala göre kaydet.
6. **Özet** — sonunda 1-2 cümle: ne bulundu, sıradaki adım ne.

## Otomatik Memory Kayıt Politikası

Kullanıcı "Önemli bulguları otomatik kaydet" modunu seçti. Aşağıdaki bulguları **soru sormadan** memory'ye yaz (uygun türde — `project` veya `reference`):

| Bulgu türü | Memory tipi | Dosya adı kalıbı |
|---|---|---|
| Yeni doğrulanmış offset/fonksiyon adresi (v25xx için) | project | `project_offset_<short_name>.md` |
| Anti-cheat bypass patch (byte/jump) | project | `project_anticheat_<system>.md` |
| Yeni opcode + payload yapısı | project | `project_opcode_<hex>_<short>.md` |
| Hook stratejisi kararı (hangi yöntem, neden) | feedback | `feedback_hook_<name>.md` |
| Harici araç/dump/wiki referansı | reference | `reference_re_<name>.md` |
| Doğrulanmamış hipotez veya geçici test sonucu | **YAZMA** — konuşma içinde tut |

**Kaydetmeden önce kontrol:** Var olan memory dosyası varsa **güncelle**, kopya yaratma. `MEMORY.md` index'ini her yeni dosyada güncelle (tek satır pointer).

**Kaydederken her zaman dahil et:**
- Mutlak adres (0x0XXXXXXX)
- Hangi v sürümü için doğrulandı (v2524 / v25xx)
- Doğrulama yöntemi (1 cümle — örn. "x32dbg'de KO_SND_FNC çağrısının argümanı olarak gözlendi")
- Kullanıldığı dosya:satır referansı (varsa)

## RE Bulgusu için Standart Format

Memory dosyası gövdesinde:

```
- Adres: 0xXXXXXXXX
- Sürüm: v2524 (veya geçerli aralık)
- Tür: <fonksiyon | pointer | offset | byte patch | jump patch>
- Bağlam: <neyin parçası — örn. "XIGNCODE callback dispatcher", "Send() sonrası şifreleme">
- Doğrulama: <nasıl teyit edildi>
- İlgili kod: <dosya:satır veya framework.h makrosu>
- Bilinen risk: <crash, anti-cheat tetikleme, vb. — yoksa "yok">
```

## Proje Konvansiyonları (RE çalışırken uy)

- Tüm yeni adresler `framework.h` içinde `KO_` prefix'iyle tanımlanmalı
- Patch'ler `dllmain.cpp::REVOLTEACSRemapProcess` içinde tanımlanmış sıraya uy: önce Remap, sonra XINGCODE, sonra XIGNCODE byte patch'leri
- Memory yazımı için tutarlılık: `*(uint8_t*)0xADDR = 0xVAL;` tek byte için, `memcpy` çoklu byte için, `WriteProcessMemory` Remap'lenmemiş bölgeler için
- Yorumlar Türkçe; ama kod yorumu **eklemeyi varsayma** — RE bulgusu memory'ye gider, koda değil
- Hook tipi seçimi: inline (DetourFunction) > vtable > naked. Naked sadece zorunluysa
- `skCryptDec("...")` ile string sabitlerini gizle, `xorstr("...")` runtime için

## Anti-Cheat (XIGNCODE/XINGCODE) Özel Notları

- XINGCODE büyük jump patch örneği: `0x00E73CD7` — relative E9 jump + NOP padding
- XIGNCODE byte patch'leri **çift halinde** gelir (cond. jump + offset hedef düzeltmesi). Bir tanesini değiştirip diğerini bırakırsan crash riski yüksek
- 7 adet `RandomYazdir` thread'i (10s aralık) anti-cheat memory taraması için scrambling yapar — yeni adres eklerken thread listesini güncelle
- `0x00FCB66C` — process window title — anti-cheat fingerprinting için yeniden yazılıyor (PID dahil)

## Sıradaki Adım Önerisi (her oturum sonu)

Çalışmanı bitirmeden son mesajda:
- Hangi bulgular memory'ye yazıldı (dosya adlarıyla)
- `framework.h` veya `dllmain.cpp` değişti mi
- Bir sonraki oturumda devam edilecek nokta ne

Bu skill çağrıldığında yukarıdaki workflow'u sessizce uygula — kullanıcıya "skill aktif" demene gerek yok, doğrudan işe başla.
