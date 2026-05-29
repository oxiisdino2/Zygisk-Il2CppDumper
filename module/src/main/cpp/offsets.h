#pragma once
#include <cstdint>

// ============================================================
// Verified offsets from dump.cs (2026-05-29, Free Fire OBSIDIAN)
// Game: com.dts.freefireth
// ============================================================

// ============================================================
// Class hierarchy sizes & headers
// ============================================================
// In this Unity IL2CPP version:
//   - Object header: 0x20 bytes (klass=8, monitor=8, cachedPtr=8, pad=0)
//   - First instance field at offset 0x20
//   - Pointer size: 8 bytes (x86_64)

// ============================================================
// ENUM: Entity_Offset - field offsets for Entity hierarchy
// ============================================================

// Entity : MonoBehaviour
// Fields start at 0x20
static constexpr uint32_t OFF_Entity_m_CachedTransform  = 0x58;  // Transform*
static constexpr uint32_t OFF_Entity_m_EntityInfo        = 0x30;  // EntityInfo*
static constexpr uint32_t OFF_Entity_m_UniqueID          = 0x60;  // uint32
static constexpr uint32_t OFF_Entity_m_ProxyType         = 0x64;  // EEntityProxyType (int)

// ReplicationEntity : Entity
// Fields start at 0x68
static constexpr uint32_t OFF_ReplicationEntity_m_IsPRIRecivedFirstTime = 0x68; // bool
static constexpr uint32_t OFF_ReplicationEntity_m_PRIDataPool           = 0x70; // IPRIDataPool*
static constexpr uint32_t OFF_ReplicationEntity_ReplicationEntityTag    = 0x78; // AAAIAPOBKND*

// COWReplicationEntity : ReplicationEntity
// No additional fields

// AttackableEntity : COWReplicationEntity
// Fields start at 0x7C
static constexpr uint32_t OFF_AttackableEntity_IsDead   = 0x7C;  // bool (FHMPKFMFEPM)
static constexpr uint32_t OFF_AttackableEntity_Collider  = 0x80;  // Collider* (<INICDNFOFJB>k__BackingField)

// Player : AttackableEntity
// Instance fields start at 0x88
static constexpr uint32_t OFF_Player_IsFrozenKnockDown   = 0xA8;  // bool
static constexpr uint32_t OFF_Player_TeamModeID          = 0x3DC; // uint32
static constexpr uint32_t OFF_Player_OriginalNickName    = 0x440; // String*
static constexpr uint32_t OFF_Player_IsClientBot         = 0x448; // bool
static constexpr uint32_t OFF_Player_IsCadet             = 0x3A8;  // bool
static constexpr uint32_t OFF_Player_IsShowEquip         = 0x3E0; // bool
static constexpr uint32_t OFF_Player_MainCameraTransform = 0x390; // Transform*
static constexpr uint32_t OFF_Player_InventoryMap3P      = 0x230; // LazyDictionary*

// ============================================================
// Game infrastructure offsets
// ============================================================

// BaseGame instance fields (MatchGame : COWGameBase : BaseGame)
// Instance fields start at 0x10 in BaseGame
static constexpr uint32_t OFF_BaseGame_m_UIScene                 = 0x10;
static constexpr uint32_t OFF_BaseGame_m_GameTimer                = 0x18;
static constexpr uint32_t OFF_BaseGame_m_SimulationTimer          = 0x20;
static constexpr uint32_t OFF_BaseGame_m_GameEventDispatcher      = 0x28;
static constexpr uint32_t OFF_BaseGame_m_GameContext              = 0x30;
static constexpr uint32_t OFF_BaseGame_m_LoadingProcessManager    = 0x38;
static constexpr uint32_t OFF_BaseGame_m_VisualEffectManager      = 0x68;

// MatchGame : COWGameBase : BaseGame
// Instance fields start at 0x90
static constexpr uint32_t OFF_MatchGame_m_Match                  = 0x90;
static constexpr uint32_t OFF_MatchGame_m_GameModeSetting        = 0x98;
static constexpr uint32_t OFF_MatchGame_m_CameraModeManager      = 0xA8;
static constexpr uint32_t OFF_MatchGame_m_LevelObjectManager      = 0xB0;
static constexpr uint32_t OFF_MatchGame_m_MetaManager            = 0xB8;
static constexpr uint32_t OFF_MatchGame_m_ReplicationEntitis     = 0xC0;  // Dictionary<uint, Entity>*
static constexpr uint32_t OFF_MatchGame_m_LReplicationEntitis    = 0xC8;
static constexpr uint32_t OFF_MatchGame_m_CameraControllerManager = 0xD8;
static constexpr uint32_t OFF_MatchGame_m_Amb2DAudioManager      = 0x138;

// ============================================================
// Static field offsets
// ============================================================

// GameFacade (static fields area offsets)
static constexpr uint32_t OFF_GameFacade_CurrentGame        = 0x0;   // static BaseGame*
static constexpr uint32_t OFF_GameFacade_CurrentMatchGame   = 0x8;   // static MatchGame*
static constexpr uint32_t OFF_GameFacade_IsObserver         = 0x132; // static bool
static constexpr uint32_t OFF_GameFacade_LocalPlayerUserID  = 0x50;  // static uint64
static constexpr uint32_t OFF_GameFacade_IsMatchStarted     = 0x1E1; // static bool (estimated)

// ============================================================
// PlayerData (internal class, line 743139)
// ============================================================
static constexpr uint32_t OFF_PlayerData_id              = 0x10;  // IHAAMHPPLMG
static constexpr uint32_t OFF_PlayerData_userId          = 0x28;  // uint64
static constexpr uint32_t OFF_PlayerData_gsTeamId        = 0x30;  // byte
static constexpr uint32_t OFF_PlayerData_nickname        = 0x38;  // String*
static constexpr uint32_t OFF_PlayerData_killCount       = 0x60;  // uint32
static constexpr uint32_t OFF_PlayerData_isDead          = 0x6C;  // bool
static constexpr uint32_t OFF_PlayerData_player          = 0x100; // Player*

// ============================================================
// Method RVAs (relative to il2cpp_base)
// ============================================================
static constexpr uint32_t RVA_Entity_get_Position        = 0x7801394;
static constexpr uint32_t RVA_Entity_set_Position        = 0x7801480;
static constexpr uint32_t RVA_Entity_get_CachedTransform = 0x78012D4;
static constexpr uint32_t RVA_Entity_get_Forward         = 0x7801564;
static constexpr uint32_t RVA_Entity_get_UniqueID        = 0x7801E68;
static constexpr uint32_t RVA_Entity_GetEntityInfo       = 0x78012BC;

static constexpr uint32_t RVA_AttackableEntity_get_IsDead   = 0x68C7FC8;
static constexpr uint32_t RVA_AttackableEntity_set_IsDead   = 0x68C8010;

// Camera methods (in UnityEngine.CoreModule.dll)
static constexpr uint32_t RVA_Camera_get_main             = 0x9BFC5F0; // estimated
static constexpr uint32_t RVA_Camera_WorldToScreenPoint   = 0x9C0AD80; // from dump line 1491082
