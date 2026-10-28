#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define QMI8658_ADDRESS                   0x6B
#define QMI8658_WHO_AM_I_REG              0x00
#define QMI8658_REVISION_REG              0x01
#define QMI8658_CTRL1_REG                 0x02
#define QMI8658_CTRL2_REG                 0x03
#define QMI8658_CTRL5_REG                 0x06
#define QMI8658_CTRL7_REG                 0x08
#define QMI8658_CTRL8_REG                 0x09
#define QMI8658_CTRL9_REG                 0x0A
#define QMI8658_CAL1_L_REG                0x0B
#define QMI8658_CAL1_H_REG                0x0C
#define QMI8658_STATUSINT_REG              0x2D
#define QMI8658_STATUS0_REG                0x2E
#define QMI8658_STATUS1_REG               0x2F
#define QMI8658_AX_L_REG                   0x35
#define QMI8658_RESET_REG                  0x60
#define QMI8658_CTRL1_AUTO_INCREMENT_INT1 0x48
#define QMI8658_CTRL2_ACC_2G_21HZ         0x0D
#define QMI8658_CTRL7_ACC_ENABLE          0x01
#define QMI8658_CTRL9_WRITE_WOM_SETTINGS  0x08
#define QMI8658_STATUS1_WOM               BIT(2)
#define QMI8658_STATUSINT_CMD_DONE         BIT(7)
#define QMI8658_WOM_THRESHOLD_MG          100
#define QMI8658_WOM_BLANKING_SAMPLES      8
#define QMI8658_INT1_GPIO                 GPIO_NUM_21
#define QMI8658_INT1_SETTLE_TIMEOUT_MS    2000
#define QMI8658_CTRL9_TIMEOUT_MS           1000
#define QMI8658_BASELINE_STATIONARY_MS     10000
#define QMI8658_SINGLE_MOTION_TIMEOUT_MS   10000
#define QMI8658_POST_MOTION_OBSERVE_MS     10000
#define QMI8658_FINAL_QUIET_MS             10000
#define QMI8658_MAX_POST_MOTION_EVENTS     512
#define QMI8658_MAX_NON_WOM_DIAGNOSTICS      8
#define QMI8658_ACCEL_LSB_PER_G            16384
#define QMI8658_PHASE_COUNT                4
#define QMI8658_ACCEL_ODR_HZ               21
#define QMI8658_NORMAL_ACCEL_ODR_HZ        125
#define QMI8658_MIN_SAMPLING_COVERAGE_PCT  90
#define QMI8658_ACCEL_CHARACTERIZATION_CHECKPOINT 0

static const char *TAG = "qmi8658_wom";

static void qmi8658_write(i2c_master_dev_handle_t qmi, uint8_t reg, uint8_t value)
{
    ESP_ERROR_CHECK(i2c_master_transmit(qmi, (uint8_t[]) {reg, value}, 2, -1));
}

static uint8_t qmi8658_read(i2c_master_dev_handle_t qmi, uint8_t reg)
{
    uint8_t value;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(qmi, &reg, 1, &value, 1, -1));
    return value;
}

#if QMI8658_ACCEL_CHARACTERIZATION_CHECKPOINT
static void qmi8658_read_acceleration(i2c_master_dev_handle_t qmi, int32_t acceleration_mg[3])
{
    uint8_t reg = QMI8658_AX_L_REG;
    uint8_t data[6];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(qmi, &reg, 1, data, sizeof(data), -1));
    for (int axis = 0; axis < 3; axis++) {
        const int16_t raw = (int16_t)((uint16_t)data[axis * 2] | ((uint16_t)data[axis * 2 + 1] << 8));
        acceleration_mg[axis] = ((int32_t)raw * 1000) / QMI8658_ACCEL_LSB_PER_G;
    }
}

static uint32_t vector_magnitude(const int32_t vector[3])
{
    const double squared = (double)vector[0] * vector[0] + (double)vector[1] * vector[1] +
                           (double)vector[2] * vector[2];
    return (uint32_t)sqrt(squared);
}
#endif

#if !QMI8658_ACCEL_CHARACTERIZATION_CHECKPOINT
#if 0
typedef struct {
    const char *name;
    const char *instruction;
    uint32_t duration_ms;
} phase_definition_t;

typedef struct {
    uint32_t samples;
    uint32_t wom_events;
    uint32_t elapsed_ms;
    uint32_t expected_samples;
    int32_t min[3];
    int32_t max[3];
    double sum[3];
    double sum_squared[3];
    uint32_t max_delta[3];
    uint32_t delta_over_threshold[3];
    uint32_t any_delta_over_threshold;
    uint32_t max_magnitude;
    uint32_t max_delta_magnitude;
    int32_t previous[3];
    bool has_previous;
} phase_stats_t;

static const phase_definition_t phases[QMI8658_PHASE_COUNT] __attribute__((unused)) = {
    {"STATIONARY", "Place the watch flat on a stable surface. Do not touch or move it during measurement.", 20000},
    {"WRIST RAISE", "Hold or wear it relaxed. Raise and rotate naturally to read it, hold one second, return. Repeat exactly five times; do not shake.", 30000},
    {"SLOW ROTATION", "Hold it securely. Slowly rotate around all three axes; keep motion smooth and avoid shaking or rapid translation.", 15000},
    {"SHAKE", "Hold it securely. Shake moderately and repeatably; avoid impacts or excessive force.", 10000},
};

static void phase_stats_reset(phase_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
    for (int axis = 0; axis < 3; axis++) {
        stats->min[axis] = INT32_MAX;
        stats->max[axis] = INT32_MIN;
    }
}

static void phase_stats_add_sample(phase_stats_t *stats, const int32_t acceleration_mg[3])
{
    int32_t delta[3] = {0};
    bool any_delta_over_threshold = false;

    for (int axis = 0; axis < 3; axis++) {
        const int32_t value = acceleration_mg[axis];
        if (value < stats->min[axis]) stats->min[axis] = value;
        if (value > stats->max[axis]) stats->max[axis] = value;
        stats->sum[axis] += value;
        stats->sum_squared[axis] += (double)value * value;
        if (stats->has_previous) {
            delta[axis] = value - stats->previous[axis];
            const uint32_t absolute_delta = delta[axis] < 0 ? (uint32_t)-delta[axis] : (uint32_t)delta[axis];
            if (absolute_delta > stats->max_delta[axis]) stats->max_delta[axis] = absolute_delta;
            if (absolute_delta > QMI8658_WOM_THRESHOLD_MG) {
                stats->delta_over_threshold[axis]++;
                any_delta_over_threshold = true;
            }
        }
        stats->previous[axis] = value;
    }
    if (stats->has_previous && any_delta_over_threshold) stats->any_delta_over_threshold++;
    if (stats->has_previous) {
        const uint32_t delta_magnitude = vector_magnitude(delta);
        if (delta_magnitude > stats->max_delta_magnitude) stats->max_delta_magnitude = delta_magnitude;
    }
    const uint32_t magnitude = vector_magnitude(acceleration_mg);
    if (magnitude > stats->max_magnitude) stats->max_magnitude = magnitude;
    stats->has_previous = true;
    stats->samples++;
}

static void qmi8658_service_wom(i2c_master_dev_handle_t qmi, phase_stats_t *stats)
{
    if (gpio_get_level(QMI8658_INT1_GPIO) != 0) {
        const uint8_t status1 = qmi8658_read(qmi, QMI8658_STATUS1_REG);
        if (stats != NULL && (status1 & QMI8658_STATUS1_WOM)) {
            stats->wom_events++;
        }
    }
}

static void phase_countdown(i2c_master_dev_handle_t qmi, const phase_definition_t *phase)
{
    ESP_LOGI(TAG, "%s: %s", phase->name, phase->instruction);
    for (int seconds = 5; seconds > 0; seconds--) {
        ESP_LOGI(TAG, "%s starts in %d", phase->name, seconds);
        for (int elapsed_ms = 0; elapsed_ms < 1000; elapsed_ms += 20) {
            qmi8658_service_wom(qmi, NULL);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

static void phase_print_summary(const phase_definition_t *phase, const phase_stats_t *stats)
{
    double mean[3] = {0};
    double stddev[3] = {0};
    for (int axis = 0; axis < 3; axis++) {
        mean[axis] = stats->samples ? stats->sum[axis] / stats->samples : 0;
        const double variance = stats->samples ? stats->sum_squared[axis] / stats->samples - mean[axis] * mean[axis] : 0;
        stddev[axis] = sqrt(variance > 0 ? variance : 0);
    }
    const uint32_t coverage_pct = stats->expected_samples ? (stats->samples * 100) / stats->expected_samples : 0;
    const bool valid = stats->expected_samples > 0 && coverage_pct >= QMI8658_MIN_SAMPLING_COVERAGE_PCT;
    ESP_LOGI(TAG, "%s summary: elapsed=%" PRIu32 "ms expected=%" PRIu32 " actual=%" PRIu32
             " coverage=%" PRIu32 "%% %s WoM=%" PRIu32,
             phase->name, stats->elapsed_ms, stats->expected_samples, stats->samples, coverage_pct,
             valid ? "VALID" : "INVALID", stats->wom_events);
    if (!valid) {
        ESP_LOGE(TAG, "%s statistics are not trustworthy: sampling coverage is below %d%%",
                 phase->name, QMI8658_MIN_SAMPLING_COVERAGE_PCT);
        return;
    }
    ESP_LOGI(TAG, "axis  min  max  mean  stddev  max|delta|  |delta|>100mg");
    ESP_LOGI(TAG, "X %" PRId32 " %" PRId32 " %.1f %.1f %" PRIu32 " %" PRIu32,
             stats->min[0], stats->max[0], mean[0], stddev[0], stats->max_delta[0], stats->delta_over_threshold[0]);
    ESP_LOGI(TAG, "Y %" PRId32 " %" PRId32 " %.1f %.1f %" PRIu32 " %" PRIu32,
             stats->min[1], stats->max[1], mean[1], stddev[1], stats->max_delta[1], stats->delta_over_threshold[1]);
    ESP_LOGI(TAG, "Z %" PRId32 " %" PRId32 " %.1f %.1f %" PRIu32 " %" PRIu32,
             stats->min[2], stats->max[2], mean[2], stddev[2], stats->max_delta[2], stats->delta_over_threshold[2]);
    ESP_LOGI(TAG, "any |delta|>100mg=%" PRIu32 " max|a|=%" PRIu32 "mg max|delta-vector|=%" PRIu32 "mg",
             stats->any_delta_over_threshold, stats->max_magnitude, stats->max_delta_magnitude);
}

static uint32_t phase_max_axis_delta(const phase_stats_t *stats)
{
    uint32_t maximum = stats->max_delta[0];
    for (int axis = 1; axis < 3; axis++) {
        if (stats->max_delta[axis] > maximum) maximum = stats->max_delta[axis];
    }
    return maximum;
}

static uint32_t phase_max_stddev(const phase_stats_t *stats)
{
    double maximum = 0;
    for (int axis = 0; axis < 3; axis++) {
        const double mean = stats->samples ? stats->sum[axis] / stats->samples : 0;
        const double variance = stats->samples ? stats->sum_squared[axis] / stats->samples - mean * mean : 0;
        const double stddev = sqrt(variance > 0 ? variance : 0);
        if (stddev > maximum) maximum = stddev;
    }
    return (uint32_t)maximum;
}

static uint32_t phase_coverage_pct(const phase_stats_t *stats)
{
    return stats->expected_samples ? (stats->samples * 100) / stats->expected_samples : 0;
}

static uint32_t phase_wom_per_second(const phase_stats_t *stats)
{
    return stats->elapsed_ms ? (stats->wom_events * 1000) / stats->elapsed_ms : 0;
}

static uint32_t phase_any_delta_pct(const phase_stats_t *stats)
{
    return stats->samples > 1 ? (stats->any_delta_over_threshold * 100) / (stats->samples - 1) : 0;
}
#endif

static bool qmi8658_configure_wom(i2c_master_dev_handle_t qmi, uint32_t *cmd_done_events)
{
    qmi8658_write(qmi, QMI8658_CTRL7_REG, 0x00);
    qmi8658_write(qmi, QMI8658_CTRL1_REG, QMI8658_CTRL1_AUTO_INCREMENT_INT1);
    qmi8658_write(qmi, QMI8658_CTRL2_REG, QMI8658_CTRL2_ACC_2G_21HZ);
    qmi8658_write(qmi, QMI8658_CAL1_L_REG, QMI8658_WOM_THRESHOLD_MG);
    qmi8658_write(qmi, QMI8658_CAL1_H_REG, QMI8658_WOM_BLANKING_SAMPLES);
    qmi8658_write(qmi, QMI8658_CTRL9_REG, QMI8658_CTRL9_WRITE_WOM_SETTINGS);

    for (int elapsed_ms = 0; elapsed_ms < QMI8658_CTRL9_TIMEOUT_MS; elapsed_ms += 10) {
        const uint8_t statusint = qmi8658_read(qmi, QMI8658_STATUSINT_REG);
        if (statusint & QMI8658_STATUSINT_CMD_DONE) {
            (*cmd_done_events)++;
            ESP_LOGI(TAG, "CTRL9 command-completion: STATUSINT=0x%02X CmdDone=set", statusint);
            const uint8_t status1 = qmi8658_read(qmi, QMI8658_STATUS1_REG);
            ESP_LOGI(TAG, "STATUS1 after Ctrl9 completion=0x%02X (WoM=%s; CmdDone is STATUSINT.bit7)",
                     status1, (status1 & QMI8658_STATUS1_WOM) ? "set" : "not set");
            qmi8658_write(qmi, QMI8658_CTRL9_REG, 0x00);
            for (int ack_elapsed_ms = 0; ack_elapsed_ms < QMI8658_CTRL9_TIMEOUT_MS; ack_elapsed_ms += 10) {
                if (!(qmi8658_read(qmi, QMI8658_STATUSINT_REG) & QMI8658_STATUSINT_CMD_DONE)) {
                    const int int1_level = gpio_get_level(QMI8658_INT1_GPIO);
                    ESP_LOGI(TAG, "CTRL9 acknowledgement complete; GPIO21=%d", int1_level);
                    if (int1_level == 0) {
                        return true;
                    }
                    ESP_LOGE(TAG, "CTRL9 acknowledgement failed: GPIO21 did not return LOW");
                    return false;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            ESP_LOGE(TAG, "CTRL9 acknowledgement timed out");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGE(TAG, "CTRL9 command completion timed out");
    return false;
}

static bool qmi8658_wait_for_int1_low(void)
{
    for (int elapsed_ms = 0; elapsed_ms < QMI8658_INT1_SETTLE_TIMEOUT_MS; elapsed_ms += 20) {
        if (gpio_get_level(QMI8658_INT1_GPIO) == 0) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return false;
}

typedef struct {
    uint32_t int1_assertions;
    uint32_t wom_events;
    uint32_t non_wom_assertions;
    uint32_t acknowledged_events;
    uint32_t idle_failures;
    uint32_t wom_cleared_immediately;
    uint32_t wom_reasserted_within_20ms;
    uint32_t second_status1_revealed_wom;
    uint32_t later_status1_revealed_wom;
    uint32_t high_wom_clear_over_20ms;
    uint32_t secondary_high_cleared_spontaneously;
    uint32_t secondary_high_exceeded_full_timeout;
    uint32_t max_secondary_high_duration_us;
    uint32_t cleared_by_status1_read;
    uint32_t cleared_by_second_status1_read;
    uint32_t cleared_by_time_alone;
    uint32_t not_cleared_within_timeout;
    int final_gpio_level;
    uint8_t final_status1;
    bool last_ack_returned_low;
    bool non_wom_line_high_active;
    bool valid;
} wom_test_stats_t;

static uint32_t post_motion_timestamps[QMI8658_MAX_POST_MOTION_EVENTS];

#define QMI8658_INT1_TIMING_WINDOW_US 20000

static const uint32_t qmi8658_timing_offsets_us[] = {100, 500, 1000, 2000, 5000, 10000, 20000};

static void qmi8658_wait_until_us(int64_t target_us)
{
    while (esp_timer_get_time() < target_us) {
        const int64_t remaining_us = target_us - esp_timer_get_time();
        esp_rom_delay_us((uint32_t)(remaining_us > 100 ? 100 : remaining_us));
    }
}

static void qmi8658_trace_high_wom_clear(i2c_master_dev_handle_t qmi, wom_test_stats_t *stats,
                                          int64_t assertion_us, uint32_t requested_offset_us,
                                          uint8_t first_status1, int gpio_after_first_status1,
                                          uint8_t first_statusint, bool record_occurrence,
                                          bool log_diagnostic)
{
    const uint8_t second_status1 = qmi8658_read(qmi, QMI8658_STATUS1_REG);
    const int gpio_after_second_status1 = gpio_get_level(QMI8658_INT1_GPIO);
    const uint8_t second_statusint = qmi8658_read(qmi, QMI8658_STATUSINT_REG);
    const int gpio_after_second_statusint = gpio_get_level(QMI8658_INT1_GPIO);
    const int64_t actual_offset_us = esp_timer_get_time() - assertion_us;

    if (record_occurrence) {
        stats->non_wom_assertions++;
        if (second_status1 & QMI8658_STATUS1_WOM) {
            stats->second_status1_revealed_wom++;
        }
        if (gpio_after_first_status1 == 0) {
            stats->cleared_by_status1_read++;
        } else if (gpio_after_second_status1 == 0) {
            stats->cleared_by_second_status1_read++;
        }
    }
    if (log_diagnostic) {
        ESP_LOGW(TAG, "INT1 timing anomaly: requested +%" PRIu32 "us actual +%" PRId64
                 "us STATUS1=0x%02X GPIO-after-STATUS1=%d STATUSINT=0x%02X second-STATUS1=0x%02X "
                 "GPIO-after-second=%d second-STATUSINT=0x%02X GPIO-after-STATUSINT=%d",
                 requested_offset_us, actual_offset_us, first_status1, gpio_after_first_status1,
                 first_statusint, second_status1, gpio_after_second_status1, second_statusint,
                 gpio_after_second_statusint);
    }
}

static bool qmi8658_trace_int1_timing(i2c_master_dev_handle_t qmi, wom_test_stats_t *stats,
                                       int64_t assertion_us, int gpio_after_first_status1,
                                       bool primary_wom)
{
    bool reasserted = false;
    bool high_wom_clear_at_20ms = false;
    bool high_wom_clear_seen = false;
    int gpio_at_end = gpio_after_first_status1;

    if (primary_wom && gpio_after_first_status1 == 0) {
        stats->wom_cleared_immediately++;
    }
    for (size_t index = 0; index < sizeof(qmi8658_timing_offsets_us) / sizeof(qmi8658_timing_offsets_us[0]); index++) {
        const uint32_t requested_offset_us = qmi8658_timing_offsets_us[index];
        qmi8658_wait_until_us(assertion_us + requested_offset_us);
        const int gpio_before_status1 = gpio_get_level(QMI8658_INT1_GPIO);
        const uint8_t status1 = qmi8658_read(qmi, QMI8658_STATUS1_REG);
        const int gpio_after_status1 = gpio_get_level(QMI8658_INT1_GPIO);
        const uint8_t statusint = qmi8658_read(qmi, QMI8658_STATUSINT_REG);
        gpio_at_end = gpio_get_level(QMI8658_INT1_GPIO);

        if (gpio_before_status1 != 0) {
            reasserted = true;
            if (status1 & QMI8658_STATUS1_WOM) {
                stats->later_status1_revealed_wom++;
            } else {
                qmi8658_trace_high_wom_clear(qmi, stats, assertion_us, requested_offset_us,
                                              status1, gpio_after_status1, statusint,
                                              primary_wom && !high_wom_clear_seen,
                                              !high_wom_clear_seen);
                high_wom_clear_seen = true;
                gpio_at_end = gpio_get_level(QMI8658_INT1_GPIO);
                if (requested_offset_us == QMI8658_INT1_TIMING_WINDOW_US && gpio_at_end != 0) {
                    high_wom_clear_at_20ms = true;
                }
            }
        }
    }
    if (primary_wom && reasserted) {
        stats->wom_reasserted_within_20ms++;
    }
    if (high_wom_clear_at_20ms) {
        stats->high_wom_clear_over_20ms++;
    }
    return gpio_at_end == 0;
}

static void wom_test_stats_reset(wom_test_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->final_gpio_level = -1;
    stats->last_ack_returned_low = true;
    stats->valid = true;
}

static bool qmi8658_consume_int1(i2c_master_dev_handle_t qmi, wom_test_stats_t *stats,
                                  uint8_t *status1_out, int *int1_before_out, int *int1_after_out)
{
    const int int1_before = gpio_get_level(QMI8658_INT1_GPIO);
    if (int1_before == 0) {
        stats->non_wom_line_high_active = false;
        return false;
    }

    if (stats->non_wom_line_high_active) {
        return false;
    }

    const int64_t assertion_timestamp_us = esp_timer_get_time();
    const uint8_t status1 = qmi8658_read(qmi, QMI8658_STATUS1_REG);
    const int gpio_after_status1 = gpio_get_level(QMI8658_INT1_GPIO);
    const uint8_t statusint = qmi8658_read(qmi, QMI8658_STATUSINT_REG);
    stats->final_status1 = status1;
    stats->int1_assertions++;
    if (status1 & QMI8658_STATUS1_WOM) {
        bool returned_idle = qmi8658_trace_int1_timing(qmi, stats, assertion_timestamp_us,
                                                        gpio_after_status1, true);
        if (!returned_idle) {
            returned_idle = qmi8658_wait_for_int1_low();
            if (returned_idle) {
                stats->cleared_by_time_alone++;
            } else {
                stats->not_cleared_within_timeout++;
            }
        }
        stats->wom_events++;
        if (returned_idle) {
            stats->acknowledged_events++;
            stats->last_ack_returned_low = true;
        } else {
            stats->idle_failures++;
            stats->last_ack_returned_low = false;
        }
    } else {
        qmi8658_trace_high_wom_clear(qmi, stats, assertion_timestamp_us, 0, status1,
                                      gpio_after_status1, statusint, true, true);
        const int gpio_after_second_status1 = gpio_get_level(QMI8658_INT1_GPIO);
        bool returned_idle = qmi8658_trace_int1_timing(qmi, stats, assertion_timestamp_us,
                                                        gpio_after_status1, false);
        if (!returned_idle) {
            returned_idle = qmi8658_wait_for_int1_low();
        }
        if (returned_idle) {
            if (gpio_after_second_status1 != 0) {
                stats->cleared_by_time_alone++;
                stats->secondary_high_cleared_spontaneously++;
                const uint32_t duration_us = (uint32_t)(esp_timer_get_time() - assertion_timestamp_us);
                if (duration_us > stats->max_secondary_high_duration_us) {
                    stats->max_secondary_high_duration_us = duration_us;
                }
            }
        } else {
            stats->not_cleared_within_timeout++;
            stats->secondary_high_exceeded_full_timeout++;
            const uint32_t duration_us = (uint32_t)(esp_timer_get_time() - assertion_timestamp_us);
            if (duration_us > stats->max_secondary_high_duration_us) {
                stats->max_secondary_high_duration_us = duration_us;
            }
            stats->non_wom_line_high_active = true;
        }
    }
    if (status1_out != NULL) *status1_out = status1;
    if (int1_before_out != NULL) *int1_before_out = int1_before;
    if (int1_after_out != NULL) *int1_after_out = gpio_get_level(QMI8658_INT1_GPIO);
    return true;
}

static void qmi8658_finalize_measurement(i2c_master_dev_handle_t qmi, wom_test_stats_t *stats)
{
    uint8_t final_status1;
    if (!qmi8658_consume_int1(qmi, stats, &final_status1, NULL, NULL)) {
        final_status1 = qmi8658_read(qmi, QMI8658_STATUS1_REG);
    }
    stats->final_status1 = final_status1;
    stats->final_gpio_level = gpio_get_level(QMI8658_INT1_GPIO);
    if (stats->final_gpio_level != 0) {
        stats->valid = false;
    }
}

static void qmi8658_measure_stationary(i2c_master_dev_handle_t qmi, uint32_t duration_ms,
                                        wom_test_stats_t *stats)
{
    const TickType_t start = xTaskGetTickCount();
    while (xTaskGetTickCount() - start < pdMS_TO_TICKS(duration_ms)) {
        qmi8658_consume_int1(qmi, stats, NULL, NULL, NULL);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    qmi8658_finalize_measurement(qmi, stats);
}

static void qmi8658_countdown(i2c_master_dev_handle_t qmi, const char *instruction, int seconds)
{
    wom_test_stats_t discarded_events;

    wom_test_stats_reset(&discarded_events);
    ESP_LOGI(TAG, "%s", instruction);
    for (int remaining = seconds; remaining > 0; remaining--) {
        ESP_LOGI(TAG, "Measurement starts in %d", remaining);
        for (int elapsed_ms = 0; elapsed_ms < 1000; elapsed_ms += 20) {
            qmi8658_consume_int1(qmi, &discarded_events, NULL, NULL, NULL);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

static bool wom_test_is_quiet(const wom_test_stats_t *stats)
{
    return stats->valid && stats->wom_events == 0 && stats->idle_failures == 0 &&
           stats->final_gpio_level == 0;
}

static void wom_test_print_row(const char *name, const wom_test_stats_t *stats, int64_t last_event_ms,
                               bool passed)
{
    if (last_event_ms >= 0) {
        ESP_LOGI(TAG, "%s | %" PRIu32 " | %" PRId64 " ms | %s | %s", name, stats->wom_events,
                 last_event_ms, stats->final_gpio_level == 0 ? "yes" : "no", passed ? "PASS" : "FAIL");
    } else {
        ESP_LOGI(TAG, "%s | %" PRIu32 " | none | %s | %s", name, stats->wom_events,
                 stats->final_gpio_level == 0 ? "yes" : "no", passed ? "PASS" : "FAIL");
    }
}

static void wom_test_print_details(const char *name, const wom_test_stats_t *stats)
{
    ESP_LOGI(TAG, "%s details: WoM=%" PRIu32 " non-WoM GPIO21=%" PRIu32 " ack failures=%" PRIu32
             " idle-return failures=%" PRIu32 " final GPIO21=%d final STATUS1=0x%02X "
             "secondary-high=%" PRIu32 " spontaneous-clears=%" PRIu32 " max-secondary-us=%" PRIu32
             " high-clear>20ms=%" PRIu32 " high-full-timeout=%" PRIu32,
             name, stats->wom_events, stats->non_wom_assertions, stats->idle_failures,
             stats->idle_failures, stats->final_gpio_level, stats->final_status1,
             stats->non_wom_assertions, stats->secondary_high_cleared_spontaneously,
             stats->max_secondary_high_duration_us, stats->high_wom_clear_over_20ms,
             stats->secondary_high_exceeded_full_timeout);
}

static const char *qmi8658_print_interrupt_timing_summary(const wom_test_stats_t *baseline,
                                                           const wom_test_stats_t *intentional_motion,
                                                           const wom_test_stats_t *post_motion,
                                                           const wom_test_stats_t *final_quiet)
{
    const wom_test_stats_t *phases[] = {baseline, intentional_motion, post_motion, final_quiet};
    uint32_t immediate_clears = 0;
    uint32_t reassertions = 0;
    uint32_t high_wom_clear = 0;
    uint32_t second_read_wom = 0;
    uint32_t later_read_wom = 0;
    uint32_t exceeded_20ms = 0;
    uint32_t secondary_cleared = 0;
    uint32_t exceeded_full_timeout = 0;
    uint32_t max_secondary_high_duration_us = 0;
    uint32_t cleared_status1 = 0;
    uint32_t cleared_second_status1 = 0;
    uint32_t cleared_time = 0;
    uint32_t not_cleared = 0;

    for (size_t index = 0; index < sizeof(phases) / sizeof(phases[0]); index++) {
        immediate_clears += phases[index]->wom_cleared_immediately;
        reassertions += phases[index]->wom_reasserted_within_20ms;
        high_wom_clear += phases[index]->non_wom_assertions;
        second_read_wom += phases[index]->second_status1_revealed_wom;
        later_read_wom += phases[index]->later_status1_revealed_wom;
        exceeded_20ms += phases[index]->high_wom_clear_over_20ms;
        secondary_cleared += phases[index]->secondary_high_cleared_spontaneously;
        exceeded_full_timeout += phases[index]->secondary_high_exceeded_full_timeout;
        if (phases[index]->max_secondary_high_duration_us > max_secondary_high_duration_us) {
            max_secondary_high_duration_us = phases[index]->max_secondary_high_duration_us;
        }
        cleared_status1 += phases[index]->cleared_by_status1_read;
        cleared_second_status1 += phases[index]->cleared_by_second_status1_read;
        cleared_time += phases[index]->cleared_by_time_alone;
        not_cleared += phases[index]->not_cleared_within_timeout;
    }
    const char *classification = "UNRESOLVED";
    if (exceeded_full_timeout > 0) classification = "PERSISTENT_LINE_LATCH";
    else if (second_read_wom > 0 || later_read_wom > 0) classification = "RAPID_WOM_REASSERTION";
    else if (high_wom_clear > 0 && secondary_cleared > 0) classification = "INT1_TOGGLE_EDGE_CASE";

    ESP_LOGI(TAG, "INT1 TIMING SUMMARY: immediate WoM clears=%" PRIu32 " WoM reassertions<20ms=%" PRIu32
             " HIGH-with-WoM-clear=%" PRIu32 " second STATUS1 reveals WoM=%" PRIu32
             " later STATUS1 reveals WoM=%" PRIu32,
             immediate_clears, reassertions, high_wom_clear, second_read_wom, later_read_wom);
    ESP_LOGI(TAG, "Secondary INT1 HIGH: occurrences=%" PRIu32 " spontaneous-clears=%" PRIu32
             " max-duration=%" PRIu32 "us exceeded-20ms=%" PRIu32 " exceeded-full-timeout=%" PRIu32,
             high_wom_clear, secondary_cleared, max_secondary_high_duration_us,
             exceeded_20ms, exceeded_full_timeout);
    ESP_LOGI(TAG, "INT1 clear method: STATUS1=%" PRIu32 " second STATUS1=%" PRIu32
             " time alone=%" PRIu32 " not within timeout=%" PRIu32 " classification=%s",
             cleared_status1, cleared_second_status1, cleared_time, not_cleared, classification);
    return classification;
}

static void qmi8658_print_post_motion_metrics(const uint32_t timestamps[], uint32_t count)
{
    uint32_t buckets[8] = {0};
    uint64_t interval_sum = 0;
    uint32_t min_interval = UINT32_MAX;
    uint32_t max_interval = 0;

    for (uint32_t index = 0; index < count; index++) {
        const uint32_t timestamp = timestamps[index];
        if (timestamp < 250) buckets[0]++;
        else if (timestamp < 500) buckets[1]++;
        else if (timestamp < 1000) buckets[2]++;
        else if (timestamp < 2000) buckets[3]++;
        else if (timestamp < 3000) buckets[4]++;
        else if (timestamp < 5000) buckets[5]++;
        else if (timestamp < 7000) buckets[6]++;
        else buckets[7]++;
        const uint32_t previous_timestamp = index == 0 ? 0 : timestamps[index - 1];
        const uint32_t interval = timestamp - previous_timestamp;
        interval_sum += interval;
        if (interval < min_interval) min_interval = interval;
        if (interval > max_interval) max_interval = interval;
    }
    if (count > 0) {
        ESP_LOGI(TAG, "Post-motion metrics: subsequent=%" PRIu32 " first=%" PRIu32 "ms last=%" PRIu32
                 "ms min interval=%" PRIu32 "ms max interval=%" PRIu32 "ms mean interval=%.1fms",
                 count, timestamps[0], timestamps[count - 1], min_interval, max_interval,
                 interval_sum / (double)count);
    } else {
        ESP_LOGI(TAG, "Post-motion metrics: subsequent=0");
    }
    ESP_LOGI(TAG, "Post-motion buckets: 0-250ms=%" PRIu32 " 250-500ms=%" PRIu32 " 500ms-1s=%" PRIu32
             " 1-2s=%" PRIu32 " 2-3s=%" PRIu32 " 3-5s=%" PRIu32 " 5-7s=%" PRIu32 " 7-10s=%" PRIu32,
             buckets[0], buckets[1], buckets[2], buckets[3], buckets[4], buckets[5], buckets[6], buckets[7]);
}

#endif

#if !QMI8658_ACCEL_CHARACTERIZATION_CHECKPOINT
void app_main(void)
{
    const gpio_config_t int1_config = {
        .pin_bit_mask = BIT64(QMI8658_INT1_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const i2c_device_config_t qmi_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDRESS,
        .scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ,
    };
    i2c_master_dev_handle_t qmi;
    uint32_t cmd_done_events = 0;

    ESP_ERROR_CHECK(gpio_config(&int1_config));
    ESP_LOGI(TAG, "GPIO21 before WoM: %d", gpio_get_level(QMI8658_INT1_GPIO));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bsp_i2c_get_handle(), &qmi_config, &qmi));
    ESP_LOGI(TAG, "QMI8658 WHO_AM_I (0x00): 0x%02X", qmi8658_read(qmi, QMI8658_WHO_AM_I_REG));
    ESP_LOGI(TAG, "QMI8658 REVISION (0x01): 0x%02X", qmi8658_read(qmi, QMI8658_REVISION_REG));

    if (!qmi8658_configure_wom(qmi, &cmd_done_events)) {
        return;
    }
    qmi8658_write(qmi, QMI8658_CTRL7_REG, QMI8658_CTRL7_ACC_ENABLE);

    ESP_LOGI(TAG, "WoM readback: CTRL1=0x%02X CTRL2=0x%02X CTRL5=0x%02X CTRL7=0x%02X CTRL8=0x%02X",
             qmi8658_read(qmi, QMI8658_CTRL1_REG), qmi8658_read(qmi, QMI8658_CTRL2_REG),
             qmi8658_read(qmi, QMI8658_CTRL5_REG), qmi8658_read(qmi, QMI8658_CTRL7_REG),
             qmi8658_read(qmi, QMI8658_CTRL8_REG));
    ESP_LOGI(TAG, "WoM encoding: CAL1_L=0x%02X (%u mg absolute-slope threshold); CAL1_H=0x%02X (INT1 initial LOW, blanking=%u samples)",
             qmi8658_read(qmi, QMI8658_CAL1_L_REG), QMI8658_WOM_THRESHOLD_MG,
             qmi8658_read(qmi, QMI8658_CAL1_H_REG), QMI8658_WOM_BLANKING_SAMPLES);
    const uint8_t stale_status1 = qmi8658_read(qmi, QMI8658_STATUS1_REG);
    ESP_LOGI(TAG, "cleared stale STATUS1=0x%02X; waiting for GPIO21 LOW", stale_status1);
    if (!qmi8658_wait_for_int1_low()) {
        ESP_LOGE(TAG, "VALIDATION FAILED: GPIO21 did not settle LOW within %d ms", QMI8658_INT1_SETTLE_TIMEOUT_MS);
        return;
    }
    wom_test_stats_t baseline;
    wom_test_stats_t intentional_motion;
    wom_test_stats_t post_motion;
    wom_test_stats_t final_quiet;
    uint32_t post_motion_timestamp_count = 0;
    bool final_quiet_executed = false;

    ESP_LOGI(TAG, "WoM ready: GPIO21 idle level: 0");
    qmi8658_countdown(qmi, "BASELINE STATIONARY: Place the watch flat on a stable surface and do not touch it.", 5);
    wom_test_stats_reset(&baseline);
    ESP_LOGI(TAG, "BASELINE STATIONARY measurement START");
    qmi8658_measure_stationary(qmi, QMI8658_BASELINE_STATIONARY_MS, &baseline);
    ESP_LOGI(TAG, "BASELINE STATIONARY: WoM=%" PRIu32 " rate=%.2f events/sec final GPIO21=%d",
             baseline.wom_events, baseline.wom_events * 1000.0 / QMI8658_BASELINE_STATIONARY_MS,
             baseline.final_gpio_level);

    qmi8658_countdown(qmi, "SINGLE MOTION: After the countdown, raise the watch once with a natural wrist-reading motion, hold it about one second, then place it flat and do not touch it again.", 5);
    wom_test_stats_reset(&intentional_motion);
    ESP_LOGI(TAG, "SINGLE MOTION measurement START: waiting up to %d seconds", QMI8658_SINGLE_MOTION_TIMEOUT_MS / 1000);
    const int64_t motion_start_us = esp_timer_get_time();
    uint8_t single_status1 = 0;
    int single_int1_before = 0;
    int single_int1_after = 0;
    bool single_asserted = false;
    int64_t first_assertion_ms = -1;
    int64_t wom_confirmation_ms = -1;
    int64_t first_wom_event_us = -1;
    while (esp_timer_get_time() - motion_start_us < (int64_t)QMI8658_SINGLE_MOTION_TIMEOUT_MS * 1000) {
        const int64_t assertion_observed_us = esp_timer_get_time();
        if (gpio_get_level(QMI8658_INT1_GPIO) != 0 &&
            qmi8658_consume_int1(qmi, &intentional_motion, &single_status1,
                                  &single_int1_before, &single_int1_after)) {
            single_asserted = true;
            const int64_t event_time_ms = (assertion_observed_us - motion_start_us) / 1000;
            if (first_assertion_ms < 0) {
                first_assertion_ms = event_time_ms;
            }
            if (single_status1 & QMI8658_STATUS1_WOM) {
                wom_confirmation_ms = event_time_ms;
                first_wom_event_us = assertion_observed_us;
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    const bool single_passed = single_asserted && (single_status1 & QMI8658_STATUS1_WOM) &&
                               single_int1_after == 0 && intentional_motion.idle_failures == 0;
    intentional_motion.final_gpio_level = single_int1_after;
    if (intentional_motion.final_gpio_level != 0) {
        intentional_motion.valid = false;
    }
    ESP_LOGI(TAG, "Intentional motion: GPIO21 first assertion=%" PRId64 "ms STATUS1.WoM confirmation=%" PRId64
             "ms acknowledgement=%s GPIO21 after acknowledgement=%s",
             first_assertion_ms, wom_confirmation_ms,
             intentional_motion.last_ack_returned_low ? "success" : "failure",
             single_int1_after == 0 ? "low" : "high");

    if (!single_passed) {
        intentional_motion.valid = false;
        ESP_LOGE(TAG, "POST-MOTION OBSERVATION SKIPPED: no valid intentional WoM event");
    } else {
        wom_test_stats_reset(&post_motion);
        ESP_LOGI(TAG, "IMMEDIATE POST-MOTION OBSERVATION START: t=0 is the confirmed intentional WoM event; do not touch the watch for 10 seconds");
        while (esp_timer_get_time() - first_wom_event_us < (int64_t)QMI8658_POST_MOTION_OBSERVE_MS * 1000) {
            const uint32_t wom_before = post_motion.wom_events;
            qmi8658_consume_int1(qmi, &post_motion, NULL, NULL, NULL);
            if (post_motion.wom_events != wom_before && post_motion_timestamp_count < QMI8658_MAX_POST_MOTION_EVENTS) {
                post_motion_timestamps[post_motion_timestamp_count++] =
                    (uint32_t)((esp_timer_get_time() - first_wom_event_us) / 1000);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        const uint32_t wom_before_finalize = post_motion.wom_events;
        qmi8658_finalize_measurement(qmi, &post_motion);
        if (post_motion.wom_events != wom_before_finalize &&
            post_motion_timestamp_count < QMI8658_MAX_POST_MOTION_EVENTS) {
            post_motion_timestamps[post_motion_timestamp_count++] =
                (uint32_t)((esp_timer_get_time() - first_wom_event_us) / 1000);
        }
        qmi8658_print_post_motion_metrics(post_motion_timestamps, post_motion_timestamp_count);

        wom_test_stats_reset(&final_quiet);
        ESP_LOGI(TAG, "FINAL QUIET START: continue without touching the watch for 10 seconds");
        final_quiet_executed = true;
        qmi8658_measure_stationary(qmi, QMI8658_FINAL_QUIET_MS, &final_quiet);
        ESP_LOGI(TAG, "FINAL QUIET: WoM=%" PRIu32 " final GPIO21=%d", final_quiet.wom_events,
                 final_quiet.final_gpio_level);
    }

    if (!single_passed) {
        wom_test_stats_reset(&post_motion);
        wom_test_stats_reset(&final_quiet);
        post_motion.valid = false;
        final_quiet.valid = false;
    }
    ESP_LOGI(TAG, "FINAL SUMMARY: Phase | WoM events | Last event | GPIO idle | Result");
    wom_test_print_row("Baseline stationary", &baseline, -1, wom_test_is_quiet(&baseline));
    wom_test_print_row("Intentional motion", &intentional_motion, single_passed ? 0 : -1,
                       single_passed && intentional_motion.valid);
    wom_test_print_row("Immediate 0-10s post-motion", &post_motion,
                       post_motion_timestamp_count ? post_motion_timestamps[post_motion_timestamp_count - 1] : -1,
                       single_passed && post_motion.valid);
    wom_test_print_row("10-20s final quiet", &final_quiet, -1,
                       single_passed && wom_test_is_quiet(&final_quiet));
    wom_test_print_details("Baseline stationary", &baseline);
    wom_test_print_details("Intentional motion", &intentional_motion);
    wom_test_print_details("Immediate 0-10s post-motion", &post_motion);
    wom_test_print_details("10-20s final quiet", &final_quiet);
    qmi8658_print_interrupt_timing_summary(&baseline, &intentional_motion, &post_motion, &final_quiet);
    ESP_LOGI(TAG, "Final quiet phase ran: %s", final_quiet_executed ? "yes" : "no");
    ESP_LOGI(TAG, "Final quiet phase valid: %s", final_quiet.valid ? "yes" : "no");
    if (final_quiet_executed && !final_quiet.valid) {
        if (final_quiet.final_gpio_level != 0 && !(final_quiet.final_status1 & QMI8658_STATUS1_WOM)) {
            ESP_LOGW(TAG, "Final quiet invalid reason: GPIO21 high with STATUS1.WoM clear");
        } else {
            ESP_LOGW(TAG, "Final quiet invalid reason: WoM acknowledgement or GPIO21 idle-return failure");
        }
    }
    if (post_motion_timestamp_count > 0) {
        ESP_LOGI(TAG, "Last post-motion event occurred: %" PRIu32 " ms after first motion event",
                 post_motion_timestamps[post_motion_timestamp_count - 1]);
    }
}
#else

#define QMI8658_CTRL2_ACC_2G_125HZ   0x06
#define QMI8658_CTRL2_ACC_2G_62_5HZ  0x07
#define QMI8658_CTRL2_ACC_2G_31_25HZ 0x08
#define ACCEL_ODR_COUNT               3

typedef struct {
    const char *name;
    uint32_t odr_millihz;
    uint8_t ctrl2;
} accel_odr_t;

typedef struct {
    const char *name;
    const char *instruction;
    uint32_t duration_ms;
} accel_phase_t;

static const accel_odr_t accel_odrs[ACCEL_ODR_COUNT] = {
    {"125 Hz", 125000, QMI8658_CTRL2_ACC_2G_125HZ},
    {"62.5 Hz", 62500, QMI8658_CTRL2_ACC_2G_62_5HZ},
    {"31.25 Hz", 31250, QMI8658_CTRL2_ACC_2G_31_25HZ},
};

static const accel_phase_t accel_phases[] = {
    {"FLAT / STILL", "Place the watch flat on a stable surface and do not move it.", 5000},
    {"SLOW ROTATION", "Slowly rotate the watch through different orientations.", 5000},
    {"MODERATE SHAKE", "Shake the watch securely with moderate intensity.", 3000},
};

typedef struct {
    uint32_t samples;
    uint32_t elapsed_ms;
    uint32_t expected_samples;
    int32_t min[3];
    int32_t max[3];
    double sum[3];
    double sum_squared[3];
    int32_t previous[3];
    bool has_previous;
    uint32_t max_delta[3];
    uint32_t delta_over[6];
    uint32_t max_vector;
    uint32_t min_vector;
    double vector_sum;
    double vector_sum_squared;
    uint32_t max_delta_vector;
    uint32_t all_zero_samples;
    int64_t first_sample_us;
    int64_t last_sample_us;
} accel_stats_t;

static const uint32_t accel_delta_thresholds[] = {25, 50, 75, 100, 150, 200};

static void accel_stats_reset(accel_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->min_vector = UINT32_MAX;
    for (int axis = 0; axis < 3; axis++) {
        stats->min[axis] = INT32_MAX;
        stats->max[axis] = INT32_MIN;
    }
}

static void accel_stats_add(accel_stats_t *stats, const int32_t sample[3], int64_t timestamp_us)
{
    int32_t delta[3] = {0};
    uint32_t max_axis_delta = 0;
    for (int axis = 0; axis < 3; axis++) {
        const int32_t value = sample[axis];
        if (value < stats->min[axis]) stats->min[axis] = value;
        if (value > stats->max[axis]) stats->max[axis] = value;
        stats->sum[axis] += value;
        stats->sum_squared[axis] += (double)value * value;
        if (stats->has_previous) {
            delta[axis] = value - stats->previous[axis];
            const uint32_t absolute_delta = delta[axis] < 0 ? (uint32_t)-delta[axis] : (uint32_t)delta[axis];
            if (absolute_delta > stats->max_delta[axis]) stats->max_delta[axis] = absolute_delta;
            if (absolute_delta > max_axis_delta) max_axis_delta = absolute_delta;
        }
        stats->previous[axis] = value;
    }
    if (stats->has_previous) {
        for (size_t threshold = 0; threshold < sizeof(accel_delta_thresholds) / sizeof(accel_delta_thresholds[0]); threshold++) {
            if (max_axis_delta > accel_delta_thresholds[threshold]) stats->delta_over[threshold]++;
        }
        const uint32_t delta_vector = vector_magnitude(delta);
        if (delta_vector > stats->max_delta_vector) stats->max_delta_vector = delta_vector;
    }
    const uint32_t vector = vector_magnitude(sample);
    if (sample[0] == 0 && sample[1] == 0 && sample[2] == 0) stats->all_zero_samples++;
    if (vector < stats->min_vector) stats->min_vector = vector;
    if (vector > stats->max_vector) stats->max_vector = vector;
    stats->vector_sum += vector;
    stats->vector_sum_squared += (double)vector * vector;
    stats->has_previous = true;
    stats->samples++;
    if (stats->first_sample_us == 0) stats->first_sample_us = timestamp_us;
    stats->last_sample_us = timestamp_us;
}

static void accel_measure(i2c_master_dev_handle_t qmi, uint32_t duration_ms, uint32_t odr_millihz,
                          accel_stats_t *stats)
{
    accel_stats_reset(stats);
    const int64_t start_us = esp_timer_get_time();
    const int64_t duration_us = (int64_t)duration_ms * 1000;
    const int64_t period_us = 1000000000LL / odr_millihz;
    const int64_t end_us = start_us + duration_us;
    int64_t next_sample_us = start_us;

    while (esp_timer_get_time() < end_us) {
        const int64_t now_us = esp_timer_get_time();
        if (now_us >= next_sample_us) {
            int32_t sample[3];
            qmi8658_read_acceleration(qmi, sample);
            accel_stats_add(stats, sample, esp_timer_get_time());
            do {
                next_sample_us += period_us;
            } while (next_sample_us <= now_us);
        } else {
            taskYIELD();
        }
    }
    stats->elapsed_ms = (uint32_t)((esp_timer_get_time() - start_us) / 1000);
    stats->expected_samples = (uint32_t)(((uint64_t)stats->elapsed_ms * odr_millihz + 500000) / 1000000);
}

static void accel_countdown(const char *name, const char *instruction)
{
    ESP_LOGI(TAG, "%s: %s", name, instruction);
    for (int seconds = 5; seconds > 0; seconds--) {
        ESP_LOGI(TAG, "%s starts in %d", name, seconds);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static uint32_t accel_achieved_millihz(const accel_stats_t *stats)
{
    const int64_t sample_interval_us = stats->last_sample_us - stats->first_sample_us;
    return stats->samples > 1 && sample_interval_us > 0
               ? (uint32_t)(((uint64_t)(stats->samples - 1) * 1000000000ULL) / sample_interval_us)
               : 0;
}

static bool accel_samples_valid(const accel_stats_t *stats)
{
    const uint32_t coverage = stats->expected_samples ? (stats->samples * 100) / stats->expected_samples : 0;
    return coverage >= QMI8658_MIN_SAMPLING_COVERAGE_PCT && stats->all_zero_samples == 0 &&
           stats->max_vector > 100 && stats->min_vector < 2000;
}

static void accel_print_summary(const accel_phase_t *phase, const accel_stats_t *stats)
{
    const uint32_t coverage = stats->expected_samples ? (stats->samples * 100) / stats->expected_samples : 0;
    double mean[3];
    double stddev[3];
    for (int axis = 0; axis < 3; axis++) {
        mean[axis] = stats->samples ? stats->sum[axis] / stats->samples : 0;
        const double variance = stats->samples ? stats->sum_squared[axis] / stats->samples - mean[axis] * mean[axis] : 0;
        stddev[axis] = sqrt(variance > 0 ? variance : 0);
    }
    const double vector_mean = stats->samples ? stats->vector_sum / stats->samples : 0;
    const double vector_variance = stats->samples ? stats->vector_sum_squared / stats->samples - vector_mean * vector_mean : 0;
    const uint32_t achieved_millihz = accel_achieved_millihz(stats);
    ESP_LOGI(TAG, "%s: elapsed=%" PRIu32 "ms expected=%" PRIu32 " actual=%" PRIu32
             " achieved=%.2fHz coverage=%" PRIu32 "%% all-zero=%" PRIu32,
             phase->name, stats->elapsed_ms, stats->expected_samples, stats->samples,
             achieved_millihz / 1000.0, coverage, stats->all_zero_samples);
    ESP_LOGI(TAG, "XYZ min/max/mean/std mg: X=%" PRId32 "/%" PRId32 "/%.1f/%.1f Y=%" PRId32 "/%" PRId32 "/%.1f/%.1f Z=%" PRId32 "/%" PRId32 "/%.1f/%.1f",
             stats->min[0], stats->max[0], mean[0], stddev[0], stats->min[1], stats->max[1], mean[1], stddev[1], stats->min[2], stats->max[2], mean[2], stddev[2]);
    ESP_LOGI(TAG, "max delta XYZ=%" PRIu32 "/%" PRIu32 "/%" PRIu32 " vector min/mean/std/max=%" PRIu32 "/%.1f/%.1f/%" PRIu32 " max delta-vector=%" PRIu32,
             stats->max_delta[0], stats->max_delta[1], stats->max_delta[2], stats->min_vector, vector_mean,
             sqrt(vector_variance > 0 ? vector_variance : 0), stats->max_vector, stats->max_delta_vector);
    ESP_LOGI(TAG, "any-axis delta >25/50/75/100/150/200mg: %" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32 "/%" PRIu32,
             stats->delta_over[0], stats->delta_over[1], stats->delta_over[2], stats->delta_over[3], stats->delta_over[4], stats->delta_over[5]);
}

static uint32_t accel_max_axis_delta(const accel_stats_t *stats)
{
    uint32_t maximum = stats->max_delta[0];
    for (int axis = 1; axis < 3; axis++) if (stats->max_delta[axis] > maximum) maximum = stats->max_delta[axis];
    return maximum;
}

static uint32_t accel_max_axis_span(const accel_stats_t *stats)
{
    uint32_t maximum = 0;
    for (int axis = 0; axis < 3; axis++) {
        const uint32_t span = (uint32_t)(stats->max[axis] - stats->min[axis]);
        if (span > maximum) maximum = span;
    }
    return maximum;
}

static void qmi8658_configure_normal_accel(i2c_master_dev_handle_t qmi, uint8_t ctrl2)
{
    qmi8658_write(qmi, QMI8658_RESET_REG, 0xB0);
    vTaskDelay(pdMS_TO_TICKS(10));
    qmi8658_write(qmi, QMI8658_CTRL7_REG, 0x00);
    qmi8658_write(qmi, QMI8658_CTRL1_REG, 0x60);
    qmi8658_write(qmi, QMI8658_CTRL2_REG, ctrl2);
    qmi8658_write(qmi, QMI8658_CTRL7_REG, QMI8658_CTRL7_ACC_ENABLE);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static bool accel_still_is_one_g(const accel_stats_t *stats)
{
    const double vector_mean = stats->samples ? stats->vector_sum / stats->samples : 0;
    return accel_samples_valid(stats) && vector_mean >= 700.0 && vector_mean <= 1300.0;
}

void app_main(void)
{
    const i2c_device_config_t qmi_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDRESS,
        .scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ,
    };
    i2c_master_dev_handle_t qmi;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bsp_i2c_get_handle(), &qmi_config, &qmi));
    accel_stats_t results[ACCEL_ODR_COUNT][3];
    bool valid[ACCEL_ODR_COUNT] = {false};

    for (int odr_index = 0; odr_index < ACCEL_ODR_COUNT; odr_index++) {
        const accel_odr_t *odr = &accel_odrs[odr_index];
        qmi8658_configure_normal_accel(qmi, odr->ctrl2);
        ESP_LOGI(TAG, "ACCEL-ONLY normal %s: CTRL1=0x%02X CTRL2=0x%02X CTRL5=0x%02X CTRL7=0x%02X STATUS0=0x%02X",
                 odr->name, qmi8658_read(qmi, QMI8658_CTRL1_REG), qmi8658_read(qmi, QMI8658_CTRL2_REG),
                 qmi8658_read(qmi, 0x06), qmi8658_read(qmi, QMI8658_CTRL7_REG), qmi8658_read(qmi, 0x2E));
        ESP_LOGW(TAG, "STATUS0.aDA is 0x%02X; valid output must be determined from registers 0x35..0x3A, not aDA.",
                 qmi8658_read(qmi, 0x2E) & BIT(0));

        for (int phase_index = 0; phase_index < 3; phase_index++) {
            const accel_phase_t *phase = &accel_phases[phase_index];
            accel_countdown(phase->name, phase->instruction);
            ESP_LOGI(TAG, "%s %s measurement START", odr->name, phase->name);
            accel_measure(qmi, phase->duration_ms, odr->odr_millihz, &results[odr_index][phase_index]);
            ESP_LOGI(TAG, "%s %s measurement STOP", odr->name, phase->name);
            accel_print_summary(phase, &results[odr_index][phase_index]);
        }

        const accel_stats_t *still = &results[odr_index][0];
        const accel_stats_t *rotation = &results[odr_index][1];
        const accel_stats_t *shake = &results[odr_index][2];
        const bool still_one_g = accel_still_is_one_g(still);
        const bool rotation_detected = accel_samples_valid(rotation) && accel_max_axis_span(rotation) >= 300;
        const bool shake_detected = accel_samples_valid(shake) && accel_max_axis_delta(shake) >= 100;
        valid[odr_index] = still_one_g && rotation_detected && shake_detected;
        ESP_LOGI(TAG, "%s result: live=%s ~1g-still=%s rotation=%s shake=%s VALID=%s",
                 odr->name, accel_samples_valid(still) ? "yes" : "no", still_one_g ? "yes" : "no",
                 rotation_detected ? "yes" : "no", shake_detected ? "yes" : "no",
                 valid[odr_index] ? "yes" : "no");
    }

    ESP_LOGI(TAG, "FINAL: ODR | Live data | Achieved Hz | ~1 g still | Rotation | Shake | Valid");
    for (int index = 0; index < ACCEL_ODR_COUNT; index++) {
        const accel_stats_t *still = &results[index][0];
        const accel_stats_t *rotation = &results[index][1];
        const accel_stats_t *shake = &results[index][2];
        ESP_LOGI(TAG, "%s | %s | %.2f | %s | %s | %s | %s", accel_odrs[index].name,
                 accel_samples_valid(still) ? "yes" : "no", accel_achieved_millihz(still) / 1000.0,
                 accel_still_is_one_g(still) ? "yes" : "no",
                 accel_samples_valid(rotation) && accel_max_axis_span(rotation) >= 300 ? "yes" : "no",
                 accel_samples_valid(shake) && accel_max_axis_delta(shake) >= 100 ? "yes" : "no",
                 valid[index] ? "yes" : "no");
    }
    ESP_LOGI(TAG, "STATUS0.aDA remained zero with valid normal-mode data; do not use it as a read gate without further hardware evidence.");
    ESP_LOGI(TAG, "This comparison holds reset, CTRL1=0x60, CTRL7=0x01, range, and gyro-disabled state together; only CTRL2 ODR changes.");
    ESP_LOGI(TAG, "Follow-up experiment (not implemented): vary reset/init, CTRL1, and CTRL7 independently while holding CTRL2 at 125 Hz.");
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
#endif
