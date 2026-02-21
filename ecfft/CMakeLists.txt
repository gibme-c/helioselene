# helioselene-ecfft: ECFFT polynomial multiplication over Fp and Fq

if(ENABLE_ECFFT)
    add_library(helioselene-ecfft STATIC
        src/ecfft.cpp
    )

    target_include_directories(helioselene-ecfft PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_compile_definitions(helioselene-ecfft PUBLIC HELIOSELENE_ECFFT=1)

    target_link_libraries(helioselene-ecfft PUBLIC
        helioselene-fp
        helioselene-fq
        helioselene-common
    )
else()
    # Header-only stub so downstream can unconditionally link
    add_library(helioselene-ecfft INTERFACE)

    target_include_directories(helioselene-ecfft INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()
