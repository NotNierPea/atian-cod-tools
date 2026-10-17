#include <includes.hpp>
#include <tools/fastfile/linkers/linker_bo4.hpp>
#include <games/bo4/sound_alias.hpp>

namespace {
    using namespace fastfile::linker::bo4;
    using namespace games::bo4::sound;

    struct XPakEntryInfo {
        uint64_t key;
        int64_t offset;
        uint64_t size;
        unsigned __int64 xpakIndex : 5;
        unsigned __int64 compressed : 1;
        unsigned __int64 valid : 1;
        unsigned __int64 adjacentLeftType : 3;
        unsigned __int64 adjacentRightType : 3;
        unsigned __int64 adjacentLeft : 18;
        unsigned __int64 adjacentRight : 18;
        unsigned __int64 padding : 15;
    };
    static_assert(sizeof(XPakEntryInfo) == 0x20);

    enum StreamKeyFlags : unsigned __int8 {
        SKF_STREAMER = 0x0,
        SKF_STREAMER_CPU = 0x2,
        SKF_MEMORY = 0x1,
    };

    struct StreamKey {
        XHash name;
        XPakEntryInfo xpakEntry;
        byte* data;
        uint64_t unk38;
        uint32_t size;
        uint16_t unk44;
        byte unk46;
        StreamKeyFlags keyFlags;
    };
    static_assert(sizeof(StreamKey) == 0x48);

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
        SndMusicSet* musicSets;
        uint32_t unkc0_count;
        uint64_t unkc0;
        SndBankStreamKeys streamKeysSABL;
        SndBankStreamKeys streamKeysSABS;
        bool patchZone;
    };
    static_assert(sizeof(SndBank) == 0x130);

    constexpr uint32_t MAGIC = 0x23585532;
    enum SndAssetBankVersion : uint32_t {
        SNDV15 = 0x15,
    };
    enum SndAssetFormat : byte {
        SND_ASSET_FORMAT_PCMS16 = 0x0,
        SND_ASSET_FORMAT_PCMS24 = 0x1,
        SND_ASSET_FORMAT_PCMS32 = 0x2,
        SND_ASSET_FORMAT_IEEE = 0x3,
        SND_ASSET_FORMAT_XMA4 = 0x4,
        SND_ASSET_FORMAT_MP3 = 0x5,
        SND_ASSET_FORMAT_MSADPCM = 0x6,
        SND_ASSET_FORMAT_WMA = 0x7,
        SND_ASSET_FORMAT_FLAC = 0x8,
        SND_ASSET_FORMAT_WIIUADPCM = 0x9,
        SND_ASSET_FORMAT_MPC = 0xA,
        SND_ASSET_FORMAT_COUNT = 0xB,
    };

    const char* SndAssetFormatNames[]{
        "PCMS16", "PCMS24", "PCMS32", "IEEE", "XMA4", "MP3", "MSADPCM", "WMA", "FLAC", "WIIUADPCM", "MPC",
    };
    const char* SndAssetFormatName(SndAssetFormat format) {
        if (format < ACTS_ARRAYSIZE(SndAssetFormatNames))
            return SndAssetFormatNames[format];
        return utils::va("Invalid:%d", (int)format);
    }

    SndAssetFormat SndAssetFormatId(const char* name) {
        for (size_t i = 0; i < ACTS_ARRAYSIZE(SndAssetFormatNames); i++) {
            if (!_strcmpi(SndAssetFormatNames[i], name)) {
                return (SndAssetFormat)i;
            }
        }
        return SndAssetFormat::SND_ASSET_FORMAT_COUNT;
    }

    struct SndAssetBankEntryBo4 {
        uint64_t id;
        uint64_t idnull;
        uint64_t offset;
        uint32_t size;
        uint32_t frameCount;
        uint32_t order;
        uint32_t unk24;
        uint8_t frameRateIndex;
        uint8_t channelCount;
        uint8_t looping;
        SndAssetFormat format;
        uint32_t unk3c;
        std::string buffer{};

        void Read(core::bytebuffer::AbstractByteBuffer& buffer) {
            // XHash
            id = buffer.Read<uint64_t>();            // 8
            buffer.Skip<uint64_t>();                 // 10
            offset = buffer.Read<uint64_t>();        // 18
            size = buffer.Read<uint32_t>();          // 1c
            frameCount = buffer.Read<uint32_t>();    // 20
            order = buffer.Read<uint32_t>();         // 24
            unk24 = buffer.Read<uint32_t>();         // 28
            frameRateIndex = buffer.Read<uint8_t>(); // 29
            channelCount = buffer.Read<uint8_t>();   // 2a
            looping = buffer.Read<uint8_t>();        // 2b
            format = buffer.Read<SndAssetFormat>();  // 2c
            unk3c = buffer.Read<uint32_t>();         // 30
        }

        void Write(std::vector<byte>& data) const {
            size_t k{ data.size() };
            utils::WriteValue<uint64_t>(data, id);            // 8
            utils::WriteValue<uint64_t>(data, 0);             // 10
            utils::WriteValue<uint64_t>(data, offset);        // 18
            utils::WriteValue<uint32_t>(data, size);          // 1c
            utils::WriteValue<uint32_t>(data, frameCount);    // 20
            utils::WriteValue<uint32_t>(data, order);         // 24
            utils::WriteValue<uint32_t>(data, unk24);         // 28
            utils::WriteValue<uint8_t>(data, frameRateIndex); // 29
            utils::WriteValue<uint8_t>(data, channelCount);   // 2a
            utils::WriteValue<uint8_t>(data, looping);        // 2b
            utils::WriteValue<SndAssetFormat>(data, format);  // 2c
            utils::WriteValue<uint32_t>(data, unk3c);         // 30
            assert(data.size() - k == 0x30 && "invalid asset write");
        }
    };
    struct SndAssetName {
        uint64_t hash;
        byte pad[0x78];
    };
    static_assert(sizeof(SndAssetName) == 0x80);
    struct SndAssetBankHeader {
        uint32_t magic;
        SndAssetBankVersion version;
        uint32_t entrySize;
        uint32_t checksumSize;
        uint32_t dependencySize;
        uint32_t entryCount;
        uint32_t dependencyCount;
        uint32_t pad32;
        int64_t fileSize;
        int64_t entryOffset;
        int64_t checksumOffset;
        unsigned __int8 checksumChecksum[16];
        char dependencies[512];
        int64_t SourceChecksumOffset;
        int64_t AssetNameOffset;
        char zoneName[64];
        char platform[8];
        char language[2];
        char convertedAssetVersion0[4];
        char padding[1370];
    };
    static_assert(sizeof(SndAssetBankHeader) == 0x800);

    // a sound asset bank: header, names, checksums, the data of the assets, the entries
    std::vector<byte> CreateSab(
        const std::string& zone, const std::string& lang, const std::string& plt,
        std::vector<SndAssetBankEntryBo4>& entries
    ) {
        std::vector<byte> sab{};

        // the game finds an asset by a binary search on its id (sub_1440839C0)
        std::sort(entries.begin(), entries.end(), [](const auto& e1, const auto& e2) { return e1.id < e2.id; });

        // alloc header
        utils::Allocate<SndAssetBankHeader>(sab);

        size_t assetNameOffset{ sab.size() };
        utils::AllocateArray<SndAssetName>(sab, entries.size());
        size_t checksumOffset{ sab.size() };
        utils::Allocate(sab, entries.size() * 0x10);
        size_t srcChecksumOffset{ sab.size() };
        utils::Allocate(sab, entries.size() * 0x10);

        for (size_t i = 0; i < entries.size(); i++) {
            SndAssetBankEntryBo4& entry{ entries[i] };
            // write entry
            entry.offset = (uint64_t)utils::WriteValue(sab, entry.buffer.data(), entry.buffer.size());
            entry.size = (uint32_t)entry.buffer.size();
        }

        size_t entryOffsets{ sab.size() };
        for (size_t i = 0; i < entries.size(); i++) {
            const SndAssetBankEntryBo4& entry{ entries[i] };
            // write entry
            entry.Write(sab);
            // write name
            *(uint64_t*)&sab[assetNameOffset + i * sizeof(SndAssetName)] = entry.id;
        }

        // write header
        SndAssetBankHeader& sndHeader{ *(SndAssetBankHeader*)sab.data() };
        sndHeader.magic = MAGIC;
        sndHeader.version = SNDV15;
        *(uint32_t*)(&sndHeader.convertedAssetVersion0[0]) = 0x14;
        sndHeader.pad32 = 2; // idk

        std::snprintf(sndHeader.zoneName, sizeof(sndHeader.zoneName), "%s", zone.c_str());
        //  cut at 2 chars fr,al,en,...
        std::memcpy(
            sndHeader.language,
            lang.data(),
            lang.size() < sizeof(sndHeader.language) ? lang.size() + 1 : sizeof(sndHeader.language)
        );
        std::memcpy(
            sndHeader.platform,
            plt.data(),
            plt.size() < sizeof(sndHeader.platform) ? plt.size() + 1 : sizeof(sndHeader.platform)
        );

        sndHeader.entrySize = 0x30;
        sndHeader.checksumSize = 0x10;
        sndHeader.dependencySize = 0x40;

        sndHeader.dependencyCount = 0;
        sndHeader.entryCount = (uint32_t)entries.size();
        sndHeader.entryOffset = (int64_t)entryOffsets;
        sndHeader.AssetNameOffset = (int64_t)assetNameOffset;
        sndHeader.SourceChecksumOffset = (int64_t)srcChecksumOffset;
        sndHeader.checksumOffset = (int64_t)checksumOffset;
        // the banks of the game are 0x80 aligned
        size_t fileSize{ (sab.size() + 0x7F) & ~(size_t)0x7F };
        sndHeader.fileSize = (int64_t)fileSize;
        sab.resize(fileSize);

        return sab;
    }

    // frame rates of SndAssetBankEntry::frameRateIndex
    constexpr uint32_t SND_FRAME_RATES[]{ 8000, 12000, 16000, 24000, 32000, 44100, 48000, 96000, 192000 };

    // a 16 bits pcm wav into an entry, a format the pc decoders read (sub_143C99EA0)
    bool ReadWav(const std::filesystem::path& path, SndAssetBankEntryBo4& entry) {
        std::string data{};
        if (!utils::ReadFile(path, data) || data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) ||
            std::memcmp(data.data() + 8, "WAVE", 4)) {
            LOG_ERROR("{} isn't a wav file", path.string());
            return false;
        }
        uint16_t format{}, channels{}, bits{};
        uint32_t rate{};
        const char* pcm{};
        size_t pcmSize{};
        for (size_t off = 12; off + 8 <= data.size();) {
            const char* id{ data.data() + off };
            size_t len{ *(const uint32_t*)(data.data() + off + 4) };
            const char* chunk{ data.data() + off + 8 };
            len = std::min(len, data.size() - off - 8);
            if (!std::memcmp(id, "fmt ", 4) && len >= 16) {
                format = *(const uint16_t*)chunk;
                channels = *(const uint16_t*)(chunk + 2);
                rate = *(const uint32_t*)(chunk + 4);
                bits = *(const uint16_t*)(chunk + 14);
                // WAVE_FORMAT_EXTENSIBLE, the sub format starts with the format
                if (format == 0xFFFE && len >= 26) {
                    format = *(const uint16_t*)(chunk + 24);
                }
            } else if (!std::memcmp(id, "data", 4)) {
                pcm = chunk;
                pcmSize = len;
            }
            off += 8 + len + (len & 1);
        }
        if (format != 1 || bits != 16 || !channels || !pcm || !pcmSize) {
            LOG_ERROR("{}: only 16 bits pcm wav files are supported (format {}, {} bits)", path.string(), format, bits);
            return false;
        }
        auto it{ std::find(std::begin(SND_FRAME_RATES), std::end(SND_FRAME_RATES), rate) };
        if (it == std::end(SND_FRAME_RATES)) {
            LOG_ERROR("{}: unsupported frame rate {}", path.string(), rate);
            return false;
        }
        entry.format = SND_ASSET_FORMAT_PCMS16;
        entry.channelCount = (uint8_t)channels;
        entry.frameRateIndex = (uint8_t)(it - std::begin(SND_FRAME_RATES));
        entry.frameCount = (uint32_t)(pcmSize / (2ull * channels));
        entry.buffer.assign(pcm, pcmSize);
        return true;
    }


    /*
     * FLAC as the game decodes it (sub_143C99900, libFLAC from the entry's bytes): a whole stream with only its
     * STREAMINFO, 16 bits, frames of exactly 1024 samples (its write callback takes each frame as 1024, sub_143C995B0),
     * and less than 0x2000 bytes read per frame. The entry gives the rate, channels and frame count.
     */
    constexpr uint32_t FLAC_BLOCK_SIZE = 1024;

    class FlacBitWriter {
        std::vector<byte>& out;
        uint64_t acc{};
        int count{};

      public:
        FlacBitWriter(std::vector<byte>& out) : out(out) {}

        void Write(uint64_t v, int bits) {
            if (bits > 32) {
                Write(v >> 32, bits - 32);
                bits = 32;
            }
            acc = (acc << bits) | (v & ((1ull << bits) - 1));
            count += bits;
            while (count >= 8) {
                count -= 8;
                out.push_back((byte)(acc >> count));
            }
            acc &= (1ull << count) - 1;
        }

        void Unary(uint32_t zeros) {
            while (zeros >= 32) {
                Write(0, 32);
                zeros -= 32;
            }
            Write(1, zeros + 1);
        }

        void Align() {
            if (count) {
                Write(0, 8 - count);
            }
        }
    };

    byte FlacCrc8(const byte* data, size_t len) {
        byte crc{};
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (int b = 0; b < 8; b++) {
                crc = (byte)(crc & 0x80 ? (crc << 1) ^ 0x07 : crc << 1);
            }
        }
        return crc;
    }

    uint16_t FlacCrc16(const byte* data, size_t len) {
        uint16_t crc{};
        for (size_t i = 0; i < len; i++) {
            crc ^= (uint16_t)(data[i] << 8);
            for (int b = 0; b < 8; b++) {
                crc = (uint16_t)(crc & 0x8000 ? (crc << 1) ^ 0x8005 : crc << 1);
            }
        }
        return crc;
    }

    // residuals of a fixed predictor (orders 0 to 4)
    void FlacFixedResiduals(const int32_t* x, uint32_t n, uint32_t order, std::vector<int32_t>& res) {
        res.resize(n - order);
        for (uint32_t i = order; i < n; i++) {
            int64_t r{};
            switch (order) {
            case 0: r = x[i]; break;
            case 1: r = (int64_t)x[i] - x[i - 1]; break;
            case 2: r = (int64_t)x[i] - 2ll * x[i - 1] + x[i - 2]; break;
            case 3: r = (int64_t)x[i] - 3ll * x[i - 1] + 3ll * x[i - 2] - x[i - 3]; break;
            default: r = (int64_t)x[i] - 4ll * x[i - 1] + 6ll * x[i - 2] - 4ll * x[i - 3] + x[i - 4]; break;
            }
            res[i - order] = (int32_t)r;
        }
    }

    inline uint32_t FlacZigZag(int32_t r) { return ((uint32_t)r << 1) ^ (uint32_t)(r >> 31); }

    struct FlacRicePlan {
        uint32_t partitionOrder{};
        uint32_t params[16]{};
        uint64_t bits{ UINT64_MAX };
    };

    // the partition order and rice parameters with the fewest bits
    FlacRicePlan FlacPlanRice(const std::vector<int32_t>& res, uint32_t blockSize, uint32_t order) {
        FlacRicePlan best{};
        for (uint32_t p = 0; p <= 4; p++) {
            uint32_t parts{ 1u << p };
            if (blockSize % parts || (blockSize >> p) <= order) {
                break;
            }
            FlacRicePlan plan{};
            plan.partitionOrder = p;
            plan.bits = 2 + 4;
            size_t start{};
            for (uint32_t k = 0; k < parts; k++) {
                size_t len{ (blockSize >> p) - (k ? 0 : order) };
                uint64_t bestBits{ UINT64_MAX };
                for (uint32_t param = 0; param < 15; param++) {
                    uint64_t bits{ 4 };
                    for (size_t i = start; i < start + len; i++) {
                        bits += (FlacZigZag(res[i]) >> param) + 1 + param;
                    }
                    if (bits < bestBits) {
                        bestBits = bits;
                        plan.params[k] = param;
                    }
                }
                plan.bits += bestBits;
                start += len;
            }
            if (plan.bits < best.bits) {
                best = plan;
            }
        }
        return best;
    }

    void FlacWriteSubframe(FlacBitWriter& bw, const int32_t* x, uint32_t n) {
        std::vector<int32_t> res{};
        uint32_t bestOrder{};
        FlacRicePlan bestPlan{};
        for (uint32_t order = 0; order <= 4 && order < n; order++) {
            FlacFixedResiduals(x, n, order, res);
            FlacRicePlan plan{ FlacPlanRice(res, n, order) };
            if (plan.bits == UINT64_MAX) {
                continue;
            }
            plan.bits += 16ull * order;
            if (plan.bits < bestPlan.bits) {
                bestPlan = plan;
                bestOrder = order;
            }
        }
        // verbatim when it is smaller or nothing fits
        if (bestPlan.bits >= 16ull * n) {
            bw.Write(0, 1);
            bw.Write(0b000001, 6);
            bw.Write(0, 1);
            for (uint32_t i = 0; i < n; i++) {
                bw.Write((uint16_t)x[i], 16);
            }
            return;
        }
        bw.Write(0, 1);
        bw.Write(0b001000 | bestOrder, 6);
        bw.Write(0, 1);
        for (uint32_t i = 0; i < bestOrder; i++) {
            bw.Write((uint16_t)x[i], 16);
        }
        FlacFixedResiduals(x, n, bestOrder, res);
        bw.Write(0, 2);
        bw.Write(bestPlan.partitionOrder, 4);
        uint32_t parts{ 1u << bestPlan.partitionOrder };
        size_t start{};
        for (uint32_t k = 0; k < parts; k++) {
            size_t len{ (n >> bestPlan.partitionOrder) - (k ? 0 : bestOrder) };
            uint32_t param{ bestPlan.params[k] };
            bw.Write(param, 4);
            for (size_t i = start; i < start + len; i++) {
                uint32_t u{ FlacZigZag(res[i]) };
                bw.Unary(u >> param);
                if (param) {
                    bw.Write(u & ((1u << param) - 1), param);
                }
            }
            start += len;
        }
    }

    // the 16 bits pcm of an entry to a flac stream
    void EncodeFlac(SndAssetBankEntryBo4& entry) {
        uint32_t channels{ entry.channelCount };
        uint32_t frames{ entry.frameCount };
        const int16_t* pcm{ (const int16_t*)entry.buffer.data() };
        std::vector<byte> out{ 'f', 'L', 'a', 'C' };
        {
            FlacBitWriter bw{ out };
            bw.Write(1, 1); // last metadata block
            bw.Write(0, 7);
            bw.Write(34, 24);
            bw.Write(FLAC_BLOCK_SIZE, 16);
            bw.Write(FLAC_BLOCK_SIZE, 16);
            bw.Write(0, 24);
            bw.Write(0, 24);
            bw.Write(SND_FRAME_RATES[entry.frameRateIndex], 20);
            bw.Write(channels - 1, 3);
            bw.Write(15, 5);
            bw.Write(frames, 36);
            bw.Write(0, 64); // no md5
            bw.Write(0, 64);
        }
        std::vector<int32_t> samples(FLAC_BLOCK_SIZE);
        uint32_t frameNumber{};
        for (uint32_t start = 0; start < frames; start += FLAC_BLOCK_SIZE, frameNumber++) {
            uint32_t n{ std::min(FLAC_BLOCK_SIZE, frames - start) };
            size_t frameStart{ out.size() };
            FlacBitWriter bw{ out };
            bw.Write(0x3FFE, 14);
            bw.Write(0, 1);
            bw.Write(0, 1); // fixed block size
            bw.Write(n == FLAC_BLOCK_SIZE ? 0b1010 : 0b0111, 4);
            bw.Write(0, 4); // rate of the streaminfo
            bw.Write(channels - 1, 4);
            bw.Write(0b100, 3); // 16 bits
            bw.Write(0, 1);
            // frame number, utf-8 coded
            if (frameNumber < 0x80) {
                bw.Write(frameNumber, 8);
            } else if (frameNumber < 0x800) {
                bw.Write(0xC0 | (frameNumber >> 6), 8);
                bw.Write(0x80 | (frameNumber & 0x3F), 8);
            } else if (frameNumber < 0x10000) {
                bw.Write(0xE0 | (frameNumber >> 12), 8);
                bw.Write(0x80 | ((frameNumber >> 6) & 0x3F), 8);
                bw.Write(0x80 | (frameNumber & 0x3F), 8);
            } else {
                bw.Write(0xF0 | (frameNumber >> 18), 8);
                bw.Write(0x80 | ((frameNumber >> 12) & 0x3F), 8);
                bw.Write(0x80 | ((frameNumber >> 6) & 0x3F), 8);
                bw.Write(0x80 | (frameNumber & 0x3F), 8);
            }
            if (n != FLAC_BLOCK_SIZE) {
                bw.Write(n - 1, 16);
            }
            bw.Write(FlacCrc8(out.data() + frameStart, out.size() - frameStart), 8);
            for (uint32_t c = 0; c < channels; c++) {
                for (uint32_t i = 0; i < n; i++) {
                    samples[i] = pcm[(size_t)(start + i) * channels + c];
                }
                FlacWriteSubframe(bw, samples.data(), n);
            }
            bw.Align();
            bw.Write(FlacCrc16(out.data() + frameStart, out.size() - frameStart), 16);
        }
        entry.format = SND_ASSET_FORMAT_FLAC;
        entry.buffer.assign((const char*)out.data(), out.size());
    }

    // a flac file into an entry: checked against what the game reads and cut to its STREAMINFO and frames
    bool ReadFlac(const std::filesystem::path& path, SndAssetBankEntryBo4& entry) {
        std::string data{};
        if (!utils::ReadFile(path, data) || data.size() < 42 || std::memcmp(data.data(), "fLaC", 4)) {
            LOG_ERROR("{} isn't a flac file", path.string());
            return false;
        }
        const byte* d{ (const byte*)data.data() };
        size_t off{ 4 };
        const byte* info{};
        bool last{};
        while (!last) {
            if (off + 4 > data.size()) {
                LOG_ERROR("{}: bad flac metadata", path.string());
                return false;
            }
            last = (d[off] & 0x80) != 0;
            uint32_t type{ d[off] & 0x7Fu };
            uint32_t len{ ((uint32_t)d[off + 1] << 16) | ((uint32_t)d[off + 2] << 8) | d[off + 3] };
            if (off + 4 + len > data.size()) {
                LOG_ERROR("{}: bad flac metadata", path.string());
                return false;
            }
            if (type == 0 && len == 34) {
                info = d + off + 4;
            }
            off += 4 + len;
        }
        if (!info) {
            LOG_ERROR("{}: no flac STREAMINFO", path.string());
            return false;
        }
        uint32_t minBlock{ ((uint32_t)info[0] << 8) | info[1] };
        uint32_t maxBlock{ ((uint32_t)info[2] << 8) | info[3] };
        uint32_t rate{ ((uint32_t)info[10] << 12) | ((uint32_t)info[11] << 4) | (info[12] >> 4) };
        uint32_t channels{ ((info[12] >> 1) & 7u) + 1 };
        uint32_t bits{ (((info[12] & 1u) << 4) | (info[13] >> 4)) + 1 };
        uint64_t frames{ ((uint64_t)(info[13] & 0xF) << 32) | ((uint64_t)info[14] << 24) | ((uint64_t)info[15] << 16) |
                         ((uint64_t)info[16] << 8) | info[17] };
        if (minBlock != FLAC_BLOCK_SIZE || maxBlock != FLAC_BLOCK_SIZE || bits != 16 || !frames) {
            LOG_ERROR(
                "{}: the game reads 16 bits flac of 1024 samples blocks ({} bits, blocks {}-{}), encode it with "
                "flac -b 1024 or let acts do it from a wav with \"format\": \"flac\"",
                path.string(), bits, minBlock, maxBlock
            );
            return false;
        }
        auto it{ std::find(std::begin(SND_FRAME_RATES), std::end(SND_FRAME_RATES), rate) };
        if (it == std::end(SND_FRAME_RATES)) {
            LOG_ERROR("{}: unsupported frame rate {}", path.string(), rate);
            return false;
        }
        // fLaC, the STREAMINFO as the last block, the frames
        std::string out{ "fLaC" };
        out.push_back((char)0x80);
        out.push_back(0);
        out.push_back(0);
        out.push_back(34);
        out.append((const char*)info, 34);
        out.append(data, off, std::string::npos);
        entry.format = SND_ASSET_FORMAT_FLAC;
        entry.channelCount = (uint8_t)channels;
        entry.frameRateIndex = (uint8_t)(it - std::begin(SND_FRAME_RATES));
        entry.frameCount = (uint32_t)frames;
        entry.buffer = std::move(out);
        return true;
    }

    // a wav or a flac, a wav made flac when asked
    bool ReadSound(const std::filesystem::path& path, SndAssetBankEntryBo4& entry, bool flac) {
        std::string ext{ path.extension().string() };
        if (!_strcmpi(ext.data(), ".flac")) {
            return ReadFlac(path, entry);
        }
        if (!ReadWav(path, entry)) {
            return false;
        }
        if (flac) {
            EncodeFlac(entry);
        }
        return true;
    }

    int HexDigit(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    bool ReadHex(const rapidjson::Value* val, std::vector<byte>& out) {
        out.clear();
        if (!val || !val->IsString() || (val->GetStringLength() & 1)) {
            return false;
        }
        const char* str{ val->GetString() };
        for (size_t i = 0; i < val->GetStringLength(); i += 2) {
            int hi{ HexDigit(str[i]) };
            int lo{ HexDigit(str[i + 1]) };
            if (hi < 0 || lo < 0) {
                return false;
            }
            out.push_back((byte)((hi << 4) | lo));
        }
        return true;
    }

    const rapidjson::Value* Member(const rapidjson::Value& obj, const char* name) {
        if (!obj.IsObject()) {
            return nullptr;
        }
        auto it{ obj.FindMember(name) };
        return it == obj.MemberEnd() || it->value.IsNull() ? nullptr : &it->value;
    }

    /*
     * A stream key of the bank (sub_142E2DFE0). Inline (flags 3): the data is in the fastfile, the game only says it is
     * in for the keys at 0xC8/0xF8 when the bank is linked (sub_142EBB720). Streamed (flags 1, STREAMER_CPU): the data
     * is in the xpak of the zone, the streamer reads it once the first key asks for it (sub_144084060).
     */
    void WriteStreamKey(
        BO4LinkContext& ctx, BO4FFContext& ff, StreamKey*& ref, const std::string& name, std::vector<byte>&& data,
        bool streamed
    ) {
        ref = (StreamKey*)fastfile::ALLOC_PTR;
        ff.data.PushStream(XFILE_BLOCK_TEMP);
        ff.data.Align(8);
        StreamKey& key{ ff.data.AllocStreamRef<StreamKey>() };
        key.name.name = ctx.HashXHash(name);
        key.data = (byte*)fastfile::ALLOC_PTR;
        key.size = (uint32_t)data.size();
        key.unk44 = 512;
        // owner of the key, the sound system (its loaded callback queues sub_144084060)
        key.unk46 = 5;
        key.keyFlags = (StreamKeyFlags)(streamed ? 1 : 3);
        ff.data.PushStream(XFILE_BLOCK_VIRTUAL);
        if (streamed) {
            key.xpakEntry.key = ::hash::Hash64(name.data());
            ff.data.PushStream(XFILE_BLOCK_STREAMER_CPU);
            ff.data.Align(0x10000);
            ff.data.AllocRuntime(data.size());
            ff.data.PopStream();
        } else {
            ff.data.Align(0x10000);
            ff.data.WriteStream(data.data(), data.size());
        }
        ff.data.PopStream();
        ff.data.PopStream();
        LOG_INFO("Added subasset streamkey {} (0x{:x} bytes{})", name, data.size(), streamed ? ", xpak" : "");
        if (streamed) {
            AddXPakItem(ff, key.xpakEntry.key, std::move(data));
        }
    }

    // what an alias says about its list, set from its settings or the alias
    struct AliasListSettings {
        bool cullDistance{};
        bool spatial{};
    };

    // a field from a json value: a name for the fields with names, a bool or a number in the units of the field
    bool SetAliasFieldJson(BO4LinkContext& ctx, SndAlias& alias, const SndAliasField& field, const rapidjson::Value& v) {
        if (field.type == SAF_HASH) {
            if (!v.IsString()) {
                return false;
            }
            XHash& h{ *(XHash*)((byte*)&alias + field.offset) };
            h = {};
            if (*v.GetString()) {
                h.name = ctx.HashXHash(v.GetString());
            }
            return true;
        }
        if (v.IsString()) {
            return SetAliasFieldName(alias, field, v.GetString());
        }
        if (v.IsBool()) {
            return SetAliasField(alias, field, v.GetBool() ? 1 : 0);
        }
        return v.IsNumber() && SetAliasField(alias, field, v.GetDouble());
    }

    // what a field takes, for the errors
    std::string AliasFieldHelp(const SndAliasField& field) {
        if (field.type == SAF_HASH) {
            return "a name";
        }
        if (field.names) {
            std::string out{};
            for (uint32_t i = 0; i < field.nameCount; i++) {
                out += i ? ", " : "";
                out += field.names[i];
            }
            return field.type == SAF_FLAGS ? std::format("names among {}", out) : out;
        }
        if (field.type == SAF_BITS) {
            return std::format("0 to {}", (1u << field.bits) - 1);
        }
        return std::format("{} to {}", field.min, field.max);
    }

    /*
     * The settings of an alias in a json object: the fields of the game's alias editor by name (ALIAS_FIELDS, any case,
     * in its units: volumes in dB SPL 0-100, pitches in cents, distances in game units, 0-1 units, times in ms), and
     * shortcuts: volume (0-1, linear, both), pitch (cents, both), minDist, maxDist (dry and wet), loop (bool),
     * cullDistance and spatial (of the list, else from the alias). Keys of the alias (name, file, type) are skipped.
     */
    bool ApplyAliasSettings(
        BO4LinkContext& ctx, SndAlias& alias, SndAliasList& list, AliasListSettings& listSet, const rapidjson::Value& obj,
        std::string& error
    ) {
        if (!obj.IsObject()) {
            error = "settings must be an object";
            return false;
        }
        for (auto& m : obj.GetObj()) {
            const char* key{ m.name.GetString() };
            const rapidjson::Value& v{ m.value };
            if (!_strcmpi(key, "name") || !_strcmpi(key, "file") || !_strcmpi(key, "type") || !_strcmpi(key, "settings") ||
                !_strcmpi(key, "source") || !_strcmpi(key, "variants") || !_strcmpi(key, "format")) {
                continue;
            }
            auto Set = [&](const char* fieldName) {
                const SndAliasField* field{ FindAliasField(fieldName) };
                if (SetAliasFieldJson(ctx, alias, *field, v)) {
                    return true;
                }
                error = std::format("{}: takes {}", key, AliasFieldHelp(*field));
                return false;
            };
            bool ok{ true };
            if (!_strcmpi(key, "volume")) {
                if (!v.IsNumber() || v.GetDouble() < 0 || v.GetDouble() > 1) {
                    error = "volume: takes 0 to 1 (linear), VolMin/VolMax take dB";
                    return false;
                }
                alias.volMin = alias.volMax = (float)v.GetDouble();
            } else if (!_strcmpi(key, "pitch")) {
                ok = Set("PitchMin") && Set("PitchMax");
            } else if (!_strcmpi(key, "minDist")) {
                ok = Set("DistMin");
            } else if (!_strcmpi(key, "maxDist")) {
                ok = Set("DistMaxDry") && Set("DistMaxWet");
            } else if (!_strcmpi(key, "loop")) {
                ok = Set("Looping");
            } else if (!_strcmpi(key, "cullDistance")) {
                if (!v.IsNumber() || v.GetDouble() < 0) {
                    error = "cullDistance: takes a distance";
                    return false;
                }
                list.cullDistance = (float)v.GetDouble();
                listSet.cullDistance = true;
            } else if (!_strcmpi(key, "spatial")) {
                if (!v.IsBool()) {
                    error = "spatial: takes true or false";
                    return false;
                }
                list.spatial = v.GetBool();
                listSet.spatial = true;
            } else if (const SndAliasField* field{ FindAliasField(key) }) {
                ok = Set(field->name);
            } else {
                error = std::format("unknown setting {}", key);
                return false;
            }
            if (!ok) {
                return false;
            }
        }
        return true;
    }


    // a name of the music structs: 64 characters and its hash
    bool SetMusicName(BO4LinkContext& ctx, const rapidjson::Value* v, char (&name)[64], XHash& id, std::string& error) {
        if (!v || !v->IsString() || !*v->GetString() || v->GetStringLength() >= sizeof(name)) {
            error = "a name of 1 to 63 characters";
            return false;
        }
        std::memset(name, 0, sizeof(name));
        std::memcpy(name, v->GetString(), v->GetStringLength());
        id = {};
        id.name = ctx.HashXHash(v->GetString());
        return true;
    }

    /*
     * A music asset, the alias a state plays, the fields as the dump writes them: alias, looping, completeLoop,
     * removeAfterPlay, playAsFirstRandom, startSync, stopSync, completeOnStop, loopStartOffset, bpm, assetType,
     * loopNumber, order, startDelayBeats, startFadeBeats, stopDelayBeats, stopFadeBeats, startOffsetFrames, meter.
     * The game's states play their song once: 120 bpm, a meter of 4, one loop.
     */
    bool ReadMusicAsset(BO4LinkContext& ctx, const rapidjson::Value& v, SndMusicAsset& a, std::string& error) {
        a = {};
        a.bpm = 120;
        a.meter = 4;
        a.loopNumber = 1;
        if (!v.IsObject()) {
            error = "a music asset is an object";
            return false;
        }
        if (!SetMusicName(ctx, Member(v, "alias"), a.aliasName, a.aliasId, error)) {
            error = "alias: " + error;
            return false;
        }
        struct {
            const char* name;
            bool* b;
            int32_t* i;
        } fields[]{
            { "looping", &a.looping },
            { "completeLoop", &a.completeLoop },
            { "removeAfterPlay", &a.removeAfterPlay },
            { "playAsFirstRandom", &a.playAsFirstRandom },
            { "startSync", &a.startSync },
            { "stopSync", &a.stopSync },
            { "completeOnStop", &a.completeOnStop },
            { "loopStartOffset", nullptr, (int32_t*)&a.loopStartOffset },
            { "bpm", nullptr, &a.bpm },
            { "assetType", nullptr, &a.assetType },
            { "loopNumber", nullptr, &a.loopNumber },
            { "order", nullptr, &a.order },
            { "startDelayBeats", nullptr, &a.startDelayBeats },
            { "startFadeBeats", nullptr, &a.startFadeBeats },
            { "stopDelayBeats", nullptr, &a.stopDelayBeats },
            { "stopFadeBeats", nullptr, &a.stopFadeBeats },
            { "startOffsetFrames", nullptr, &a.startOffsetFrames },
            { "meter", nullptr, &a.meter },
        };
        for (auto& m : v.GetObj()) {
            const char* key{ m.name.GetString() };
            if (!_strcmpi(key, "alias") || !_strcmpi(key, "aliasId")) {
                continue;
            }
            auto it{ std::find_if(std::begin(fields), std::end(fields), [key](auto& f) { return !_strcmpi(f.name, key); }) };
            if (it == std::end(fields)) {
                error = std::format("unknown music asset field {}", key);
                return false;
            }
            if (it->b) {
                if (!m.value.IsBool()) {
                    error = std::format("{}: takes true or false", key);
                    return false;
                }
                *it->b = m.value.GetBool();
            } else {
                if (!m.value.IsInt()) {
                    error = std::format("{}: takes a number", key);
                    return false;
                }
                *it->i = m.value.GetInt();
            }
        }
        return true;
    }

    // the music of a bank: sets, their states, the loops of the states
    struct BankMusic {
        std::vector<SndMusicSet> sets{};
        std::vector<std::vector<SndMusicState>> states{};
        std::vector<std::vector<std::vector<SndMusicAsset>>> loops{};
    };

    /*
     * "music": [ { "name": "my_set", "states": [ { "name": "my_state", "intro": { asset }, "loops": [ { asset } ],
     * "exit": { asset }, "isRandom": false, "isSequential": true, "skipPreviousExit": false, "order": 0 } ] } ]
     * A state plays with music::setmusicstate("my_state") (soundsetmusicstate on the client); in zombies the game takes
     * zm_<name> first when there is one. The sets of all the banks are searched, their names don't matter.
     */
    bool ReadBankMusic(BO4LinkContext& ctx, const rapidjson::Value& music, BankMusic& out, std::string& error) {
        if (!music.IsArray()) {
            error = "music is a list of sets";
            return false;
        }
        for (const rapidjson::Value& s : music.GetArray()) {
            SndMusicSet& set{ out.sets.emplace_back() };
            set = {};
            if (!SetMusicName(ctx, Member(s, "name"), set.name, set.id, error)) {
                error = "music set name: " + error;
                return false;
            }
            std::vector<SndMusicState>& states{ out.states.emplace_back() };
            std::vector<std::vector<SndMusicAsset>>& setLoops{ out.loops.emplace_back() };
            const rapidjson::Value* sstates{ Member(s, "states") };
            if (!sstates || !sstates->IsArray() || !sstates->Size()) {
                error = std::format("music set {}: no states", set.name);
                return false;
            }
            for (const rapidjson::Value& st : sstates->GetArray()) {
                SndMusicState& state{ states.emplace_back() };
                state = {};
                state.isSequential = true;
                state.order = (int32_t)(states.size() - 1);
                if (!SetMusicName(ctx, Member(st, "name"), state.name, state.id, error)) {
                    error = std::format("music set {}: state name: {}", set.name, error);
                    return false;
                }
                std::vector<SndMusicAsset>& loops{ setLoops.emplace_back() };
                for (auto& m : st.GetObj()) {
                    const char* key{ m.name.GetString() };
                    bool ok{ true };
                    if (!_strcmpi(key, "name")) {
                        continue;
                    } else if (!_strcmpi(key, "intro")) {
                        ok = ReadMusicAsset(ctx, m.value, state.intro, error);
                    } else if (!_strcmpi(key, "exit")) {
                        ok = ReadMusicAsset(ctx, m.value, state.exit, error);
                    } else if (!_strcmpi(key, "loops")) {
                        if (!m.value.IsArray()) {
                            error = "loops is a list";
                            ok = false;
                        } else {
                            for (const rapidjson::Value& l : m.value.GetArray()) {
                                if (!(ok = ReadMusicAsset(ctx, l, loops.emplace_back(), error))) {
                                    break;
                                }
                            }
                        }
                    } else if (!_strcmpi(key, "isRandom") || !_strcmpi(key, "isSequential") ||
                               !_strcmpi(key, "skipPreviousExit")) {
                        if (!m.value.IsBool()) {
                            error = std::format("{}: takes true or false", key);
                            ok = false;
                        } else {
                            bool& b{ !_strcmpi(key, "isRandom") ? state.isRandom
                                     : !_strcmpi(key, "isSequential") ? state.isSequential
                                                                      : state.skipPreviousExit };
                            b = m.value.GetBool();
                        }
                    } else if (!_strcmpi(key, "order")) {
                        if (!m.value.IsInt()) {
                            error = "order: takes a number";
                            ok = false;
                        } else {
                            state.order = m.value.GetInt();
                        }
                    } else {
                        error = std::format("unknown music state field {}", key);
                        ok = false;
                    }
                    if (!ok) {
                        error = std::format("music state {}: {}", state.name, error);
                        return false;
                    }
                }
                if (!state.intro.aliasId && loops.empty()) {
                    error = std::format("music state {}: no intro or loops", state.name);
                    return false;
                }
                // zombies plays zm_<name> first, the name gets the prefix in 64 characters (SNDL_SetMusicState)
                if (std::strlen(state.name) > 61) {
                    LOG_WARNING("music state {}: more than 61 characters, zombies can't look for zm_{}", state.name, state.name);
                }
                state.loopCount = (uint32_t)loops.size();
                LOG_INFO(
                    "Added music state {} ({}{} loop(s){})", state.name, state.intro.aliasId ? "intro, " : "", loops.size(),
                    state.exit.aliasId ? ", exit" : ""
                );
            }
            set.stateCount = (uint32_t)states.size();
        }
        return true;
    }

    // Load_SndBank: the sets, then for each its states, then for each state its loops
    void WriteBankMusic(BO4FFContext& ff, uint32_t& count, SndMusicSet*& ref, BankMusic& music) {
        count = (uint32_t)music.sets.size();
        if (!count) {
            ref = nullptr;
            return;
        }
        ref = (SndMusicSet*)fastfile::ALLOC_PTR;
        ff.data.Align(8);
        SndMusicSet* sets{ ff.data.AllocStreamPtr<SndMusicSet>(count) };
        for (size_t i = 0; i < count; i++) {
            sets[i] = music.sets[i];
            sets[i].states = (SndMusicState*)fastfile::ALLOC_PTR;
        }
        for (size_t i = 0; i < count; i++) {
            std::vector<SndMusicState>& states{ music.states[i] };
            ff.data.Align(8);
            SndMusicState* out{ ff.data.AllocStreamPtr<SndMusicState>(states.size()) };
            for (size_t j = 0; j < states.size(); j++) {
                out[j] = states[j];
                out[j].loops = states[j].loopCount ? (SndMusicAsset*)fastfile::ALLOC_PTR : nullptr;
            }
            for (size_t j = 0; j < states.size(); j++) {
                std::vector<SndMusicAsset>& loops{ music.loops[i][j] };
                if (loops.empty()) {
                    continue;
                }
                ff.data.Align(8);
                ff.data.WriteStream(loops.data(), loops.size() * sizeof(loops[0]));
            }
        }
    }

    /*
     * Sound banks of wav files, the aliases on presets of game aliases (acts ff ... -a sound, the "raw" fields) and
     * their settings by the names of the game's alias editor (ApplyAliasSettings):
     *
     * {
     *     "name": "enh.all",                             // the zone and soundLanguage by default
     *     "templates": "templates/sound_aliases.json",   // { "3d": { "list": raw, "alias": raw, "settings": { } } }
     *     "format": "flac",                              // pcm (default) or flac, for all the aliases
     *     "aliases": [
     *         { "name": "my_sound", "file": "my_sound.wav", "type": "3d", "volume": 0.8, "maxDist": 1500,
     *           "loop": true, "FadeIn": 200, "PitchMin": -100, "PitchMax": 100, "RandomizeType": "pitch" },
     *         { "name": "my_hit", "type": "3d", "file": [ "hit1.wav", "hit2.wav" ] },       // variants, random
     *         { "name": "my_step", "type": "3d", "variants": [ { "file": "a.wav" }, { "file": "b.flac", "volume": 0.5 } ] }
     *     ]
     * }
     *
     * Wav files are 16 bits pcm, made flac with "format": "flac"; flac files are taken as they are when they are what
     * the game reads (16 bits, blocks of 1024 samples, flac -b 1024).
     *
     * The wav files go in the loaded asset bank (sabl), the file key of the bank, streamed from the xpak of the zone;
     * the streamed bank (sabs) is an empty header in the first key, in the fastfile. The game adds the bank once its
     * keys are in (sub_144083E90, sub_144084060) and finds an alias by name % count in the index (sub_144084FE0).
     * A sound played on an entity (playsound, playloopsound) needs its alias in the bgcache (soundalias,<name>).
     */
    class XAssetLinkerImpl : public XAssetLinker {
      public:
        using XAssetLinker::XAssetLinker;

        void Compute(BO4LinkContext& ctx, const char* id, BO4FFContext& ff) override {
            const char* sndPlatform{ ctx.linkCtx.zone.GetConfig("snd.platform", "pc") };
            std::filesystem::path path{ ctx.linkCtx.input / id };
            auto Fail = [&ctx, &path](const std::string& what) {
                LOG_ERROR("Invalid sound bank {}: {}", path.string(), what);
                ctx.error = true;
            };

            core::config::Config scfg{ path };
            if (!scfg.SyncConfig(false)) {
                Fail("can't read");
                return;
            }

            std::string zone{ scfg.GetString("zone", ff.ffname) };
            std::string gameLanguage{ scfg.GetString("gameLanguage", "all") };
            std::string language{ scfg.GetString("soundLanguage", "all") };
            std::string name{ scfg.GetString("name", std::format("{}.{}", zone, language)) };
            uint64_t bankHash{ ctx.HashXHash(name) };

            core::config::Config tcfg{ path.parent_path() / scfg.GetString("templates", "") };
            if (!tcfg.SyncConfig(false)) {
                Fail("can't read the alias templates");
                return;
            }

            struct Alias {
                std::string name;
                std::vector<byte> list;
                std::vector<std::vector<byte>> variants;
            };
            std::vector<Alias> aliases{};
            std::vector<SndAssetBankEntryBo4> entries{};
            const rapidjson::Value* bankFormat{ Member(scfg.main, "format") };

            const rapidjson::Value* cfgAliases{ Member(scfg.main, "aliases") };
            if (!cfgAliases || !cfgAliases->IsArray() || !cfgAliases->Size()) {
                Fail("no aliases");
                return;
            }
            for (const rapidjson::Value& a : cfgAliases->GetArray()) {
                const rapidjson::Value* aname{ Member(a, "name") };
                if (!aname || !aname->IsString()) {
                    Fail("an alias without name");
                    return;
                }
                const rapidjson::Value* atype{ Member(a, "type") };
                const char* type{ atype && atype->IsString() ? atype->GetString() : "2d" };
                const rapidjson::Value* tmpl{ Member(tcfg.main, type) };
                Alias& alias{ aliases.emplace_back() };
                alias.name = aname->GetString();
                std::vector<byte> base{};
                if (!tmpl || !ReadHex(Member(*tmpl, "list"), alias.list) ||
                    alias.list.size() != sizeof(SndAliasList) || !ReadHex(Member(*tmpl, "alias"), base) ||
                    base.size() != sizeof(SndAlias)) {
                    Fail(std::format("no template '{}'", type));
                    return;
                }

                // the variants: "file" (one or a list) or "variants" [ { "file", settings } ], picked at random
                const rapidjson::Value* afile{ Member(a, "file") };
                const rapidjson::Value* avariants{ Member(a, "variants") };
                std::vector<std::pair<const char*, const rapidjson::Value*>> files{};
                if (afile && afile->IsString()) {
                    files.emplace_back(afile->GetString(), nullptr);
                } else if (afile && afile->IsArray()) {
                    for (const rapidjson::Value& f : afile->GetArray()) {
                        if (f.IsString()) {
                            files.emplace_back(f.GetString(), nullptr);
                        }
                    }
                }
                if (avariants && avariants->IsArray()) {
                    for (const rapidjson::Value& v : avariants->GetArray()) {
                        const rapidjson::Value* vfile{ Member(v, "file") };
                        if (!vfile || !vfile->IsString()) {
                            Fail(std::format("{}: a variant without file", alias.name));
                            return;
                        }
                        files.emplace_back(vfile->GetString(), &v);
                    }
                }
                // MAX_VARIANTS of SND_PickSoundAliasFromList
                if (files.empty() || files.size() >= 64) {
                    Fail(std::format("{}: 1 to 63 files", alias.name));
                    return;
                }

                SndAliasList& sl{ *(SndAliasList*)alias.list.data() };
                AliasListSettings listSet{};
                std::string error{};
                const rapidjson::Value* tsettings{ Member(*tmpl, "settings") };
                const rapidjson::Value* asettings{ Member(a, "settings") };
                SndAlias& sb{ *(SndAlias*)base.data() };
                if ((tsettings && !ApplyAliasSettings(ctx, sb, sl, listSet, *tsettings, error)) ||
                    !ApplyAliasSettings(ctx, sb, sl, listSet, a, error) ||
                    (asettings && !ApplyAliasSettings(ctx, sb, sl, listSet, *asettings, error))) {
                    Fail(std::format("{}: {}", alias.name, error));
                    return;
                }
                const rapidjson::Value* aformat{ Member(a, "format") };
                if (!aformat) {
                    aformat = bankFormat;
                }

                double cull{};
                for (size_t k = 0; k < files.size(); k++) {
                    std::vector<byte>& raw{ alias.variants.emplace_back(base) };
                    SndAlias& sa{ *(SndAlias*)raw.data() };
                    const rapidjson::Value* vset{ files[k].second };
                    const rapidjson::Value* vsettings{ vset ? Member(*vset, "settings") : nullptr };
                    if ((vset && !ApplyAliasSettings(ctx, sa, sl, listSet, *vset, error)) ||
                        (vsettings && !ApplyAliasSettings(ctx, sa, sl, listSet, *vsettings, error))) {
                        Fail(std::format("{} variant {}: {}", alias.name, k, error));
                        return;
                    }
                    // the bank only has loaded assets
                    if (GetAliasField(sa, *FindAliasField("Storage")) != 1) {
                        Fail(std::format("{}: Storage must be loaded", alias.name));
                        return;
                    }
                    const rapidjson::Value* vformat{ vset ? Member(*vset, "format") : nullptr };
                    if (!vformat) {
                        vformat = aformat;
                    }
                    if (vformat && (!vformat->IsString() || (_strcmpi(vformat->GetString(), "flac") &&
                                                             _strcmpi(vformat->GetString(), "pcm")))) {
                        Fail(std::format("{}: format takes flac or pcm", alias.name));
                        return;
                    }
                    bool flac{ vformat && !_strcmpi(vformat->GetString(), "flac") };

                    // the asset of the variant, its id is the one of the entry
                    SndAssetBankEntryBo4& entry{ entries.emplace_back() };
                    if (!ReadSound(path.parent_path() / files[k].first, entry, flac)) {
                        ctx.error = true;
                        return;
                    }
                    entry.id = ctx.HashXHash(k ? std::format("{}/{}/{}", name, alias.name, k) : std::format("{}/{}", name, alias.name)) &
                               0x7FFFFFFFFFFFFFFFull;
                    entry.order = (uint32_t)(entries.size() - 1);
                    bool looping{ GetAliasField(sa, *FindAliasField("Looping")) != 0 };
                    entry.looping = looping ? 1 : 0;

                    sa.name = {};
                    sa.name.name = ctx.HashXHash(alias.name);
                    sa.subtitle = {};
                    sa.facialAnimation = 0;
                    sa.assetId = {};
                    sa.assetId.name = entry.id;
                    sa.assetIdSustain = {};
                    sa.assetIdRelease = {};
                    sa.assetFileName = {};
                    sa.assetFileName.name = bankHash;
                    sa.assetFileNameSustain = sa.assetFileName;
                    sa.assetFileNameRelease = sa.assetFileName;
                    sa.revFilename = {};
                    cull = std::max(
                        cull, std::max(
                                  GetAliasField(sa, *FindAliasField("DistMaxDry")),
                                  GetAliasField(sa, *FindAliasField("DistMaxWet"))
                              )
                    );

                    LOG_INFO(
                        "Added sound alias {}{} ({}, {}{}{}, {:.1f} dB, {} Hz, {} channel(s), {} frames, 0x{:x} bytes)",
                        alias.name, files.size() > 1 ? std::format(" variant {}", k) : "", type,
                        GetAliasField(sa, *FindAliasField("PanType")) ? "3d" : "2d", looping ? ", looping" : "",
                        flac || entry.format == SND_ASSET_FORMAT_FLAC ? ", flac" : "",
                        GetAliasField(sa, *FindAliasField("VolMax")), SND_FRAME_RATES[entry.frameRateIndex],
                        entry.channelCount, entry.frameCount, entry.buffer.size()
                    );
                }

                // as the game's lists: the name of its aliases, cut at their farthest distance, spatial when 3d
                SndAlias& first{ *(SndAlias*)alias.variants[0].data() };
                sl.name = first.name;
                sl.count = (int32_t)alias.variants.size();
                sl.sequence = 0;
                if (!listSet.cullDistance) {
                    sl.cullDistance = (float)cull;
                }
                if (!listSet.spatial) {
                    sl.spatial = GetAliasField(first, *FindAliasField("PanType")) != 0;
                }
            }
            if (aliases.size() >= 0xFFFF) {
                Fail("too many aliases");
                return;
            }

            BankMusic music{};
            const rapidjson::Value* cfgMusic{ Member(scfg.main, "music") };
            if (cfgMusic) {
                std::string error{};
                if (!ReadBankMusic(ctx, *cfgMusic, music, error)) {
                    Fail(error);
                    return;
                }
            }

            std::vector<SndAssetBankEntryBo4> streamed{};
            std::vector<byte> sabs{ CreateSab(zone, language, sndPlatform, streamed) };
            std::vector<byte> sabl{ CreateSab(zone, language, sndPlatform, entries) };

            // alias index: name % count, the collisions chained in the free slots
            size_t count{ aliases.size() };
            auto AliasKey = [&aliases](size_t i) {
                return ((SndAliasList*)aliases[i].list.data())->name.name & 0x7FFFFFFFFFFFFFFFull;
            };
            std::vector<SndIndexEntry> index(count, SndIndexEntry{ 0xFFFF, 0xFFFF });
            std::vector<size_t> pending{};
            for (size_t i = 0; i < count; i++) {
                SndIndexEntry& slot{ index[AliasKey(i) % count] };
                if (slot.value == 0xFFFF) {
                    slot.value = (uint16_t)i;
                } else {
                    pending.push_back(i);
                }
            }
            size_t freeSlot{};
            for (size_t i : pending) {
                while (index[freeSlot].value != 0xFFFF) {
                    freeSlot++;
                }
                size_t slot{ AliasKey(i) % count };
                while (index[slot].next != 0xFFFF) {
                    slot = index[slot].next;
                }
                index[slot].next = (uint16_t)freeSlot;
                index[freeSlot].value = (uint16_t)i;
            }

            // Load_SndBank: names, alias lists and their aliases, index, stream keys
            ff.data.PushStream(XFILE_BLOCK_TEMP);
            ff.data.AddAsset(XAssetType::ASSET_TYPE_SOUND, ff.data.Align(8));
            SndBank& bank{ ff.data.AllocStreamRef<SndBank>() };
            bank.nameHash.name = bankHash;
            bank.aliasCount = (uint32_t)count;
            bank.patchZone = false;

            ff.data.PushStream(XFILE_BLOCK_VIRTUAL);

            ff.data.AddXString(bank.name, name.data());
            ff.data.AddXString(bank.zone, zone.data());
            ff.data.AddXString(bank.gameLanguage, gameLanguage.data());
            ff.data.AddXString(bank.soundLanguage, language.data());

            bank.alias = (SndAliasList*)fastfile::ALLOC_PTR;
            ff.data.Align(8);
            SndAliasList* lists{ ff.data.AllocStreamPtr<SndAliasList>(count) };
            for (size_t i = 0; i < count; i++) {
                std::memcpy(&lists[i], aliases[i].list.data(), sizeof(lists[i]));
                lists[i].aliases = (SndAlias*)fastfile::ALLOC_PTR;
            }
            for (size_t i = 0; i < count; i++) {
                ff.data.Align(8);
                for (const std::vector<byte>& variant : aliases[i].variants) {
                    ff.data.WriteStream(variant.data(), variant.size());
                }
            }

            bank.aliasIndex = (SndIndexEntry*)fastfile::ALLOC_PTR;
            ff.data.Align(2);
            ff.data.WriteStream(index);

            // reverbs, ducks, ambients and the lookups stay empty
            WriteBankMusic(ff, bank.musicSetCount, bank.musicSets, music);

            // the keys of the bank, the patch zone ones (0xF8) stay empty
            // the keys of the bank as the game names them (core_ui_all_stream)
            std::string keyName{ name };
            std::replace(keyName.begin(), keyName.end(), '.', '_');
            bank.streamKeysSABL.len1 = (uint32_t)sabs.size();
            bank.streamKeysSABL.fileLen = (uint32_t)sabl.size();
            WriteStreamKey(ctx, ff, bank.streamKeysSABL.streamKey1, keyName + "_stream", std::move(sabs), false);
            WriteStreamKey(ctx, ff, bank.streamKeysSABL.fileKey, keyName + "_load", std::move(sabl), true);

            ff.data.PopStream();

            ff.data.PopStream();

            LOG_INFO(
                "Added asset sound {} (hash_{:x}), {} alias(es), sabl 0x{:x} bytes", name, bankHash, count,
                bank.streamKeysSABL.fileLen
            );
        }
    };

    utils::MapAdder<XAssetLinkerImpl, XAssetType, XAssetLinker> impl{ GetWorkers(), XAssetType::ASSET_TYPE_SOUND,
                                                                      true };
} // namespace
