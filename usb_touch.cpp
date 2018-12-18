#include "usb_dev.h"
#include "usb_touch.h"
#include "touch_status.h"
#include "core_pins.h"
#include "usb_serial.h" /* debugging */

#ifdef MULTITOUCH_INTERFACE // defined by usb_dev.h -> usb_desc.h
#if F_CPU >= 20000000

#define HID_MAX_COORD (32767 + 1) /* Logical Maximum of X,Y in multitouch_report_desc */

static struct touch_status last_reported;

static uint8_t reports[MAX_TOUCHES][9];
static int num_reports = 0;
static int report_idx = 0; /* next report to transmit */

static void generate_reports()
{
    uint16_t timestamp = millis() * 10;

    num_reports = 0;

    for (int i = 0; i < MAX_TOUCHES; i++) {
        int is_active = current_touch_status.is_active[i];
        int was_active = last_reported.is_active[i];

        if (is_active || was_active) {
            uint16_t x = current_touch_status.x[i];
            uint16_t y = current_touch_status.y[i];
            uint8_t size = current_touch_status.size[i];

            x = x * HID_MAX_COORD / touch_x_max;
            y = y * HID_MAX_COORD / touch_y_max;

            // Report tip_switch=1 for all active touches, and report tip_switch=0 for release events.
            reports[num_reports][0] = (size & 0xFE) | (is_active ? 1 : 0); /* pressure, tip switch */
            reports[num_reports][1] = i; /* contact id */
            reports[num_reports][2] = x;
            reports[num_reports][3] = x >> 8;
            reports[num_reports][4] = y;
            reports[num_reports][5] = y >> 8;
            reports[num_reports][6] = timestamp;
            reports[num_reports][7] = timestamp >> 8;
            reports[num_reports][8] = 0; /* contact count */
            num_reports++;
        }
    }

    if (num_reports) {
        reports[0][8] = num_reports; /* contact count */
    }

    last_reported = current_touch_status;
}

// called by the start-of-frame interrupt
void usb_touchscreen_update_callback()
{
    if (usb_tx_packet_count(MULTITOUCH_ENDPOINT) > 1) {
        // wait to begin another scan if more than
        // one prior packet remains to transmit
        return;
    }

    if (report_idx == num_reports) {
        generate_reports();
        report_idx = 0;
    }

    if (report_idx < num_reports) {
#if 0
        for (int i = 0; i < 9; i++) {
            Serial.printf("%02X ", reports[report_idx][i]);
        }
        Serial.printf("\n");
#endif

        usb_packet_t *tx_packet = usb_malloc();
        if (tx_packet == NULL) return;
        memcpy(tx_packet->buf, &reports[report_idx], 9);
        tx_packet->len = 9;
        usb_tx(MULTITOUCH_ENDPOINT, tx_packet);

        report_idx++;
    }
}

#endif // F_CPU
#endif // MULTITOUCH_INTERFACE
