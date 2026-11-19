/* SPDX-License-Identifier: GPL-3.0-only
 * KRIECHSTROM for VIBE FM: what the FM-1 sources expect of their board, without one. The sound and the edit logic
 * of KRIECHSTROM (core/kriechstrom/firmware/src) build unchanged on top of these; the FM-1 display, keys, LEDs,
 * flash and USB are not there: the VIBE's own pages, controls and storage replace them (kx_*.h). */
/* the few libc calls the sources make: the VIBE FM SDK has them (the host test: its own libc) */
#ifndef KX_REF
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
char *strcpy(char *d, const char *s);
char *strcat(char *d, const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
#endif
#define FELUCCA_CDC 0
#define FELUCCA_FLASH 0
#define FELUCCA_VERSION "0.1"
#define FM1_TICKS_PER_US 1u
#define FM1_NCOL 11u
#define __attribute__(x)
static struct { volatile uint32_t notes, buttons; } fm1_in;
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
static const int8_t FM1_KEYMAP[6][FM1_NCOL];
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL];
static void fm1_led_dim_level(uint32_t lo) { (void)lo; }
static int fm1_led_anim_on(void) { return 0; }
static void fm1_led_idle_start(uint32_t glow) { (void)glow; }
static int fm1_led_idle_on(void) { return 0; }
static void fm1_led_anim_stop(void) {}
static uint32_t kx_ticks;
static uint32_t fm1_ticks(void) { return kx_ticks; }
static uint32_t fm1_input_edges(int x) { (void)x; return 0; }
static uint32_t fm1_input_note_edges(void) { return 0; }
static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
/* the FM-1's 240 x 240 panel: a picture in memory (VIBE FM Link shows it on an FM-1, kx_link.h) */
static uint16_t kx_fm1_screen[240 * 240];
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            kx_fm1_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            kx_fm1_screen[(y + j) * 240u + x + i] = p[j * w + i];
}
static struct { uint32_t stage; } felucca_dbg;

/* the core's hook for the audio input (fx.c mix_block): kx_input.h */
#ifndef KX_REF                                   /* (tests/core_ref.c: the FM-1's mix, no input) */
static void kx_in_mix(uint32_t n);
#define KRST_EXT_MIX(n) kx_in_mix(n)
#endif
