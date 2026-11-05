#pragma once

namespace neo::ores {

// One selectable mineral family in the X-Ray module. Every block id that
// belongs to the family is listed (overworld + deepslate + nether variants);
// scanning matches block identities, so the ids follow the registry names.
struct OreGroupDef {
    const char* settingId;
    const char* label;
    unsigned char r, g, b;
    bool defaultOn;
    const char* const* blocks; // nullptr-terminated
};

inline constexpr const char* kDiamondBlocks[] = {"diamond_ore", "deepslate_diamond_ore", nullptr};
inline constexpr const char* kIronBlocks[] = {"iron_ore", "deepslate_iron_ore", nullptr};
inline constexpr const char* kGoldBlocks[] = {"gold_ore", "deepslate_gold_ore", "nether_gold_ore", nullptr};
inline constexpr const char* kCopperBlocks[] = {"copper_ore", "deepslate_copper_ore", nullptr};
inline constexpr const char* kEmeraldBlocks[] = {"emerald_ore", "deepslate_emerald_ore", nullptr};
inline constexpr const char* kRedstoneBlocks[] = {"redstone_ore", "deepslate_redstone_ore", nullptr};
inline constexpr const char* kLapisBlocks[] = {"lapis_ore", "deepslate_lapis_ore", nullptr};
inline constexpr const char* kCoalBlocks[] = {"coal_ore", "deepslate_coal_ore", nullptr};
inline constexpr const char* kQuartzBlocks[] = {"nether_quartz_ore", nullptr};
inline constexpr const char* kDebrisBlocks[] = {"ancient_debris", nullptr};

inline constexpr OreGroupDef kGroups[] = {
    {"diamond", "Diamond", 110, 232, 224, true, kDiamondBlocks},
    {"iron", "Iron", 226, 214, 202, true, kIronBlocks},
    {"gold", "Gold", 252, 214, 88, true, kGoldBlocks},
    {"copper", "Copper", 224, 128, 92, true, kCopperBlocks},
    {"emerald", "Emerald", 92, 234, 150, true, kEmeraldBlocks},
    {"redstone", "Redstone", 236, 92, 96, true, kRedstoneBlocks},
    {"lapis", "Lapis", 96, 138, 236, true, kLapisBlocks},
    {"coal", "Coal", 132, 136, 142, true, kCoalBlocks},
    {"quartz", "Nether Quartz", 238, 230, 218, false, kQuartzBlocks},
    {"debris", "Ancient Debris", 182, 124, 96, false, kDebrisBlocks},
};

inline constexpr int groupCount = static_cast<int>(sizeof(kGroups) / sizeof(kGroups[0]));

} // namespace neo::ores
