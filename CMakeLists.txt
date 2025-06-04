# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(casa_inteligente C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(${PROJECT_NAME}  
        casa_inteligente.c 
        lib/ssd1306.c # Biblioteca para o display OLED
        )

pico_set_program_name(casa_inteligente "casa_inteligente")
pico_set_program_version(casa_inteligente "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(casa_inteligente 0)
pico_enable_stdio_usb(casa_inteligente 1)

# Gerar cabeçalho PIO (se necessário)
pico_generate_pio_header(casa_inteligente ${CMAKE_CURRENT_LIST_DIR}/lib/pio_wave.pio)

# Add the standard library to the build
target_link_libraries(casa_inteligente
    pico_stdlib
    hardware_adc
    hardware_pio   # Suporte para LEDs endereçáveis
    hardware_i2c
    hardware_pwm
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
    pico_mbedtls
    pico_lwip_mbedtls
    )

# Add the standard include files to the build
target_include_directories(casa_inteligente PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(casa_inteligente 
        
        )

pico_add_extra_outputs(casa_inteligente)

