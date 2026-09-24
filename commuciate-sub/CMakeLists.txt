# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Set target definition variables required by the toolchain file
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)

# Set the toolchain file for cross-compilation
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

# Project definition
project(commuicate)

# Enable CMake support for ASM and C/CPP languages
enable_language(C ASM)

set(PROJECT_NAME commuicate)

# Create an executable object type
add_executable(${PROJECT_NAME})

# Include additional CMake files for flags, components, and files
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/components.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

# Collect the STM32F10x Standard Peripheral Library sources. CONFIGURE_DEPENDS
# makes CMake reconfigure automatically when a driver source is added/removed.
file(GLOB STM32_SPL_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Lib/*.c"
)

# Add sources to executable/library
target_sources(${PROJECT_NAME} PRIVATE
  ${STM32_SPL_SOURCES}
  "${CMAKE_CURRENT_SOURCE_DIR}/Lib/InvenSense/inv_mpu.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Src/board_protocol.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Src/attitude.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Src/imu_calibration.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Src/mpu6500.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Src/mpu6500_port.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Start/system_stm32f10x.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/Src/stm32f10x_it.c"
)

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/Inc"
  "${CMAKE_CURRENT_SOURCE_DIR}/Lib"
  "${CMAKE_CURRENT_SOURCE_DIR}/Lib/InvenSense"
  "${CMAKE_CURRENT_SOURCE_DIR}/Start"
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
  # User defined compiler options
)

# Add additional defined symbols
target_compile_definitions(${PROJECT_NAME} PRIVATE
  STM32F10X_MD
  USE_STDPERIPH_DRIVER
  MPU6500
  REMOVE_LOGGING
)

# Linker options
target_link_options(${PROJECT_NAME} PRIVATE 
  # User defined linker options
)

# Add additional linker search directories
target_link_directories(${PROJECT_NAME} PRIVATE
  # Add additional linker search directories here
)

# Add additional linker libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  m
)
