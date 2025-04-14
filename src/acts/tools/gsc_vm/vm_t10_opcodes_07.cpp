#include <includes.hpp>
#include <tools/gsc_vm.hpp>
#include <tools/gsc.hpp>
#include <tools/gsc_opcodes.hpp>

namespace {
	using namespace tool::gsc::opcode;
	void OpCode() {
		VmInfo* t107 = RegisterVM(VMI_T10_07, "Call of Duty: Black Ops 6 (07)", "t10", "bo6_7", VmFlags::VMF_CRC_DUMP | VmFlags::VMF_FOREACH_IW | VmFlags::VMF_EXPORT_NOCHECKSUM | VmFlags::VMF_HASH64 | VmFlags::VMF_HASH_CER | VmFlags::VMF_HASH_PATH_IW | VmFlags::VMF_CALL_NO_PARAMS | VmFlags::VMF_IW_LIKE | VmFlags::VMF_ISTRING_HASHED); // VmFlags::VMF_IW_CALLS | VmFlags::VMF_NO_PARAM_FLAGS
		t107->RegisterVmName("cer7", "t10_7", "blackops6_7");
		t107->AddPlatform(PLATFORM_PC);
		t107->RegisterVMGlobalVariable("level", OPCODE_IW_GetLevel);
		t107->RegisterVMGlobalVariable("game", OPCODE_IW_GetGame);
		t107->RegisterVMGlobalVariable("anim", OPCODE_IW_GetAnim);
		t107->RegisterVMOperatorFunction("waittill", "<caller> waittill(event, var*)", OPCODE_IW_SingleWaitTill, VPFD_SELF_PARAM | VPFD_UNPACK, 1);
		t107->RegisterVMOperatorFunction("isdefined", "isdefined(object) -> bool", OPCODE_IsDefined, VPFD_RETURN_VALUE, 1, 1);
		t107->RegisterVMOperatorFunction("notify", "<caller> notify(event, param*)", OPCODE_IW_Notify, VPFD_SELF_PARAM | VPFD_USE_PRE_SCRIPT_CALL, 1);
		t107->RegisterVMOperatorFunction("endon", "<caller> endon(event+)", OPCODE_EndOn, VPFD_SELF_PARAM | VPFD_USE_COUNT, 1);
		t107->RegisterVMOperatorFunction("wait", "wait(time)", OPCODE_Wait, VPFD_NONE, 1, 1);
		t107->RegisterVMOperatorFunction("waitframe", "waitframe()", OPCODE_IW_WaitFrame, VPFD_NONE, 0, 0);
		t107->RegisterVMOperatorFunction("getthread", "getthread() -> thread", OPCODE_IW_GetThread, VPFD_RETURN_VALUE, 0, 0);
		t107->RegisterVMOperatorFunction("istrue", "istrue(object) -> bool", OPCODE_IW_IsTrue, VPFD_RETURN_VALUE, 1, 1);
		t107->RegisterVMOperatorFunction("flat_args", "flat_args(array, count) -> bool", OPCODE_T10_FlatArgs, VPFD_RETURN_VALUE, 2, 2);
		t107->RegisterVMOperatorFunction("waittillframeend", "waittillframeend()", OPCODE_WaitTillFrameEnd, VPFD_NONE, 0, 0);
		t107->RegisterVMHashOPCode('#', OPCODE_GetHash, 8, [](const char* str) { return hash::Hash64A(str); });
		t107->RegisterVMHashOPCode('@', OPCODE_IW_GetDVarHash, 8, [](const char* str) { return hash::HashIWDVar(str); });
		t107->RegisterVMHashOPCode('%', OPCODE_IW_GetResourceHash, 8, [](const char* str) { return hash::HashIWAsset(str); });
		t107->RegisterVMHashOPCode('t', OPCODE_IW_GetTagHash, 4, [](const char* str) { return hash::HashX32(str); });
		t107->RegisterVMHashOPCode('&', OPCODE_T10_GetScrHash, 8, [](const char* str) { return hash::HashT10Scr(str); });
		t107->RegisterDevCall("assert", "assertmsg", "function_79901b4637c83c86", "assertex", "println", "print");
		t107->RegisterDatatype("builtinfunction", "builtinmethod", "function", "string", "istring", "struct", "int", "float", "vector", "xhash", "xhashasset", "istring");
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_Nop, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf);
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_Nop, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf);
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_Nop, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf);
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_Nop, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf);
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_Nop, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef);
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_Nop, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff);
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_CheckClearParams, 0x4d);
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_SafeCreateLocalVariables, 0x5a);
		t107->RegisterOpCode(PLATFORM_PC, OPCODE_IW_RegisterMultipleVariables, 0x53);
	}
}
REGISTER_GSC_VM_OPCODES(bo6, OpCode);