#ifndef SD_FAT_H
#define SD_FAT_H

#include <stdint.h>
#include <stdbool.h>

bool     sd_fat_init(void);
bool     sd_fat_mounted(void);

// Load complete file into dst (max dst_cap bytes). Returns bytes read or -1.
int      sd_fat_load(const char *path, uint8_t *dst, int dst_cap);

// Save complete buffer to a file. Returns true on success.
bool     sd_fat_save(const char *path, const uint8_t *src, int len);

// Streaming save — for large files written in chunks.
bool     sd_fat_save_begin(const char *path);
bool     sd_fat_save_write(const uint8_t *data, int len);
bool     sd_fat_save_end(void);

// Directory listing callback: name, is_dir, size.
typedef void (*sd_fat_dir_cb)(const char *name, bool is_dir, uint32_t size);
bool     sd_fat_dir(const char *path, sd_fat_dir_cb cb);

bool     sd_fat_delete(const char *path);
bool     sd_fat_mkdir(const char *path);
bool     sd_fat_rename(const char *src, const char *dst);

// Free space in kilobytes; 0 if unmounted or error.
uint32_t sd_fat_free_kb(void);

#endif
