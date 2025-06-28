#include <includes.hpp>
#include <core/config.hpp>
#include <core/bytebuffer_file.hpp>
#include <tools/ff/fastfile_handlers.hpp>
#include <tools/ff/fastfile_packed.hpp>
#include <utils/data_utils.hpp>

namespace fastfile::packed {
	template<typename T>
	static uint32_t ReadInto(core::bytebuffer::AbstractByteBuffer& reader, T* data) {
		uint32_t size{ reader.Read<uint32_t>() };

		size_t r{ std::min<size_t>(sizeof(T), size) };
		reader.Read(data, r);
		reader.Skip(size - r); // skip ignored data
		return size;
	}

	void PackedFastFileReader::ReadHeader(core::bytebuffer::AbstractByteBuffer& reader) {
		uint32_t magic{ reader.Read<uint32_t>() };
		if (magic != MAGIC && magic != MAGIC_A) {
			throw std::runtime_error(std::format("Invalid magic 0x{:x}", magic));
		}
		uint32_t blocks{ reader.Read<uint32_t>() };

		for (size_t i = 0; i < blocks; i++) {
			uint32_t type{ reader.Read<uint32_t>() };

			switch (type) {
			case ST_VERSION:
				ReadInto(reader, &version);
				LOG_TRACE("ff header version: 0x{:x}", version);
				break;
			case ST_FASTFILE_INFO:
				ReadInto(reader, &ff);
				LOG_TRACE("ff header name: {}", ff.fastfileName);
				break;
			case ST_PLATFORM_DATA:
				ReadInto(reader, &platform);
				LOG_TRACE("ff header platform: server:{}, compression:{}, plt:{}, encrypted:{}",
					platform.server,
					fastfile::GetFastFileCompressionName(platform.compression),
					fastfile::GetFastFilePlatformName(platform.platform),
					platform.encrypted
				);
				break;
			case ST_BUILD_DATA:
				ReadInto(reader, &build);
				LOG_TRACE("ff header build: {}, ts:{}, changeList:0x{:x} archiveChecksum:(0x{:x},0x{:x},0x{:x},0x{:x})", 
					build.builderName, build.timestamp,
					build.changelist,
					build.archiveChecksum[0], build.archiveChecksum[1], build.archiveChecksum[2], build.archiveChecksum[3]
				);
				break;
			case ST_LINK_RESULT:
				ReadInto(reader, &link);
				LOG_TRACE("ff header link: {} {} {}", link.unk50, link.linkResultCode, link.linkResult);
				break;
			case ST_SIZE_DATA:
				ReadInto(reader, &size);
				LOG_TRACE("ff header file sizes: file:0x{:x} memMappedOffset:0x{:x} externalSize:0x{:x}", size.size, size.memMappedOffset, size.externalSize);
				break;
			case ST_BLOCK_SIZES:
				blocksCount = ReadInto(reader, &blockSizes) / sizeof(blockSizes[0]);
				LOG_TRACE("ff header blocks: {}", blocksCount);
				break;
			case ST_IW_DEPS:
				ReadInto(reader, &iwDeps);

				LOG_TRACE("ff header iw deps: {} 0x{:x}", iwDeps.fastfileHash ? hashutils::ExtractTmp("hash", iwDeps.fastfileHash) : "", iwDeps.unk8);
				break;

			case ST_IW_UNK_0xfd3d473d: // 0x18
			case ST_IW_UNK_0x671833c0: // 0x4
			case ST_IW_UNK_0xb1eb41e6: // 0x8
			{
				PFFIWUnk unk{};
				size_t len{ ReadInto(reader, &unk) };
				LOG_TRACE("0x{:x} 0x{:x} : 0x{:x} 0x{:x} 0x{:x}", type, len, unk.unk0, unk.unk8, unk.unk10);
				break;
			}
			default: {
				uint32_t size{ reader.Read<uint32_t>() };
				if (HAS_LOG_LEVEL(core::logs::LVL_TRACE)) {
					std::unique_ptr<byte[]> unk{ reader.ReadArray<byte>(size) };
					LOG_TRACE("unk ff header block : 0x{:x} -> 0x{:x} : {}", type, size, utils::data::AsHex(unk.get(), size));
				}
				else {
					reader.Skip(size);
				}
				break;
			}
			}
		}
	}

	HeaderPacker::HeaderPacker(std::vector<byte>& data) : data(data) {
		utils::Allocate(data, sizeof(uint32_t) * 2); // magic + numBlock
	}

	void HeaderPacker::AddBlock(SectionType type, const void* ptr, uint32_t len) {
		utils::WriteValue<uint32_t>(data, type);
		utils::WriteValue<uint32_t>(data, len);
		size_t off{ utils::Allocate(data, len) };
		
		chunks.emplace_back(off, ptr, len);
	}

	void HeaderPacker::WriteEnd() {
		uint32_t* header{ (uint32_t*)data.data() };
		// complete header
		header[0] = MAGIC;
		header[1] = (uint32_t)chunks.size();

		// write chunks
		for (PackChunk& chunk : chunks) {
			std::memcpy(&data[chunk.offset], chunk.buffer, chunk.size);
		}
	}

	namespace {
		int readpackff(int argc, const char* argv[]) {
			if (tool::NotEnoughParam(argc, 1)) return tool::BAD_USAGE;
			int offset{ tool::NotEnoughParam(argc, 2) ? 0 : (int)utils::ParseFormatInt(argv[3]) };

			utils::InFileCE is{ argv[2], true, std::ios::binary };
			core::bytebuffer::FileReader reader{ is };
			if (offset) {
				LOG_INFO("Skip 0x{:x}", offset);
				reader.Goto(offset);
			}

			PackedFastFileReader info{};

			info.ReadHeader(reader);

			LOG_INFO("End header: 0x{:x}", reader.Loc());

			return tool::OK;
		}

		ADD_TOOL(readpackff, "fastfile", " [ff] (off=0)", "read packed header", readpackff);
	}
}