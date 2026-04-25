#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// =============================================================================
// 25xx Full Hook System - Offset Definitions
// =============================================================================
// DOGRULANMIS: OffsetScanner ile memory'den dogrudan okunarak onaylanmis
// TAHMIN: Dump pattern'inden cikarilmis, dogrulanmasi gereken
// =============================================================================

// =============================================================================
// CORE POINTERS (DOGRULANMIS - 25xx)
// =============================================================================
#define KO_PTR_DLG          0x01092A14      // UI Dialog Manager
#define KO_PTR_PKT          0x01092A2C      // Packet Manager (CAPISocket ptr)
#define KO_PTR_CHR          0x01092964      // Character/Player Base
#define KO_FLDB             0x01092958      // Field Database

// =============================================================================
// CORE FUNCTIONS (DOGRULANMIS - 25xx)
// =============================================================================
#define KO_SND_FNC          0x006FC190      // Send Packet Function
#define KO_RECV_FNC         0x0082C7D0      // Game server recv handler (push ebp prolog, dogrulanmis)

// =============================================================================
// XIGNCODE SDK SLOTS (DOGRULANMIS - 25xx, IDA dump analizi)
// =============================================================================
// Bu adresler Virtual Address (VA) — offset degil.
// Oyun 0x00400000 base adresine yukleniyor, ASLR YOK (Themida 32-bit, sabit base).
// Her versiyonda ayni adrese yuklenir — hardcoded VA kullanimi guvenli.
//
// Slot'lar .rdata bolgesinde — XIGNCODE SDK init sirasinda gercek fn adresini yazar.
// Watchdog bu slot'larin gosterdigi fonksiyonlari hook'luyor (Detour), slot degeri degismez.
// =============================================================================
#define XIGN_SLOT_DISPATCHER    0x00F661D0  // dword_F661D0 — ana SDK dispatcher, 21 xref
                                            // __stdcall 1 arg (0x0050BAA9 call site'indan dogrulanmis)
                                            // dump degerinde: 0x7624E370 (xldr.dll / XignCode.dll icinde)

// =============================================================================
// GAME FUNCTIONS (dogrulanacak - 25xx)
// =============================================================================
const DWORD KO_CALL_END_GAME           = 0x0079A5D0;
const DWORD KO_FNC_END_GAME            = 0x00E76BD9;
const DWORD KO_GAME_TICK               = 0x006CE830;
const DWORD KO_GET_CHILD_BY_ID_FUNC    = 0x005E6E5A;

// =============================================================================
// PLAYER DATA OFFSETS (DOGRULANMIS - OffsetScanner + CE)
// chrBase = *(DWORD*)KO_PTR_CHR
// =============================================================================
#define KO_OFF_TARGET       0x650           // int16  - Hedef ID (-1 = yok)
#define KO_OFF_ID           0x690           // uint16 - Socket ID
#define KO_OFF_NAME         0x694           // char[] - Karakter Adi
#define KO_OFF_CLASS        0x6A4           // uint16 - Sinif (6=Berserker)
#define KO_OFF_NAMELEN      0x6A8           // uint16 - Name uzunluk
#define KO_OFF_NATION       0x6B4           // uint8  - Ulus (2=ElMorad)
#define KO_OFF_RACE         0x6B8           // uint8  - Irk
#define KO_OFF_HAIR         0x6BC           // uint32 - Sac modeli
#define KO_OFF_LEVEL        0x6C0           // uint8  - Seviye (83)
#define KO_OFF_MAXHP        0x6C4           // int32  - Maksimum HP
#define KO_OFF_HP           0x6C8           // int32  - Mevcut HP
#define KO_OFF_X            0x3D0           // float  - X Koordinati
#define KO_OFF_Y            0x3D4           // float  - Y Koordinati
#define KO_OFF_Z            0x3D8           // float  - Z Koordinati

// =============================================================================
// PLAYER EXTRA OFFSETS (XREF scan ile dogrulanmis - chrBase)
// =============================================================================
#define KO_OFF_STATEMOVE    0x140           // uint8  - Hareket durumu (DOGRULANMIS: 0=dur, 2=yuru)
#define KO_OFF_YAWTOREACH   0x17C           // float  - Yon acisi (DOGRULANMIS: CE ile)
#define KO_OFF_KNIGHTS_ID   0x6EC           // uint32 - Knights/Clan ID (DOGRULANMIS: 15001)
#define KO_OFF_REBIRTH      0xFF4           // uint32 - Rebirth level (0, dogrulanacak)

// =============================================================================
// DLG UI OFFSETS (25xx DOGRULANMIS — UIF dosya adi taramasi ile)
// dlgBase = *(DWORD*)KO_PTR_DLG
// Nesne+0x020 offset'indeki string pointer UIF dosya adini verir
// =============================================================================
#define KO_OFF_DLG_INVENTORY        0x1C4   // ui\re_inventory.uif
#define KO_OFF_DLG_VARIOUS_FRAME    0x1C8   // ui\re_various_frame.uif
#define KO_OFF_DLG_CHATTING_BOX     0x1CC   // ui\re_chatting_box.uif
#define KO_OFF_DLG_CHAT_BLOCK       0x1D0   // ui\re_chat_block.uif
#define KO_OFF_DLG_GAMEMAIN         0x1D4   // ui\re_information_box.uif (CGameProcMain)
#define KO_OFF_DLG_TARGET_FRAME     0x1E0   // ui\re_target_framebar.uif
#define KO_OFF_DLG_CLAN_BREAK       0x1E4   // ui\re_clan_break.uif
#define KO_OFF_DLG_TRANSACTION      0x1E8   // ui\re_transaction.uif
#define KO_OFF_DLG_DROPPED_ITEM     0x1EC   // ui\re_droppeditem.uif
#define KO_OFF_DLG_MINIMAP          0x200   // (minimap — ayri UIF yok, FB64D4)
#define KO_OFF_DLG_NOTICE           0x204   // ui\re_notice.uif
#define KO_OFF_DLG_CHANGECLASS      0x208   // ui\co_changeclass.uif
#define KO_OFF_DLG_KA_CHANGE        0x210   // ui\ka_change.uif
#define KO_OFF_DLG_TOOLTIP_REPAIR   0x214   // ui\co_tooltop_repair.uif
#define KO_OFF_DLG_WAREHOUSE        0x218   // ui\re_warehouse.uif
#define KO_OFF_DLG_CASH_WAREHOUSE   0x21C   // ui\re_cash_warehouse.uif
#define KO_OFF_DLG_CREATE_CLAN      0x224   // ui\re_creat_clan.uif
#define KO_OFF_DLG_SUB_HPBAR        0x228   // ui\re_uisubhpbar.uif
#define KO_OFF_DLG_BLACKLIST        0x22C   // ui\re_ui_blacklistcapture.uif
#define KO_OFF_DLG_BORDER           0x230   // ui\re_borderdff.uif
#define KO_OFF_DLG_DRAKY_RANK       0x234   // ui\re_draky_rank.uif
#define KO_OFF_DLG_DRAKY_CHAPTER    0x238   // ui\re_draky_chapter.uif
#define KO_OFF_DLG_DRAKY_FLOOR      0x23C   // ui\ui_drakyfloor.uif
#define KO_OFF_DLG_DRAKY_QUEUE_BOX  0x240   // ui\re_draky_queuebox.uif
#define KO_OFF_DLG_DRAKY_QUEUE_BAR  0x244   // ui\re_draky_queuebar.uif
#define KO_OFF_DLG_TRAINING         0x248   // ui\re_training.uif
#define KO_OFF_DLG_KNIGHTS_OP       0x24C   // ui\co_knightsoperation.uif
#define KO_OFF_DLG_PARTYBOARD       0x250   // ui\re_partyboard.uif
#define KO_OFF_DLG_SALEBOARD        0x254   // ui\el_saleboard.uif
#define KO_OFF_DLG_QUEST_EDIT       0x258   // ui\co_quest_edit.uif
#define KO_OFF_DLG_SALE_SELECTION   0x260   // ui\co_saleboardselection.uif
#define KO_OFF_DLG_SALE_MEMO        0x264   // ui\co_saleboardmemo.uif
#define KO_OFF_DLG_ITEM_UPGRADE     0x268   // ui\re_itemupgrade.uif
#define KO_OFF_DLG_RING_UPGRADE     0x26C   // ui\re_ringupgrade.uif
#define KO_OFF_DLG_RING_DISASM      0x270   // ui\re_ringdisassemble.uif
#define KO_OFF_DLG_REVIVAL          0x274   // ui\re_revival.uif
#define KO_OFF_DLG_SEAL_EXP         0x278   // ui\re_seal_exp.uif
#define KO_OFF_DLG_UPGRADE_SELECT   0x27C   // ui\re_upgradeselect.uif
#define KO_OFF_DLG_DUELLIST         0x280   // ui\co_duellist.uif
#define KO_OFF_DLG_TRADE_INV        0x284   // ui\re_tradeinventory.uif
#define KO_OFF_DLG_TRADE_DISPLAY    0x288   // ui\re_tradeitemdisplay.uif
#define KO_OFF_DLG_TRADE_SPECIAL    0x28C   // ui\re_tradeitemdisplayspecial.uif
#define KO_OFF_DLG_TRADE_MSG        0x290   // ui\re_trademessage.uif
#define KO_OFF_DLG_CMDLIST          0x294   // ui\re_cmdlist.uif
#define KO_OFF_DLG_SHOPPING_MALL    0x298   // ui\co_shoppingmall.uif
#define KO_OFF_DLG_SHOPPING_MALL2   0x29C   // ui\co_shoppingmall.uif (2nd)
#define KO_OFF_DLG_WEBPAGE          0x2A0   // ui\co_webpage.uif
#define KO_OFF_DLG_KNIGHTS_CREST    0x2A4   // ui\re_knights_crest.uif
#define KO_OFF_DLG_FILE_SELECT      0x2A8   // ui\re_file_select.uif
#define KO_OFF_DLG_WARFARE_NPC      0x2AC   // ui\co_warfarenpc.uif
#define KO_OFF_DLG_WARFARE_PET      0x2B0   // ui\co_warfarepetition.uif
#define KO_OFF_DLG_CASTLE_UNION     0x2B4   // ui\co_castleunion.uif
#define KO_OFF_DLG_WARFARE_SCHED    0x2B8   // ui\co_warfareschedule.uif
#define KO_OFF_DLG_EXIT_MENU        0x2BC   // ui\re_exitmenu.uif
#define KO_OFF_DLG_RESURRECTION     0x2C0   // ui\re_resurrection.uif
#define KO_OFF_DLG_ID_CHANGE        0x2C4   // ui\re_idchange.uif
#define KO_OFF_DLG_ID_CHECK         0x2CC   // ui\re_id_check.uif
#define KO_OFF_DLG_CATAPULT         0x2D0   // ui\el_catapult.uif
#define KO_OFF_DLG_WARFARE_TAX      0x2D4   // ui\co_warfaretaxtariff.uif
#define KO_OFF_DLG_WARFARE_ADMIN    0x2D8   // ui\co_warfareadministrationnpc.uif
#define KO_OFF_DLG_CLOAK_SHOP       0x2DC   // ui\re_cloakshopnpc.uif
#define KO_OFF_DLG_MANTLE_SHOP      0x2E0   // ui\re_knightsmantleshop.uif
#define KO_OFF_DLG_DISGUISE_RING    0x2E4   // ui\co_disguisering.uif
#define KO_OFF_DLG_CREST_CHR        0x2E8   // ui\re_knights_crest_chr.uif
#define KO_OFF_DLG_FRIEND           0x2EC   // (friend — FBA998)
#define KO_OFF_DLG_CONVERSATION     0x2F0   // ui\re_conversation.uif
#define KO_OFF_DLG_ROOKIE_TIP       0x2F4   // ui\co_rookietip.uif
#define KO_OFF_DLG_RENTAL_TRANS     0x2F8   // ui\re_rental_transaction.uif
#define KO_OFF_DLG_RENTAL_ENTRY     0x2FC   // ui\re_rental_entry.uif
#define KO_OFF_DLG_RENTAL_ITEM      0x300   // ui\re_rental_item.uif
#define KO_OFF_DLG_RENTAL_MSG       0x304   // ui\re_rental_message.uif
#define KO_OFF_DLG_MONSTER          0x308   // ui\co_monster.uif
#define KO_OFF_DLG_PPCARD           0x328   // ui\re_ppcardinput.uif
#define KO_OFF_DLG_DURABILITY       0x32C   // ui\co_durability.uif
#define KO_OFF_DLG_PIECE_CHANGE     0x334   // ui\re_piecechange.uif
#define KO_OFF_DLG_MAIL             0x33C   // ui\re_mailsystem.uif
#define KO_OFF_DLG_SHOW_ICON        0x340   // ui\co_show_icon.uif
#define KO_OFF_DLG_CMD_EDIT         0x344   // ui\co_cmdedit.uif
#define KO_OFF_DLG_KNIGHTS          0x348   // ui\re_seed_helper.uif
#define KO_OFF_DLG_QUEST_SEED       0x34C   // ui\re_quest_seed.uif
#define KO_OFF_DLG_QUEST_NPC_TALK   0x350   // ui\co_quest_npc_talk.uif
#define KO_OFF_DLG_QUEST_NPC_MENU   0x354   // ui\re_quest_npc_menu.uif
#define KO_OFF_DLG_QUEST_NPC_MENU2  0x358   // ui\re_quest_npc_menu.uif (2nd)
#define KO_OFF_DLG_NPC_MENU         0x35C   // ui\re_npcmenu.uif
#define KO_OFF_DLG_QUEST_VIEWER     0x360   // ui\re_quest_viewer.uif
#define KO_OFF_DLG_QUEST_COMPLETED  0x364   // ui\re_quest_completed.uif
#define KO_OFF_DLG_QUEST_MAP        0x368   // ui\re_quest_map.uif
#define KO_OFF_DLG_QUEST_REQUITAL   0x36C   // ui\re_quest_requital.uif
#define KO_OFF_DLG_PARTY            0x370   // (party — FA7880)
#define KO_OFF_DLG_WAR_ICON         0x374   // ui\re_war_icon.uif
#define KO_OFF_DLG_TOP_NOTICE       0x378   // ui\re_maintopnotice.uif
#define KO_OFF_DLG_ITEM_SEAL        0x38C   // ui\re_item_seal.uif
#define KO_OFF_DLG_TASKBAR_MAIN     0x39C   // ui\re_taskbar_main.uif (25xx DOGRULANMIS)
#define KO_OFF_DLG_TASKBAR_SUB      0x3A0   // ui\re_taskbar_sub.uif (25xx DOGRULANMIS)
#define KO_OFF_DLG_QUEST            0x3A8   // ui\re_minimap.uif
#define KO_OFF_DLG_MINIMENU         0x3AC   // ui\re_minimenu.uif
#define KO_OFF_DLG_RADDER           0x3B4   // ui\co_radder.uif
#define KO_OFF_DLG_RADDER02         0x3B8   // ui\co_radder02.uif
#define KO_OFF_DLG_VOTEBOARD        0x3BC   // ui\re_voteboard.uif
#define KO_OFF_DLG_QUEST_MINI_TIP   0x3C0   // ui\re_quest_mini_tip.uif
#define KO_OFF_DLG_PET_FRAME        0x3C4   // ui\re_pet_various_frame.uif
#define KO_OFF_DLG_PET_STATE        0x3C8   // ui\re_pet_page_state.uif
#define KO_OFF_DLG_PET_INV          0x3CC   // ui\re_pet_page_inventory.uif
#define KO_OFF_DLG_PET_SKILL        0x3D0   // ui\re_pet_page_skill.uif
#define KO_OFF_DLG_PET_HOTKEY       0x3D4   // ui\co_pet_hotkey.uif
#define KO_OFF_DLG_PET_QUEST        0x3D8   // ui\re_pet_quest.uif
#define KO_OFF_DLG_PET_HPBAR        0x3DC   // ui\re_pet_hpbar.uif
#define KO_OFF_DLG_CHR_SEAL         0x3E8   // ui\re_character_seal.uif
#define KO_OFF_DLG_CHR_SEAL_TRANS   0x3EC   // ui\re_character_seal_trans.uif
#define KO_OFF_DLG_CHR_SEAL_TIP     0x3F0   // ui\re_character_seal_tooltip.uif
#define KO_OFF_DLG_FORTUNE          0x3F4   // ui\co_fortune.uif
#define KO_OFF_DLG_REPORTBOARD      0x3F8   // ui\co_reportboard.uif
#define KO_OFF_DLG_MSGBOX_OK        0x3FC   // ui\co_msgboxok00.uif
#define KO_OFF_DLG_CLAN_AD          0x404   // ui\re_clan_advertisement.uif
#define KO_OFF_DLG_FORCE_JOIN       0x408   // ui\co_force_join.uif
#define KO_OFF_DLG_CLAN_POINT       0x40C   // ui\re_clan_clanpoint.uif
#define KO_OFF_DLG_REALM_SAVE       0x410   // ui\re_clan_realmpointsave.uif
#define KO_OFF_DLG_LEADER_TRANS     0x414   // ui\co_clan_leadertransfer.uif
#define KO_OFF_DLG_POINTLIST        0x418   // ui\re_pointlist.uif
#define KO_OFF_DLG_POINTSHOW        0x41C   // ui\re_pointshow.uif
#define KO_OFF_DLG_PALETTE          0x420   // ui\co_palette.uif
#define KO_OFF_DLG_LADDER_POINT     0x424   // ui\re_clanladderpointlist.uif
#define KO_OFF_DLG_MOVIE_SAVE       0x428   // ui\co_movie_save.uif
#define KO_OFF_DLG_MOVIE_RESULT     0x42C   // ui\co_movie_result.uif
#define KO_OFF_DLG_MOVIE_RES        0x430   // ui\co_movie_resolution.uif
#define KO_OFF_DLG_COMBINATE        0x434   // ui\re_combinateitem.uif
#define KO_OFF_DLG_COMBINATION      0x438   // ui\co_combinationmethod.uif
#define KO_OFF_DLG_NPC_TALK2        0x43C   // ui\re_quest_npc_talk_2.uif
#define KO_OFF_DLG_NPC_MENU2        0x440   // ui\re_quest_npc_menu_2.uif
#define KO_OFF_DLG_SKILLBAR         0x45C   // (skillbar — FA7C44)
#define KO_OFF_DLG_CHANGE           0x468   // ui\co_change.uif
#define KO_OFF_DLG_INV_REAL         0x494   // (inventory real — FA78B0)
#define KO_OFF_DLG_CHAT_CREATE      0x4DC   // ui\re_chat_creat.uif
#define KO_OFF_DLG_CHAT_MAIN        0x4E0   // ui\re_caht_main.uif
#define KO_OFF_DLG_CHAT_INTO        0x4E4   // ui\re_chat_into.uif
#define KO_OFF_DLG_CHAT_MINIMENU    0x4E8   // ui\re_chat_minimenu.uif
#define KO_OFF_DLG_NEST_DUNGEON     0x4EC   // ui\re_nestindun.uif
#define KO_OFF_DLG_REQUEST_HELP     0x4F0   // ui\re_requesthelp.uif
#define KO_OFF_DLG_HELP_BUTTON      0x4F4   // ui\re_requesthelpbutton.uif
#define KO_OFF_DLG_STORE_SELECT     0x4F8   // ui\re_store_select.uif
#define KO_OFF_DLG_SUPERMARKET      0x500   // ui\co_supermaket_main.uif
#define KO_OFF_DLG_MOVE_CHR         0x504   // ui\re_move_chr_select.uif
#define KO_OFF_DLG_CHANGE_RACE      0x508   // ui\re_change_race.uif
#define KO_OFF_DLG_WISH_CLAN        0x50C   // ui\re_clan_wishclan.uif
#define KO_OFF_DLG_EVENT_WEB        0x510   // ui\co_eventweb.uif
#define KO_OFF_DLG_DROP_OFF         0x518   // ui\co_drop_off.uif
#define KO_OFF_DLG_CAPTURE          0x51C   // ui\co_capture.uif
#define KO_OFF_DLG_BATTLE_SCORE     0x520   // ui\co_battle_score.uif
#define KO_OFF_DLG_SCHEDULER        0x524   // ui\co_scheduler.uif
#define KO_OFF_DLG_SCHED_BUTTON     0x528   // ui\re_schedulerbutton.uif
#define KO_OFF_DLG_GENIE_SUB        0x534   // ui\re_genie_sub.uif
#define KO_OFF_DLG_LADDER_CHAOS     0x538   // ui\re_ladder_chaos.uif
#define KO_OFF_DLG_CHAOS_SKILL      0x540   // ui\re_chaosskilltree.uif
#define KO_OFF_DLG_CHAOS_POP        0x544   // ui\chaos_pop.uif
#define KO_OFF_DLG_HERMETIC_SEAL    0x548   // ui\re_itemhermeticseal.uif
#define KO_OFF_DLG_SIEGE_WARFARE    0x550   // ui\co_siegewarfare.uif
#define KO_OFF_DLG_SIEGE_SITUATION  0x558   // ui\co_siegewarfare_situation_open.uif
#define KO_OFF_DLG_SOCCER           0x55C   // ui\co_soccer_state.uif
#define KO_OFF_DLG_QUESTIONNAIRE    0x560   // ui\co_report_questionnaire.uif
#define KO_OFF_DLG_CLAN_WINDOW      0x564   // ui\re_clan_window.uif
#define KO_OFF_DLG_UNION_WINDOW     0x568   // ui\re_union_window.uif
#define KO_OFF_DLG_INSERT_ID        0x56C   // ui\co_insertid.uif
#define KO_OFF_DLG_2ND_PW           0x570   // ui\re_2nd_pw.uif
#define KO_OFF_DLG_CHANGE_CHR_IDX   0x574   // ui\re_change_chr_idx.uif
#define KO_OFF_DLG_DIVIDE           0x578   // ui\re_divide.uif
#define KO_OFF_DLG_CHANNEL_LIST     0x57C   // ui\re_channel_list.uif
#define KO_OFF_DLG_CHATROOM_CH      0x584   // ui\re_chatroom_channellist.uif
#define KO_OFF_DLG_CHATROOM_CREATE  0x588   // ui\re_chatroom_creat.uif
#define KO_OFF_DLG_CHATROOM_PW      0x58C   // ui\re_chatroom_password.uif
#define KO_OFF_DLG_CHATROOM_SEARCH  0x590   // ui\re_chatroom_search.uif
#define KO_OFF_DLG_CHATROOM_ROOM    0x594   // ui\re_chatroom_room.uif
#define KO_OFF_DLG_MINIMENU_USER    0x598   // ui\re_minimenu_userlist.uif
#define KO_OFF_DLG_MAP_INFO         0x59C   // ui\re_map_info.uif
#define KO_OFF_DLG_ACHIEVE          0x5A0   // ui\re_achieve.uif
#define KO_OFF_DLG_EXCHANGE         0x5A4   // ui\re_title_window.uif
#define KO_OFF_DLG_RONARK_SCORE     0x5B0   // ui\re_ronark_war_score.uif
#define KO_OFF_DLG_EVENT_TAX        0x5B4   // ui\re_eventtax.uif
#define KO_OFF_DLG_CMD_HOTKEY       0x5B8   // ui\re_commander_hotkey.uif
#define KO_OFF_DLG_CMD_CENTER       0x5C0   // ui\re_commander_center_skill.uif
#define KO_OFF_DLG_ACHIEVE_MINI     0x5C4   // ui\re_achieve_mini.uif
#define KO_OFF_DLG_DISTORTION       0x5C8   // ui\re_dostortionsmoradon.uif
#define KO_OFF_DLG_CHAOS_SKILL2     0x5F8   // ui\re_chaosskilltree.uif (2nd)
#define KO_OFF_DLG_FULLMOON         0x5FC   // ui\re_fullmoon_popup.uif
#define KO_OFF_DLG_CHATMGR          0x61C   // (chat manager — FA9034)
#define KO_OFF_DLG_DAYPRICE         0x60C   // ui\re_dayprice.uif
#define KO_OFF_DLG_STAT_PRESET      0x610   // ui\re_stat_preset.uif
#define KO_OFF_DLG_MERCHANT         0x66C   // (merchant)
#define KO_OFF_DLG_MSGBOX_TRADE     0x698   // ui\re_messagebox_trade.uif
#define KO_OFF_DLG_NARRATION        0x6A0   // ui\re_narration.uif
#define KO_OFF_DLG_DX_OPTION        0x6A4   // ui\re_dxtypeoptioin.uif

// =============================================================================
// UI MESSAGE CONSTANTS
// =============================================================================
#define UIMSG_BUTTON_CLICK      0x00000001  // N3UIBase buton tiklama mesaji
#define KO_OFF_VTABLE_RECVMSG   0x7C        // 25xx: ReceiveMessage vTable offset (PG2369: 0x70)

// =============================================================================
// SECONDARY PLAYER STRUCT (chrBase + 0xBxx bolge)
// Bu bolge ayri bir sub-struct gibi davranıyor
// =============================================================================
#define KO_OFF_MAXMP        0xBBC           // int32  - Maksimum MP (7803)
#define KO_OFF_MP           0xBC0           // int32  - Mevcut MP
#define KO_OFF_GOLD         0xBCC           // uint32 - Altin (99804051)
#define KO_OFF_MAXEXP       0xBD0           // uint64 - Maksimum EXP (34823947840)
#define KO_OFF_EXP          0xBD8           // uint64 - Tecrube (2820000)
#define KO_OFF_NP           0xBE0           // uint32 - Nation Points/Loyalty (498)
#define KO_OFF_NP_MONTHLY   0xBE8           // uint32 - Aylik NP (26250)
#define KO_OFF_TOTALHIT     0xBF0           // uint32 - Total Hit Rate (710)
#define KO_OFF_STAT_STR     0xBF4           // uint32 - STR (255)
#define KO_OFF_STAT_STA     0xBF8           // uint32 - Stamina/HP base (187, bufflu)
#define KO_OFF_STAT_HP      0xBFC           // uint32 - HP Stat (177)
#define KO_OFF_STAT_LUK     0xC00           // uint32 - Luck/bonus (70)
#define KO_OFF_STAT_DEX     0xC04           // uint32 - DEX (60)
#define KO_OFF_STAT_INT     0xC0C           // uint32 - INT (50)
#define KO_OFF_STAT_MP      0xC14           // uint32 - MP Stat (50)
#define KO_OFF_ATTACK       0xC1C           // uint32 - Saldiri (4253)
#define KO_OFF_DEFENCE      0xC24           // uint32 - Savunma (1252)
#define KO_OFF_FIRE_R       0xC2C           // uint32 - Fire Resist (DOGRULANMIS: 100)
#define KO_OFF_ICE_R        0xC34           // uint32 - Ice Resist (DOGRULANMIS: 46)
#define KO_OFF_LIGHTNING_R  0xC3C           // uint32 - Lightning Resist (DOGRULANMIS: 110)
#define KO_OFF_MAGIC_R      0xC44           // uint32 - Magic Resist (DOGRULANMIS: 20)
#define KO_OFF_CURSE_R      0xC4C           // uint32 - Curse Resist (DOGRULANMIS: 80)
#define KO_OFF_POISON_R     0xC54           // uint32 - Poison Resist (DOGRULANMIS: 0)
#define KO_OFF_ZONE         0xC60           // uint8  - Zone ID (21)

// =============================================================================
// WEIGHT OFFSETS (DOGRULANMIS - 10x carpilmis uint32)
// Gercek deger = okunan / 10
// =============================================================================
#define KO_OFF_MAXWEIGHT    0xBE8           // uint32 - Max agirlik x10 (26250 = 2625.0)
#define KO_OFF_WEIGHT       0xBF0           // uint32 - Mevcut agirlik x10 (710 = 71.0)

// =============================================================================
// SKILL OFFSETS (DOGRULANMIS - DLG base uzerinden, Cheat Engine)
// dlgBase = *(DWORD*)KO_PTR_DLG
// =============================================================================
#define KO_OFF_SKILL_AVAIL      0x308C      // uint32 - Dagitilmamis skill point (78)
#define KO_OFF_SKILL_ATTACK     0x30A0      // uint32 - Attack skill (5)
#define KO_OFF_SKILL_DEFENCE    0x30A4      // uint32 - Defence skill (6)
#define KO_OFF_SKILL_PASSION    0x30A8      // uint32 - Passion skill (52)
#define KO_OFF_SKILL_MASTER     0x30AC      // uint32 - Master skill (7)
#define KO_OFF_SKILLBASE        0x210       // Skill base offset (dogrulanacak)

// =============================================================================
// UI LOGIN OFFSETS (dogrulanacak)
// =============================================================================
#define KO_OFF_UILoginIntro     0x2C
#define KO_OFF_UI_EDIT_ID       0x118
#define KO_OFF_UI_EDIT_PASS     0x11C
#define KO_OFF_UI_EDIT_VALUE    0x134
#define KO_OFF_UI_CONNECT_BTN   0x120
#define KO_OFF_UI_VISIBLE       0xEA
#define KO_OFF_FIRST_SERVER     0x254
#define KO_OFF_SERVER_OFFSET    0x1C
#define KO_OFF_CH_LIST          0x480

// =============================================================================
// INVENTORY OFFSETS (XREF scan - DLG base)
// =============================================================================
#define KO_OFF_UIInventory          0x494   // XREF: 191 refs
#define KO_OFF_UIInventory_First    0x298   // dogrulanacak
#define KO_OFF_EQUIP_START          0x260   // dogrulanacak
#define KO_OFF_EQUIP_END            0x298   // dogrulanacak

// =============================================================================
// WAREHOUSE (BANK) OFFSETS (XREF scan)
// =============================================================================
#define KO_OFF_UIWareHouse          0x22C   // XREF: 5 refs
#define KO_OFF_UIWareHouse_First    0x114   // dogrulanacak
#define KO_OFF_BANKCONT             0x0E5   // Pattern scan
#define KO_BANK_TOTAL_SLOTS         192

// =============================================================================
// ITEM OFFSETS (dogrulanacak)
// =============================================================================
#define KO_OFF_ITEM_BASIC           0x60
#define KO_OFF_ITEM_UPGRADE_PTR     0x64
#define KO_OFF_ITEM_COUNT           0x68
#define KO_OFF_ITEM_DURABILITY      0x6C
#define KO_OFF_ITEMBASIC_ID         0x00
#define KO_OFF_ITEMBASIC_NAME       0x08
#define KO_OFF_UPGRADE_LEVEL        0x00

// =============================================================================
// PARTY OFFSETS (XREF scan)
// =============================================================================
#define KO_OFF_PartyManager         0x370   // XREF: 26 refs
#define KO_OFF_PartyMemberCount     0x36C   // dogrulanacak
#define KO_OFF_Pt                   0x374   // XREF: 12 refs

// =============================================================================
// PACKET OPCODES (Standart KO protokolu)
// =============================================================================
#define WIZ_LOGIN               0x01
#define WIZ_NEW_CHAR            0x02
#define WIZ_DEL_CHAR            0x03
#define WIZ_SEL_CHAR            0x04
#define WIZ_SEL_NATION          0x05
#define WIZ_MOVE                0x06
#define WIZ_USER_INOUT          0x07
#define WIZ_ATTACK              0x08
#define WIZ_ROTATE              0x09
#define WIZ_NPC_INOUT           0x0A
#define WIZ_NPC_MOVE            0x0B
#define WIZ_ALLCHAR_INFO_REQ    0x0C
#define WIZ_GAMESTART           0x0D
#define WIZ_MYINFO              0x0E
#define WIZ_LOGOUT              0x0F
#define WIZ_CHAT                0x10
#define WIZ_DEAD                0x11
#define WIZ_REGENE              0x12
#define WIZ_REGIONCHANGE        0x15
#define WIZ_HP_CHANGE           0x17
#define WIZ_MSP_CHANGE          0x18
#define WIZ_EXP_CHANGE          0x1A
#define WIZ_LEVEL_CHANGE        0x1B
#define WIZ_NPC_REGION          0x1C
#define WIZ_REQ_NPCIN           0x1D
#define WIZ_ITEM_MOVE           0x1F
#define WIZ_NPC_EVENT           0x20
#define WIZ_ITEM_TRADE          0x21
#define WIZ_TARGET_HP           0x22
#define WIZ_TRADE_NPC           0x25
#define WIZ_ZONE_CHANGE         0x27
#define WIZ_POINT_CHANGE        0x28
#define WIZ_STATE_CHANGE        0x29
#define WIZ_LOYALTY_CHANGE      0x2A
#define WIZ_VERSION_CHECK       0x2B
#define WIZ_NOTICE              0x2E
#define WIZ_PARTY               0x2F
#define WIZ_EXCHANGE            0x30
#define WIZ_MAGIC_PROCESS       0x31
#define WIZ_SKILLPT_CHANGE      0x32
#define WIZ_KNIGHTS_PROCESS     0x3C
#define WIZ_SPEEDHACK_CHECK     0x41
#define WIZ_COMPRESS_PACKET     0x42
#define WIZ_WAREHOUSE           0x45
#define WIZ_FRIEND_PROCESS      0x49
#define WIZ_GOLD_CHANGE         0x4A
#define WIZ_WARP_LIST           0x4B
#define WIZ_WEIGHT_CHANGE       0x54
#define WIZ_ITEM_UPGRADE        0x5B
#define WIZ_ZONEABILITY         0x5E
#define WIZ_EVENT               0x5F
#define WIZ_QUEST               0x64
#define WIZ_MERCHANT            0x68
#define WIZ_MERCHANT_INOUT      0x69
#define WIZ_SHOPPING_MALL       0x6A
#define WIZ_SERVER_INDEX        0x6B
#define WIZ_SIEGE               0x6D
#define WIZ_PREMIUM             0x71
#define WIZ_RENTAL              0x73
#define WIZ_PET                 0x76
#define WIZ_KING                0x78
#define WIZ_SKILLDATA           0x79
#define WIZ_RANK                0x80
#define WIZ_STORY               0x81
#define WIZ_ZONE_TERRAIN        0x83
#define WIZ_MINING              0x86
#define WIZ_HELMET              0x87
#define WIZ_GENIE               0x97
#define WIZ_SURROUNDING_USER    0x98
#define WIZ_USER_INFO           0x98
#define WIZ_LOADING_LOGIN       0x9F
#define WIZ_ORDER               0x9F

// =============================================================================
// UI FUNCTION ADDRESSES (25xx — henuz kesfedilmedi, TODO)
// Pearl Guard 2369 referanslari: SetVisible=0x00411980, SetString=0x0042A0B0
// 25xx icin IDA/x32dbg ile N3UIBase vTable uzerinden bulunacak
// =============================================================================
const DWORD KO_SET_VISIBLE_FUNC    = 0x00000000;  // TODO: 25xx icin IDA ile bul (PG2369: 0x00411980)
const DWORD KO_SET_STRING_FUNC     = 0x00000000;  // TODO: 25xx icin IDA ile bul (PG2369: 0x0042A0B0)
const DWORD KO_UIF_FILE_LOAD       = 0x00000000;  // TODO: 25xx icin IDA ile bul — UIF dosya yukleme fonksiyonu

// =============================================================================
// UI SYSTEM POINTERS (25xx — tool taramasi ile elde edildi)
// =============================================================================
#define KO_PTR_UI_WND           0x1093798   // UI Window manager pointer
#define KO_PTR_UI_LOCK          0x1092EF4   // UI lock mekanizmasi
#define KO_PTR_UIMSGBOX_MGR    0x1092A0C   // UIMessageBoxManager
#define KO_PTR_UIMANAGER_FUNC  0x702ACC    // UIManager fonksiyon/global adresi

// =============================================================================
// GAME FUNCTION ADDRESSES (25xx — tool taramasi ile elde edildi)
// =============================================================================
const DWORD KO_FNC_GAME_PROC_TICK      = 0x0079C900;  // GameProcedureTick
const DWORD KO_FNC_SITDOWN             = 0x007F60D0;  // Sitdown
const DWORD KO_FNC_ROTATE_TO           = 0x008D8070;  // RotateTo
const DWORD KO_FNC_SELECT_MOB          = 0x007F4DA0;  // SelectMob
const DWORD KO_FNC_PET_FEED            = 0x00C7C730;  // PetFeed
const DWORD KO_FNC_ENEMY               = 0x0077F7E0;  // Enemy fonksiyonu
const DWORD KO_FNC_FMBS                = 0x005209B0;  // FMBS fonksiyonu
const DWORD KO_FNC_LRCA                = 0x007FCE20;  // LRCA fonksiyonu
const DWORD KO_FNC_SBCA                = 0x0050B320;  // SBCA fonksiyonu
const DWORD KO_FNC_LSCA                = 0x00862F20;  // LSCA fonksiyonu

// =============================================================================
// LOGIN/SERVER SELECTION (25xx — tool taramasi)
// =============================================================================
const DWORD KO_FNC_LOGIN_REQ1          = 0x007A55A0;  // Login request 1
const DWORD KO_FNC_LOGIN_REQ2          = 0x007A5320;  // Login request 2
const DWORD KO_FNC_LOAD_SERVER_LIST    = 0x00B89810;  // Load server list
const DWORD KO_FNC_SERVER_SELECT       = 0x00B896A0;  // Server select
const DWORD KO_FNC_SHOW_CHANNEL        = 0x00B89BD0;  // Show channel
const DWORD KO_FNC_CONNECT_SERVER      = 0x007A07A0;  // Connect server
#define KO_PTR_CONNECT_SERVER_NEED      0x1092994
#define KO_PTR_CHARACTER_SELECT         0x1092A18
const DWORD KO_FNC_CHR_SELECT_SKIP     = 0x007B1000;
const DWORD KO_FNC_CHR_SELECT_LEFT     = 0x007B0F40;
const DWORD KO_FNC_CHR_SELECT_RIGHT    = 0x007B0E80;
const DWORD KO_FNC_CHR_SELECT_ENTER    = 0x007AEAD0;
const DWORD KO_FNC_CHR_INTRO_RECV      = 0x007AEF60;  // CGameProcIntroChrSelectRecvPacket
const DWORD KO_FNC_INTRO_RECV          = 0x007A0A70;  // CGameProcIntroRecvPacket

// =============================================================================
// TABLE POINTERS (25xx — tool taramasi)
// =============================================================================
#define KO_PTR_TBL_SELLING_ITEM     0x10927B8
#define KO_PTR_TBL_NPC_LIST         0x10927EC
#define KO_PTR_TBL_MOB_LIST         0x10927E0
#define KO_PTR_TBL_QUEST_HELPER     0x1092824
#define KO_PTR_TBL_DISGUISE         0x1092834
#define KO_PTR_TBL_LEVEL_GUIDE      0x1092978
#define KO_PTR_TBL_FX               0x109285C
#define KO_PTR_TBL_CLOAK            0x109296C

// =============================================================================
// MISC POINTERS (25xx — tool taramasi)
// =============================================================================
#define KO_PTR_MAIN_THREAD          0x79F728
#define KO_PTR_INTRO                0x1092A1C
#define KO_PTR_SELECTED_ITEM_BASE   0x114058C
#define KO_PTR_REMOVE_BASE1         0x1140584
const DWORD KO_FNC_REMOVE_CALL     = 0x00B32810;
const DWORD KO_FNC_COUNTABLE_DLG   = 0x0074F4BC;
const DWORD KO_FNC_COUNTABLE_CHG   = 0x004E2040;
const DWORD KO_FNC_COUNTABLE_ACC   = 0x0074F6FC;
const DWORD KO_FNC_PTR_06          = 0x007FBB90;
const DWORD KO_FNC_DEATH_EFFECT    = 0x008EFED1;
const DWORD KO_FNC_INV_MOUSE_HOOK  = 0x00B2435B;
#define KO_PTR_SBEC                 0x1092948
#define KO_PTR_GAME_LOGO            0x1092A28
#define KO_PTR_D3D_DEVICE           0x1093690
#define KO_PTR_SEC_PER_FRAME        0x10D3C70
const DWORD KO_FNC_X3_ENTRY        = 0x00E73CD7;

// =============================================================================
// CLASS DEFINITIONS
// =============================================================================
#define CLASS_WARRIOR           1
#define CLASS_ROGUE             2
#define CLASS_MAGE              3
#define CLASS_PRIEST            4
#define CLASS_WARRIOR_BLADE     5
#define CLASS_WARRIOR_BERSERKER 6
#define CLASS_ROGUE_ASSASSIN    7
#define CLASS_ROGUE_ARCHER      8
#define CLASS_MAGE_FIRE         9
#define CLASS_MAGE_ICE          10
#define CLASS_PRIEST_HEAL       11
#define CLASS_PRIEST_BUFFER     12
#define CLASS_KURIAN_PORUTU     13
#define CLASS_KURIAN_DIVINE     14
#define CLASS_KURIAN_SHADOW     15
