# helioselene-common: platform utilities, secure erase, ct_barrier, load helpers, mul128
add_library(helioselene-common STATIC
    src/helioselene_secure_erase.cpp
)

target_include_directories(helioselene-common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(helioselene-common PRIVATE helioselene-build-flags)

if(FORCE_PORTABLE)
    target_compile_definitions(helioselene-common PUBLIC FORCE_PORTABLE=1)
endif()
