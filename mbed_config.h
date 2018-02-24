/*
 * mbed SDK
 * Copyright (c) 2017 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Automatically generated configuration file.
// DO NOT EDIT, content will be overwritten.

#ifndef __MBED_CONFIG_DATA__
#define __MBED_CONFIG_DATA__

// Configuration parameters
#define MBED_CONF_PUSHTOGO_MAX_SPEED                      6.0f                                                                                                  // set by library:pushtogo
#define MBED_CONF_EVENTS_SHARED_DISPATCH_FROM_APPLICATION 0                                                                                                     // set by library:events
#define MBED_CONF_PUSHTOGO_HARDWARE_SETUP                 Make sure the following settings are accurate in representing your hardware                           // set by library:pushtogo
#define MBED_CONF_PPP_CELL_IFACE_APN_LOOKUP               0                                                                                                     // set by library:ppp-cell-iface
#define MBED_CONF_EVENTS_PRESENT                          1                                                                                                     // set by library:events
#define MBED_CONF_PUSHTOGO_UPDATE_ANGLE_PERIOD            10                                                                                                    // set by library:pushtogo
#define MBED_CONF_PUSHTOGO_GOTO_BEHAVIORS                 The following settings control some details in the way that Go-To is implemented. Tweak as necessary. // set by library:pushtogo
#define MBED_LFS_PROG_SIZE                                64                                                                                                    // set by library:littlefs
#define MBED_CONF_SD_SPI_CLK                              PC_10                                                                                                 // set by library:sd[DISCO_F429ZI]
#define MBED_CONF_PLATFORM_STDIO_FLUSH_AT_EXIT            1                                                                                                     // set by library:platform
#define MBED_CONF_SD_SPI_MOSI                             PC_12                                                                                                 // set by library:sd[DISCO_F429ZI]
#define MBED_CONF_PUSHTOGO_MIN_CORRECTION_TIME            0.005f                                                                                                // set by library:pushtogo
#define MBED_CONF_DRIVERS_UART_SERIAL_RXBUF_SIZE          256                                                                                                   // set by library:drivers
#define MBED_CONF_NSAPI_PRESENT                           1                                                                                                     // set by library:nsapi
#define MBED_CONF_FILESYSTEM_PRESENT                      1                                                                                                     // set by library:filesystem
#define MBED_CONF_PPP_CELL_IFACE_BAUD_RATE                115200                                                                                                // set by library:ppp-cell-iface
#define MBED_CONF_PPP_CELL_IFACE_AT_PARSER_TIMEOUT        8000                                                                                                  // set by library:ppp-cell-iface
#define MBED_CONF_PUSHTOGO_CORRECTION_TOLERANCE           0.05f                                                                                                 // set by library:pushtogo
#define MBED_LFS_BLOCK_SIZE                               512                                                                                                   // set by library:littlefs
#define MBED_CONF_PPP_CELL_IFACE_AT_PARSER_BUFFER_SIZE    256                                                                                                   // set by library:ppp-cell-iface
#define MBED_CONF_PLATFORM_FORCE_NON_COPYABLE_ERROR       0                                                                                                     // set by library:platform
#define MBED_CONF_PLATFORM_STDIO_BAUD_RATE                115200                                                                                                // set by application[*]
#define CLOCK_SOURCE                                      USE_PLL_HSE_XTAL|USE_PLL_HSI                                                                          // set by target:DISCO_F429ZI
#define MBED_CONF_SD_SPI_CS                               PA_15                                                                                                 // set by library:sd[DISCO_F429ZI]
#define MBED_CONF_PUSHTOGO_MICROSTEP                      128                                                                                                   // set by library:pushtogo
#define MBED_CONF_SD_SPI_MISO                             PC_11                                                                                                 // set by library:sd[DISCO_F429ZI]
#define MBED_CONF_SD_FSFAT_SDCARD_INSTALLED               1                                                                                                     // set by library:sd
#define MBED_LFS_READ_SIZE                                64                                                                                                    // set by library:littlefs
#define MBED_CONF_PUSHTOGO_MAX_CORRECTION_ANGLE           5.0f                                                                                                  // set by library:pushtogo
#define MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE       9600                                                                                                  // set by library:platform
#define MBED_CONF_RTOS_PRESENT                            1                                                                                                     // set by library:rtos
#define MBED_CONF_EVENTS_SHARED_EVENTSIZE                 256                                                                                                   // set by library:events
#define CLOCK_SOURCE_USB                                  1                                                                                                     // set by target:DISCO_F429ZI
#define MBED_CONF_PUSHTOGO_REDUCTION_FACTOR               180.0f                                                                                                // set by library:pushtogo
#define MBED_CONF_PUSHTOGO_MAX_GUIDE_TIME                 5.0f                                                                                                  // set by library:pushtogo
#define MBED_CONF_PUSHTOGO_STEPS_PER_REVOLUTION           400                                                                                                   // set by library:pushtogo
#define MBED_CONF_EVENTS_SHARED_STACKSIZE                 1024                                                                                                  // set by library:events
#define MBED_CONF_DRIVERS_UART_SERIAL_TXBUF_SIZE          256                                                                                                   // set by library:drivers
#define MBED_LFS_LOOKAHEAD                                512                                                                                                   // set by library:littlefs
#define MBED_CONF_EVENTS_USE_LOWPOWER_TIMER_TICKER        0                                                                                                     // set by library:events
#define MBED_CONF_PLATFORM_STDIO_CONVERT_NEWLINES         0                                                                                                     // set by library:platform
#define MBED_CONF_PUSHTOGO_MIN_SLEW_ANGLE                 0.3f                                                                                                  // set by library:pushtogo
#define MBED_CONF_TARGET_LSE_AVAILABLE                    0                                                                                                     // set by target:DISCO_F429ZI
#define MBED_LFS_ENABLE_INFO                              0                                                                                                     // set by library:littlefs
#define MBED_CONF_PUSHTOGO_ACCELERATION_STEP_TIME         0.1f                                                                                                  // set by library:pushtogo
#define MBED_CONF_EVENTS_SHARED_HIGHPRIO_STACKSIZE        1024                                                                                                  // set by library:events
#define MBED_CONF_SD_DEVICE_SPI                           1                                                                                                     // set by library:sd
#define MBED_CONF_EVENTS_SHARED_HIGHPRIO_EVENTSIZE        256                                                                                                   // set by library:events
// Macros
#define UNITY_INCLUDE_CONFIG_H                                                                                                                                  // defined by library:utest

#endif
