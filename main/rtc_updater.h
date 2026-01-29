#ifndef RTC_UPDATER_H
#define RTC_UPDATER_H

#include <stdbool.h>

typedef struct {
    char ssid[32];
    char password[64];
    int gmt_hours;
} wifi_credentials_t;

bool update_rtc_via_wifi(const wifi_credentials_t* creds);

#endif // RTC_UPDATER_H