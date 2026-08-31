idf_component_register(
    SRCS "src/hid_keyboard.c"
         "src/hid_gamepad.c"
         "src/msc_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES usb badge-bsp fatfs usb_host_msc
)
