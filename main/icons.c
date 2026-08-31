#include "icons.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "pax_codecs.h"
#include "pax_gfx.h"

static char const TAG[] = "icons";

#define ICON_WIDTH        32
#define ICON_HEIGHT       32
#define ICON_BUFFER_SIZE  (ICON_WIDTH * ICON_HEIGHT * 4)  // 32x32 pixels, 4 bytes per pixel (ARGB8888)
#define ICON_COLOR_FORMAT PAX_BUF_32_8888ARGB

#define ICON_BASE_PATH "/int/icons"
#define ICON_EXT       ".png"

char icon_suffix[64] = "_f_r_black_32";

static const char* icon_paths[] = {
    // Keyboard keys (these are custom icons)
    [ICON_ESC] = "esc", [ICON_F1] = "f1", [ICON_F2] = "f2", [ICON_F3] = "f3",
    [ICON_F4] = "f4",   [ICON_F5] = "f5", [ICON_F6] = "f6",
};

pax_buf_t EXT_RAM_BSS_ATTR icons[ICON_LAST] = {0};
bool                       icons_missing    = false;

void get_icon_path(icon_t icon, char* out_path, size_t max_path_len)
{
    const char* icon_path = icon_paths[icon];
    if (icon_path == NULL) {
        ESP_LOGE(TAG, "Icon path is NULL for %u", icon);
        memset(out_path, 0, max_path_len);
    } else if (icon_path[0] == '/') {
        // Absolute path, use as is
        snprintf(out_path, max_path_len, "%s", icon_path);
    } else {
        // Relative path, prepend base path
        snprintf(out_path, max_path_len, ICON_BASE_PATH "/%s" ICON_EXT, icon_path);
    }
}

void load_icons(void)
{
    for (int i = 0; i < ICON_LAST; i++) {
        char path[512] = {0};
        get_icon_path(i, path, sizeof(path));
        FILE* fd = fopen(path, "rb");
        if (fd == NULL) {
            ESP_LOGE(TAG, "Failed to open icon file %s", path);
            icons_missing = true;
            continue;
        }
        void* buffer = heap_caps_calloc(1, ICON_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for icon %s", path);
            fclose(fd);
            icons_missing = true;
            continue;
        }
        pax_buf_init(&icons[i], buffer, ICON_WIDTH, ICON_HEIGHT, ICON_COLOR_FORMAT);
#if defined(CONFIG_BSP_TARGET_KAMI)
        // icons[i].palette      = palette;
        // icons[i].palette_size = sizeof(palette) / sizeof(pax_col_t);
#endif
        if (!pax_insert_png_fd(&icons[i], fd, 0, 0, 0)) {
            pax_buf_destroy(&icons[i]);
            free(buffer);
            memset(&icons[i], 0, sizeof(pax_buf_t));
            ESP_LOGE(TAG, "Failed to decode icon file %s", icon_paths[i]);
            icons_missing = true;
        }
        fclose(fd);
    }
}

void unload_icons(void)
{
    for (int i = 0; i < ICON_LAST; i++) {
        if (pax_buf_get_width(&icons[i]) == 0 || pax_buf_get_height(&icons[i]) == 0) {
            continue;  // Not loaded, skip
        }
        uint8_t* buffer = (uint8_t*)pax_buf_get_pixels(&icons[i]);
        pax_buf_destroy(&icons[i]);
        free(buffer);
        memset(&icons[i], 0, sizeof(pax_buf_t));
    }
}

pax_buf_t* get_icon(icon_t icon)
{
    if (icon < 0 || icon >= ICON_LAST) {
        ESP_LOGE(TAG, "Invalid icon index %d", icon);
        return NULL;
    }
    return &icons[icon];
}
