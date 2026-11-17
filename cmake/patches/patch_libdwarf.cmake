set(dwarf_cmake "src/lib/libdwarf/CMakeLists.txt")
file(READ "${dwarf_cmake}" content)
string(REGEX REPLACE "(export\\([\r\n]+[ \t]*TARGETS[ \t]+dwarf)[^\r\n]*"
        "\\1 zlibstatic zlib ZSTD" patched_content "${content}")
if (NOT patched_content STREQUAL content)
    file(WRITE "${dwarf_cmake}" "${patched_content}")
endif ()
