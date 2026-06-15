find_program(CLANG_FORMAT
    NAMES clang-format clang-format.exe
)
message(STATUS "CLANG_FORMAT= ${CLANG_FORMAT}")

file(GLOB_RECURSE ALL_CXX_SOURCE_FILES
    "src/*.cpp"
    "src/*.hpp"
    "examples/*.c"
    "examples/*.h"
    "include/*.h"
    "tools/*.cpp"
    "tools/*.hpp"
)

set(EXCLUDED_FORMAT_FILE 
    "${CMAKE_SOURCE_DIR}/src/core/shared/games/bo6/t10_errors_generated.hpp"
    "${CMAKE_SOURCE_DIR}/src/core/shared/games/bo4/t8_errors_generated.hpp"
    "${CMAKE_SOURCE_DIR}/src/core/shared/core/updater_endpoint.hpp"
    "${CMAKE_SOURCE_DIR}/src/core/shared/core/actsinfo.hpp"
    "${CMAKE_SOURCE_DIR}/include/acts_api/version_info.h"
)

list(REMOVE_ITEM ALL_CXX_SOURCE_FILES ${EXCLUDED_FORMAT_FILE})


file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/cppfiles.txt"
    CONTENT "$<JOIN:${ALL_CXX_SOURCE_FILES},\n>")


add_custom_target(format
    COMMAND "${CLANG_FORMAT}" -i @${CMAKE_BINARY_DIR}/cppfiles.txt
)

add_custom_target(check-format
    COMMAND "${CLANG_FORMAT}" --dry-run --Werror  @${CMAKE_BINARY_DIR}/cppfiles.txt
)