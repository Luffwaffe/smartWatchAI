# ESP32-C6-Touch-AMOLED-2.06 Development Template

A comprehensive development template for the **Waveshare ESP32-C6-Touch-AMOLED-2.06** board, demonstrating initialization and usage of all onboard hardware components.

![Board](https://www.waveshare.com/img/devkit/ESP32-C6-Touch-AMOLED-2.06/ESP32-C6-Touch-AMOLED-2.06-2.jpg)

## 📋 Table of Contents

- [Overview](#overview)
- [Hardware Specifications](#hardware-specifications)
- [Quick Start](#quick-start)
- [Hardware Components](#hardware-components)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Usage Examples](#usage-examples)
- [Troubleshooting](#troubleshooting)
- [References](#references)

## 🎯 Overview

This template provides a complete reference implementation for the ESP32-C6-Touch-AMOLED-2.06 board. All hardware components are initialized with extensive inline documentation, making it easy to understand and modify for your own projects.

**Features:**
- ✅ Complete hardware initialization for all onboard components
- ✅ Extensive inline comments explaining each driver
- ✅ Example code for common use cases (commented out by default)
- ✅ LVGL v9 graphics library integration
- ✅ Modular design - easily enable/disable components
- ✅ Production-ready error handling
- ✅ Optimized for performance (SPIRAM, compiler optimization)

## 🔧 Hardware Specifications

### Main Controller
- **MCU**: ESP32-C6 (RISC-V 32-bit, up to 160 MHz)
- **RAM**: 512KB SRAM + 16KB LP-SRAM
- **Flash**: 16MB external flash memory
- **Wireless**: WiFi 6 (802.11ax), Bluetooth 5 (LE), IEEE 802.15.4 (Zigbee/Thread)

### Display & Touch
- **Display**: 2.06" AMOLED, 410×502 pixels, 16.7M colors
- **Driver IC**: SH8601 (CO5300 variant)
- **Interface**: QSPI (4-bit data lines)
- **Brightness**: Up to 600 nits
- **Touch**: FT3168 capacitive touch controller (I2C)
- **Touch Points**: Multi-touch support

### Sensors
- **IMU**: QMI8658 6-axis sensor (I2C)
  - 3-axis accelerometer (±2G to ±16G)
  - 3-axis gyroscope (±16 to ±2048 dps)
- **RTC**: PCF85063 real-time clock (I2C)
  - Battery backup support
  - Alarm and timer functions

### Audio
- **Codec**: ES8311 audio codec (I2S + I2C)
- **Input**: Dual digital microphones
- **Output**: Onboard speaker with amplifier
- **Sample Rates**: 8-48 kHz
- **Bit Depth**: 16/24/32-bit

### Power Management
- **PMIC**: AXP2101 power management IC (I2C)
- **Battery**: Support for 3.7V Li-Po battery (JST connector)
- **Charging**: Configurable charging current (up to 1A)
- **Monitoring**: Voltage, current, and temperature monitoring
- **Power Rails**: Multiple DC-DC and LDO regulators

### Storage
- **Flash**: 16MB for code and assets
- **SPIFFS**: 7MB partition for files (configurable)

## 🚀 Quick Start

### Prerequisites

- **ESP-IDF**: Version 5.3 or later (tested with 5.5.1)
- **Python**: 3.8 or later
- **USB Cable**: USB-C cable for programming

### Installation

1. **Install ESP-IDF** (if not already installed):
   ```bash
   # Follow instructions at:
   # https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/
   ```

2. **Clone or copy this template**:
   ```bash
   cd ~/your-projects
   cp -r path/to/template my-project
   cd my-project
   ```

3. **Set ESP-IDF environment**:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

### Building

```bash
# Set the target (ESP32-C6)
idf.py set-target esp32c6

# Configure the project (optional - defaults are provided)
idf.py menuconfig

# Build the project
idf.py build

# Or use the build script
./build.sh
```

### Flashing

```bash
# Flash to device
idf.py -p /dev/ttyUSB0 flash

# Flash and monitor serial output
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your actual port (Windows: `COM3`, macOS: `/dev/cu.usbserial-*`).

### First Run

After flashing, you should see:
1. Display turns on with AMOLED backlight
2. Simple UI showing available hardware
3. Serial output showing initialization status for all components

## 🧩 Hardware Components

### 1. Display & Touch ([main.c:163](main/main.c#L163))

**Driver**: SH8601 via BSP
**Resolution**: 410×502 pixels
**Interface**: QSPI

```c
// Initialize display
lv_display_t *disp = bsp_display_start();
bsp_display_backlight_on();

// Set brightness (0-100)
bsp_display_brightness_set(80);

// LVGL UI creation
bsp_display_lock(0);
lv_obj_t *label = lv_label_create(lv_screen_active());
lv_label_set_text(label, "Hello World!");
bsp_display_unlock();
```

**Features**:
- Hardware-accelerated rendering via LVGL
- Double-buffering for smooth animations
- Touch input integration
- Power-efficient AMOLED technology

### 2. IMU - Accelerometer & Gyroscope ([main.c:258](main/main.c#L258))

**Sensor**: QMI8658
**Interface**: I2C (0x6B)

```c
// Initialize IMU
qmi8658_init(&imu_device, i2c_bus_handle, QMI8658_ADDRESS_HIGH);

// Read sensor data
qmi8658_data_t data;
qmi8658_read_sensor_data(&imu_device, &data);
ESP_LOGI(TAG, "Accel: X=%.2f Y=%.2f Z=%.2f m/s²",
         data.accelX, data.accelY, data.accelZ);
```

**Use Cases**:
- Motion detection and gesture recognition
- Step counting and activity tracking
- Screen orientation detection
- Vibration monitoring

### 3. Real-Time Clock ([main.c:325](main/main.c#L325))

**IC**: PCF85063
**Interface**: I2C (0x51)

```c
// Initialize RTC
pcf85063a_init(i2c_bus_handle, PCF85063_I2C_ADDRESS, &rtc_device);

// Set time
pcf85063a_time_t time = {
    .year = 24, .month = 12, .day = 15,
    .hour = 14, .minute = 30, .second = 0
};
pcf85063a_set_time(rtc_device, &time);

// Read time
pcf85063a_get_time(rtc_device, &time);
```

**Features**:
- Battery backup (time persists without power)
- Alarm with interrupt
- Timer functionality

### 4. Audio - Speaker & Microphone ([main.c:395](main/main.c#L395))

**Codec**: ES8311
**Interface**: I2S (data) + I2C (control)

```c
// Initialize audio
bsp_extra_codec_init();
bsp_extra_codec_set_fs(16000, 16, I2S_SLOT_MODE_STEREO);

// Set volume (0-100)
int volume;
bsp_extra_codec_volume_set(60, &volume);

// Play audio file
bsp_extra_player_init();
bsp_extra_player_play_file("/spiffs/audio.wav");

// Record audio
uint8_t buffer[1024];
size_t bytes_read;
bsp_extra_i2s_read(buffer, sizeof(buffer), &bytes_read, 100);
```

**Use Cases**:
- Music playback
- Voice recording
- Voice UI / wake word detection
- Audio spectrum analysis
- Real-time audio effects

### 5. Power Management ([main.c:210](main/main.c#L210))

**IC**: AXP2101
**Interface**: I2C (0x34)

```c
// Initialize PMU
pmu_init();

// Print status (voltage, current, temperature)
pmu_print_status();

// Access PMU directly (C++ code)
// PMU.getBatteryVoltage();
// PMU.isCharging();
// PMU.getTemperature();
```

**Features**:
- Battery charging management
- Voltage and current monitoring
- Temperature monitoring
- Multiple power rails (DC-DC, LDO)
- Power event interrupts

## 📁 Project Structure

```
template/
├── main/
│   ├── main.c                      # Main application with all drivers
│   ├── idf_component.yml           # Component dependencies
│   └── CMakeLists.txt              # Build configuration
├── components/
│   ├── XPowersLib/                 # AXP2101 PMU driver (C++)
│   │   ├── src/                    # Library source
│   │   ├── port_axp2101.cpp        # C wrapper for main.c
│   │   └── CMakeLists.txt
│   └── bsp_extra/                  # Audio utilities
│       ├── src/bsp_board_extra.c
│       ├── include/bsp_board_extra.h
│       └── CMakeLists.txt
├── CMakeLists.txt                  # Top-level build config
├── sdkconfig.defaults              # Default ESP-IDF configuration
├── partitions.csv                  # Flash partition table
├── README.md                       # This file
├── HARDWARE.md                     # Detailed pinout reference
└── DRIVERS.md                      # Driver API documentation
```

## ⚙️ Configuration

### Key `sdkconfig.defaults` Settings

```ini
# Performance
CONFIG_COMPILER_OPTIMIZATION_PERF=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y

# SPIRAM (PSRAM)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# LVGL
CONFIG_LV_DEF_REFR_PERIOD=15        # 60 FPS refresh rate
CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=2   # Parallel rendering

# Audio
CONFIG_BSP_I2S_NUM=1
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096

# C++ Support (for XPowersLib)
CONFIG_COMPILER_CXX_EXCEPTIONS=y
CONFIG_COMPILER_CXX_RTTI=y
```

### Customization

**Enable/Disable Components**: Edit [main.c](main/main.c) and comment out components you don't need:

```c
// Disable IMU initialization
// ret = init_imu();
// if (ret != ESP_OK) {
//     ESP_LOGW(TAG, "IMU initialization failed");
// }
```

**Modify Dependencies**: Edit [main/idf_component.yml](main/idf_component.yml):

```yaml
dependencies:
  waveshare/esp32_c6_touch_amoled_2_06: "*"
  # Add your dependencies here
```

## 💡 Usage Examples

### Example 1: Display Sensor Data on Screen

Uncomment the `imu_read_task` in [main.c:462](main/main.c#L462) and enable it in `app_main()`:

```c
xTaskCreate(imu_read_task, "imu_read", 4096, NULL, 5, NULL);
```

### Example 2: Battery Monitoring

Uncomment `pmu_monitor_task` in [main.c:437](main/main.c#L437):

```c
xTaskCreate(pmu_monitor_task, "pmu_monitor", 4096, NULL, 5, NULL);
```

### Example 3: Clock Display

Uncomment `rtc_display_task` in [main.c:506](main/main.c#L506):

```c
xTaskCreate(rtc_display_task, "rtc_display", 4096, NULL, 5, NULL);
```

### Example 4: Audio Playback

1. Place an audio file (WAV format) in the SPIFFS partition
2. Uncomment `audio_playback_example()` in [main.c:543](main/main.c#L543)
3. Call it from `app_main()`

## 🐛 Troubleshooting

### Build Errors

**Problem**: `fatal error: XPowersLib.h: No such file or directory`
**Solution**: Ensure XPowersLib component is in `components/` directory

**Problem**: `Component dependency not found`
**Solution**: Run `idf.py reconfigure` to download managed components

### Runtime Issues

**Problem**: Display stays black
**Solution**:
- Check USB power supply (needs sufficient current)
- Verify `bsp_display_backlight_on()` is called

**Problem**: PMU initialization fails
**Solution**: Battery might not be connected (this is expected and safe to ignore)

**Problem**: Audio not working
**Solution**:
- Ensure I2S pins are not used by other peripherals
- Check that `CONFIG_BSP_I2S_NUM=1` in sdkconfig

### Debugging

Enable verbose logging:
```bash
idf.py menuconfig
# Component config → Log output → Default log verbosity → Verbose
```

Monitor serial output:
```bash
idf.py monitor
```

## 📚 References

### Official Documentation

- [ESP32-C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)
- [LVGL Documentation](https://docs.lvgl.io/master/)

### Board Documentation

- [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-C6-Touch-AMOLED-2.06)
- [Waveshare GitHub](https://github.com/Waveshare/ESP32-C6-Touch-AMOLED-2.06)

### Component Documentation

- [SH8601 Display Driver](https://components.espressif.com/components/waveshare/esp_lcd_sh8601)
- [QMI8658 IMU Driver](https://components.espressif.com/components/waveshare/qmi8658)
- [PCF85063 RTC Driver](https://components.espressif.com/components/waveshare/pcf85063a)
- [ESP32-C6 BSP](https://components.espressif.com/components/waveshare/esp32_c6_touch_amoled_2_06)

### Datasheets

- [ESP32-C6 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-c6_technical_reference_manual_en.pdf)
- [QMI8658 IMU](https://www.qstcorp.com/upload/pdf/QMI8658A%20Datasheet%20Rev.%200.9.pdf)
- [PCF85063 RTC](https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf)
- [ES8311 Audio Codec](https://www.everest-semi.com/pdf/ES8311%20PB.pdf)
- [AXP2101 PMIC](https://github.com/lewisxhe/XPowersLib)

## 📝 License

This template is provided as-is for educational and development purposes. Individual components may have their own licenses:
- ESP-IDF: Apache 2.0
- LVGL: MIT
- XPowersLib: MIT
- Board schematics and hardware design: Waveshare

## 🤝 Contributing

Feel free to:
- Report issues
- Suggest improvements
- Add more examples
- Improve documentation

## 🙏 Acknowledgments

- **Espressif Systems** - ESP-IDF and ESP32-C6
- **Waveshare** - Hardware design and BSP
- **LVGL Team** - Graphics library
- **Community Contributors** - Driver development and testing

---

**Happy Building! 🚀**

For detailed hardware pinout, see [HARDWARE.md](HARDWARE.md)
For driver API documentation, see [DRIVERS.md](DRIVERS.md)
