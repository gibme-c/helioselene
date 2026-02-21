# helioselene-helios: Helios curve operations (coordinates in Fp), all backends

if(NOT FORCE_PORTABLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^([xX]86_64|[aA][mM][dD]64|[aA][rR][mM]64|[aA][aA][rR][cC][hH]64)$")
    set(HELIOS_SRC
        src/x64/helios_dbl.cpp
        src/x64/helios_madd.cpp
        src/x64/helios_add.cpp
        src/x64/helios_tobytes.cpp
        src/x64/helios_frombytes.cpp
        src/x64/helios_scalarmult.cpp
        src/x64/helios_scalarmult_vartime.cpp
        src/x64/helios_msm_vartime.cpp
        src/x64/helios_map_to_curve.cpp
    )
else()
    set(HELIOS_SRC
        src/portable/helios_dbl.cpp
        src/portable/helios_madd.cpp
        src/portable/helios_add.cpp
        src/portable/helios_tobytes.cpp
        src/portable/helios_frombytes.cpp
        src/portable/helios_scalarmult.cpp
        src/portable/helios_scalarmult_vartime.cpp
        src/portable/helios_msm_vartime.cpp
        src/portable/helios_map_to_curve.cpp
    )
endif()

# AVX2 backend sources
if(ENABLE_AVX2)
    set(HELIOS_AVX2_SRC
        src/x64/avx2/helios_scalarmult.cpp
        src/x64/avx2/helios_scalarmult_vartime.cpp
        src/x64/avx2/helios_msm_vartime.cpp
    )
    list(APPEND HELIOS_SRC ${HELIOS_AVX2_SRC})

    if(MSVC)
        set_source_files_properties(${HELIOS_AVX2_SRC} PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    else()
        set_source_files_properties(${HELIOS_AVX2_SRC} PROPERTIES COMPILE_FLAGS "-mavx2")
    endif()
endif()

# IFMA backend sources
if(ENABLE_AVX512)
    set(HELIOS_IFMA_SRC
        src/x64/ifma/helios_scalarmult.cpp
        src/x64/ifma/helios_scalarmult_vartime.cpp
        src/x64/ifma/helios_msm_vartime.cpp
    )
    list(APPEND HELIOS_SRC ${HELIOS_IFMA_SRC})

    if(MSVC)
        set_source_files_properties(${HELIOS_IFMA_SRC} PROPERTIES COMPILE_FLAGS "/arch:AVX512")
    else()
        set_source_files_properties(${HELIOS_IFMA_SRC} PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512ifma")
    endif()
endif()

add_library(helioselene-helios STATIC ${HELIOS_SRC})

target_include_directories(helioselene-helios PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(helioselene-helios PUBLIC helioselene-fp helioselene-common)
