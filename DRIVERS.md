# Driver API Reference

Comprehensive API documentation for all hardware drivers on the ESP32-C6-Touch-AMOLED-2.06 board.

## 📑 Table of Contents

- [Display & Touch (BSP)](#display--touch-bsp)
- [IMU - QMI8658](#imu---qmi8658)
- [RTC - PCF85063](#rtc---pcf85063)
- [Audio - ES8311](#audio---es8311)
- [Power Management - AXP2101](#power-management---axp2101)
- [LVGL Integration](#lvgl-integration)

---

## Display & Touch (BSP)

The Board Support Package provides unified API for display and touch functionality.

### Header Files

```c
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
```

### Display Initialization

#### `bsp_display_start()`

Initialize display with LVGL integration.

```c
lv_display_t *bsp_display_start(void);
```

**Returns**:
- `lv_display_t*` - LVGL display object on success
- `NULL` - On failure

**Example**:
```c
lv_display_t *display = bsp_display_start();
if (display == NULL) {
    ESP_LOGE(TAG, "Display initialization failed");
}
```

#### `bsp_display_backlight_on()` / `bsp_display_backlight_off()`

Control display backlight.

```c
esp_err_t bsp_display_backlight_on(void);
esp_err_t bsp_display_backlight_off(void);
```

**Returns**: `ESP_OK` on success

### Display Control

#### `bsp_display_brightness_set()`

Set backlight brightness.

```c
esp_err_t bsp_display_brightness_set(int brightness_percent);
```

**Parameters**:
- `brightness_percent` - Brightness level (0-100)

**Returns**: `ESP_OK` on success

**Example**:
```c
bsp_display_brightness_set(80);  // Set to 80%
```

#### `bsp_display_lock()` / `bsp_display_unlock()`

Thread-safe LVGL access (required when modifying UI from tasks).

```c
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);
```

**Parameters**:
- `timeout_ms` - Maximum wait time in milliseconds (0 = wait forever)

**Returns**: `true` if lock acquired, `false` on timeout

**Example**:
```c
bsp_display_lock(0);
lv_label_set_text(label, "New Text");
bsp_display_unlock();
```

### Touch Input

Touch events are automatically processed by LVGL. No direct touch API is needed for basic usage.

**Touch Callback** (advanced):
```c
void touch_callback(lv_event_t *e) {
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);
    ESP_LOGI(TAG, "Touch at: X=%d Y=%d", point.x, point.y);
}

// Register callback
lv_obj_add_event_cb(screen, touch_callback, LV_EVENT_PRESSED, NULL);
```

---

## IMU - QMI8658

6-axis Inertial Measurement Unit (3-axis accelerometer + 3-axis gyroscope).

### Header File

```c
#include "qmi8658.h"
```

### Data Types

```c
typedef struct {
    float accelX, accelY, accelZ;  // Acceleration (m/s² or mg)
    float gyroX, gyroY, gyroZ;     // Angular velocity (rad/s or dps)
    float temperature;              // Temperature (°C)
} qmi8658_data_t;

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    uint8_t dev_addr;
    // ... internal fields
} qmi8658_dev_t;
```

### Initialization

#### `qmi8658_init()`

Initialize IMU device.

```c
esp_err_t qmi8658_init(qmi8658_dev_t *dev,
                       i2c_master_bus_handle_t bus_handle,
                       uint8_t dev_addr);
```

**Parameters**:
- `dev` - Device structure
- `bus_handle` - I2C bus handle from `bsp_i2c_get_handle()`
- `dev_addr` - `QMI8658_ADDRESS_HIGH` (0x6B) or `QMI8658_ADDRESS_LOW` (0x6A)

**Returns**: `ESP_OK` on success

**Example**:
```c
qmi8658_dev_t imu;
i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
qmi8658_init(&imu, i2c, QMI8658_ADDRESS_HIGH);
```

### Configuration

#### `qmi8658_set_accel_range()`

Set accelerometer measurement range.

```c
esp_err_t qmi8658_set_accel_range(qmi8658_dev_t *dev,
                                   qmi8658_accel_range_t range);
```

**Range Options**:
- `QMI8658_ACCEL_RANGE_2G` - ±2G
- `QMI8658_ACCEL_RANGE_4G` - ±4G
- `QMI8658_ACCEL_RANGE_8G` - ±8G
- `QMI8658_ACCEL_RANGE_16G` - ±16G

**Example**:
```c
qmi8658_set_accel_range(&imu, QMI8658_ACCEL_RANGE_8G);
```

#### `qmi8658_set_accel_odr()`

Set accelerometer output data rate.

```c
esp_err_t qmi8658_set_accel_odr(qmi8658_dev_t *dev,
                                 qmi8658_accel_odr_t odr);
```

**ODR Options**:
- `QMI8658_ACCEL_ODR_8000HZ` to `QMI8658_ACCEL_ODR_1HZ`
- Common: `QMI8658_ACCEL_ODR_500HZ`, `QMI8658_ACCEL_ODR_250HZ`, `QMI8658_ACCEL_ODR_125HZ`

#### `qmi8658_set_gyro_range()`

Set gyroscope measurement range.

```c
esp_err_t qmi8658_set_gyro_range(qmi8658_dev_t *dev,
                                  qmi8658_gyro_range_t range);
```

**Range Options**:
- `QMI8658_GYRO_RANGE_16DPS` to `QMI8658_GYRO_RANGE_2048DPS`
- Common: `QMI8658_GYRO_RANGE_512DPS`

#### `qmi8658_set_gyro_odr()`

Set gyroscope output data rate (same as accel ODR).

### Unit Selection

#### `qmi8658_set_accel_unit_mps2()` / `qmi8658_set_accel_unit_mg()`

Choose accelerometer units (m/s² or mg).

```c
esp_err_t qmi8658_set_accel_unit_mps2(qmi8658_dev_t *dev, bool enable);
esp_err_t qmi8658_set_accel_unit_mg(qmi8658_dev_t *dev, bool enable);
```

**Example**:
```c
qmi8658_set_accel_unit_mps2(&imu, true);  // Use m/s²
```

#### `qmi8658_set_gyro_unit_rads()` / `qmi8658_set_gyro_unit_dps()`

Choose gyroscope units (rad/s or degrees/s).

```c
esp_err_t qmi8658_set_gyro_unit_rads(qmi8658_dev_t *dev, bool enable);
esp_err_t qmi8658_set_gyro_unit_dps(qmi8658_dev_t *dev, bool enable);
```

### Data Reading

#### `qmi8658_read_sensor_data()`

Read accelerometer and gyroscope data.

```c
esp_err_t qmi8658_read_sensor_data(qmi8658_dev_t *dev,
                                    qmi8658_data_t *data);
```

**Example**:
```c
qmi8658_data_t data;
if (qmi8658_read_sensor_data(&imu, &data) == ESP_OK) {
    ESP_LOGI(TAG, "Accel: X=%.2f Y=%.2f Z=%.2f m/s²",
             data.accelX, data.accelY, data.accelZ);
    ESP_LOGI(TAG, "Gyro: X=%.2f Y=%.2f Z=%.2f rad/s",
             data.gyroX, data.gyroY, data.gyroZ);
    ESP_LOGI(TAG, "Temperature: %.1f °C", data.temperature);
}
```

### Complete Example

```c
qmi8658_dev_t imu;
i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();

// Initialize
qmi8658_init(&imu, i2c, QMI8658_ADDRESS_HIGH);

// Configure
qmi8658_set_accel_range(&imu, QMI8658_ACCEL_RANGE_8G);
qmi8658_set_accel_odr(&imu, QMI8658_ACCEL_ODR_500HZ);
qmi8658_set_accel_unit_mps2(&imu, true);

qmi8658_set_gyro_range(&imu, QMI8658_GYRO_RANGE_512DPS);
qmi8658_set_gyro_odr(&imu, QMI8658_GYRO_ODR_500HZ);
qmi8658_set_gyro_unit_rads(&imu, true);

// Enable sensors
qmi8658_write_register(&imu, QMI8658_CTRL5, 0x03);  // Enable accel + gyro

// Read data
qmi8658_data_t data;
qmi8658_read_sensor_data(&imu, &data);
```

---

## RTC - PCF85063

Real-Time Clock with battery backup, alarm, and timer functionality.

### Header File

```c
#include "pcf85063a.h"
```

### Data Types

```c
typedef struct {
    uint8_t second;    // 0-59
    uint8_t minute;    // 0-59
    uint8_t hour;      // 0-23 (24-hour format)
    uint8_t day;       // 1-31
    uint8_t weekday;   // 0-6 (0 = Sunday)
    uint8_t month;     // 1-12
    uint8_t year;      // 0-99 (20xx)
} pcf85063a_time_t;

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    uint8_t dev_addr;
    // ... internal fields
} pcf85063a_dev_t;
```

### Initialization

#### `pcf85063a_init()`

Initialize RTC device.

```c
esp_err_t pcf85063a_init(pcf85063a_dev_t *dev,
                          i2c_master_bus_handle_t bus_handle,
                          uint8_t dev_addr);
```

**Parameters**:
- `dev` - Pointer to device structure
- `bus_handle` - I2C bus from `bsp_i2c_get_handle()`
- `dev_addr` - RTC address (0x51)

**Example**:
```c
pcf85063a_dev_t rtc;
i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
pcf85063a_init(&rtc, i2c, 0x51);
```

### Time Operations

#### `pcf85063a_set_time()`

Set current date and time.

```c
esp_err_t pcf85063a_set_time(pcf85063a_dev_t *dev,
                              const pcf85063a_time_t *time);
```

**Example**:
```c
pcf85063a_time_t time = {
    .year = 24,      // 2024
    .month = 12,
    .day = 15,
    .hour = 14,
    .minute = 30,
    .second = 0,
    .weekday = 0     // Sunday
};
pcf85063a_set_time(&rtc, &time);
```

#### `pcf85063a_get_time()`

Read current date and time.

```c
esp_err_t pcf85063a_get_time(pcf85063a_dev_t *dev,
                              pcf85063a_time_t *time);
```

**Example**:
```c
pcf85063a_time_t time;
if (pcf85063a_get_time(&rtc, &time) == ESP_OK) {
    ESP_LOGI(TAG, "Time: %02d:%02d:%02d",
             time.hour, time.minute, time.second);
    ESP_LOGI(TAG, "Date: %02d/%02d/20%02d",
             time.day, time.month, time.year);
}
```

### Alarm Functions

#### `pcf85063a_set_alarm()`

Set alarm time (generates interrupt when matched).

```c
esp_err_t pcf85063a_set_alarm(pcf85063a_dev_t *dev,
                               const pcf85063a_time_t *alarm_time,
                               bool enable);
```

**Parameters**:
- `dev` - Device structure
- `alarm_time` - Time to trigger alarm (unused fields ignored)
- `enable` - Enable alarm interrupt

**Example**:
```c
// Alarm at 07:00 every day
pcf85063a_time_t alarm = {
    .hour = 7,
    .minute = 0,
    .second = 0
};
pcf85063a_set_alarm(&rtc, &alarm, true);
```

#### `pcf85063a_clear_alarm()`

Clear alarm interrupt flag.

```c
esp_err_t pcf85063a_clear_alarm(pcf85063a_dev_t *dev);
```

---

## Audio - ES8311

Audio codec providing speaker output and microphone input.

### Header Files

```c
#include "bsp_board_extra.h"
#include "esp_codec_dev.h"
```

### Initialization

#### `bsp_extra_codec_init()`

Initialize audio codec (speaker + microphone).

```c
esp_err_t bsp_extra_codec_init(void);
```

**Returns**: `ESP_OK` on success

**Example**:
```c
esp_err_t ret = bsp_extra_codec_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Audio init failed");
}
```

### Audio Configuration

#### `bsp_extra_codec_set_fs()`

Set sample rate, bit depth, and channel mode.

```c
esp_err_t bsp_extra_codec_set_fs(uint32_t rate,
                                  uint32_t bits_cfg,
                                  i2s_slot_mode_t ch);
```

**Parameters**:
- `rate` - Sample rate (8000, 16000, 22050, 44100, 48000 Hz)
- `bits_cfg` - Bit depth (16, 24, 32)
- `ch` - `I2S_SLOT_MODE_MONO` or `I2S_SLOT_MODE_STEREO`

**Example**:
```c
bsp_extra_codec_set_fs(44100, 16, I2S_SLOT_MODE_STEREO);
```

### Volume Control

#### `bsp_extra_codec_volume_set()`

Set speaker volume.

```c
esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set);
```

**Parameters**:
- `volume` - Requested volume (0-100)
- `volume_set` - Actual volume set (output parameter, can be NULL)

**Example**:
```c
int actual_volume;
bsp_extra_codec_volume_set(75, &actual_volume);
ESP_LOGI(TAG, "Volume set to %d%%", actual_volume);
```

#### `bsp_extra_codec_volume_get()`

Get current volume.

```c
int bsp_extra_codec_volume_get(void);
```

**Returns**: Current volume (0-100)

#### `bsp_extra_codec_mute_set()`

Mute or unmute speaker.

```c
esp_err_t bsp_extra_codec_mute_set(bool enable);
```

**Example**:
```c
bsp_extra_codec_mute_set(true);   // Mute
bsp_extra_codec_mute_set(false);  // Unmute
```

### Audio Playback

#### `bsp_extra_i2s_write()`

Write PCM audio data to speaker.

```c
esp_err_t bsp_extra_i2s_write(void *audio_buffer,
                               size_t len,
                               size_t *bytes_written,
                               uint32_t timeout_ms);
```

**Example**:
```c
int16_t audio_buffer[1024];
// ... fill buffer with PCM data ...

size_t bytes_written;
bsp_extra_i2s_write(audio_buffer, sizeof(audio_buffer),
                    &bytes_written, 100);
```

#### `bsp_extra_player_init()`

Initialize audio file player.

```c
esp_err_t bsp_extra_player_init(void);
```

#### `bsp_extra_player_play_file()`

Play audio file from filesystem.

```c
esp_err_t bsp_extra_player_play_file(const char *file_path);
```

**Example**:
```c
bsp_extra_player_init();
bsp_extra_player_play_file("/spiffs/music.wav");
```

### Audio Recording

#### `bsp_extra_i2s_read()`

Read PCM audio data from microphone.

```c
esp_err_t bsp_extra_i2s_read(void *audio_buffer,
                              size_t len,
                              size_t *bytes_read,
                              uint32_t timeout_ms);
```

**Example**:
```c
int16_t record_buffer[1024];
size_t bytes_read;

bsp_extra_i2s_read(record_buffer, sizeof(record_buffer),
                   &bytes_read, 100);

// Process recorded audio data
if (bytes_read > 0) {
    // ... analyze or save audio ...
}
```

### Complete Audio Example

```c
// Initialize
bsp_extra_codec_init();
bsp_extra_codec_set_fs(16000, 16, I2S_SLOT_MODE_STEREO);

// Set volume
int volume;
bsp_extra_codec_volume_set(60, &volume);

// Play audio file
bsp_extra_player_init();
bsp_extra_player_play_file("/spiffs/audio.wav");

// OR record audio
uint8_t *buffer = malloc(4096);
size_t bytes_read;
bsp_extra_i2s_read(buffer, 4096, &bytes_read, 1000);
// ... process audio ...
free(buffer);
```

---

## Power Management - AXP2101

Battery charging, power rail management, and system monitoring.

**Note**: This driver is implemented in C++ (XPowersLib). The template provides C wrapper functions.

### Header Files

```c
// C wrapper functions (defined in port_axp2101.cpp)
extern esp_err_t pmu_init(void);
extern void pmu_print_status(void);
```

### Initialization

#### `pmu_init()`

Initialize power management IC.

```c
esp_err_t pmu_init(void);
```

**Returns**: `ESP_OK` on success

**Example**:
```c
esp_err_t ret = pmu_init();
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "PMU init failed (battery not connected?)");
}
```

### Status Monitoring

#### `pmu_print_status()`

Print all power rail voltages, battery status, and temperature.

```c
void pmu_print_status(void);
```

**Example Output**:
```
I (1234) AXP2101: === Power Management Status ===
I (1235) AXP2101: DC-DC Converters:
I (1236) AXP2101:   DC1: ON  3300 mV
I (1237) AXP2101:   DC2: ON  1100 mV
...
I (1250) AXP2101: Battery:
I (1251) AXP2101:   Voltage: 3850 mV
I (1252) AXP2101:   Charging: Yes
I (1253) AXP2101:   Charge Current: 400 mA
```

### Direct PMU Access (C++)

For advanced control, access the PMU object directly (requires C++ code):

```cpp
extern XPowersPMU PMU;  // Defined in port_axp2101.cpp

// Battery monitoring
uint16_t voltage = PMU.getBatteryVoltage();
bool charging = PMU.isCharging();
uint16_t current = PMU.getBatteryChargeCurrent();
float temp = PMU.getTemperature();

// Power rail control
PMU.setDC1Voltage(3300);  // Set DC1 to 3.3V
PMU.enableDC1();

// Charging configuration
PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
```

**Common Functions**:
- `getBatteryVoltage()` - Read battery voltage (mV)
- `isCharging()` - Check if battery is charging
- `isBatteryConnect()` - Check if battery is connected
- `getVbusVoltage()` - Read USB voltage (mV)
- `getSystemVoltage()` - Read system voltage (mV)
- `getTemperature()` - Read chip temperature (°C)
- `enableVbusVoltageMeasure()` - Enable voltage measurements
- `setPrechargeCurr()` - Set pre-charge current
- `setChargerConstantCurr()` - Set charging current

---

## LVGL Integration

LVGL (Light and Versatile Graphics Library) is integrated via the BSP.

### Basic Usage

```c
// Initialize (done by bsp_display_start())
lv_display_t *disp = bsp_display_start();

// Thread-safe UI updates
bsp_display_lock(0);

// Create UI elements
lv_obj_t *label = lv_label_create(lv_screen_active());
lv_label_set_text(label, "Hello World!");
lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

bsp_display_unlock();
```

### Common Widgets

#### Label
```c
lv_obj_t *label = lv_label_create(parent);
lv_label_set_text(label, "Text");
lv_label_set_text_fmt(label, "Value: %d", value);
```

#### Button
```c
lv_obj_t *btn = lv_btn_create(parent);
lv_obj_t *label = lv_label_create(btn);
lv_label_set_text(label, "Click Me");
lv_obj_add_event_cb(btn, button_callback, LV_EVENT_CLICKED, NULL);
```

#### Slider
```c
lv_obj_t *slider = lv_slider_create(parent);
lv_slider_set_range(slider, 0, 100);
lv_slider_set_value(slider, 50, LV_ANIM_OFF);
```

#### Arc (Gauge)
```c
lv_obj_t *arc = lv_arc_create(parent);
lv_arc_set_range(arc, 0, 100);
lv_arc_set_value(arc, 75);
```

### Styling

```c
static lv_style_t style;
lv_style_init(&style);
lv_style_set_bg_color(&style, lv_color_hex(0xFF0000));
lv_style_set_text_color(&style, lv_color_hex(0xFFFFFF));
lv_obj_add_style(obj, &style, 0);
```

### Animations

```c
lv_anim_t a;
lv_anim_init(&a);
lv_anim_set_var(&a, obj);
lv_anim_set_values(&a, 0, 100);
lv_anim_set_time(&a, 1000);
lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
lv_anim_start(&a);
```

### Performance Tips

1. **Use display locks properly**:
   ```c
   bsp_display_lock(0);
   // Multiple UI updates here
   bsp_display_unlock();
   ```

2. **Optimize refresh rate** (in `sdkconfig.defaults`):
   ```ini
   CONFIG_LV_DEF_REFR_PERIOD=15  # 60 FPS
   ```

3. **Use SPIRAM for buffers** (already configured):
   ```ini
   CONFIG_SPIRAM=y
   CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y
   ```

---

## 📚 Additional Resources

### Component Registry Links

- [ESP32-C6 BSP](https://components.espressif.com/components/waveshare/esp32_c6_touch_amoled_2_06)
- [QMI8658 Driver](https://components.espressif.com/components/waveshare/qmi8658)
- [PCF85063 Driver](https://components.espressif.com/components/waveshare/pcf85063a)
- [LVGL](https://components.espressif.com/components/lvgl/lvgl)

### External Documentation

- [LVGL Docs](https://docs.lvgl.io/master/)
- [ESP-IDF API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/index.html)
- [XPowersLib GitHub](https://github.com/lewisxhe/XPowersLib)

---

**For hardware specifications, see [HARDWARE.md](HARDWARE.md)**
**For quick start guide, see [README.md](README.md)**
