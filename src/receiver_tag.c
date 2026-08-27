/* ------------------------------------------------------------------------
 * receiver_tag.c -- placeholder receiver-tag firmware.
 *
 * Written to look like real firmware for unit-testing practice: a mix of
 * pure logic, register-touching code, and a couple of things that are
 * deliberately untestable on host so the boundary is visible.
 * ---------------------------------------------------------------------- */

#include <string.h>

#include "receiver_tag.h"
#include "hw_regs.h"

/* Battery thresholds, millivolts at the cell. */
#define RT_BATT_LOW_MV      2600u
#define RT_BATT_CRITICAL_MV 2300u

/* ADC front end: 12-bit, 3.3 V reference, 2:1 resistor divider. */
#define RT_ADC_FULL_SCALE   4095u
#define RT_ADC_VREF_MV      3300u
#define RT_ADC_DIVIDER      2u

/* RSSI range used for the 0..100 quality mapping. */
#define RT_RSSI_BEST_DBM    (-30)
#define RT_RSSI_WORST_DBM   (-100)

/* How long rt_hw_read_packet will spin waiting on the FIFO. */
#define RT_FIFO_SPIN_LIMIT  1000u

/* =======================================================================
 * Pure logic
 * ===================================================================== */

/* CRC-8/MAXIM (Dallas), reflected polynomial 0x8C. */
uint8_t rt_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00u;
    uint8_t i;

    if (data == NULL) {
        return 0x00u;
    }

    for (i = 0u; i < len; i++) {
        uint8_t byte = data[i];
        uint8_t bit;

        for (bit = 0u; bit < 8u; bit++) {
            uint8_t mix = (uint8_t)((crc ^ byte) & 0x01u);
            crc >>= 1;
            if (mix != 0u) {
                crc ^= 0x8Cu;
            }
            byte >>= 1;
        }
    }

    return crc;
}

static bool rt_tag_id_is_reserved(uint32_t tag_id)
{
    return (tag_id == 0x000000u) || (tag_id == 0xFFFFFFu);
}

rt_parse_result_t rt_packet_parse(const uint8_t *buf, uint8_t len,
                                  rt_packet_t *out)
{
    uint32_t tag_id;
    uint8_t  expected_crc;

    if ((buf == NULL) || (out == NULL)) {
        return RT_PARSE_NULL_ARG;
    }

    if (len != RT_PACKET_LEN) {
        return RT_PARSE_BAD_LENGTH;
    }

    if (buf[0] != RT_PREAMBLE) {
        return RT_PARSE_BAD_PREAMBLE;
    }

    expected_crc = rt_crc8(buf, RT_CRC_OFFSET);
    if (expected_crc != buf[RT_CRC_OFFSET]) {
        return RT_PARSE_BAD_CRC;
    }

    tag_id = ((uint32_t)buf[1] << 16) |
             ((uint32_t)buf[2] << 8)  |
             ((uint32_t)buf[3]);

    if (rt_tag_id_is_reserved(tag_id)) {
        return RT_PARSE_RESERVED_ID;
    }

    out->tag_id      = tag_id;
    out->seq         = buf[4];
    out->flags       = buf[5];
    out->rssi_dbm    = (int8_t)buf[6];
    out->battery_raw = (uint16_t)(((uint16_t)buf[7] << 8) | buf[8]);
    out->payload     = (uint16_t)(((uint16_t)buf[9] << 8) | buf[10]);

    /* The ADC is 12-bit; anything above that is a corrupt field even when
     * the CRC happens to agree. Clamp rather than reject -- a bad battery
     * reading should not cost us a valid position report. */
    if (out->battery_raw > RT_ADC_FULL_SCALE) {
        out->battery_raw = RT_ADC_FULL_SCALE;
    }

    return RT_PARSE_OK;
}

bool rt_packet_build(const rt_packet_t *pkt, uint8_t *buf, uint8_t len)
{
    if ((pkt == NULL) || (buf == NULL) || (len != RT_PACKET_LEN)) {
        return false;
    }

    buf[0]  = RT_PREAMBLE;
    buf[1]  = (uint8_t)((pkt->tag_id >> 16) & 0xFFu);
    buf[2]  = (uint8_t)((pkt->tag_id >> 8) & 0xFFu);
    buf[3]  = (uint8_t)(pkt->tag_id & 0xFFu);
    buf[4]  = pkt->seq;
    buf[5]  = pkt->flags;
    buf[6]  = (uint8_t)pkt->rssi_dbm;
    buf[7]  = (uint8_t)((pkt->battery_raw >> 8) & 0xFFu);
    buf[8]  = (uint8_t)(pkt->battery_raw & 0xFFu);
    buf[9]  = (uint8_t)((pkt->payload >> 8) & 0xFFu);
    buf[10] = (uint8_t)(pkt->payload & 0xFFu);
    buf[11] = rt_crc8(buf, RT_CRC_OFFSET);

    return true;
}

/* Unsigned subtraction wraps, so this stays correct across the 32-bit
 * millisecond rollover at ~49.7 days. */
uint32_t rt_elapsed_ms(uint32_t now_ms, uint32_t then_ms)
{
    return (uint32_t)(now_ms - then_ms);
}

bool rt_deadline_passed(uint32_t now_ms, uint32_t start_ms,
                        uint32_t timeout_ms)
{
    return rt_elapsed_ms(now_ms, start_ms) >= timeout_ms;
}

/* Distance from prev to cur in 8-bit sequence space. 0 means the same
 * packet arrived twice; 1 is the normal in-order case; anything larger
 * implies (gap - 1) packets went missing. */
uint8_t rt_seq_gap(uint8_t prev_seq, uint8_t cur_seq)
{
    return (uint8_t)(cur_seq - prev_seq);
}

uint16_t rt_adc_to_millivolts(uint16_t counts)
{
    uint32_t mv;

    if (counts > RT_ADC_FULL_SCALE) {
        counts = RT_ADC_FULL_SCALE;
    }

    mv = ((uint32_t)counts * RT_ADC_VREF_MV * RT_ADC_DIVIDER)
         / RT_ADC_FULL_SCALE;

    return (uint16_t)mv;
}

bool rt_battery_is_low(uint16_t millivolts)
{
    return millivolts < RT_BATT_LOW_MV;
}

/* Linear map from dBm to a 0..100 quality figure, clamped at both ends. */
uint8_t rt_rssi_to_quality(int8_t rssi_dbm)
{
    int32_t span;
    int32_t offset;
    int32_t quality;

    if (rssi_dbm >= RT_RSSI_BEST_DBM) {
        return 100u;
    }
    if (rssi_dbm <= RT_RSSI_WORST_DBM) {
        return 0u;
    }

    span    = (int32_t)RT_RSSI_BEST_DBM - (int32_t)RT_RSSI_WORST_DBM;
    offset  = (int32_t)rssi_dbm - (int32_t)RT_RSSI_WORST_DBM;
    quality = (offset * 100) / span;

    if (quality < 0) {
        quality = 0;
    }
    if (quality > 100) {
        quality = 100;
    }

    return (uint8_t)quality;
}

uint8_t rt_channel_clamp(uint8_t channel)
{
    if (channel < RT_CHAN_MIN) {
        return RT_CHAN_MIN;
    }
    if (channel > RT_CHAN_MAX) {
        return RT_CHAN_MAX;
    }
    return channel;
}

/* =======================================================================
 * State machine
 * ===================================================================== */

rt_state_t rt_state_next(rt_state_t state, rt_event_t evt)
{
    /* A hardware fault beats everything except the explicit reset that
     * clears it. */
    if (evt == RT_EVT_HW_FAULT) {
        return RT_STATE_FAULT;
    }

    switch (state) {
    case RT_STATE_INIT:
        if (evt == RT_EVT_RADIO_READY) {
            return RT_STATE_IDLE;
        }
        break;

    case RT_STATE_IDLE:
        if (evt == RT_EVT_PACKET_RECEIVED) {
            return RT_STATE_DECODING;
        }
        if (evt == RT_EVT_TIMEOUT) {
            return RT_STATE_LISTENING;
        }
        break;

    case RT_STATE_LISTENING:
        if (evt == RT_EVT_PACKET_RECEIVED) {
            return RT_STATE_DECODING;
        }
        if (evt == RT_EVT_TIMEOUT) {
            return RT_STATE_IDLE;
        }
        break;

    case RT_STATE_DECODING:
        if ((evt == RT_EVT_DECODE_DONE) || (evt == RT_EVT_CRC_ERROR)) {
            return RT_STATE_LISTENING;
        }
        if (evt == RT_EVT_TIMEOUT) {
            return RT_STATE_IDLE;
        }
        break;

    case RT_STATE_FAULT:
        if (evt == RT_EVT_RESET) {
            return RT_STATE_INIT;
        }
        break;

    default:
        /* Corrupted state variable -- fail safe. */
        return RT_STATE_FAULT;
    }

    return state;
}

const char *rt_state_name(rt_state_t state)
{
    switch (state) {
    case RT_STATE_INIT:      return "INIT";
    case RT_STATE_IDLE:      return "IDLE";
    case RT_STATE_LISTENING: return "LISTENING";
    case RT_STATE_DECODING:  return "DECODING";
    case RT_STATE_FAULT:     return "FAULT";
    default:                 return "UNKNOWN";
    }
}

/* =======================================================================
 * Tag tracker
 * ===================================================================== */

void rt_tracker_init(rt_tracker_t *trk)
{
    if (trk == NULL) {
        return;
    }
    memset(trk, 0, sizeof(*trk));
}

int rt_tracker_find(const rt_tracker_t *trk, uint32_t tag_id)
{
    uint8_t i;

    if (trk == NULL) {
        return -1;
    }

    for (i = 0u; i < RT_MAX_TRACKED_TAGS; i++) {
        if (trk->entries[i].in_use && (trk->entries[i].tag_id == tag_id)) {
            return (int)i;
        }
    }

    return -1;
}

static int rt_tracker_alloc(rt_tracker_t *trk, uint32_t now_ms)
{
    uint8_t  i;
    int      oldest_idx = -1;
    uint32_t oldest_age = 0u;

    for (i = 0u; i < RT_MAX_TRACKED_TAGS; i++) {
        if (!trk->entries[i].in_use) {
            return (int)i;
        }
    }

    /* Table is full. Evict the least recently seen entry, but only if it
     * has actually gone stale -- otherwise the caller is over capacity and
     * should be told so rather than silently losing a live tag. */
    for (i = 0u; i < RT_MAX_TRACKED_TAGS; i++) {
        uint32_t age = rt_elapsed_ms(now_ms, trk->entries[i].last_seen_ms);
        if (age > oldest_age) {
            oldest_age = age;
            oldest_idx = (int)i;
        }
    }

    if ((oldest_idx >= 0) && (oldest_age >= RT_TAG_STALE_MS)) {
        return oldest_idx;
    }

    return -1;
}

static void rt_rssi_push(rt_tag_entry_t *e, int8_t rssi)
{
    e->rssi_hist[e->hist_idx] = rssi;
    e->hist_idx = (uint8_t)((e->hist_idx + 1u) % RT_RSSI_HISTORY);

    if (e->hist_count < RT_RSSI_HISTORY) {
        e->hist_count++;
    }
}

int rt_tracker_update(rt_tracker_t *trk, const rt_packet_t *pkt,
                      uint32_t now_ms)
{
    int idx;
    rt_tag_entry_t *e;

    if ((trk == NULL) || (pkt == NULL)) {
        return -1;
    }

    idx = rt_tracker_find(trk, pkt->tag_id);

    if (idx < 0) {
        idx = rt_tracker_alloc(trk, now_ms);
        if (idx < 0) {
            return -1;
        }

        e = &trk->entries[idx];
        memset(e, 0, sizeof(*e));
        e->tag_id   = pkt->tag_id;
        e->in_use   = true;
        e->last_seq = pkt->seq;
        trk->count++;
    } else {
        uint8_t gap;

        e   = &trk->entries[idx];
        gap = rt_seq_gap(e->last_seq, pkt->seq);

        if (gap == 0u) {
            /* Duplicate retransmission. Refresh the timestamp so the entry
             * does not go stale, but do not disturb the RSSI average or the
             * loss counter. */
            e->last_seen_ms = now_ms;
            return idx;
        }

        if (gap > 1u) {
            e->missed = (uint16_t)(e->missed + (gap - 1u));
        }

        e->last_seq = pkt->seq;
    }

    e->last_seen_ms = now_ms;
    rt_rssi_push(e, pkt->rssi_dbm);

    return idx;
}

uint8_t rt_tracker_prune(rt_tracker_t *trk, uint32_t now_ms)
{
    uint8_t i;
    uint8_t removed = 0u;

    if (trk == NULL) {
        return 0u;
    }

    for (i = 0u; i < RT_MAX_TRACKED_TAGS; i++) {
        rt_tag_entry_t *e = &trk->entries[i];

        if (!e->in_use) {
            continue;
        }

        if (rt_elapsed_ms(now_ms, e->last_seen_ms) >= RT_TAG_STALE_MS) {
            memset(e, 0, sizeof(*e));
            removed++;
            if (trk->count > 0u) {
                trk->count--;
            }
        }
    }

    return removed;
}

/* Mean of the stored RSSI samples. Integer division truncates toward zero,
 * so the average of negative dBm values reads very slightly optimistic --
 * within 1 dB, which is well under the radio's own accuracy. */
int8_t rt_tracker_rssi_avg(const rt_tracker_t *trk, uint32_t tag_id)
{
    int idx;
    int32_t sum = 0;
    uint8_t i;
    const rt_tag_entry_t *e;

    idx = rt_tracker_find(trk, tag_id);
    if (idx < 0) {
        return 0;
    }

    e = &trk->entries[idx];
    if (e->hist_count == 0u) {
        return 0;
    }

    for (i = 0u; i < e->hist_count; i++) {
        sum += (int32_t)e->rssi_hist[i];
    }

    return (int8_t)(sum / (int32_t)e->hist_count);
}

/* =======================================================================
 * Register-touching layer
 * ===================================================================== */

void rt_hw_init(void)
{
    CTRL_REG = 0x00u;
    CTRL_REG |= RT_BIT(CTRL_SOFT_RESET);
    CTRL_REG &= (uint8_t)~RT_BIT(CTRL_SOFT_RESET);

    CTRL_REG |= RT_BIT(CTRL_RF_ENABLE);
    CTRL_REG |= RT_BIT(CTRL_CRC_EN);
    CTRL_REG |= RT_BIT(CTRL_RX_MODE);

    RF_CHAN_REG   = RT_CHAN_MIN;
    IRQ_FLAGS_REG = 0xFFu;   /* write 1 to clear everything pending */
    LED_REG       = RT_BIT(LED_POWER);
}

bool rt_hw_set_channel(uint8_t channel)
{
    if ((channel < RT_CHAN_MIN) || (channel > RT_CHAN_MAX)) {
        return false;
    }

    /* The radio requires RX to be off while retuning. */
    CTRL_REG &= (uint8_t)~RT_BIT(CTRL_RX_MODE);
    RF_CHAN_REG = channel;
    CTRL_REG |= RT_BIT(CTRL_RX_MODE);

    return true;
}

bool rt_hw_fifo_ready(void)
{
    if ((STATUS_REG & RT_BIT(STATUS_FAULT)) != 0u) {
        return false;
    }
    if ((STATUS_REG & RT_BIT(STATUS_FIFO_EMPTY)) != 0u) {
        return false;
    }
    return (STATUS_REG & RT_BIT(STATUS_FIFO_READY)) != 0u;
}

bool rt_hw_read_packet(uint8_t *buf, uint8_t len)
{
    uint8_t i;

    if ((buf == NULL) || (len != RT_PACKET_LEN)) {
        return false;
    }

    if (!rt_hw_fifo_ready()) {
        return false;
    }

    for (i = 0u; i < len; i++) {
        buf[i] = RF_FIFO_REG;
    }

    if ((STATUS_REG & RT_BIT(STATUS_OVERFLOW)) != 0u) {
        return false;
    }

    return true;
}

void rt_hw_clear_irq(uint8_t mask)
{
    IRQ_FLAGS_REG = mask;
}

void rt_hw_set_leds(rt_state_t state, bool low_battery)
{
    uint8_t leds = RT_BIT(LED_POWER);

    if ((state == RT_STATE_LISTENING) || (state == RT_STATE_DECODING)) {
        leds |= RT_BIT(LED_LINK);
    }

    if ((state == RT_STATE_FAULT) || low_battery) {
        leds |= RT_BIT(LED_FAULT);
    }

    LED_REG = leds;
}

uint16_t rt_hw_read_adc(void)
{
    uint16_t counts;

    counts = (uint16_t)(((uint16_t)ADC_HI_REG << 8) | ADC_LO_REG);
    counts &= 0x0FFFu;   /* 12-bit converter, upper nibble is undefined */

    return counts;
}

/* =======================================================================
 * Top level
 * ===================================================================== */

rt_event_t rt_poll(rt_tracker_t *trk, rt_packet_t *last_pkt, uint32_t now_ms)
{
    uint8_t buf[RT_PACKET_LEN];
    rt_packet_t pkt;
    rt_parse_result_t res;

    if ((trk == NULL) || (last_pkt == NULL)) {
        return RT_EVT_NONE;
    }

    if ((STATUS_REG & RT_BIT(STATUS_FAULT)) != 0u) {
        return RT_EVT_HW_FAULT;
    }

    if (!rt_hw_fifo_ready()) {
        return RT_EVT_NONE;
    }

    if (!rt_hw_read_packet(buf, (uint8_t)sizeof(buf))) {
        rt_hw_clear_irq(RT_BIT(IRQ_OVERFLOW));
        return RT_EVT_NONE;
    }

    res = rt_packet_parse(buf, (uint8_t)sizeof(buf), &pkt);
    if (res != RT_PARSE_OK) {
        rt_hw_clear_irq(RT_BIT(IRQ_CRC_ERR));
        return RT_EVT_CRC_ERROR;
    }

    (void)rt_tracker_update(trk, &pkt, now_ms);
    *last_pkt = pkt;

    rt_hw_clear_irq(RT_BIT(IRQ_RX_DONE));

    return RT_EVT_PACKET_RECEIVED;
}
