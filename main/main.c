#include "gias.h"

void app_main(void)
{
    gias();
}

/*
===============================================================================
 Project: GIAS - Intelligent Audio Recorder (ESP32-S3)
-------------------------------------------------------------------------------
 Description:
   Autonomous audio recording system for the ESP32-S3 microcontroller.
   The device records 16-bit audio at 44.1 kHz from an external I2S codec
   (PMOD I2S2), storing samples in PSRAM and writing them to SD/MMC in bursts.
   Recording schedules are loaded from /Calendar.csv, and the system
   synchronizes its internal RTC via Wi-Fi using NTP servers defined in /config.txt.

 Features:
   - Audio recording at 16-bit / 44.1 kHz
   - Uses I2S Standard Mode with MCLK
   - Data buffered in external PSRAM (8 MB)
   - SD/MMC 4-bit mode for high-speed writes
   - Autonomous recording schedule
   - RTC synchronization via Wi-Fi (configurable GMT)
   - Low-power operation between recording cycles

 Hardware Requirements:
   • ESP32-S3 (with 8 MB PSRAM enabled)
   • PMOD I2S2 Audio Codec (requires MCLK on GPIO 14)
   • SD Card (formatted FAT32, high-speed, 4-bit SDIO mode)
   • External 10kΩ pull-ups on SD lines (CMD, CLK, D0–D3)
   • Stable 3.3V supply (avoid GPIO 35–37 if OPI PSRAM is active)

 Pin Configuration (default):
   SD/MMC 4-bit Bus:
     D2  -> GPIO 4
     D3  -> GPIO 5
     CMD -> GPIO 6
     CLK -> GPIO 7
     D0  -> GPIO 15
     D1  -> GPIO 16
   I2S PMOD Interface:
     MCLK -> GPIO 14
     WS   -> GPIO 13
     BCLK -> GPIO 12
     SDOUT-> GPIO 11
     SDIN -> GPIO 10

 Software Requirements:
   • ESP-IDF v5.5.2 or later
   • FreeRTOS tasks for SD initialization and write operations
   • FATFS and SD_MMC components enabled
   • Wi-Fi and SNTP enabled (for RTC sync)
   • PSRAM support enabled in menuconfig

 Menuconfig adjustments:
   - Component config → ESP System Settings → Enable PSRAM
   - Component config → FATFS → Long filename support (optional)
   - Component config → FreeRTOS → Increase main task stack if required
   - Component config → ESP Wi-Fi → Enable Wi-Fi station mode
   - Component config → LWIP → Enable SNTP client
   - Serial flasher config → “CDC on Boot” disabled if not required
   - Power management → Disable WDT for long SD operations

 Author: Miguel López
 License: MIT
 Date: 2026
===============================================================================
*/
