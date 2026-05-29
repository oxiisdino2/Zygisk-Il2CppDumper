#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include "offsets.h"
#include "log.h"
#include "il2cpp-class.h"

// Forward declare Il2CppObject inline struct
struct Il2CppString : Il2CppObject {
    int32_t length;
    char16_t chars[1];
};

// Declare IL2CPP API function pointers from the project's header
#define DO_API(r, n, p) extern r (*n) p;
#include "il2cpp-api-functions.h"
#undef DO_API

// ============================================================
// Math types
// ============================================================
struct Vector3 {
    float x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct Vector2 {
    float x, y;
    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}
};

// ============================================================
// Entity info for ESP
// ============================================================
struct EspEntity {
    void* playerPtr = nullptr;
    Vector3 worldPos{};
    Vector2 screenPos{};
    float distance = 0.0f;
    bool isDead = false;
    bool isBot = false;
    bool onScreen = false;
    char name[64]{};
};

// ============================================================
// Game data reader - resolves classes/fields at runtime
// ============================================================
class GameData {
public:
    bool init();
    bool update();
    const std::vector<EspEntity>& getEntities() const { return m_entities; }
    int getEntityCount() const { return (int)m_entities.size(); }
    bool isInMatch() const { return m_inMatch; }
    Vector3 getLocalPos() const { return m_localPos; }
    void* getMainCamera() const { return m_mainCamera; }
    void setScreenSize(int w, int h) { m_screenW = w; m_screenH = h; }

private:
    bool resolveClasses();
    bool resolveStaticFields();
    bool findAssemblyCSharp();
    void* getCurrentMatchGame();
    void* getLocalPlayer();
    void* getEntityDict();
    void iterateEntities();
    bool worldToScreen(const Vector3& world, Vector2& screen, float& outDist);
    void* readPointer(void* addr, uint32_t offset);
    template<typename T> T readValue(void* addr, uint32_t offset);
    std::string readIl2CppString(void* strPtr);

    // IL2CPP class pointers
    Il2CppClass* m_gameFacadeClass = nullptr;
    Il2CppClass* m_matchGameClass = nullptr;
    Il2CppClass* m_playerClass = nullptr;
    Il2CppClass* m_entityClass = nullptr;
    Il2CppClass* m_vector3Class = nullptr;
    Il2CppClass* m_cameraClass = nullptr;

    // Method pointers
    const MethodInfo* m_getMainMethod = nullptr;
    const MethodInfo* m_worldToScreenMethod = nullptr;

    // Static fields storage pointer (GameFacade static fields)
    void* m_staticFieldsBase = nullptr;

    // Current match state
    void* m_currentMatchGame = nullptr;
    void* m_mainCamera = nullptr;
    void* m_localPlayer = nullptr;
    bool m_inMatch = false;

    // Image reference
    const Il2CppImage* m_asmCSharp = nullptr;

    // Screen dimensions (set from ESP hook)
    int m_screenW = 1920;
    int m_screenH = 1080;

    // Entity list
    std::vector<EspEntity> m_entities;
    Vector3 m_localPos{};
};

// ============================================================
// Template: read value at instance + offset
// ============================================================
template<typename T>
inline T GameData::readValue(void* addr, uint32_t offset) {
    if (!addr) return T{};
    return *(T*)((uintptr_t)addr + offset);
}

inline void* GameData::readPointer(void* addr, uint32_t offset) {
    if (!addr) return nullptr;
    return *(void**)((uintptr_t)addr + offset);
}

// ============================================================
// Read an IL2CPP System.String from a pointer
// ============================================================
inline std::string GameData::readIl2CppString(void* strPtr) {
    if (!strPtr) return "";
    auto* str = (Il2CppString*)strPtr;
    int len = str->length;
    if (len <= 0 || len > 128) return "";
    std::string out;
    for (int i = 0; i < len; i++) {
        char16_t c = str->chars[i];
        if (c > 127) c = '?';
        out += (char)c;
    }
    return out;
}

// ============================================================
// Find the Assembly-CSharp image
// ============================================================
inline bool GameData::findAssemblyCSharp() {
    auto domain = il2cpp_domain_get();
    if (!domain) return false;

    size_t assemblyCount = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &assemblyCount);
    if (!assemblies) return false;

    for (size_t i = 0; i < assemblyCount; i++) {
        auto img = il2cpp_assembly_get_image(assemblies[i]);
        auto name = il2cpp_image_get_name(img);
        if (name && strstr(name, "Assembly-CSharp") != nullptr) {
            m_asmCSharp = img;
            LOGI("Found Assembly-CSharp: %s", name);
            return true;
        }
    }

    LOGE("Assembly-CSharp not found");
    return false;
}

// ============================================================
// Resolve class pointers at runtime
// ============================================================
inline bool GameData::resolveClasses() {
    if (!m_asmCSharp) return false;

    // Resolve game facade
    m_gameFacadeClass = il2cpp_class_from_name(m_asmCSharp, "COW", "GameFacade");
    if (!m_gameFacadeClass) {
        LOGE("GameFacade class not found");
        return false;
    }
    LOGI("GameFacade class resolved");

    // MatchGame
    m_matchGameClass = il2cpp_class_from_name(m_asmCSharp, "COW", "MatchGame");
    if (!m_matchGameClass) {
        LOGE("MatchGame class not found");
        return false;
    }
    LOGI("MatchGame class resolved");

    // Player (internal class) - namespace COW.GamePlay
    m_playerClass = il2cpp_class_from_name(m_asmCSharp, "COW.GamePlay", "Player");
    if (!m_playerClass) {
        LOGE("Player class not found");
        return false;
    }
    LOGI("Player class resolved");

    // Entity
    m_entityClass = il2cpp_class_from_name(m_asmCSharp, "", "Entity");
    LOGI("Entity class: %p", m_entityClass);

    // Find UnityEngine.CoreModule for Vector3 and Camera
    size_t count = 0;
    auto assemblies = il2cpp_domain_get_assemblies(il2cpp_domain_get(), &count);
    for (size_t i = 0; i < count; i++) {
        auto img = il2cpp_assembly_get_image(assemblies[i]);
        auto name = il2cpp_image_get_name(img);
        if (name && strcmp(name, "UnityEngine.CoreModule.dll") == 0) {
            m_vector3Class = il2cpp_class_from_name(img, "UnityEngine", "Vector3");
            if (!m_vector3Class) m_vector3Class = il2cpp_class_from_name(img, "", "Vector3");
            if (m_vector3Class) LOGI("Vector3 class resolved");

            m_cameraClass = il2cpp_class_from_name(img, "UnityEngine", "Camera");
            if (m_cameraClass) {
                LOGI("Camera class resolved");
                m_getMainMethod = il2cpp_class_get_method_from_name(m_cameraClass, "get_main", 0);
                m_worldToScreenMethod = il2cpp_class_get_method_from_name(m_cameraClass, "WorldToScreenPoint", 1);
                if (m_getMainMethod) LOGI("Camera.get_main resolved");
                if (m_worldToScreenMethod) LOGI("Camera.WorldToScreenPoint resolved");
            }
            break;
        }
    }

    return true;
}

// ============================================================
// Resolve static fields storage pointer for GameFacade
// ============================================================
inline bool GameData::resolveStaticFields() {
    if (!m_gameFacadeClass) return false;

    // Iterate fields to verify our offsets
    void* iter = nullptr;
    while (auto field = il2cpp_class_get_fields(m_gameFacadeClass, &iter)) {
        auto name = il2cpp_field_get_name(field);
        auto offset = il2cpp_field_get_offset(field);
        LOGI("  GameFacade field: %s = 0x%zx", name, offset);
    }

    // Get the static field data pointer
    // We can use the first static field to get the base
    void* firstStatic = nullptr;
    iter = nullptr;
    while (auto field = il2cpp_class_get_fields(m_gameFacadeClass, &iter)) {
        auto flags = 0; // We'd need il2cpp_field_get_flags but for now use direct approach
        // For static field resolution, we read via il2cpp_field_static_get_value
        // But we need the storage base for direct reading
        // Use il2cpp_class_get_static_field_data if available, or find it manually
        break;
    }

    // Alternative: use class static field data
    // This is an internal API exposed in our header
    extern void* il2cpp_class_get_static_field_data(Il2CppClass* klass);
    m_staticFieldsBase = il2cpp_class_get_static_field_data(m_gameFacadeClass);
    if (m_staticFieldsBase) {
        LOGI("GameFacade static fields base: %p", m_staticFieldsBase);
    } else {
        LOGW("Could not get static field data directly, trying field resolution");
    }

    return true;
}

// ============================================================
// Get the current MatchGame instance
// ============================================================
inline void* GameData::getCurrentMatchGame() {
    if (!m_gameFacadeClass) return nullptr;

    if (m_staticFieldsBase) {
        // Direct read from static fields storage
        return readPointer(m_staticFieldsBase, OFF_GameFacade_CurrentMatchGame);
    }

    // Fallback: use il2cpp_field_static_get_value
    void* iter = nullptr;
    while (auto field = il2cpp_class_get_fields(m_gameFacadeClass, &iter)) {
        auto name = il2cpp_field_get_name(field);
        if (name && strcmp(name, "CurrentMatchGame") == 0) {
            void* val = nullptr;
            il2cpp_field_static_get_value(field, &val);
            return val;
        }
    }

    return nullptr;
}

// ============================================================
// Get the local player from MatchGame
// We iterate m_ReplicationEntitis and find the entity owned
// by the local user. For now, we don't strictly need local player
// since we show all players.
// ============================================================
inline void* GameData::getLocalPlayer() {
    // For now return nullptr - we don't filter by local player yet
    return nullptr;
}

// ============================================================
// Get the entity dictionary from MatchGame
// ============================================================
inline void* GameData::getEntityDict() {
    if (!m_currentMatchGame) return nullptr;
    return readPointer(m_currentMatchGame, OFF_MatchGame_m_ReplicationEntitis);
}

// ============================================================
// IL2CPP Dictionary layout (for Dictionary<uint, Entity>)
// struct Dictionary_Entry {
//     int hashCode;
//     uint key;
//     void* value;
// };
// struct Dictionary {
//     Il2CppObject header;  // 0x00: klass(8), 0x08: monitor(8)
//     void* _buckets;       // 0x10: int[] (array ptr)
//     void* _entries;       // 0x18: Entry[] (array ptr)
//     int _count;           // 0x20
//     int _version;         // 0x24
//     int _freeList;        // 0x28
//     int _freeCount;       // 0x2C
// };
// We need to find the exact layout by reading from dump.
// For now, use a common IL2CPP Dictionary layout.
// ============================================================
inline void GameData::iterateEntities() {
    m_entities.clear();

    auto dict = getEntityDict();
    if (!dict) {
        LOGW("Entity dict is null");
        return;
    }

    // Try to read Dictionary entries - common IL2CPP layout
    // These offsets might need adjustment for the specific Unity version
    // Dictionary fields (typical IL2CPP layout):
    #define DICT_OFF_BUCKETS   0x10
    #define DICT_OFF_ENTRIES   0x18
    #define DICT_OFF_COUNT     0x20
    #define DICT_OFF_VERSION   0x24
    #define DICT_OFF_FREELIST  0x28
    #define DICT_OFF_FREECOUNT 0x2C

    auto entriesArray = readPointer(dict, DICT_OFF_ENTRIES);
    int count = readValue<int>(dict, DICT_OFF_COUNT);

    if (!entriesArray || count <= 0 || count > 200) {
        LOGW("Invalid dictionary entries: ptr=%p count=%d", entriesArray, count);
        return;
    }

    LOGI("Dictionary entries: %d", count);

    // Entry array header: 0x00: klass(8), 0x08: monitor(8), 0x10: length(4)
    int arrLen = readValue<int>(entriesArray, 0x10);
    int entryCount = (arrLen < count) ? arrLen : count;
    if (entryCount <= 0) entryCount = count;

    // Entry size: 4(hashCode) + 4(key:uint) + 8(value:ptr) = 16 bytes
    const int ENTRY_SIZE = 16;
    const int ENTRY_HASH_OFF = 0;
    const int ENTRY_KEY_OFF = 4;
    const int ENTRY_VAL_OFF = 8;

    // Array data starts after header (0x14 for IL2CPP arrays on 64-bit with GC desc)
    // Common: 0x10 (length) + padding = 0x14 or 0x18
    const int ARRAY_DATA_OFFSET = 0x18;

    uintptr_t entryBase = (uintptr_t)entriesArray + ARRAY_DATA_OFFSET;

    for (int i = 0; i < entryCount; i++) {
        auto entryAddr = entryBase + i * ENTRY_SIZE;
        int hashCode = *(int*)entryAddr;
        if (hashCode == -1) continue; // empty slot

        uint32_t key = *(uint32_t*)(entryAddr + ENTRY_KEY_OFF);
        void* value = *(void**)(entryAddr + ENTRY_VAL_OFF);
        if (!value || (uintptr_t)value < 0x100000) continue;

        // Try to cast to Player
        // Check if this entity is a Player by comparing class
        auto objClass = il2cpp_object_get_class((Il2CppObject*)value);
        if (!objClass) continue;

        // Check if it's a Player or subclass thereof
        bool isPlayer = false;
        if (objClass == m_playerClass) {
            isPlayer = true;
        } else if (m_playerClass && il2cpp_class_is_subclass_of) {
            isPlayer = il2cpp_class_is_subclass_of(objClass, m_playerClass, false);
        }
        if (!isPlayer && m_entityClass) {
            // Check if it's at least an Entity
            bool isEntity = (objClass == m_entityClass) ||
                (il2cpp_class_is_subclass_of && il2cpp_class_is_subclass_of(objClass, m_entityClass, false));
            if (!isEntity) continue;
        }

        // Read player data
        EspEntity ent;
        ent.playerPtr = value;

        // Position: read via get_Position() method call
        // We can call the managed method, or directly read from Transform
        // Direct approach: read m_CachedTransform at 0x58
        auto transform = readPointer(value, OFF_Entity_m_CachedTransform);
        if (transform) {
            // In Unity IL2CPP, Transform.position is a property
            // We'll try to call get_Position via RVA for now
            // For direct memory access, we'd need the Transform layout
            // Common Transform layout in recent Unity:
            // 0x00: klass, 0x08: monitor, 0x10-0x1F: internal data
            // m_LocalPosition at 0x38 (approx)
            // For now, use a fallback: read from common Transform position offset

            // Try calling get_Position via method pointer
            if (m_entityClass) {
                auto posMethod = il2cpp_class_get_method_from_name(m_entityClass, "get_Position", 0);
                if (posMethod && posMethod->methodPointer) {
                    typedef Vector3 (*GetPosFn)(void*);
                    auto pos = ((GetPosFn)posMethod->methodPointer)(value);
                    ent.worldPos = pos;
                }
            }
        }

        // IsDead
        ent.isDead = readValue<bool>(value, OFF_AttackableEntity_IsDead);

        // Name
        auto namePtr = readPointer(value, OFF_Player_OriginalNickName);
        if (namePtr) {
            auto nameStr = readIl2CppString(namePtr);
            strncpy(ent.name, nameStr.c_str(), sizeof(ent.name) - 1);
        }

        // IsBot
        ent.isBot = readValue<bool>(value, OFF_Player_IsClientBot);

        m_entities.push_back(ent);
    }

    LOGI("Found %zu entities", m_entities.size());
}

// ============================================================
// World to screen using Camera.WorldToScreenPoint
// Returns true if on screen, sets screen position and distance
// ============================================================
inline bool GameData::worldToScreen(const Vector3& world, Vector2& screen, float& outDist) {
    if (!m_mainCamera || !m_worldToScreenMethod) return false;

    auto boxedPos = il2cpp_value_box(m_vector3Class, (void*)&world);
    if (!boxedPos) return false;

    void* params[1] = { boxedPos };
    Il2CppException* exc = nullptr;
    auto result = il2cpp_runtime_invoke(m_worldToScreenMethod, m_mainCamera, params, &exc);
    if (!result || exc) return false;

    auto unboxed = (float*)il2cpp_object_unbox(result);
    if (!unboxed) return false;

    screen.x = unboxed[0];
    screen.y = (float)m_screenH - unboxed[1]; // Flip Y for screen coords
    outDist = unboxed[2];
    return (unboxed[2] > 0.0f); // z > 0 means in front of camera
}

// ============================================================
// Main update - called every frame from the hook
// ============================================================
inline bool GameData::update() {
    m_entities.clear();

    // Get current match
    m_currentMatchGame = getCurrentMatchGame();
    if (!m_currentMatchGame) {
        m_inMatch = false;
        return false;
    }
    m_inMatch = true;

    // Get main camera (try fresh each frame)
    if (m_getMainMethod && m_getMainMethod->methodPointer) {
        typedef void* (*GetMainFn)();
        m_mainCamera = ((GetMainFn)m_getMainMethod->methodPointer)();
    }

    // Iterate entities
    iterateEntities();

    // Calculate screen positions and distances
    for (auto& ent : m_entities) {
        Vector2 screen;
        float dist = 0.0f;
        if (worldToScreen(ent.worldPos, screen, dist)) {
            ent.screenPos = screen;
            ent.distance = dist;
            ent.onScreen = true;
        }
    }

    return true;
}

// ============================================================
// Full initialization
// ============================================================
inline bool GameData::init() {
    LOGI("GameData::init() starting...");

    if (!findAssemblyCSharp()) {
        LOGE("Failed to find Assembly-CSharp");
        return false;
    }

    if (!resolveClasses()) {
        LOGE("Failed to resolve classes");
        return false;
    }

    if (!resolveStaticFields()) {
        LOGE("Failed to resolve static fields");
        return false;
    }

    LOGI("GameData::init() OK");
    return true;
}
