#pragma once
#include <games/bo4/pool.hpp>

/*
 * The retail SndAlias and its fields as the game's alias editor knows them: names, offsets, kinds and ranges from
 * the alias field table of the retail exe (devhost, next to the "PanType" string), units as the old build's
 * SND_CsvSetField reads them (volumes in dB SPL, pitches in cents, distances halved, units in /255).
 */
namespace games::bo4::sound {
    struct SndAlias {
        XHash name;
        XHash subtitle;
        XHash secondary;
        XHash stopAlias;
        uint32_t facialAnimation; // ScrString_t
        XHash assetId;
        XHash assetIdSustain;
        XHash assetIdRelease;
        XHash assetFileName; // the bank of the asset
        XHash assetFileNameSustain;
        XHash assetFileNameRelease;
        uint32_t flags0;
        uint32_t flags1;
        uint32_t flags2;
        XHash duck;
        XHash contextType;
        XHash contextValue;
        XHash contextType1;
        XHash contextValue1;
        XHash contextType2;
        XHash contextValue2;
        XHash contextType3;
        XHash contextValue3;
        XHash stopOnPlay;
        XHash futzPatch;
        XHash revFilename;
        float reverbSend;
        float centerSend;
        float volMin;
        float volMax;
        float envelopPercent;
        float sosDelay;
        uint16_t fluxTime;
        uint16_t startDelay;
        uint16_t pitchMin;
        uint16_t pitchMax;
        uint16_t distMin;
        uint16_t distMaxDry;
        uint16_t distMaxWet;
        uint16_t envelopMin;
        uint16_t envelopMax;
        uint16_t distanceLpfMin;
        uint16_t distanceLpfMax;
        uint16_t distanceHpfMin;
        uint16_t distanceHpfMax;
        int16_t fadeIn;
        int16_t fadeOut;
        int16_t dopplerScale;
        uint8_t priorityThresholdMin;
        uint8_t priorityThresholdMax;
        uint8_t probability;
        uint8_t occlusionLevel;
        uint8_t priorityMin;
        uint8_t priorityMax;
        uint8_t enemyPriorityBoost;
        uint8_t pan;
        uint8_t limitCount;
        uint8_t entityLimitCount;
        uint8_t duckGroup;
        uint8_t bus;
        uint8_t volumeGroup;
    };
    static_assert(sizeof(SndAlias) == 0x1C0);
    static_assert(offsetof(SndAlias, assetId) == 0x48);
    static_assert(offsetof(SndAlias, assetFileName) == 0x78);
    static_assert(offsetof(SndAlias, flags0) == 0xA8);
    static_assert(offsetof(SndAlias, duck) == 0xB8);
    static_assert(offsetof(SndAlias, revFilename) == 0x168);
    static_assert(offsetof(SndAlias, reverbSend) == 0x178);
    static_assert(offsetof(SndAlias, fluxTime) == 0x190);
    static_assert(offsetof(SndAlias, fadeIn) == 0x1AA);
    static_assert(offsetof(SndAlias, priorityThresholdMin) == 0x1B0);
    static_assert(offsetof(SndAlias, volumeGroup) == 0x1BC);

    struct SndAliasList {
        XHash name;
        SndAlias* aliases;
        int32_t count;
        int32_t sequence;
        float cullDistance;
        bool spatial;
    };
    static_assert(sizeof(SndAliasList) == 0x28);

    // music: the bank's sets (SndBank+0xA8/0xB0), their states and the aliases they play, from the music json field table
    struct SndMusicAsset {
        char aliasName[64];
        XHash aliasId;
        bool looping;
        bool completeLoop;
        bool removeAfterPlay;
        bool playAsFirstRandom;
        bool startSync;
        bool stopSync;
        bool completeOnStop;
        uint32_t loopStartOffset;
        int32_t bpm;
        int32_t assetType;
        int32_t loopNumber;
        int32_t order;
        int32_t startDelayBeats;
        int32_t startFadeBeats;
        int32_t stopDelayBeats;
        int32_t stopFadeBeats;
        int32_t startOffsetFrames;
        int32_t meter;
    };
    static_assert(sizeof(SndMusicAsset) == 0x88);
    static_assert(offsetof(SndMusicAsset, loopStartOffset) == 0x58);
    static_assert(offsetof(SndMusicAsset, meter) == 0x80);

    struct SndMusicState {
        char name[64];
        XHash id;
        SndMusicAsset intro;
        SndMusicAsset exit;
        uint32_t loopCount;
        SndMusicAsset* loops;
        int32_t order;
        bool isRandom;
        bool isSequential;
        bool skipPreviousExit;
        int32_t status;
    };
    static_assert(sizeof(SndMusicState) == 0x180);
    static_assert(offsetof(SndMusicState, loopCount) == 0x160);
    static_assert(offsetof(SndMusicState, isRandom) == 0x174);

    struct SndMusicSet {
        char name[64];
        XHash id;
        uint32_t stateCount;
        SndMusicState* states;
    };
    static_assert(sizeof(SndMusicSet) == 0x60);

    enum SndAliasFieldType : uint8_t {
        SAF_DB,    // float, linear in the alias, dB SPL (0-100) in the field
        SAF_CENTS, // uint16, 0x7FFF = 1.0, cents in the field
        SAF_DIST,  // uint16, halved in the alias
        SAF_UNIT,  // uint8, 0-255 in the alias, 0-1 in the field
        SAF_FLOAT,
        SAF_U8,
        SAF_U16,
        SAF_I16,
        SAF_BITS,  // a value in bits of a flags field, by name when it has names
        SAF_FLAGS, // bits of a flags field, each a name
        SAF_HASH,  // XHash of a name
    };

    // the names the game gives the values of a field
    inline const char* const SAFN_BOOL[]{ "false", "true" };
    inline const char* const SAFN_LOOPING[]{ "nonlooping", "looping" };
    inline const char* const SAFN_PANTYPE[]{ "2d", "3d" };
    inline const char* const SAFN_STORAGE[]{ "unknown", "loaded", "streamed", "primed", "vehicle" };
    inline const char* const SAFN_LIMITTYPE[]{ "none", "oldest", "reject", "priority" };
    inline const char* const SAFN_RANDOMIZE[]{ "volume", "pitch", "variant" };
    inline const char* const SAFN_FLUX[]{
        "none", "left_player", "center_player", "right_player", "random_player", "left_shot", "center_shot", "right_shot",
        "random_direction",
    };
    inline const char* const SAFN_BUS[]{
        "bus_fx", "bus_voice", "bus_pfutz", "bus_hdrfx", "bus_ui", "bus_music", "bus_movie", "bus_reference",
        "bus_license", "bus_nearverb_send", "bus_nearverb_work", "bus_nearverb_return", "bus_farverb_send",
        "bus_farverb_work", "bus_farverb_return", "bus_earlyverb_send", "bus_earlyverb_work", "bus_earlyverb_return",
    };

    struct SndAliasField {
        const char* name;
        uint32_t offset;
        SndAliasFieldType type;
        uint8_t bits{};
        uint8_t shift{};
        const char* const* names{};
        uint32_t nameCount{};
        float min{};
        float max{};
    };

#define SAF_NAMES(arr) arr, (uint32_t)std::size(arr)
    // the volume group, duck group, pan and curve indexes are into the lists of the snddriverglobals, by number
    inline const SndAliasField ALIAS_FIELDS[]{
        { "VolMin", 0x180, SAF_DB, 0, 0, nullptr, 0, 0, 100 },
        { "VolMax", 0x184, SAF_DB, 0, 0, nullptr, 0, 0, 100 },
        { "PitchMin", 0x194, SAF_CENTS, 0, 0, nullptr, 0, -2400, 1200 },
        { "PitchMax", 0x196, SAF_CENTS, 0, 0, nullptr, 0, -2400, 1200 },
        { "DistMin", 0x198, SAF_DIST, 0, 0, nullptr, 0, 0, 131070 },
        { "DistMaxDry", 0x19A, SAF_DIST, 0, 0, nullptr, 0, 0, 131070 },
        { "DistMaxWet", 0x19C, SAF_DIST, 0, 0, nullptr, 0, 0, 131070 },
        { "Probability", 0x1B2, SAF_UNIT, 0, 0, nullptr, 0, 0, 1 },
        { "EnvelopMin", 0x19E, SAF_DIST, 0, 0, nullptr, 0, 0, 131070 },
        { "EnvelopMax", 0x1A0, SAF_DIST, 0, 0, nullptr, 0, 0, 131070 },
        { "EnvelopPercent", 0x188, SAF_DB, 0, 0, nullptr, 0, 0, 100 },
        { "CenterSend", 0x17C, SAF_DB, 0, 0, nullptr, 0, 0, 100 },
        { "ReverbSend", 0x178, SAF_DB, 0, 0, nullptr, 0, 0, 100 },
        { "SosDelay", 0x18C, SAF_FLOAT, 0, 0, nullptr, 0, 0, 10000 },
        { "StartDelay", 0x192, SAF_U16, 0, 0, nullptr, 0, 0, 65535 },
        { "FluxTime", 0x190, SAF_U16, 0, 0, nullptr, 0, 0, 65535 },
        { "FadeIn", 0x1AA, SAF_I16, 0, 0, nullptr, 0, 0, 32767 },
        { "FadeOut", 0x1AC, SAF_I16, 0, 0, nullptr, 0, 0, 32767 },
        { "DopplerScale", 0x1AE, SAF_I16, 0, 0, nullptr, 0, -100, 100 },
        { "DistanceLpfMin", 0x1A2, SAF_U16, 0, 0, nullptr, 0, 0, 65535 },
        { "DistanceLpfMax", 0x1A4, SAF_U16, 0, 0, nullptr, 0, 0, 65535 },
        { "DistanceHpfMin", 0x1A6, SAF_U16, 0, 0, nullptr, 0, 0, 65535 },
        { "DistanceHpfMax", 0x1A8, SAF_U16, 0, 0, nullptr, 0, 0, 65535 },
        { "PriorityThresholdMin", 0x1B0, SAF_UNIT, 0, 0, nullptr, 0, 0, 1 },
        { "PriorityThresholdMax", 0x1B1, SAF_UNIT, 0, 0, nullptr, 0, 0, 1 },
        { "OcclusionLevel", 0x1B3, SAF_UNIT, 0, 0, nullptr, 0, 0, 1 },
        { "PriorityMin", 0x1B4, SAF_U8, 0, 0, nullptr, 0, 0, 128 },
        { "PriorityMax", 0x1B5, SAF_U8, 0, 0, nullptr, 0, 0, 128 },
        { "EnemyPriorityBoost", 0x1B6, SAF_U8, 0, 0, nullptr, 0, 0, 128 },
        { "LimitCount", 0x1B8, SAF_U8, 0, 0, nullptr, 0, 0, 128 },
        { "EntityLimitCount", 0x1B9, SAF_U8, 0, 0, nullptr, 0, 0, 128 },
        { "Pan", 0x1B7, SAF_U8, 0, 0, nullptr, 0, 0, 255 },
        { "DuckGroup", 0x1BA, SAF_U8, 0, 0, nullptr, 0, 0, 255 },
        { "Bus", 0x1BB, SAF_U8, 0, 0, SAF_NAMES(SAFN_BUS), 0, 255 },
        { "VolumeGroup", 0x1BC, SAF_U8, 0, 0, nullptr, 0, 0, 255 },

        { "Looping", 0xA8, SAF_BITS, 1, 0, SAF_NAMES(SAFN_LOOPING) },
        { "PanType", 0xA8, SAF_BITS, 1, 1, SAF_NAMES(SAFN_PANTYPE) },
        { "SilentInCPZ", 0xA8, SAF_BITS, 1, 2, SAF_NAMES(SAFN_BOOL) },
        { "ContextFailsafe", 0xA8, SAF_BITS, 1, 3, SAF_NAMES(SAFN_BOOL) },
        { "DistanceLpf", 0xA8, SAF_BITS, 1, 4, SAF_NAMES(SAFN_BOOL) },
        { "DistanceHpf", 0xA8, SAF_BITS, 1, 5, SAF_NAMES(SAFN_BOOL) },
        { "Doppler", 0xA8, SAF_BITS, 1, 6, SAF_NAMES(SAFN_BOOL) },
        { "Pauseable", 0xA8, SAF_BITS, 1, 7, SAF_NAMES(SAFN_BOOL) },
        { "IsMusic", 0xA8, SAF_BITS, 1, 8, SAF_NAMES(SAFN_BOOL) },
        { "StopOnEntDeath", 0xA8, SAF_BITS, 1, 9, SAF_NAMES(SAFN_BOOL) },
        { "Timescale", 0xA8, SAF_BITS, 1, 10, SAF_NAMES(SAFN_BOOL) },
        { "VoiceLimit", 0xA8, SAF_BITS, 1, 11, SAF_NAMES(SAFN_BOOL) },
        { "IgnoreMaxDist", 0xA8, SAF_BITS, 1, 12, SAF_NAMES(SAFN_BOOL) },
        { "Storage", 0xA8, SAF_BITS, 3, 13, SAF_NAMES(SAFN_STORAGE) },
        { "FluxType", 0xA8, SAF_BITS, 4, 16, SAF_NAMES(SAFN_FLUX) },
        { "LimitType", 0xA8, SAF_BITS, 2, 20, SAF_NAMES(SAFN_LIMITTYPE) },
        { "EntityLimitType", 0xA8, SAF_BITS, 2, 22, SAF_NAMES(SAFN_LIMITTYPE) },
        { "RandomizeType", 0xA8, SAF_FLAGS, 3, 24, SAF_NAMES(SAFN_RANDOMIZE) },
        { "GPAD", 0xA8, SAF_BITS, 1, 27, SAF_NAMES(SAFN_BOOL) },
        { "GPADOnly", 0xA8, SAF_BITS, 1, 28, SAF_NAMES(SAFN_BOOL) },
        { "MuteVoice", 0xA8, SAF_BITS, 1, 29, SAF_NAMES(SAFN_BOOL) },
        { "MuteMusic", 0xA8, SAF_BITS, 1, 30, SAF_NAMES(SAFN_BOOL) },
        { "IsRobovoice", 0xA8, SAF_BITS, 1, 31, SAF_NAMES(SAFN_BOOL) },
        { "NeverPlayTwice", 0xAC, SAF_BITS, 1, 0, SAF_NAMES(SAFN_BOOL) },
        { "DryMaxCurve", 0xAC, SAF_BITS, 6, 2 },
        { "WetMaxCurve", 0xAC, SAF_BITS, 6, 8 },
        { "DryMinCurve", 0xAC, SAF_BITS, 6, 14 },
        { "WetMinCurve", 0xAC, SAF_BITS, 6, 20 },
        { "ServerPrecache", 0xAC, SAF_BITS, 1, 26, SAF_NAMES(SAFN_BOOL) },
        { "IsCinematic", 0xAC, SAF_BITS, 1, 27, SAF_NAMES(SAFN_BOOL) },
        { "IsBig", 0xAC, SAF_BITS, 1, 28, SAF_NAMES(SAFN_BOOL) },
        { "AmplitudePriority", 0xAC, SAF_BITS, 1, 29, SAF_NAMES(SAFN_BOOL) },
        { "ContinuousPan", 0xAC, SAF_BITS, 1, 30, SAF_NAMES(SAFN_BOOL) },
        { "RestartContextLoops", 0xAC, SAF_BITS, 1, 31, SAF_NAMES(SAFN_BOOL) },
        { "UseAmbRoomOcclusion", 0xB0, SAF_BITS, 1, 0, SAF_NAMES(SAFN_BOOL) },
        { "UseAmbRoomVerbAtten", 0xB0, SAF_BITS, 1, 1, SAF_NAMES(SAFN_BOOL) },

        { "Secondary", 0x20, SAF_HASH },
        { "StopAlias", 0x30, SAF_HASH },
        { "Duck", 0xB8, SAF_HASH },
        { "ContextType", 0xC8, SAF_HASH },
        { "ContextValue", 0xD8, SAF_HASH },
        { "ContextType1", 0xE8, SAF_HASH },
        { "ContextValue1", 0xF8, SAF_HASH },
        { "ContextType2", 0x108, SAF_HASH },
        { "ContextValue2", 0x118, SAF_HASH },
        { "ContextType3", 0x128, SAF_HASH },
        { "ContextValue3", 0x138, SAF_HASH },
        { "StopOnPlay", 0x148, SAF_HASH },
        { "FutzPatch", 0x158, SAF_HASH },
    };
#undef SAF_NAMES

    inline const SndAliasField* FindAliasField(const char* name) {
        for (const SndAliasField& field : ALIAS_FIELDS) {
            if (!_strcmpi(field.name, name)) {
                return &field;
            }
        }
        return nullptr;
    }

    // the field as a number, in the units of the field
    inline double GetAliasField(const SndAlias& alias, const SndAliasField& field) {
        const byte* p{ (const byte*)&alias + field.offset };
        switch (field.type) {
        case SAF_DB: {
            float v{ *(const float*)p };
            return v > 0 ? 20.0 * std::log10(v) + 100.0 : 0.0;
        }
        case SAF_CENTS: {
            uint16_t v{ *(const uint16_t*)p };
            return v ? 1200.0 * std::log2(v / 32767.0) : field.min;
        }
        case SAF_DIST: return *(const uint16_t*)p * 2.0;
        case SAF_UNIT: return *p / 255.0;
        case SAF_FLOAT: return *(const float*)p;
        case SAF_U8: return *p;
        case SAF_U16: return *(const uint16_t*)p;
        case SAF_I16: return *(const int16_t*)p;
        case SAF_BITS:
        case SAF_FLAGS: return (*(const uint32_t*)p >> field.shift) & ((1u << field.bits) - 1);
        default: return 0;
        }
    }

    // sets the field from a number in its units, false when it is out of its range
    inline bool SetAliasField(SndAlias& alias, const SndAliasField& field, double value) {
        byte* p{ (byte*)&alias + field.offset };
        if (field.type != SAF_BITS && field.type != SAF_FLAGS && field.type != SAF_HASH && field.min != field.max &&
            (value < field.min || value > field.max)) {
            return false;
        }
        switch (field.type) {
        case SAF_DB: {
            float v{ (float)std::pow(10.0, (value - 100.0) * 0.05) };
            *(float*)p = v < 1.52879e-05f ? 0 : v;
            return true;
        }
        case SAF_CENTS: *(uint16_t*)p = (uint16_t)(std::exp2(value / 1200.0) * 32767.0); return true;
        case SAF_DIST: *(uint16_t*)p = (uint16_t)((int)value / 2); return true;
        case SAF_UNIT: *p = (uint8_t)(value * 255.0); return true;
        case SAF_FLOAT: *(float*)p = (float)value; return true;
        case SAF_U8: *p = (uint8_t)value; return true;
        case SAF_U16: *(uint16_t*)p = (uint16_t)value; return true;
        case SAF_I16: *(int16_t*)p = (int16_t)value; return true;
        case SAF_BITS:
        case SAF_FLAGS: {
            uint32_t mask{ (1u << field.bits) - 1 };
            uint32_t v{ (uint32_t)value };
            if (value < 0 || v > mask || (field.type == SAF_BITS && field.names && v >= field.nameCount)) {
                return false;
            }
            uint32_t& flags{ *(uint32_t*)p };
            flags = (flags & ~(mask << field.shift)) | (v << field.shift);
            return true;
        }
        default: return false;
        }
    }

    // the value of a field with names by its name ("3d", "looping", "bus_ui"), or for flags the names with spaces
    inline bool SetAliasFieldName(SndAlias& alias, const SndAliasField& field, const char* value) {
        if (!field.names) {
            return false;
        }
        if (field.type == SAF_FLAGS) {
            uint32_t v{};
            std::string str{ value };
            for (char& c : str) {
                if (c == ',' || c == '|') {
                    c = ' ';
                }
            }
            size_t start{};
            while (start < str.size()) {
                size_t end{ str.find(' ', start) };
                if (end == std::string::npos) {
                    end = str.size();
                }
                std::string token{ str.substr(start, end - start) };
                start = end + 1;
                if (token.empty()) {
                    continue;
                }
                size_t i{};
                while (i < field.nameCount && _strcmpi(field.names[i], token.data())) {
                    i++;
                }
                if (i == field.nameCount) {
                    return false;
                }
                v |= 1u << i;
            }
            return SetAliasField(alias, field, v);
        }
        for (uint32_t i = 0; i < field.nameCount; i++) {
            if (!_strcmpi(field.names[i], value)) {
                return SetAliasField(alias, field, i);
            }
        }
        // booleans as yes/no too
        if (field.names == SAFN_BOOL) {
            if (!_strcmpi(value, "yes")) {
                return SetAliasField(alias, field, 1);
            }
            if (!_strcmpi(value, "no")) {
                return SetAliasField(alias, field, 0);
            }
        }
        return false;
    }

    // the name of the value of a field, its names with spaces for flags, empty when it has none
    inline std::string GetAliasFieldName(const SndAlias& alias, const SndAliasField& field) {
        if (!field.names) {
            return {};
        }
        uint32_t v{ (uint32_t)GetAliasField(alias, field) };
        if (field.type == SAF_FLAGS) {
            std::string out{};
            for (uint32_t i = 0; i < field.nameCount; i++) {
                if (v & (1u << i)) {
                    if (!out.empty()) {
                        out += ' ';
                    }
                    out += field.names[i];
                }
            }
            return out;
        }
        return v < field.nameCount ? field.names[v] : std::to_string(v);
    }
} // namespace games::bo4::sound
