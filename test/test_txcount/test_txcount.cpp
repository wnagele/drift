#include <unity.h>

#include "txcount.h"

void setUp() {
    txcount_init(true, true, true);
}

static TxRates rates() {
    TxRates r;
    txcount_rates(&r);
    return r;
}

void test_bt4_counts_frames_and_messages() {
    txcount_bt4();
    txcount_bt4();
    txcount_bt4();
    txcount_sample(1000);
    TxRates r = rates();
    TEST_ASSERT_EQUAL_UINT32(3, r.bt4_frames);
    TEST_ASSERT_EQUAL_UINT32(3, r.bt4_messages);
}

void test_pack_len_derives_message_count() {
    txcount_bt5(3 + 25 * 3);     // N = 3
    txcount_beacon(3 + 25 * 5);  // N = 5
    txcount_nan(3 + 25 * 3);
    txcount_sample(1000);
    TxRates r = rates();
    TEST_ASSERT_EQUAL_UINT32(1, r.bt5_frames);
    TEST_ASSERT_EQUAL_UINT32(3, r.bt5_messages);
    TEST_ASSERT_EQUAL_UINT32(1, r.wifi_beacon_frames);
    TEST_ASSERT_EQUAL_UINT32(5, r.wifi_beacon_messages);
    // Only action frames count (the sync beacon carries no ODID data), so
    // every transport's frames:messages is 1:N.
    TEST_ASSERT_EQUAL_UINT32(1, r.wifi_nan_frames);
    TEST_ASSERT_EQUAL_UINT32(3, r.wifi_nan_messages);
}

void test_degenerate_pack_counts_frame_without_messages() {
    // A header-only pack cannot occur (the builder rejects empty packs) but
    // must not corrupt the counts if it ever did.
    txcount_bt5(3);
    txcount_sample(1000);
    TxRates r = rates();
    TEST_ASSERT_EQUAL_UINT32(1, r.bt5_frames);
    TEST_ASSERT_EQUAL_UINT32(0, r.bt5_messages);
}

void test_disabled_transports_do_not_count() {
    txcount_init(false, false, false);
    txcount_bt4();  // BT4 has no config gate: counts unconditionally
    txcount_bt5(78);
    txcount_beacon(78);
    txcount_nan(78);
    txcount_sample(1000);
    TxRates r = rates();
    TEST_ASSERT_EQUAL_UINT32(1, r.bt4_frames);
    TEST_ASSERT_EQUAL_UINT32(1, r.bt4_messages);
    TEST_ASSERT_EQUAL_UINT32(0, r.bt5_frames);
    TEST_ASSERT_EQUAL_UINT32(0, r.bt5_messages);
    TEST_ASSERT_EQUAL_UINT32(0, r.wifi_beacon_frames);
    TEST_ASSERT_EQUAL_UINT32(0, r.wifi_beacon_messages);
    TEST_ASSERT_EQUAL_UINT32(0, r.wifi_nan_frames);
    TEST_ASSERT_EQUAL_UINT32(0, r.wifi_nan_messages);
}

void test_window_closes_only_when_due() {
    txcount_bt4();
    txcount_sample(999);
    TxRates r = rates();
    TEST_ASSERT_EQUAL_UINT32(0, r.bt4_frames);
    txcount_sample(1000);
    r = rates();
    TEST_ASSERT_EQUAL_UINT32(1, r.bt4_frames);
}

void test_rates_normalize_window_length() {
    // 20 frames over a 2 s window: the window drifted, the rate did not.
    for (int i = 0; i < 20; i++)
        txcount_bt4();
    txcount_sample(2000);
    TxRates r = rates();
    TEST_ASSERT_EQUAL_UINT32(10, r.bt4_frames);
}

void test_windows_do_not_overlap() {
    for (int i = 0; i < 10; i++)
        txcount_bt4();
    txcount_sample(1000);
    txcount_sample(2000);  // nothing sent in the second window
    TxRates r = rates();
    TEST_ASSERT_EQUAL_UINT32(0, r.bt4_frames);
    TEST_ASSERT_EQUAL_UINT32(0, r.bt4_messages);
}

void test_rates_survive_millis_wraparound() {
    // Per the test_dri convention: the now values wrap past 2^32 on the
    // 32-bit target while staying coherent on 64-bit hosts, so the
    // subtraction holds everywhere and exercises the wrap on device.
    // 1024 ms must elapse (a full window) across the wrap.
    txcount_sample(0xFFFFFF00UL);
    for (int i = 0; i < 51; i++)
        txcount_bt4();
    txcount_sample(0xFFFFFF00UL + 1024UL);  // wraps to 0x300 on target
    TxRates r = rates();
    // 51 frames in 1024 ms = 49.8/s, rounds to 50 - pins the wrap arithmetic.
    TEST_ASSERT_EQUAL_UINT32(50, r.bt4_frames);
    TEST_ASSERT_EQUAL_UINT32(50, r.bt4_messages);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_bt4_counts_frames_and_messages);
    RUN_TEST(test_pack_len_derives_message_count);
    RUN_TEST(test_degenerate_pack_counts_frame_without_messages);
    RUN_TEST(test_disabled_transports_do_not_count);
    RUN_TEST(test_window_closes_only_when_due);
    RUN_TEST(test_rates_normalize_window_length);
    RUN_TEST(test_windows_do_not_overlap);
    RUN_TEST(test_rates_survive_millis_wraparound);
    return UNITY_END();
}
