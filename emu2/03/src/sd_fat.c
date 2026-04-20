#include "sd_fat.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

static FATFS s_fs;
static bool  s_mounted = false;
static FIL   s_wfile;
static bool  s_writing = false;

bool sd_fat_init(void) {
    FRESULT fr = f_mount(&s_fs, "", 1);
    s_mounted = (fr == FR_OK);
    if (!s_mounted)
        printf("SD: mount failed fr=%d\n", (int)fr);
    return s_mounted;
}

bool sd_fat_mounted(void) { return s_mounted; }

int sd_fat_load(const char *path, uint8_t *dst, int dst_cap) {
    if (!s_mounted) return -1;
    FIL f;
    FRESULT fr = f_open(&f, path, FA_READ);
    if (fr != FR_OK) {
        printf("SD: f_open(%s) fr=%d\n", path, (int)fr);
        return -1;
    }
    UINT got = 0;
    fr = f_read(&f, dst, (UINT)dst_cap, &got);
    f_close(&f);
    if (fr != FR_OK) {
        printf("SD: f_read(%s) fr=%d\n", path, (int)fr);
        return -1;
    }
    return (int)got;
}

bool sd_fat_save(const char *path, const uint8_t *src, int len) {
    if (!s_mounted) return false;
    FIL f;
    if (f_open(&f, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return false;
    UINT bw = 0;
    FRESULT fr = f_write(&f, src, (UINT)len, &bw);
    f_close(&f);
    return (fr == FR_OK && (int)bw == len);
}

bool sd_fat_save_begin(const char *path) {
    if (!s_mounted || s_writing) return false;
    if (f_open(&s_wfile, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return false;
    s_writing = true;
    return true;
}

bool sd_fat_save_write(const uint8_t *data, int len) {
    if (!s_writing || len <= 0) return s_writing;
    UINT bw = 0;
    FRESULT fr = f_write(&s_wfile, data, (UINT)len, &bw);
    return (fr == FR_OK && (int)bw == len);
}

bool sd_fat_save_end(void) {
    if (!s_writing) return false;
    f_close(&s_wfile);
    s_writing = false;
    return true;
}

bool sd_fat_dir(const char *path, sd_fat_dir_cb cb) {
    if (!s_mounted) return false;
    DIR dir;
    if (f_opendir(&dir, path) != FR_OK) return false;
    FILINFO fno;
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != '\0')
        cb(fno.fname, (fno.fattrib & AM_DIR) != 0, fno.fsize);
    f_closedir(&dir);
    return true;
}

bool sd_fat_delete(const char *path) {
    return s_mounted && f_unlink(path) == FR_OK;
}

bool sd_fat_mkdir(const char *path) {
    if (!s_mounted) return false;
    FRESULT fr = f_mkdir(path);
    return (fr == FR_OK || fr == FR_EXIST);
}

bool sd_fat_rename(const char *src, const char *dst) {
    return s_mounted && f_rename(src, dst) == FR_OK;
}

uint32_t sd_fat_free_kb(void) {
    if (!s_mounted) return 0;
    DWORD fre_clust;
    FATFS *fsp;
    if (f_getfree("", &fre_clust, &fsp) != FR_OK) return 0;
    return (uint32_t)(fre_clust * fsp->csize / 2);  // 512-byte sectors → KB
}
