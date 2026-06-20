#pragma once
#include <data/bo4.hpp>

namespace systems::errors {
    constexpr uint64_t CUSTOM_ERROR_ID = 0x53544341;
    void ScrVm_Error(bo4::scriptInstance_t inst, const char* msg, bool terminal, ...);

#ifndef CI_BUILD
#define ScrVm_Assert(inst, value)                                                                                      \
    do {                                                                                                               \
        if (!(value)) {                                                                                                \
            systems::errors::ScrVm_Error(inst, "Assertion error: %s (%s:%lld)", true, #value, __FILE__, __LINE__);     \
        }                                                                                                              \
    } while (0)
#else
#define ScrVm_Assert(inst, value)
#endif
} // namespace systems::errors