#include "wm8960.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

static const char *TAG = "wm8960";

#define WM8960_ADDR 0x1A

// Register (Datenblatt WM8960)
#define R_LINVOL   0x00
#define R_RINVOL   0x01
#define R_LOUT1    0x02
#define R_ROUT1    0x03
#define R_CLOCK1   0x04
#define R_DACCTL1  0x05
#define R_DACCTL2  0x06
#define R_IFACE1   0x07
#define R_CLOCK2   0x08
#define R_IFACE2   0x09
#define R_LDAC     0x0A
#define R_RDAC     0x0B
#define R_RESET    0x0F
#define R_NOISEG   0x14
#define R_LADC     0x15
#define R_RADC     0x16
#define R_ADDCTL1  0x17
#define R_ADDCTL2  0x18
#define R_POWER1   0x19
#define R_POWER2   0x1A
#define R_ADDCTL3  0x1B
#define R_LINPATH  0x20
#define R_RINPATH  0x21
#define R_LOUTMIX  0x22
#define R_ROUTMIX  0x25
#define R_POWER3   0x2F
#define R_CLASSD1  0x31
#define R_PLL1     0x34
#define R_PLL2     0x35
#define R_PLL3     0x36
#define R_PLL4     0x37

static i2c_master_dev_handle_t s_dev;
static uint32_t s_hz = 50000;     // langsam = robuster bei fliegender Verdrahtung
static uint16_t s_cache[56];
static bool s_present;
static uint32_t s_write_errors;

bool wm8960_present(void)
{
    return s_present;
}

uint32_t wm8960_write_errors(void)
{
    return s_write_errors;
}

esp_err_t wm8960_write(uint8_t reg, uint16_t val)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    uint8_t buf[2] = { (uint8_t)((reg << 1) | ((val >> 8) & 1)), (uint8_t)(val & 0xFF) };
    esp_err_t err = ESP_FAIL;
    for (int i = 0; i < 3; i++) {     // bis zu 3 Versuche
        err = i2c_master_transmit(s_dev, buf, sizeof(buf), 50);
        if (err == ESP_OK) break;
    }
    if (err == ESP_OK) {
        if (reg < 56) s_cache[reg] = val & 0x1FF;
    } else {
        s_write_errors++;
        ESP_LOGE(TAG, "Schreiben R%02X fehlgeschlagen: %s", reg, esp_err_to_name(err));
    }
    return err;
}

uint32_t wm8960_get_i2c_hz(void)
{
    return s_hz;
}

void wm8960_set_i2c_hz(uint32_t hz)
{
    s_hz = hz;
}

int wm8960_i2c_test(i2c_master_bus_handle_t bus, uint32_t hz, int n, int *probe_ok)
{
    // n x Adresse abfragen und n x ein harmloses Register (Noisegate aus) schreiben
    int p = 0, w = 0;
    for (int i = 0; i < n; i++) {
        if (i2c_master_probe(bus, WM8960_ADDR, 20) == ESP_OK) p++;
    }
    i2c_device_config_t cfg = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = WM8960_ADDR,
                                .scl_speed_hz = hz };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(bus, &cfg, &dev) == ESP_OK) {
        uint8_t buf[2] = { (uint8_t)(R_NOISEG << 1), 0x00 };
        for (int i = 0; i < n; i++) {
            if (i2c_master_transmit(dev, buf, sizeof(buf), 20) == ESP_OK) w++;
        }
        i2c_master_bus_rm_device(dev);
    }
    if (probe_ok) *probe_ok = p;
    return w;
}

uint16_t wm8960_read_cache(uint8_t reg)
{
    return reg < 56 ? s_cache[reg] : 0;
}

void wm8960_i2c_recover(int sda, int scl)
{
    // Klassische I2C-Bus-Freigabe: Haengt ein Slave mitten in einem Byte (z. B. nach einem
    // Neustart des ESP waehrend er SDA auf LOW zieht), takten wir SCL bis zu 9x, bis er SDA
    // loslaesst, und erzeugen dann eine STOP-Bedingung.
    gpio_config_t io = { .pin_bit_mask = (1ULL << sda) | (1ULL << scl), .mode = GPIO_MODE_INPUT_OUTPUT_OD,
                         .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE };
    gpio_config(&io);
    gpio_set_level(sda, 1);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(10);
    for (int i = 0; i < 9 && !gpio_get_level(sda); i++) {
        gpio_set_level(scl, 0);
        esp_rom_delay_us(10);
        gpio_set_level(scl, 1);
        esp_rom_delay_us(10);
    }
    // STOP: SDA LOW -> SCL HIGH -> SDA HIGH
    gpio_set_level(scl, 0);
    esp_rom_delay_us(10);
    gpio_set_level(sda, 0);
    esp_rom_delay_us(10);
    gpio_set_level(scl, 1);
    esp_rom_delay_us(10);
    gpio_set_level(sda, 1);
    esp_rom_delay_us(10);
    gpio_reset_pin(sda);
    gpio_reset_pin(scl);
}

void wm8960_detach(void)
{
    if (s_dev) {
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
    }
    s_present = false;
}

esp_err_t wm8960_init(i2c_master_bus_handle_t bus)
{
    wm8960_detach();
    s_write_errors = 0;

    // Mehrere Versuche: der erste Zugriff nach dem Einschalten/Neustart kann scheitern,
    // wenn der Codec den Bus noch festhaelt. Der Treiber setzt den Bus danach selbst zurueck.
    esp_err_t err = ESP_FAIL;
    int attempt;
    for (attempt = 1; attempt <= 8; attempt++) {
        err = i2c_master_probe(bus, WM8960_ADDR, 100);
        if (err == ESP_OK) break;
        i2c_master_bus_reset(bus);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WM8960 nicht gefunden (0x%02X) nach %d Versuchen: %s", WM8960_ADDR, attempt - 1,
                 esp_err_to_name(err));
        return err;
    }
    if (attempt > 1) ESP_LOGW(TAG, "WM8960 antwortet erst beim %d. Versuch", attempt);

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = WM8960_ADDR,
        .scl_speed_hz = s_hz,
    };
    err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        s_dev = NULL;
        return err;
    }

    err = wm8960_write(R_RESET, 0x000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WM8960-Reset fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    // Stromversorgung: VMID 50k, VREF, beide Eingangs-PGAs, beide ADCs, MICBIAS
    wm8960_write(R_POWER1, 0x0FE);
    // DAC L/R, LOUT1/ROUT1 (Kopfhoerer), PLL an. Lautsprecher aus.
    wm8960_write(R_POWER2, 0x1E1);
    // LMIC/RMIC (PGA), Ausgangsmischer L/R
    wm8960_write(R_POWER3, 0x03C);

    // PLL: MCLK 24 MHz /2 = 12 MHz -> f2 = 98,304 MHz, R = 8,192 -> N = 8, K = 0x3126E9
    // R52: SDM (fraktional) | PLLPRESCALE (/2) | N = 8
    wm8960_write(R_PLL1, 0x020 | 0x010 | 0x008);
    wm8960_write(R_PLL2, 0x31);
    wm8960_write(R_PLL3, 0x26);
    wm8960_write(R_PLL4, 0xE9);
    vTaskDelay(pdMS_TO_TICKS(10));

    // SYSCLK = PLL / 2 = 12,288 MHz, ADC/DAC = SYSCLK/256 = 48 kHz
    wm8960_write(R_CLOCK1, 0x004 | 0x001);
    // BCLK = SYSCLK / 8 = 1,536 MHz (32 Bit pro Frame), Class-D-Takt Standard
    wm8960_write(R_CLOCK2, 0x1C0 | 0x007);
    // I2S-Format, 16 Bit, Master
    wm8960_write(R_IFACE1, 0x040 | 0x002);
    // ADCLRC und DACLRC sind auf dem Board verbunden -> gemeinsamer LRCLK
    wm8960_write(R_IFACE2, 0x000);
    wm8960_write(R_ADDCTL2, 0x004);   // LRCM

    // ADC: Hochpass an, DAC: nicht stumm
    wm8960_write(R_DACCTL1, 0x000);
    wm8960_write(R_DACCTL2, 0x000);
    wm8960_write(R_ADDCTL1, 0x1C0);   // Standard (linker ADC -> linker Kanal)
    wm8960_write(R_LADC, 0x1C3);      // 0 dB
    wm8960_write(R_RADC, 0x1C3);
    wm8960_write(R_LDAC, 0x1FF);      // 0 dB
    wm8960_write(R_RDAC, 0x1FF);
    wm8960_write(R_NOISEG, 0x000);    // Hardware-Noisegate aus

    // DAC -> Ausgangsmischer -> Kopfhoerer
    wm8960_write(R_LOUTMIX, 0x100);
    wm8960_write(R_ROUTMIX, 0x100);
    wm8960_write(R_CLASSD1, 0x037);   // Lautsprecher aus

    s_present = (s_write_errors == 0);
    ESP_LOGI(TAG, "WM8960 initialisiert (Master, PLL 12,288 MHz, 48 kHz, I2S 16 Bit)%s",
             s_present ? "" : " - ABER mit Schreibfehlern!");
    return ESP_OK;
}

esp_err_t wm8960_set_mic(int src, int pga_db, int boost)
{
    if (pga_db < -17) pga_db = -17;
    if (pga_db > 30) pga_db = 30;
    if (boost < 0) boost = 0;
    if (boost > 3) boost = 3;
    // PGA: 0x00 = -17,25 dB ... 0x3F = +30 dB, 0,75-dB-Schritte
    int vol = (int)(((float)pga_db + 17.25f) / 0.75f + 0.5f);
    if (vol > 0x3F) vol = 0x3F;
    if (vol < 0) vol = 0;

    // Pfad: INPUT1 an den invertierenden PGA-Eingang, PGA -> Boost-Mischer
    uint16_t path = 0x100 | 0x008 | ((uint16_t)boost << 4);
    uint16_t on   = 0x100 | (uint16_t)vol;        // IPVU + Lautstaerke
    uint16_t mute = 0x100 | 0x080;                // IPVU + Stumm

    if (src == 0) {
        wm8960_write(R_LINPATH, path);
        wm8960_write(R_RINPATH, 0x000);
        wm8960_write(R_LINVOL, on);
        wm8960_write(R_RINVOL, mute);
    } else {
        wm8960_write(R_LINPATH, 0x000);
        wm8960_write(R_RINPATH, path);
        wm8960_write(R_LINVOL, mute);
        wm8960_write(R_RINVOL, on);
    }
    return ESP_OK;
}

esp_err_t wm8960_set_hp_volume(int percent)
{
    uint16_t reg;
    if (percent <= 0) {
        reg = 0x000;                               // < 0x30 = stumm
    } else {
        if (percent > 100) percent = 100;
        reg = 0x40 + (uint16_t)((percent - 1) * (0x7F - 0x40) / 99);   // ca. -57 .. +6 dB
    }
    wm8960_write(R_LOUT1, 0x100 | reg);   // OUT1VU: beide Kanaele gleichzeitig uebernehmen
    wm8960_write(R_ROUT1, 0x100 | reg);
    return ESP_OK;
}
