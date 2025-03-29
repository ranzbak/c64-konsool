idf_component_register(
	SRCS
		"main.cpp"
		"src/C64Emu.cpp"
		"src/CPU6502.cpp"
		"src/CPUC64.cpp"
		"src/VIC.cpp"
		"src/CIA.cpp"
		"src/Joystick.cpp"
		"src/SDCard.cpp"
		"src/Pax.cpp"
	PRIV_REQUIRES
		# esp-idf-cxx
		esp_lcd
		esp_adc
		fatfs
		nvs_flash
		badge-bsp
	INCLUDE_DIRS
		"."
		"src"
)

# add_subdirectory(src)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++11)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")