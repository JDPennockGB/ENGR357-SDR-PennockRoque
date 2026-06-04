cmake_minimum_required(VERSION 3.13)

add_executable(lab8_sdr
    main.c
    usb_descriptors.c
    si5351.c
)

target_include_directories(lab8_sdr PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_generate_pio_header(lab8_sdr ${CMAKE_CURRENT_LIST_DIR}/i2s_rx.pio)

target_link_libraries(lab8_sdr
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

pico_enable_stdio_usb(lab8_sdr 0)
pico_enable_stdio_uart(lab8_sdr 0)
pico_add_extra_outputs(lab8_sdr)
