set(COMPONENT_SRCS main.c ili9340.c fontx.c m5stack.c udp_receiver.c)
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
spiffs_create_partition_image(storage ../fonts FLASH_IN_PROJECT)
