#pragma once
#include <hook/module_mapper.hpp>

namespace acts::decryptutils {
	/*
	 * Decrypt a string using the configured implementation, return str if no implementation was loaded
	 * @param str string to decrypt
	 * @return decrypted string
	 */
	char* DecryptString(char* str);
	/*
	 * Load a decryption implementation from an executable
	 * @param exec the executable
	 */
	bool LoadDecrypt(const std::filesystem::path& exec);
	/*
	 * Load a decryption implementation from an already loaded library
	 * @param mod the module
	 */
	bool LoadDecryptModule(const hook::library::Library& lib);
	/*
	 * Load a decryption implementation from an already loaded module
	 * @param mod the module
	 */
	inline bool LoadDecryptModule(const hook::module_mapper::Module& mod) {
		return LoadDecryptModule(*mod);
	}

	enum T8Decryption {
		T8D_X1_LATEST = 0,
		T8D_X2_UNK_OLD,
		T8D_X3_20181013,
		T8D_COUNT
	};

	void SetT8OldDecryption(T8Decryption alg);
	const char* GetT8OldDecryptionName(T8Decryption alg);
	char* DecryptStringT8(char* str);
}