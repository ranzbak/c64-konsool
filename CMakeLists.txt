# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

if(DEVICE STREQUAL "")
	set(DEVICE "$ENV{TARGET}")
endif()

if(DEVICE STREQUAL "")
	message(FATAL_ERROR "DEVICE not set")
endif()

if(DEVICE STREQUAL "tanmatsu")
	set(SDKCONFIG_DEFAULTS "sdkconfigs/general;sdkconfigs/tanmatsu")
	set(IDF_TARGET esp32p4)
elseif(DEVICE STREQUAL "mch2022")
	set(SDKCONFIG_DEFAULTS "sdkconfigs/general;sdkconfigs/mch2022")
	set(IDF_TARGET esp32)
else()
	message(FATAL_ERROR "Unknown target device ${DEVICE}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(application)

# The vendored zlib (pulled in by robotman2412/pax-codecs) uses pre-C99
# K&R style function definitions, which newer GCC versions reject under
# -Werror=old-style-definition. Silence just that warning for zlib.
foreach(zlib_target zlib zlibstatic)
	if(TARGET ${zlib_target})
		target_compile_options(${zlib_target} PRIVATE
			-Wno-error=old-style-definition
			-Wno-error=implicit-fallthrough
		)
	endif()
endforeach()
