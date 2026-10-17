#include <includes.hpp>
#include <core/raw_file_json.hpp>
#include <utils/data_utils.hpp>
#include <core/hashes/raw_file_extractor.hpp>
#include <tools/fastfile/handlers/handler_game_bo4.hpp>
#include <tools/fastfile/handlers/bo4/bo4_unlinker_scriptbundle.hpp>
#include <games/bo4/sound_alias.hpp>

namespace {
    using namespace fastfile::handlers::bo4;
    using games::bo4::sound::SndAlias;
    using games::bo4::sound::SndAliasList;

    // the blocks as they are, templates for the bo4 sound linker
    std::string Hex(const void* data, size_t len) {
        static const char* digits{ "0123456789abcdef" };
        std::string out{};
        out.reserve(len * 2);
        for (size_t i = 0; i < len; i++) {
            byte b{ ((const byte*)data)[i] };
            out += digits[b >> 4];
            out += digits[b & 0xF];
        }
        return out;
    }

    struct StreamKey;

    struct SndBankStreamKeys {
        StreamKey* streamKey1;
        byte* data1;
        uint32_t len1;
        StreamKey* fileKey;
        byte* fileData;
        uint32_t fileLen;
    };

    struct SndIndexEntry {
        unsigned __int16 value;
        unsigned __int16 next;
    };
    typedef float SndFloatAlign32;

    struct SndDuck {
        char name[64];
        XHash id;
        float fadeIn;
        float fadeOut;
        float startDelay;
        float distance;
        float length;
        XHash fadeInCurve;
        XHash fadeOutCurve;
        SndFloatAlign32* attenuation;
        SndFloatAlign32* filter;
        SndFloatAlign32* unk98;
        bool updateWhilePaused;
        bool trackAmplitude;
        bool disableInSplitscreen;
        float aliasAttenuation;
        float aliasLpf;
        XHash duckAlias;
    };
    static_assert(sizeof(SndDuck) == 0xc0);

    struct SndAmbient {
        char name[64];
        XHash id;
        bool defaultRoom;
        XHash reverb;
        char reverbName[64];
        XHash nearVerb;
        char nearVerbName[64];
        XHash farVerb;
        char farVerbName[64];
        float reverbDryLevel;
        float reverbWetLevel;
        float unk230;
        float unk234;
        XHash loop;
        char duck[64];
        XHash duckId;
        XHash entityContextType0;
        XHash entityContextValue0;
        XHash entityContextType1;
        XHash entityContextValue1;
        XHash entityContextType2;
        XHash entityContextValue2;
        XHash globalContextType;
        XHash globalContextValue;
    };
    static_assert(sizeof(SndAmbient) == 0x238);

    struct SndDialogScriptIdLookup {
        XHash scriptId;
        XHash aliasId;
    };
    static_assert(sizeof(SndDialogScriptIdLookup) == 0x20);

    struct SndBank {
        const char* name;
        XHash nameHash;
        const char* zone;
        const char* gameLanguage;
        const char* soundLanguage;
        uint32_t aliasCount;
        SndAliasList* alias;
        SndIndexEntry* aliasIndex;
        uint32_t reverbCount;
        uint64_t reverbs;
        uint32_t duckCount;
        SndDuck* ducks;
        uint32_t ambientCount;
        SndAmbient* ambients;
        uint32_t scriptIdLookupCount;
        SndDialogScriptIdLookup* scriptIdLookups;
        uint32_t unk90_count;
        uint64_t unk90;
        uint32_t unka0_count;
        uint64_t unka0;
        uint32_t musicSetCount;
        games::bo4::sound::SndMusicSet* musicSets;
        uint32_t unkc0_count;
        uint64_t unkc0;
        SndBankStreamKeys streamKeysSABL;
        SndBankStreamKeys streamKeysSABS;
        bool patchZone;
    };
    static_assert(sizeof(SndBank) == 0x130);

    class ImplWorker : public Worker {

        void PreLoadWorker(fastfile::FastFileContext* ctx) override {
            if (!ctx) {
                return; // no base ones
            }

            const char* suffixes[]{ "all", "bp", "ea", "en", "es", "fj", "fr", "ge", "it", "ko", "ms", "po", "ru" };

            for (const char* suffix : suffixes) {
                const char* str{ utils::va("%s.%s", ctx->ffname, suffix) };
                hashutils::AddPrecomputed(hash::Hash64(str), str, true);
                const char* str2{ utils::va("%s_%s_stream", ctx->ffname, suffix) };
                hashutils::AddPrecomputed(hash::Hash64(str2), str2, true);
            }
        }

        void Unlink(fastfile::FastFileOption& opt, void* ptr) {
            SndBank* asset{ (SndBank*)ptr };

            if (asset->name)
                hashutils::Add(asset->name);
            if (asset->zone)
                hashutils::Add(asset->zone);
            if (asset->gameLanguage)
                hashutils::Add(asset->gameLanguage);
            if (asset->soundLanguage)
                hashutils::Add(asset->soundLanguage);

            const char* n{ hashutils::ExtractTmp("file", asset->nameHash.name) };
            std::filesystem::path outFile{ opt.m_output / "bo4" / "source" / "tables" / "sound" /
                                           std::format("{}.json", n) };
            std::filesystem::create_directories(outFile.parent_path());

            BO4JsonWriter json{};

            LOG_OPT_INFO("Dump sound {}", outFile.string());
            json.BeginObject();

            if (asset->name)
                json.WriteFieldValueString("name", asset->name);
            if (asset->zone)
                json.WriteFieldValueString("zone", asset->zone);
            if (asset->gameLanguage)
                json.WriteFieldValueString("gameLanguage", asset->gameLanguage);
            if (asset->soundLanguage)
                json.WriteFieldValueString("soundLanguage", asset->soundLanguage);
            json.WriteFieldValueXHash("nameHash", asset->nameHash);
            json.WriteFieldValueBool("patchZone", asset->patchZone);

            json.WriteFieldNameString("alias");
            json.BeginArray();
            for (size_t i = 0; i < asset->aliasCount; i++) {
                SndAliasList* list{ asset->alias + i };

                json.BeginObject();
                if (asset->aliasIndex) {
                    json.WriteFieldValueNumber("value", asset->aliasIndex[i].value);
                    json.WriteFieldValueNumber("next", asset->aliasIndex[i].next);
                }
                json.WriteFieldValueXHash("name", list->name);
                json.WriteFieldValueNumber("sequence", list->sequence);
                json.WriteFieldValueNumber("cullDistance", list->cullDistance);
                json.WriteFieldValueNumber("spatial", list->spatial);
                json.WriteFieldValueString("raw", Hex(list, sizeof(*list)));

                json.WriteFieldNameString("aliases");
                json.BeginArray();
                for (size_t j = 0; j < list->count; j++) {
                    SndAlias* alias{ list->aliases + j };

                    json.BeginObject();
                    json.WriteFieldValueXHash("name", alias->name);
                    json.WriteFieldValueXHash("subtitle", alias->subtitle);
                    json.WriteFieldValueXHash("assetId", alias->assetId);
                    json.WriteFieldValueXHash("assetIdSustain", alias->assetIdSustain);
                    json.WriteFieldValueXHash("assetIdRelease", alias->assetIdRelease);
                    json.WriteFieldValueXHash("assetFileName", alias->assetFileName);
                    json.WriteFieldValueXHash("assetFileNameSustain", alias->assetFileNameSustain);
                    json.WriteFieldValueXHash("assetFileNameRelease", alias->assetFileNameRelease);
                    json.WriteFieldValueXHash("revFilename", alias->revFilename);
                    json.WriteFieldValueScrString("facialAnimation", *(ScrString_t*)&alias->facialAnimation);

                    // the fields as the bo4 sound linker takes them: game names and units (dB, cents, units)
                    json.WriteFieldNameString("settings");
                    json.BeginObject();
                    for (const games::bo4::sound::SndAliasField& field : games::bo4::sound::ALIAS_FIELDS) {
                        if (field.type == games::bo4::sound::SAF_HASH) {
                            XHash& h{ *(XHash*)((byte*)alias + field.offset) };
                            if (h) {
                                json.WriteFieldValueXHash(field.name, h);
                            }
                        } else if (field.names) {
                            json.WriteFieldValueString(field.name, games::bo4::sound::GetAliasFieldName(*alias, field));
                        } else {
                            // dB and cents to 1/100
                            json.WriteFieldValueNumber(field.name, std::round(games::bo4::sound::GetAliasField(*alias, field) * 100) / 100);
                        }
                    }
                    json.EndObject();
                    json.WriteFieldValueString("raw", Hex(alias, sizeof(*alias)));

                    json.EndObject();
                }
                json.EndArray();

                json.EndObject();
            }
            json.EndArray();

            json.WriteFieldNameString("duck");
            json.BeginArray();
            for (size_t i = 0; i < asset->duckCount; i++) {
                SndDuck* duck{ asset->ducks + i };

                hashutils::Add(duck->name);

                json.BeginObject();
                json.WriteFieldValueString("name", duck->name);
                json.WriteFieldValueXHash("id", duck->id);
                json.WriteFieldValueXHash("fadeInCurve", duck->fadeInCurve);
                json.WriteFieldValueXHash("fadeOutCurve", duck->fadeOutCurve);
                json.WriteFieldValueXHash("duckAlias", duck->duckAlias);
                json.WriteFieldValueBool("updateWhilePaused", duck->updateWhilePaused);
                json.WriteFieldValueBool("trackAmplitude", duck->trackAmplitude);
                json.WriteFieldValueBool("disableInSplitscreen", duck->disableInSplitscreen);
                json.WriteFieldValueNumber("fadeIn", duck->fadeIn);
                json.WriteFieldValueNumber("fadeOut", duck->fadeOut);
                json.WriteFieldValueNumber("startDelay", duck->startDelay);
                json.WriteFieldValueNumber("distance", duck->distance);
                json.WriteFieldValueNumber("length", duck->length);
                json.WriteFieldValueNumber("aliasAttenuation", duck->aliasAttenuation);
                json.WriteFieldValueNumber("aliasLpf", duck->aliasLpf);

                if (duck->attenuation)
                    json.WriteFieldValueString(
                        "attenuation",
                        utils::data::ArrayAsString(duck->attenuation, 64, " ", "", "")
                    );
                if (duck->filter)
                    json.WriteFieldValueString("filter", utils::data::ArrayAsString(duck->filter, 64, " ", "", ""));
                if (duck->unk98)
                    json.WriteFieldValueString("unk98", utils::data::ArrayAsString(duck->unk98, 64, " ", "", ""));

                json.EndObject();
            }
            json.EndArray();

            json.WriteFieldNameString("ambients");
            json.BeginArray();
            for (size_t i = 0; i < asset->ambientCount; i++) {
                SndAmbient* ambient{ asset->ambients + i };

                if (*ambient->name)
                    hashutils::Add(ambient->name);
                if (*ambient->reverbName)
                    hashutils::Add(ambient->reverbName);
                if (*ambient->nearVerbName)
                    hashutils::Add(ambient->nearVerbName);
                if (*ambient->farVerbName)
                    hashutils::Add(ambient->farVerbName);
                if (*ambient->duck)
                    hashutils::Add(ambient->duck);

                json.BeginObject();
                json.WriteFieldValueString("name", ambient->name);
                json.WriteFieldValueString("reverbName", ambient->reverbName);
                json.WriteFieldValueString("nearVerbName", ambient->nearVerbName);
                json.WriteFieldValueString("farVerbName", ambient->farVerbName);
                json.WriteFieldValueString("duck", ambient->duck);
                json.WriteFieldValueXHash("id", ambient->id);
                json.WriteFieldValueXHash("reverb", ambient->reverb);
                json.WriteFieldValueXHash("nearVerb", ambient->nearVerb);
                json.WriteFieldValueXHash("farVerb", ambient->farVerb);
                json.WriteFieldValueXHash("loop", ambient->loop);
                json.WriteFieldValueXHash("duckId", ambient->duckId);
                json.WriteFieldValueXHash("entityContextType0", ambient->entityContextType0);
                json.WriteFieldValueXHash("entityContextValue0", ambient->entityContextValue0);
                json.WriteFieldValueXHash("entityContextType1", ambient->entityContextType1);
                json.WriteFieldValueXHash("entityContextValue1", ambient->entityContextValue1);
                json.WriteFieldValueXHash("entityContextType2", ambient->entityContextType2);
                json.WriteFieldValueXHash("entityContextValue2", ambient->entityContextValue2);
                json.WriteFieldValueXHash("globalContextType", ambient->globalContextType);
                json.WriteFieldValueXHash("globalContextValue", ambient->globalContextValue);

                json.WriteFieldValueBool("defaultRoom", ambient->defaultRoom);
                json.WriteFieldValueNumber("reverbDryLevel", ambient->reverbDryLevel);
                json.WriteFieldValueNumber("reverbWetLevel", ambient->reverbWetLevel);
                json.WriteFieldValueNumber("unk230", ambient->unk230);
                json.WriteFieldValueNumber("unk234", ambient->unk234);

                json.EndObject();
            }
            json.EndArray();

            json.WriteFieldNameString("scriptIdLookups");
            json.BeginArray();
            for (size_t i = 0; i < asset->scriptIdLookupCount; i++) {
                SndDialogScriptIdLookup* scriptIdLookup{ asset->scriptIdLookups + i };

                json.BeginObject();
                json.WriteFieldValueXHash("scriptId", scriptIdLookup->scriptId);
                json.WriteFieldValueXHash("aliasId", scriptIdLookup->aliasId);
                json.EndObject();
            }
            json.EndArray();

            auto WriteSndStream = [&json](const char* name, SndBankStreamKeys* bnk) {
                json.WriteFieldNameString(name);
                json.BeginObject();
                json.WriteFieldValueXAsset("streamKey1", games::bo4::pool::ASSET_TYPE_STREAMKEY, bnk->streamKey1);
                json.WriteFieldValueNumber("dataSize1", bnk->len1);
                json.WriteFieldValueXAsset("fileKey", games::bo4::pool::ASSET_TYPE_STREAMKEY, bnk->fileKey);
                json.WriteFieldValueNumber("fileLen", bnk->fileLen);
                json.EndObject();
            };

            auto WriteMusicAsset = [&json](const char* name, games::bo4::sound::SndMusicAsset& a) {
                json.WriteFieldNameString(name);
                json.BeginObject();
                json.WriteFieldValueString("alias", std::string{ a.aliasName, strnlen(a.aliasName, sizeof(a.aliasName)) });
                json.WriteFieldValueXHash("aliasId", a.aliasId);
                json.WriteFieldValueBool("looping", a.looping);
                json.WriteFieldValueBool("completeLoop", a.completeLoop);
                json.WriteFieldValueBool("removeAfterPlay", a.removeAfterPlay);
                json.WriteFieldValueBool("playAsFirstRandom", a.playAsFirstRandom);
                json.WriteFieldValueBool("startSync", a.startSync);
                json.WriteFieldValueBool("stopSync", a.stopSync);
                json.WriteFieldValueBool("completeOnStop", a.completeOnStop);
                json.WriteFieldValueNumber("loopStartOffset", a.loopStartOffset);
                json.WriteFieldValueNumber("bpm", a.bpm);
                json.WriteFieldValueNumber("assetType", a.assetType);
                json.WriteFieldValueNumber("loopNumber", a.loopNumber);
                json.WriteFieldValueNumber("order", a.order);
                json.WriteFieldValueNumber("startDelayBeats", a.startDelayBeats);
                json.WriteFieldValueNumber("startFadeBeats", a.startFadeBeats);
                json.WriteFieldValueNumber("stopDelayBeats", a.stopDelayBeats);
                json.WriteFieldValueNumber("stopFadeBeats", a.stopFadeBeats);
                json.WriteFieldValueNumber("startOffsetFrames", a.startOffsetFrames);
                json.WriteFieldValueNumber("meter", a.meter);
                json.EndObject();
            };

            // music: sets of states, each an intro, loops and an exit of aliases (soundsetmusicstate)
            json.WriteFieldNameString("musicSets");
            json.BeginArray();
            for (size_t i = 0; i < asset->musicSetCount; i++) {
                games::bo4::sound::SndMusicSet& set{ asset->musicSets[i] };
                json.BeginObject();
                json.WriteFieldValueString("name", std::string{ set.name, strnlen(set.name, sizeof(set.name)) });
                json.WriteFieldValueXHash("id", set.id);
                json.WriteFieldNameString("states");
                json.BeginArray();
                for (size_t j = 0; j < set.stateCount; j++) {
                    games::bo4::sound::SndMusicState& state{ set.states[j] };
                    json.BeginObject();
                    json.WriteFieldValueString("name", std::string{ state.name, strnlen(state.name, sizeof(state.name)) });
                    json.WriteFieldValueXHash("id", state.id);
                    json.WriteFieldValueNumber("order", state.order);
                    json.WriteFieldValueBool("isRandom", state.isRandom);
                    json.WriteFieldValueBool("isSequential", state.isSequential);
                    json.WriteFieldValueBool("skipPreviousExit", state.skipPreviousExit);
                    json.WriteFieldValueNumber("status", state.status);
                    WriteMusicAsset("intro", state.intro);
                    WriteMusicAsset("exit", state.exit);
                    json.WriteFieldNameString("loops");
                    json.BeginArray();
                    for (size_t k = 0; k < state.loopCount; k++) {
                        json.BeginObject();
                        WriteMusicAsset("loop", state.loops[k]);
                        json.EndObject();
                    }
                    json.EndArray();
                    json.EndObject();
                }
                json.EndArray();
                json.EndObject();
            }
            json.EndArray();

            WriteSndStream("streamKeysSABL", &asset->streamKeysSABL);
            WriteSndStream("streamKeysSABS", &asset->streamKeysSABS);

            json.EndObject();

            if (!json.WriteToFile(outFile)) {
                LOG_ERROR("Error when dumping {}", outFile.string());
            }
        }
    };

    utils::MapAdder<ImplWorker, games::bo4::pool::XAssetType, Worker> impl{
        GetWorkers(), games::bo4::pool::XAssetType::ASSET_TYPE_SOUND
    };
} // namespace