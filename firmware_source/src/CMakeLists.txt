# Gather source files
FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.cpp)

# Register component and declare dependencies
idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "." "../include"
    REQUIRES esp_tinyusb
)


# Embed unifont.hex binary data
target_add_binary_data(${COMPONENT_TARGET} "fonts/unifont.hex" BIN)
target_add_binary_data(${COMPONENT_TARGET} "fonts/charWidths.hex" BIN)
target_add_binary_data(${COMPONENT_TARGET} "fonts/pageShowcase.html" BIN)