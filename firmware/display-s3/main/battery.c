// Akkuspannung ueber BAT_ADC (GPIO1, Spannungsteiler 100k/100k auf dem Board)
#include "battery.h"
#include "sdkconfig.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "battery";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static adc_channel_t s_chan;
static bool s_ok;

esp_err_t battery_init(void)
{
    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(CONFIG_NRD_BAT_ADC_GPIO, &unit, &s_chan) != ESP_OK) return ESP_FAIL;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = unit };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ucfg, &s_adc));
    adc_oneshot_chan_cfg_t ccfg = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, s_chan, &ccfg));
    adc_cali_curve_fitting_config_t cal = { .unit_id = unit, .chan = s_chan, .atten = ADC_ATTEN_DB_12,
                                            .bitwidth = ADC_BITWIDTH_DEFAULT };
    if (adc_cali_create_scheme_curve_fitting(&cal, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "keine ADC-Kalibrierung, Werte ungenauer");
        s_cali = NULL;
    }
    s_ok = true;
    return ESP_OK;
}

int battery_mv(void)
{
    if (!s_ok) return -1;
    int sum = 0, n = 0;
    for (int i = 0; i < 16; i++) {
        int raw, mv;
        if (adc_oneshot_read(s_adc, s_chan, &raw) != ESP_OK) continue;
        if (s_cali && adc_cali_raw_to_voltage(s_cali, raw, &mv) == ESP_OK) sum += mv;
        else sum += raw * 3100 / 4095;
        n++;
    }
    return n ? 2 * sum / n : -1;   // Teiler 1:2
}

int battery_percent(void)
{
    // Ruhespannungskennlinie Li-Ionen (grob)
    static const int mv_tab[] = { 3300, 3500, 3600, 3700, 3750, 3800, 3870, 3950, 4050, 4150, 4200 };
    int mv = battery_mv();
    if (mv < 0) return -1;
    if (mv <= mv_tab[0]) return 0;
    for (int i = 1; i < 11; i++) {
        if (mv <= mv_tab[i]) {
            return (i - 1) * 10 + (mv - mv_tab[i - 1]) * 10 / (mv_tab[i] - mv_tab[i - 1]);
        }
    }
    return 100;
}
