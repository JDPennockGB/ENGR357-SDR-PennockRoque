cmake_minimum_required(VERSION 3.13)
# fixed final version for new drivers, see commit history for old version
# 1. Pull in the Pico SDK helper
include(pico_sdk_import.cmake)

project(SDR_Audio)

# 2. Initialize the Pico SDK
pico_sdk_init()

# 3. Define the executable
add_executable(sdr_driver
    main.c
    usb_descriptors.c
    lcd_driver.c
    si5351.c
)

target_include_directories(sdr_driver PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# 4. Generate the PIO header
pico_generate_pio_header(sdr_driver ${CMAKE_CURRENT_LIST_DIR}/i2s_rx.pio)

# 5. Link libraries
target_link_libraries(sdr_driver
    pico_stdlib
    hardware_dma
    hardware_pio
    hardware_irq
    hardware_i2c
    hardware_gpio
    pico_multicore
    tinyusb_device
    tinyusb_board
)

# 6. USB/UART Configuration
pico_enable_stdio_usb(sdr_driver 0)
pico_enable_stdio_uart(sdr_driver 0)

# 7. REQUIRED: This generates the .uf2 file
pico_add_extra_outputs(sdr_driver)