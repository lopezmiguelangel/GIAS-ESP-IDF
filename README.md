# GIAS - ESP32 Audio Recording System 🎛️

Autonomous audio recording system for ESP32/ESP32-S3 boards, built using **ESP-IDF** and FreeRTOS. Designed for long-term, scheduled recordings with PSRAM buffering, deep sleep power management, and RTC synchronization.

---

## 📜 Description

The **GIAS** project allows the ESP32 to:

- Record audio continuously or according to a schedule.
- Store recordings in non-volatile storage (SD card) with buffer handling.
- Use PSRAM for high-capacity audio buffering to prevent sample loss (8MB).
- Synchronize the internal RTC via WiFi and NTP servers.
- Enter deep sleep between recordings to save power.

This system is optimized for **autonomous operation**, with 80MHz CPU speed.

---

## 🛠️ Hardware Requirements

- **ESP32 / ESP32-S3** microcontroller with PSRAM support.
- Audio input via an I2S-compatible microphone or codec with Master Clock.
- SD card interface for storing recordings.
- Optional: LEDs (WS2812) for status indication.

---

## ⚙️ Features

### Recording and Buffering
- Utilizes I2S interface for audio acquisition.
- Uses PSRAM to buffer audio and ensure smooth write operations.
- Handles automatic start/stop according to schedule or continuous mode.
- Monitors recording state and writes data in blocks to prevent loss.

### Scheduling
- Reads a **calendar.csv file** with per-hour and per-day recording configuration.
- Determines whether to record immediately or enter deep sleep until the next scheduled event.
- Supports both continuous and timed recording sessions.

### Power Management
- Automatically enters **deep sleep** during idle periods.
- Wakes up based on the next scheduled recording or external trigger.

### Time Synchronization
- Connects to WiFi and synchronizes the internal RTC using NTP servers.
- Supports configurable GMT offset.
- Handles retries and WiFi cleanup automatically.

### LED Feedback
- Supports a single WS2812 LED for status indication:
  - Red, Green, Blue, White, or Off.
- Test sequence included for hardware verification.

### Configuration
- Stores WiFi credentials and GMT offset in a **config.txt** file.
- Automatically creates a default **config.txt** if none exists.
- **calendar.csv** file is created with default values if missing.

---

## 🏗️ Software Architecture

The project is organized into the following modules:

- **`gias.c`** – Main application logic and initialization.
- **`led_control.c`** – LED initialization and test sequences.
- **`rtc_updater.c`** – WiFi connection, NTP time synchronization, and RTC update.
- **`sd_mmc.c`** – Storage initialization, file creation, and read/write helpers.
- **`calendar.c`** – Loads and interprets recording schedule; calculates next sleep duration.
- **`audio_recorder.c`** – I2S audio acquisition, PSRAM buffering, and data storage tasks.

The default Core used is 0. SD write tasks run on Core 1.

---

## 🔧 Workflow

1. **Initialization**
   - CPU info logging.
   - LED setup.
   - Non-volatile storage initialization.
   - SD card check and configuration file validation.

2. **Time Synchronization**
   - WiFi connection using stored credentials.
   - NTP synchronization based on GMT offset.

3. **Calendar Evaluation**
   - Loads schedule from calendar file.
   - Determines whether recording should start or system should sleep.

4. **Recording**
   - Initializes audio buffers and I2S interface.
   - Starts recording session for scheduled duration.
   - Writes buffered audio to storage in blocks.

5. **Power Management**
   - After recording, enters deep sleep for the remaining time until the next scheduled recording.

---

## ⚡ Future Improvements

- Battery monitoring and low-power mode.
- External RTC.
---
