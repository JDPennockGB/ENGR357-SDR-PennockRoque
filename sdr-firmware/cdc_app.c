/*
 * cdc_app.c — Lab 5 Quisk serial control protocol handler.
 *
 * Implements the line-oriented command protocol used by quisk_conf_2026.py:
 *
 *   VER              → VER,<version>\r\nOK\r\n
 *   XTAL             → XTAL,24576000\r\nOK\r\n
 *   MODE             → MODE,DIRECT\r\nOK\r\n
 *   RATE,<n>         → OK\r\n
 *   FREQ,            → <last_hz>\r\n<last_status>\r\n
 *   FREQ,hz,N,a,b,c,P1,P2,P3  → <hz>\r\n<OK,G,0 or OK,F,0>\r\n
 *   <anything else>  → ERR\r\n
 *
 * Ctrl-C (0x03) is discarded. Ctrl-D (0x04) re-sends the startup sentinel.
 * "SDR ready\r\n" is sent when the host opens the port (DTR rises).
 */

  // fixed verson for new drivers

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "tusb.h"
#include "si5351.h"

/* Provided by main.c */
extern i2c_inst_t *g_i2c;

/* ---- Module state ----------------------------------------------------- */
static char    rx_buf[256];
static int     rx_pos    = 0;
static uint32_t last_hz  = 0;
static char    last_status[16] = "OK,X,0";
static bool    g_send_ready    = false;

/* ---- Internal helpers ------------------------------------------------- */

/* Write s to the CDC TX FIFO and flush immediately. */
static void cdc_send(const char *s) {
    uint32_t len = (uint32_t)strlen(s);
    uint32_t sent = 0;
    while (sent < len) {
        sent += tud_cdc_write(s + sent, len - sent);
    }
    tud_cdc_write_flush();
}

/* Write a printf-style formatted string to CDC (max 128 bytes). */
static void cdc_printf(const char *fmt, ...) {
    char tmp[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    cdc_send(tmp);
}

/* ---- Command dispatcher ----------------------------------------------- */
static void process_command(const char *line) {
    if (strcmp(line, "VER") == 0) {
        cdc_send("VER,1.0\r\nOK\r\n");

    } else if (strcmp(line, "XTAL") == 0) {
        cdc_send("XTAL,24576000\r\nOK\r\n");

    } else if (strcmp(line, "MODE") == 0) {
        cdc_send("MODE,DIRECT\r\nOK\r\n");

    } else if (strncmp(line, "RATE,", 5) == 0) {
        /* Acknowledge any sample rate — we don't use it on this side. */
        cdc_send("OK\r\n");

    } else if (strncmp(line, "FREQ,", 5) == 0) {
        const char *args = line + 5;

        if (*args == '\0') {
            /* Bare "FREQ," — return last programmed frequency and status. */
            cdc_printf("%u\r\n%s\r\n", last_hz, last_status);

        } else {
            /* Full FREQ,hz,N,a,b,c,P1,P2,P3 */
            uint32_t hz, n, a, b, c, p1, p2, p3;
            int matched = sscanf(args, "%u,%u,%u,%u,%u,%u,%u,%u",
                                 &hz, &n, &a, &b, &c, &p1, &p2, &p3);
            if (matched == 8) {
                si5351_program_from_quisk(g_i2c, hz, n, a, b, c, p1, p2, p3);
                last_hz = hz;
                if (b == 0u) {
                    snprintf(last_status, sizeof(last_status), "OK,G,0");
                } else {
                    snprintf(last_status, sizeof(last_status), "OK,F,0");
                }
                cdc_printf("%u\r\n%s\r\n", last_hz, last_status);
            } else {
                cdc_send("ERR\r\n");
            }
        }

    } else {
        cdc_send("ERR\r\n");
    }
}

/* ---- Public API ------------------------------------------------------- */

/* Called from tud_cdc_line_state_cb when DTR rises — schedules the sentinel. */
void cdc_set_send_ready(void) {
    g_send_ready = true;
}

/*
 * cdc_task() — call from the main loop after tud_task().
 *
 * Sends the "SDR ready" sentinel on first connection, then accumulates
 * incoming bytes into a line buffer and dispatches complete lines.
 */
void cdc_task(void) {
    if (!tud_cdc_connected()) {
        return;
    }

    /* Send boot sentinel once when host opens the port. */
    if (g_send_ready) {
        g_send_ready = false;
        cdc_send("SDR ready\r\n");
    }

    /* Drain all available bytes. */
    while (tud_cdc_available()) {
        uint8_t ch;
        if (tud_cdc_read(&ch, 1) == 0) break;

        /* Ctrl-C is ignored; Ctrl-D re-sends the startup sentinel. */
        if (ch == 0x03u) continue;
        if (ch == 0x04u) {
            g_send_ready = true;
            continue;
        }

        /* Newline (either CR or LF) terminates a line. */
        if (ch == '\r' || ch == '\n') {
            if (rx_pos > 0) {
                rx_buf[rx_pos] = '\0';
                process_command(rx_buf);
                rx_pos = 0;
            }
            continue;
        }

        /* Accumulate into buffer, leaving room for NUL terminator. */
        if (rx_pos < (int)(sizeof(rx_buf) - 1)) {
            rx_buf[rx_pos++] = (char)ch;
        }
    }
}
