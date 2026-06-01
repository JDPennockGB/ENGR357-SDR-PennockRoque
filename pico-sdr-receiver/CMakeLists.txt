cmake_minimum_required(VERSION 3.13)

# Pull in the Pico SDK (MUST be before project())
include(pico_sdk_import.cmake)

project(pico_sdr_receiver C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Define our executable
add_executable(sdr_receiver
    src/main.c
    src/hw_drivers/si5351.c
)

# Generate header from your PCM1808 PIO file
pico_generate_pio_header(sdr_receiver ${CMAKE_CURRENT_LIST_DIR}/src/dsp_audio/i2s_rx.pio)

# Link standard libraries
target_link_libraries(sdr_receiver
    pico_stdlib
    hardware_i2c
    hardware_pio
    hardware_dma
    hardware_irq
)

# Set the include directories so the compiler can find your .h files
target_include_directories(sdr_receiver PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/hw_drivers
    ${CMAKE_CURRENT_LIST_DIR}/src/dsp_audio
    ${CMAKE_CURRENT_LIST_DIR}/src/application
)

# Enable USB for standard printf output, disable UART
pico_enable_stdio_usb(sdr_receiver 1)
pico_enable_stdio_uart(sdr_receiver 0)

# Generate the .uf2 file for flashing
pico_add_extra_outputs(sdr_receiver)