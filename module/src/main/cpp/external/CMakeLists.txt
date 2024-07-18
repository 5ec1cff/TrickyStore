project(external)

# lsplt
set(SOURCES LSPlt/lsplt/src/main/jni/lsplt.cc LSPlt/lsplt/src/main/jni/elf_util.cc)

add_library(lsplt STATIC ${SOURCES})
target_include_directories(lsplt PUBLIC LSPlt/lsplt/src/main/jni/include)
target_include_directories(lsplt PRIVATE LSPlt/lsplt/src/main/jni)
target_link_libraries(lsplt PUBLIC my_logging)
# end lsplt

add_subdirectory(glaze)
