import json 

def gen(j: list[dict]):
    ids = []
    caps = []
    names = []
    isPercents = []
    rarityValues = []

    for ent in j:
        name = str(ent["effectName"])
        id = name.lower().replace(" ", "_")
        cap = float(ent["effectCap"])
        isPercent = bool(ent["isPercent"])

        ids.append(id)
        caps.append(cap)
        names.append(name)
        isPercents.append(isPercent)
        rarityValues.append(ent["rarityValues"][0] >= 5)

    print(f"""
    #pragma once 
    #include <array>
    #include <cstddef>
    #include <cstdint>
    #include <string_view>
    namespace mtce
    {{
        enum class charm_effects : std::uint16_t {{ {",\n".join([i.upper() for i in ids])}, ABILITY_COUNT }};

        inline static constexpr std::size_t ABILITY_COUNT = static_cast<std::size_t>(charm_effects::ABILITY_COUNT);

        inline static constexpr std::array<double, ABILITY_COUNT> EFFECT_CAPS = {{ {",".join([str(i) for i in caps])} }};

        inline static constexpr std::array<std::string_view, ABILITY_COUNT> EFFECT_NAMES = {{ {"\n".join([f'"{i}",' for i in ids])} }};
        
        inline static constexpr std::array<std::string_view, ABILITY_COUNT> EFFECT_DISPLAY_NAMES = {{ {"\n".join([f'"{i}",' for i in names])} }};
        
        inline static constexpr std::array<bool, ABILITY_COUNT> EFFECT_IS_PERCENT = {{ {"\n".join([f'{"true" if i else "false"},' for i in isPercents])} }};

        inline static constexpr std::array<bool, ABILITY_COUNT> EFFECT_ROUND_TO_INTEGER = {{ {"\n".join([f'{"true" if i else "false"},' for i in rarityValues])} }};
    }}
    """)


if __name__ == "__main__":
    with open("zenith_charm_config.json") as f:
        gen(json.load(f))
