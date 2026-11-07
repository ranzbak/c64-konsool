#pragma once

#include <complex.h>
#include "esp_err.h"
#include "pax_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    // Keyboard keys
    ICON_ESC,
    ICON_F1,
    ICON_F2,
    ICON_F3,
    ICON_F4,
    ICON_F5,
    ICON_F6,
    ICON_LAST
} icon_t;

void       load_icons(void);
void       unload_icons(void);
pax_buf_t* get_icon(icon_t icon);

#ifdef __cplusplus
}
#endif
