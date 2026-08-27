#include <string.h>

#include "unity.h"
#include "receiver_tag.h"
#include "hw_regs.h"

void hw_regs_reset(void);

void setUp(void)
{
    hw_regs_reset();
}

void tearDown(void) { }

/* Helper: a well-formed packet with a valid CRC. */
static void make_packet(uint8_t *buf, uint32_t tag_id, uint8_t seq,
                        int8_t rssi)
{
    rt_packet_t pkt;

    memset(&pkt, 0, sizeof(pkt));
    pkt.tag_id      = tag_id;
    pkt.seq         = seq;
    pkt.rssi_dbm    = rssi;
    pkt.battery_raw = 3000u;

    TEST_ASSERT_TRUE(rt_packet_build(&pkt, buf, RT_PACKET_LEN));
}

/* --- Pure logic -------------------------------------------------------- */

static void test_crc8_of_known_vector(void)
{
    const uint8_t data[] = { 0x00u };
    TEST_ASSERT_EQUAL_HEX8(0x00u, rt_crc8(data, 1u));
}

static void test_crc8_detects_single_bit_flip(void)
{
    uint8_t buf[RT_PACKET_LEN];
    uint8_t good;

    make_packet(buf, 0x123456u, 7u, -55);
    good = rt_crc8(buf, RT_CRC_OFFSET);

    buf[3] ^= 0x01u;
    TEST_ASSERT_NOT_EQUAL_UINT8(good, rt_crc8(buf, RT_CRC_OFFSET));
}

static void test_build_then_parse_round_trips(void)
{
    uint8_t buf[RT_PACKET_LEN];
    rt_packet_t out;

    make_packet(buf, 0xABCDEFu, 42u, -70);
    TEST_ASSERT_EQUAL(RT_PARSE_OK,
                      rt_packet_parse(buf, RT_PACKET_LEN, &out));

    TEST_ASSERT_EQUAL_HEX32(0xABCDEFu, out.tag_id);
    TEST_ASSERT_EQUAL_UINT8(42u, out.seq);
    TEST_ASSERT_EQUAL_INT8(-70, out.rssi_dbm);
}

static void test_parse_rejects_bad_preamble(void)
{
    uint8_t buf[RT_PACKET_LEN];
    rt_packet_t out;

    make_packet(buf, 0x123456u, 1u, -60);
    buf[0] = 0x00u;

    TEST_ASSERT_EQUAL(RT_PARSE_BAD_PREAMBLE,
                      rt_packet_parse(buf, RT_PACKET_LEN, &out));
}

static void test_parse_rejects_bad_crc(void)
{
    uint8_t buf[RT_PACKET_LEN];
    rt_packet_t out;

    make_packet(buf, 0x123456u, 1u, -60);
    buf[RT_CRC_OFFSET] ^= 0xFFu;

    TEST_ASSERT_EQUAL(RT_PARSE_BAD_CRC,
                      rt_packet_parse(buf, RT_PACKET_LEN, &out));
}

static void test_parse_rejects_reserved_tag_ids(void)
{
    uint8_t buf[RT_PACKET_LEN];
    rt_packet_t out;

    make_packet(buf, 0x000000u, 1u, -60);
    TEST_ASSERT_EQUAL(RT_PARSE_RESERVED_ID,
                      rt_packet_parse(buf, RT_PACKET_LEN, &out));

    make_packet(buf, 0xFFFFFFu, 1u, -60);
    TEST_ASSERT_EQUAL(RT_PARSE_RESERVED_ID,
                      rt_packet_parse(buf, RT_PACKET_LEN, &out));
}

static void test_elapsed_ms_survives_rollover(void)
{
    TEST_ASSERT_EQUAL_UINT32(10u, rt_elapsed_ms(5u, 0xFFFFFFFBu));
    TEST_ASSERT_EQUAL_UINT32(0u, rt_elapsed_ms(100u, 100u));
}

static void test_seq_gap_wraps(void)
{
    TEST_ASSERT_EQUAL_UINT8(1u, rt_seq_gap(254u, 255u));
    TEST_ASSERT_EQUAL_UINT8(1u, rt_seq_gap(255u, 0u));
    TEST_ASSERT_EQUAL_UINT8(3u, rt_seq_gap(254u, 1u));
    TEST_ASSERT_EQUAL_UINT8(0u, rt_seq_gap(77u, 77u));
}

static void test_adc_conversion_endpoints(void)
{
    TEST_ASSERT_EQUAL_UINT16(0u, rt_adc_to_millivolts(0u));
    TEST_ASSERT_EQUAL_UINT16(6600u, rt_adc_to_millivolts(4095u));
    TEST_ASSERT_EQUAL_UINT16(6600u, rt_adc_to_millivolts(9999u));
}

static void test_rssi_quality_clamps(void)
{
    TEST_ASSERT_EQUAL_UINT8(100u, rt_rssi_to_quality(-10));
    TEST_ASSERT_EQUAL_UINT8(100u, rt_rssi_to_quality(-30));
    TEST_ASSERT_EQUAL_UINT8(0u, rt_rssi_to_quality(-100));
    TEST_ASSERT_EQUAL_UINT8(0u, rt_rssi_to_quality(-120));
    TEST_ASSERT_EQUAL_UINT8(50u, rt_rssi_to_quality(-65));
}

/* --- State machine ----------------------------------------------------- */

static void test_state_happy_path(void)
{
    rt_state_t s = RT_STATE_INIT;

    s = rt_state_next(s, RT_EVT_RADIO_READY);
    TEST_ASSERT_EQUAL(RT_STATE_IDLE, s);

    s = rt_state_next(s, RT_EVT_PACKET_RECEIVED);
    TEST_ASSERT_EQUAL(RT_STATE_DECODING, s);

    s = rt_state_next(s, RT_EVT_DECODE_DONE);
    TEST_ASSERT_EQUAL(RT_STATE_LISTENING, s);
}

static void test_fault_is_sticky_until_reset(void)
{
    rt_state_t s = rt_state_next(RT_STATE_LISTENING, RT_EVT_HW_FAULT);
    TEST_ASSERT_EQUAL(RT_STATE_FAULT, s);

    s = rt_state_next(s, RT_EVT_PACKET_RECEIVED);
    TEST_ASSERT_EQUAL(RT_STATE_FAULT, s);

    s = rt_state_next(s, RT_EVT_RESET);
    TEST_ASSERT_EQUAL(RT_STATE_INIT, s);
}

static void test_unknown_event_does_not_change_state(void)
{
    TEST_ASSERT_EQUAL(RT_STATE_IDLE,
                      rt_state_next(RT_STATE_IDLE, RT_EVT_NONE));
}

/* --- Tracker ----------------------------------------------------------- */

static void test_tracker_adds_and_finds_tag(void)
{
    rt_tracker_t trk;
    rt_packet_t pkt;

    rt_tracker_init(&trk);
    memset(&pkt, 0, sizeof(pkt));
    pkt.tag_id = 0x001122u;
    pkt.seq = 1u;
    pkt.rssi_dbm = -60;

    TEST_ASSERT_EQUAL_INT(0, rt_tracker_update(&trk, &pkt, 1000u));
    TEST_ASSERT_EQUAL_INT(0, rt_tracker_find(&trk, 0x001122u));
    TEST_ASSERT_EQUAL_INT(-1, rt_tracker_find(&trk, 0x999999u));
}

static void test_tracker_counts_missed_packets(void)
{
    rt_tracker_t trk;
    rt_packet_t pkt;
    int idx;

    rt_tracker_init(&trk);
    memset(&pkt, 0, sizeof(pkt));
    pkt.tag_id = 0x001122u;
    pkt.rssi_dbm = -60;

    pkt.seq = 1u;
    idx = rt_tracker_update(&trk, &pkt, 1000u);

    pkt.seq = 5u;   /* 2, 3 and 4 went missing */
    (void)rt_tracker_update(&trk, &pkt, 1100u);

    TEST_ASSERT_EQUAL_UINT16(3u, trk.entries[idx].missed);
}

static void test_tracker_ignores_duplicate_sequence(void)
{
    rt_tracker_t trk;
    rt_packet_t pkt;

    rt_tracker_init(&trk);
    memset(&pkt, 0, sizeof(pkt));
    pkt.tag_id = 0x001122u;
    pkt.seq = 9u;
    pkt.rssi_dbm = -50;

    (void)rt_tracker_update(&trk, &pkt, 1000u);
    (void)rt_tracker_update(&trk, &pkt, 1050u);

    TEST_ASSERT_EQUAL_UINT8(1u, trk.entries[0].hist_count);
    TEST_ASSERT_EQUAL_UINT32(1050u, trk.entries[0].last_seen_ms);
}

static void test_tracker_prunes_stale_entries(void)
{
    rt_tracker_t trk;
    rt_packet_t pkt;

    rt_tracker_init(&trk);
    memset(&pkt, 0, sizeof(pkt));
    pkt.tag_id = 0x001122u;
    pkt.seq = 1u;

    (void)rt_tracker_update(&trk, &pkt, 1000u);
    TEST_ASSERT_EQUAL_UINT8(0u, rt_tracker_prune(&trk, 2000u));
    TEST_ASSERT_EQUAL_UINT8(1u, rt_tracker_prune(&trk, 1000u + RT_TAG_STALE_MS));
    TEST_ASSERT_EQUAL_UINT8(0u, trk.count);
}

static void test_tracker_rssi_average(void)
{
    rt_tracker_t trk;
    rt_packet_t pkt;
    uint8_t i;
    const int8_t samples[] = { -60, -62, -64, -66 };

    rt_tracker_init(&trk);
    memset(&pkt, 0, sizeof(pkt));
    pkt.tag_id = 0x001122u;

    for (i = 0u; i < 4u; i++) {
        pkt.seq = (uint8_t)(i + 1u);
        pkt.rssi_dbm = samples[i];
        (void)rt_tracker_update(&trk, &pkt, 1000u + i);
    }

    TEST_ASSERT_EQUAL_INT8(-63, rt_tracker_rssi_avg(&trk, 0x001122u));
}

/* --- Register-touching ------------------------------------------------- */

static void test_hw_init_sets_expected_control_bits(void)
{
    rt_hw_init();

    TEST_ASSERT_BIT_HIGH(CTRL_RF_ENABLE, CTRL_REG);
    TEST_ASSERT_BIT_HIGH(CTRL_CRC_EN, CTRL_REG);
    TEST_ASSERT_BIT_HIGH(CTRL_RX_MODE, CTRL_REG);
    TEST_ASSERT_BIT_LOW(CTRL_SOFT_RESET, CTRL_REG);
    TEST_ASSERT_EQUAL_UINT8(RT_CHAN_MIN, RF_CHAN_REG);
}

static void test_set_channel_rejects_out_of_range(void)
{
    RF_CHAN_REG = 5u;

    TEST_ASSERT_FALSE(rt_hw_set_channel(0u));
    TEST_ASSERT_FALSE(rt_hw_set_channel(65u));
    TEST_ASSERT_EQUAL_UINT8(5u, RF_CHAN_REG);

    TEST_ASSERT_TRUE(rt_hw_set_channel(20u));
    TEST_ASSERT_EQUAL_UINT8(20u, RF_CHAN_REG);
    TEST_ASSERT_BIT_HIGH(CTRL_RX_MODE, CTRL_REG);
}

static void test_fifo_not_ready_when_fault_set(void)
{
    STATUS_REG = RT_BIT(STATUS_FIFO_READY) | RT_BIT(STATUS_FAULT);
    TEST_ASSERT_FALSE(rt_hw_fifo_ready());

    STATUS_REG = RT_BIT(STATUS_FIFO_READY);
    TEST_ASSERT_TRUE(rt_hw_fifo_ready());
}

static void test_read_adc_masks_to_12_bits(void)
{
    ADC_HI_REG = 0xFFu;
    ADC_LO_REG = 0x34u;

    TEST_ASSERT_EQUAL_UINT16(0x0F34u, rt_hw_read_adc());
}

static void test_leds_reflect_state(void)
{
    rt_hw_set_leds(RT_STATE_LISTENING, false);
    TEST_ASSERT_BIT_HIGH(LED_LINK, LED_REG);
    TEST_ASSERT_BIT_LOW(LED_FAULT, LED_REG);

    rt_hw_set_leds(RT_STATE_IDLE, true);
    TEST_ASSERT_BIT_HIGH(LED_FAULT, LED_REG);
    TEST_ASSERT_BIT_LOW(LED_LINK, LED_REG);
}

/* --- Top level, driven entirely through the mocked registers ----------- */

static void test_poll_reports_fault_from_status_register(void)
{
    rt_tracker_t trk;
    rt_packet_t pkt;

    rt_tracker_init(&trk);
    STATUS_REG = RT_BIT(STATUS_FAULT);

    TEST_ASSERT_EQUAL(RT_EVT_HW_FAULT, rt_poll(&trk, &pkt, 1000u));
}

static void test_poll_returns_none_when_fifo_empty(void)
{
    rt_tracker_t trk;
    rt_packet_t pkt;

    rt_tracker_init(&trk);
    STATUS_REG = RT_BIT(STATUS_FIFO_EMPTY);

    TEST_ASSERT_EQUAL(RT_EVT_NONE, rt_poll(&trk, &pkt, 1000u));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_crc8_of_known_vector);
    RUN_TEST(test_crc8_detects_single_bit_flip);
    RUN_TEST(test_build_then_parse_round_trips);
    RUN_TEST(test_parse_rejects_bad_preamble);
    RUN_TEST(test_parse_rejects_bad_crc);
    RUN_TEST(test_parse_rejects_reserved_tag_ids);
    RUN_TEST(test_elapsed_ms_survives_rollover);
    RUN_TEST(test_seq_gap_wraps);
    RUN_TEST(test_adc_conversion_endpoints);
    RUN_TEST(test_rssi_quality_clamps);

    RUN_TEST(test_state_happy_path);
    RUN_TEST(test_fault_is_sticky_until_reset);
    RUN_TEST(test_unknown_event_does_not_change_state);

    RUN_TEST(test_tracker_adds_and_finds_tag);
    RUN_TEST(test_tracker_counts_missed_packets);
    RUN_TEST(test_tracker_ignores_duplicate_sequence);
    RUN_TEST(test_tracker_prunes_stale_entries);
    RUN_TEST(test_tracker_rssi_average);

    RUN_TEST(test_hw_init_sets_expected_control_bits);
    RUN_TEST(test_set_channel_rejects_out_of_range);
    RUN_TEST(test_fifo_not_ready_when_fault_set);
    RUN_TEST(test_read_adc_masks_to_12_bits);
    RUN_TEST(test_leds_reflect_state);

    RUN_TEST(test_poll_reports_fault_from_status_register);
    RUN_TEST(test_poll_returns_none_when_fifo_empty);

    return UNITY_END();
}
