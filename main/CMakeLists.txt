idf_component_register(
	SRCS
		"main.cpp"
		"src/C64Emu.cpp"
		"src/CIA.cpp"
		"src/CPU6502.cpp"
		"src/CPUC64.cpp"
		"src/ExternalCmds.cpp"
		"src/Joystick.cpp"
		"src/KonsoolKB.cpp"
		"src/Pax.cpp"
		"src/SDCard.cpp"
		"src/VIC.cpp"
		"src/konsoolled.cpp"
	PRIV_REQUIRES
		esp_lcd
		esp_adc
		esp_driver_ppa
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