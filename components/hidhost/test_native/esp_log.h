// Enough of the ESP logging API to build the parser on a host
#pragma once

#include <stdio.h>

#define ESP_LOGE(tag, fmt, ...) printf("E %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("W %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) printf("I %s: " fmt "\n", tag, ##__VA_ARGS__)
// Silent, but the arguments still have to be used or a build with only debug logging warns
#define ESP_LOGD(tag, fmt, ...) ((void)(tag), (void)(fmt))
