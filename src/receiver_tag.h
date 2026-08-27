#ifndef RECEIVER_TAG_H
#define RECEIVER_TAG_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------------
 * Receiver-tag firmware -- placeholder standing in for the real thing.
 *
 * Wire format, 12 bytes, big-endian:
 *
 *   [0]     preamble, always 0xA5
 *   [1..3]  tag id, 24-bit
 *   [4]     sequence number, wraps at 8 bits
 *   [5]     flags
 *   [6]     rssi, dBm, signed
 *   [7..8]  battery, raw 12-bit ADC counts
 *   [9..10] payload, application defined
 *   [11]    CRC-8/MAXIM over bytes 0..10
 * ---------------------------------------------------------------------- */

#define RT_PACKET_LEN        12u
#define RT_PREAMBLE          0xA5u
#define RT_CRC_OFFSET        11u

#define RT_MAX_TRACKED_TAGS  8u
#define RT_RSSI_HISTORY      4u
#define RT_TAG_STALE_MS      3000u

#define RT_CHAN_MIN          1u
#define RT_CHAN_MAX          64u

/* Packet flag bits */
#define RT_FLAG_LOW_BATTERY  0u
#define RT_FLAG_BUTTON       1u
#define RT_FLAG_MOVING       2u
#define RT_FLAG_TAMPER       3u

typedef enum {
    RT_STATE_INIT = 0,
    RT_STATE_IDLE,
    RT_STATE_LISTENING,
    RT_STATE_DECODING,
    RT_STATE_FAULT
} rt_state_t;

typedef enum {
    RT_EVT_NONE = 0,
    RT_EVT_RADIO_READY,
    RT_EVT_PACKET_RECEIVED,
    RT_EVT_DECODE_DONE,
    RT_EVT_CRC_ERROR,
    RT_EVT_TIMEOUT,
    RT_EVT_HW_FAULT,
    RT_EVT_RESET
} rt_event_t;

typedef enum {
    RT_PARSE_OK = 0,
    RT_PARSE_BAD_PREAMBLE,
    RT_PARSE_BAD_CRC,
    RT_PARSE_BAD_LENGTH,
    RT_PARSE_NULL_ARG,
    RT_PARSE_RESERVED_ID
} rt_parse_result_t;

typedef struct {
    uint32_t tag_id;      /* 24-bit, 0x000000 and 0xFFFFFF are reserved */
    uint8_t  seq;
    uint8_t  flags;
    int8_t   rssi_dbm;
    uint16_t battery_raw; /* raw ADC counts, 12-bit */
    uint16_t payload;
} rt_packet_t;

typedef struct {
    uint32_t tag_id;
    uint32_t last_seen_ms;
    int8_t   rssi_hist[RT_RSSI_HISTORY];
    uint8_t  hist_count;
    uint8_t  hist_idx;
    uint8_t  last_seq;
    uint16_t missed;      /* packets inferred lost from sequence gaps */
    bool     in_use;
} rt_tag_entry_t;

typedef struct {
    rt_tag_entry_t entries[RT_MAX_TRACKED_TAGS];
    uint8_t        count;
} rt_tracker_t;

/* --- Pure logic: no hardware access, test these first ------------------ */

uint8_t  rt_crc8(const uint8_t *data, uint8_t len);
rt_parse_result_t rt_packet_parse(const uint8_t *buf, uint8_t len,
                                  rt_packet_t *out);
bool     rt_packet_build(const rt_packet_t *pkt, uint8_t *buf, uint8_t len);

uint32_t rt_elapsed_ms(uint32_t now_ms, uint32_t then_ms);
bool     rt_deadline_passed(uint32_t now_ms, uint32_t start_ms,
                            uint32_t timeout_ms);
uint8_t  rt_seq_gap(uint8_t prev_seq, uint8_t cur_seq);

uint16_t rt_adc_to_millivolts(uint16_t counts);
bool     rt_battery_is_low(uint16_t millivolts);
uint8_t  rt_rssi_to_quality(int8_t rssi_dbm);
uint8_t  rt_channel_clamp(uint8_t channel);

rt_state_t rt_state_next(rt_state_t state, rt_event_t evt);
const char *rt_state_name(rt_state_t state);

/* --- Tracker: pure logic over a caller-owned struct -------------------- */

void  rt_tracker_init(rt_tracker_t *trk);
int   rt_tracker_update(rt_tracker_t *trk, const rt_packet_t *pkt,
                        uint32_t now_ms);
int   rt_tracker_find(const rt_tracker_t *trk, uint32_t tag_id);
uint8_t rt_tracker_prune(rt_tracker_t *trk, uint32_t now_ms);
int8_t rt_tracker_rssi_avg(const rt_tracker_t *trk, uint32_t tag_id);

/* --- Register-touching: testable through the mocked registers ---------- */

void rt_hw_init(void);
bool rt_hw_set_channel(uint8_t channel);
bool rt_hw_fifo_ready(void);
bool rt_hw_read_packet(uint8_t *buf, uint8_t len);
void rt_hw_clear_irq(uint8_t mask);
void rt_hw_set_leds(rt_state_t state, bool low_battery);
uint16_t rt_hw_read_adc(void);

/* --- Top level -------------------------------------------------------- */

rt_event_t rt_poll(rt_tracker_t *trk, rt_packet_t *last_pkt, uint32_t now_ms);

#endif /* RECEIVER_TAG_H */
