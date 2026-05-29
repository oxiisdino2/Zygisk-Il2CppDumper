#include "esp_dump.h"
#include "log.h"
#include "offsets.h"
#include "il2cpp-class.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <thread>

// Declare IL2CPP API function pointers (defined in il2cpp_dump.cpp)
#define DO_API(r, n, p) extern r (*n) p;
#include "il2cpp-api-functions.h"
#undef DO_API

struct EntityDump {
    float screenX, screenY;
    float worldX, worldY, worldZ;
    float distance;
    bool isDead;
    bool isBot;
    char name[64];
};

static bool read_il2cpp_string(void* strPtr, char* out, size_t outSize) {
    if (!strPtr) { out[0] = 0; return false; }
    int len = *(int*)((uintptr_t)strPtr + 0x10);
    if (len <= 0 || len > 128) { out[0] = 0; return false; }
    char16_t* chars = (char16_t*)((uintptr_t)strPtr + 0x14);
    for (int i = 0; i < len && i < (int)(outSize - 1); i++) {
        char c = (chars[i] > 127) ? '?' : (char)chars[i];
        out[i] = c;
    }
    out[len < (int)(outSize - 1) ? len : (int)(outSize - 1)] = 0;
    return true;
}

static void dump_entities() {
    // Find Assembly-CSharp
    auto domain = il2cpp_domain_get();
    if (!domain) return;

    size_t asmCount = 0;
    auto assemblies = il2cpp_domain_get_assemblies(domain, &asmCount);
    if (!assemblies) return;

    const Il2CppImage* asmCSharp = nullptr;
    for (size_t i = 0; i < asmCount; i++) {
        auto img = il2cpp_assembly_get_image(assemblies[i]);
        auto name = il2cpp_image_get_name(img);
        if (name && strstr(name, "Assembly-CSharp")) {
            asmCSharp = img;
            break;
        }
    }
    if (!asmCSharp) return;

    // Resolve classes
    auto gameFacadeClass = il2cpp_class_from_name(asmCSharp, "COW", "GameFacade");
    auto matchGameClass = il2cpp_class_from_name(asmCSharp, "COW", "MatchGame");
    auto playerClass = il2cpp_class_from_name(asmCSharp, "COW.GamePlay", "Player");
    if (!gameFacadeClass || !matchGameClass || !playerClass) return;

    // Read CurrentMatchGame static field
    void* iter = nullptr;
    void* matchGamePtr = nullptr;
    while (auto field = il2cpp_class_get_fields(gameFacadeClass, &iter)) {
        auto fName = il2cpp_field_get_name(field);
        if (fName && strcmp(fName, "CurrentMatchGame") == 0) {
            il2cpp_field_static_get_value(field, &matchGamePtr);
            break;
        }
    }
    if (!matchGamePtr) return;

    // Read m_ReplicationEntitis dictionary
    void* dict = *(void**)((uintptr_t)matchGamePtr + OFF_MatchGame_m_ReplicationEntitis);
    if (!dict) return;

    // Read dictionary entries
    void* entriesArr = *(void**)((uintptr_t)dict + DICT_OFF_ENTRIES);
    int count = *(int*)((uintptr_t)dict + DICT_OFF_COUNT);
    if (!entriesArr || count <= 0 || count > 200) return;

    int arrLen = *(int*)((uintptr_t)entriesArr + 0x10);
    int entryCount = (arrLen < count) ? arrLen : count;

    uintptr_t base = (uintptr_t)entriesArr + ARRAY_DATA_OFFSET;

    // Dump entities to file
    FILE* f = fopen("/sdcard/ff_entities.dat", "w");
    if (!f) return;

    int dumped = 0;
    for (int i = 0; i < entryCount; i++) {
        uintptr_t addr = base + i * ENTRY_SIZE;
        int hash = *(int*)addr;
        if (hash == -1) continue;

        void* value = *(void**)(addr + ENTRY_VAL_OFF);
        if (!value || (uintptr_t)value < 0x10000) continue;

        // Check if it's a Player
        auto objClass = il2cpp_object_get_class((Il2CppObject*)value);
        if (!objClass) continue;
        bool isPlayer = (objClass == playerClass);
        if (!isPlayer && il2cpp_class_is_subclass_of) {
            isPlayer = il2cpp_class_is_subclass_of(objClass, playerClass, false);
        }
        if (!isPlayer) continue;

        EntityDump e{};
        e.isDead = *(bool*)((uintptr_t)value + OFF_AttackableEntity_IsDead);
        e.isBot = *(bool*)((uintptr_t)value + OFF_Player_IsClientBot);
        e.distance = 0;
        e.screenX = 0;
        e.screenY = 0;
        e.worldX = 0;
        e.worldY = 0;
        e.worldZ = 0;

        void* namePtr = *(void**)((uintptr_t)value + OFF_Player_OriginalNickName);
        read_il2cpp_string(namePtr, e.name, sizeof(e.name));

        fprintf(f, "%.1f,%.1f,%.1f,%d,%d,%s\n",
                e.screenX, e.screenY, e.distance,
                e.isDead ? 1 : 0, e.isBot ? 1 : 0, e.name);
        dumped++;
    }

    fclose(f);
}

void esp_dump_start(const char* game_data_dir) {
    LOGI("esp_dump: waiting for game to load...");
    sleep(15);

    while (true) {
        dump_entities();
        sleep(2);
    }
}
