// sd_mmc.h
#pragma once
#include <stdio.h>
#include <stdbool.h>

// Funciones básicas SD
void sd_card_init(void);
void sd_card_deinit(void);
bool sd_card_exists(const char* path);
FILE* sd_card_open(const char* path, const char* mode);
void sd_card_close(FILE* file);

// Estructura y funciones para config.txt
typedef struct {
    char ssid[32];
    char password[64];
    int gmt_offset_hours;
} wifi_config_t;

extern wifi_config_t g_wifi_config;

bool create_config_file(const char* path);
bool read_config_file(const char* path, wifi_config_t* config);
void check_configuration(void);