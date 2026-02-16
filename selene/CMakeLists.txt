# helioselene-selene: Selene curve operations (coordinates in Fq), all backends

if(NOT FORCE_PORTABLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^([xX]86_64|[aA][mM][dD]64|[aA][rR][mM]64|[aA][aA][rR][cC][hH]64)$")
    set(SELENE_SRC
        src/x64/selene_dbl.cpp
        src/x64/selene_madd.cpp
        src/x64/selene_add.cpp
        src/x64/selene_tobytes.cpp
        src/x64/selene_frombytes.cpp
        src/x64/selene_scalarmult.cpp
        src/x64/selene_scalarmult_vartime.cpp
        src/x64/selene_msm_vartime.cpp
        src/x64/selene_map_to_curve.cpp
    )
else()
    set(SELENE_SRC
        src/portable/selene_dbl.cpp
        src/portable/selene_madd.cpp
        src/portable/selene_add.cpp
        src/portable/selene_tobytes.cpp
        src/portable/selene_frombytes.cpp
        src/portable/selene_scalarmult.cpp
        src/portable/selene_scalarmult_vartime.cpp
        src/portable/selene_msm_vartime.cpp
        src/portable/selene_map_to_curve.cpp
    )
endif()

# AVX2 backend sources
if(ENABLE_AVX2)
    set(SELENE_AVX2_SRC
        src/x64/avx2/selene_scalarmult.cpp
        src/x64/avx2/selene_scalarmult_vartime.cpp
        src/x64/avx2/selene_msm_vartime.cpp
    )
    list(APPEND SELENE_SRC ${SELENE_AVX2_SRC})

    if(MSVC)
        set_source_files_properties(${SELENE_AVX2_SRC} PROPERTIES COMPILE_FLAGS "/arch:AVX2")
    else()
        set_source_files_properties(${SELENE_AVX2_SRC} PROPERTIES
            COMPILE_FLAGS "-mavx2 ${_MINGW_SIMD_FIX}")
    endif()
endif()

# IFMA backend sources
if(ENABLE_AVX512)
    set(SELENE_IFMA_SRC
        src/x64/ifma/selene_scalarmult.cpp
        src/x64/ifma/selene_scalarmult_vartime.cpp
        src/x64/ifma/selene_msm_vartime.cpp
    )
    list(APPEND SELENE_SRC ${SELENE_IFMA_SRC})

    if(MSVC)
        set_source_files_properties(${SELENE_IFMA_SRC} PROPERTIES COMPILE_FLAGS "/arch:AVX512")
    else()
        set_source_files_properties(${SELENE_IFMA_SRC} PROPERTIES
            COMPILE_FLAGS "-mavx512f -mavx512ifma ${_MINGW_SIMD_FIX}")
    endif()
endif()

add_library(helioselene-selene STATIC ${SELENE_SRC})

target_include_directories(helioselene-selene PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(helioselene-selene
    PUBLIC helioselene-fq helioselene-common
    PRIVATE helioselene-build-flags
)
