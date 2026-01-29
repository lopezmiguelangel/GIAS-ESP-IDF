idf_component_register(
    SRCS 
        "audio_recorder.c" 
        "main.c" 
        "gias.c" 
        "led_control.c" 
        "sd_mmc.c" 
        "rtc_updater.c" 
        "calendar.c"
    INCLUDE_DIRS "."
    REQUIRES 
        led_strip 
        fatfs 
        esp_wifi 
        nvs_flash 
        esp_event 
        esp_netif 
        driver           # Para driver/i2s_std.h
        esp_http_client  # Para NTP
        esp_timer        # Para esp_timer.h
)
