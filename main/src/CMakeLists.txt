# file(GLOB_RECURSE SOURCES "*.cpp")

idf_component_register(
    SRCS 
        # ${SOURCES}
        CPUC64.cpp
        CPU6502.cpp
    REQUIRES 
        driver 
        sdmmc 
        fatfs 
        vfs
    INCLUDE_DIRS 
        "."
)

set_source_files_properties(${SORUCES} PROPERTIES LANGUAGE CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")