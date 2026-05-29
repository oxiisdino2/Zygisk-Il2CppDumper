#pragma once
#include <cstdint>

// Verified offsets from dump.cs (com.dts.freefireth Bluestacks x86_64)

static constexpr uint32_t OFF_Entity_m_CachedTransform  = 0x58;
static constexpr uint32_t OFF_AttackableEntity_IsDead   = 0x7C;
static constexpr uint32_t OFF_Player_OriginalNickName    = 0x440;
static constexpr uint32_t OFF_Player_IsClientBot         = 0x448;
static constexpr uint32_t OFF_MatchGame_m_ReplicationEntitis = 0xC0;
static constexpr uint32_t OFF_GameFacade_CurrentMatchGame = 0x8;

static constexpr uint32_t DICT_OFF_ENTRIES   = 0x18;
static constexpr uint32_t DICT_OFF_COUNT     = 0x20;
static constexpr uint32_t ARRAY_DATA_OFFSET  = 0x18;
static constexpr uint32_t ENTRY_SIZE         = 16;
static constexpr uint32_t ENTRY_VAL_OFF      = 8;

static constexpr uint32_t RVA_Camera_get_main            = 0x9BFC5F0;
static constexpr uint32_t RVA_Camera_WorldToScreenPoint  = 0x9C0AD80;
static constexpr uint32_t RVA_Entity_get_Position        = 0x7801394;
