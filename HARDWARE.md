# Hardware Reference

Complete hardware specification and pin mapping for the **Waveshare ESP32-C6-Touch-AMOLED-2.06** board.

## 📌 GPIO Pin Mapping

### Display Interface (QSPI)

| Pin | GPIO | Function | Description |
|-----|------|----------|-------------|
| CS | GPIO 5 | Chip Select | Display SPI chip select |
| SCLK | GPIO 0 | Clock | SPI clock signal |
| DATA0 | GPIO 1 | Data Line 0 | QSPI data bit 0 |
| DATA1 | GPIO 2 | Data Line 1 | QSPI data bit 1 |
| DATA2 | GPIO 3 | Data Line 2 | QSPI data bit 2 |
| DATA3 | GPIO 4 | Data Line 3 | QSPI data bit 3 |
| RST | GPIO 11 | Reset | Display reset (active low) |

**Notes**:
- QSPI mode provides 4x bandwidth compared to standard SPI
- All data lines operate in parallel for maximum throughput
- Supports up to 40 MHz clock frequency

### Touch Controller (I2C)

| Pin | GPIO | Function | Description |
|-----|------|----------|-------------|
| SDA | GPIO 8 | I2C Data | Shared I2C data line |
| SCL | GPIO 7 | I2C Clock | Shared I2C clock line |
| INT | GPIO 15 | Interrupt | Touch interrupt (active low) |
| RST | GPIO 10 | Reset | Touch reset (active low) |

**I2C Address**: `0x38`

### I2C Bus (Shared)

Multiple peripherals share the same I2C bus:

| Device | I2C Address | GPIO SDA | GPIO SCL | Description |
|--------|-------------|----------|----------|-------------|
| FT3168 (Touch) | 0x38 | GPIO 8 | GPIO 7 | Capacitive touch controller |
| AXP2101 (PMU) | 0x34 | GPIO 8 | GPIO 7 | Power management IC |
| QMI8658 (IMU) | 0x6B* | GPIO 8 | GPIO 7 | 6-axis accelerometer + gyroscope |
| PCF85063 (RTC) | 0x51 | GPIO 8 | GPIO 7 | Real-time clock |
| ES8311 (Audio) | 0x18 | GPIO 8 | GPIO 7 | Audio codec control |

*Note: QMI8658 address can be 0x6A or 0x6B depending on SA0 pin state. This board uses 0x6B (HIGH).

**Bus Configuration**:
- **Clock Speed**: 400 kHz (Fast Mode)
- **Pull-ups**: 4.7kΩ on SDA and SCL
- **Bus Number**: I2C_NUM_0

### Audio Interface (I2S)

| Pin | GPIO | Function | Description |
|-----|------|----------|-------------|
| MCLK | GPIO 19 | Master Clock | 256× sampling frequency |
| BCLK | GPIO 20 | Bit Clock | Serial clock for I2S data |
| LRCK (WS) | GPIO 22 | Word Select | Left/Right channel select |
| DOUT (SDO) | GPIO 23 | Data Out | Audio data to speaker (DAC) |
| DIN (SDI) | GPIO 21 | Data In | Audio data from microphone (ADC) |
| AMP_EN | GPIO 6 | Amplifier Enable | Speaker amplifier power control |

**I2S Configuration**:
- **Mode**: I2S Standard (Philips)
- **Sample Rates**: 8000, 11025, 16000, 22050, 32000, 44100, 48000 Hz
- **Bit Depth**: 16, 24, 32 bits
- **Channels**: Mono or Stereo
- **MCLK**: 256× or 384× sampling frequency

**Audio Codec**: ES8311 (via I2C control at address 0x18)

### Buttons

| Button | GPIO | Function | Active State |
|--------|------|----------|--------------|
| BOOT | GPIO 9 | Boot/User Button | Low (pressed) |
| RESET | - | Hardware Reset | Low (pressed) |

**Usage**:
- **BOOT Button**: Can be used as user input in application (GPIO9)
- **RESET Button**: Hardware reset, cannot be read by software

### USB Interface

| Pin | Function | Description |
|-----|----------|-------------|
| D+ | USB_D_P | USB Data + |
| D- | USB_D_N | USB Data - |
| 5V | VBUS | USB 5V power input |
| GND | Ground | USB ground |

**Features**:
- USB-C connector
- Native USB support (no UART bridge required)
- USB Serial/JTAG for programming and debugging
- USB OTG support

## 🔌 Power System

### Power Management IC (AXP2101)

The AXP2101 manages all power rails and battery charging.

#### DC-DC Converters

| Rail | Voltage Range | Typical Output | Max Current | Usage |
|------|---------------|----------------|-------------|-------|
| DC1 | 1.5V - 3.4V | 3.3V | 2A | External peripherals |
| DC2 | 0.5V - 1.2V | 1.1V | 2A | CPU core voltage |
| DC3 | 0.5V - 3.4V | 3.3V | 2A | System power |
| DC4 | 0.5V - 1.4V | 1.2V | 1.5A | Memory/IO voltage |
| DC5 | 1.2V - 3.7V | 3.3V | 1A | Additional power |

#### LDO Regulators

**ALDO (Analog LDO)**:

| Rail | Voltage Range | Typical Output | Max Current | Usage |
|------|---------------|----------------|-------------|-------|
| ALDO1 | 0.5V - 3.5V | 1.8V | 300mA | Analog devices |
| ALDO2 | 0.5V - 3.5V | 2.8V | 300mA | Analog devices |
| ALDO3 | 0.5V - 3.5V | 3.3V | 300mA | Analog devices |
| ALDO4 | 0.5V - 3.5V | 3.3V | 300mA | Analog devices |

**BLDO (Buck LDO)**:

| Rail | Voltage Range | Typical Output | Max Current | Usage |
|------|---------------|----------------|-------------|-------|
| BLDO1 | 0.5V - 3.5V | 3.3V | 300mA | Display/sensors |
| BLDO2 | 0.5V - 3.5V | 3.3V | 300mA | Display/sensors |

**DLDO (Digital LDO)**:

| Rail | Voltage Range | Typical Output | Max Current | Usage |
|------|---------------|----------------|-------------|-------|
| DLDO1 | 0.5V - 3.4V | 3.3V | 300mA | Digital peripherals |
| DLDO2 | 0.5V - 1.4V | 1.2V | 300mA | Digital peripherals |

**CPUSLDO**:

| Rail | Voltage Range | Typical Output | Max Current | Usage |
|------|---------------|----------------|-------------|-------|
| CPUSLDO | 0.5V - 1.4V | Variable | 30mA | CPU sleep mode |

### Battery Specifications

| Parameter | Specification |
|-----------|--------------|
| **Type** | Lithium Polymer (Li-Po) |
| **Voltage** | 3.7V nominal (4.2V max, 3.0V min) |
| **Connector** | JST 1.25mm 2-pin |
| **Recommended Capacity** | 400mAh - 1000mAh |
| **Recommended Size** | 6mm × 25mm × 25mm (400mAh) |
| **Charging Voltage** | 4.2V (configurable: 4.0V, 4.1V, 4.2V, 4.35V) |
| **Charging Current** | 400mA (configurable: 50mA - 1000mA) |
| **Pre-charge Current** | 50mA (configurable: 25mA, 50mA, 100mA, 150mA) |
| **Termination Current** | 25mA (configurable: 25mA, 50mA, 75mA, 100mA) |

**Safety Features**:
- Over-voltage protection
- Over-current protection
- Over-temperature protection
- Short-circuit protection
- Battery detection

### Power Consumption

| Mode | Typical Current | Voltage | Notes |
|------|----------------|---------|-------|
| **Active (WiFi TX)** | ~200mA | 3.3V | Peak during transmission |
| **Active (Display On)** | ~150mA | 3.3V | AMOLED + CPU |
| **Active (CPU Only)** | ~30mA | 3.3V | No WiFi, display off |
| **Light Sleep** | ~3mA | 3.3V | CPU paused, peripherals off |
| **Deep Sleep** | ~150µA | 3.3V | RTC running, RAM retained |
| **Hibernate** | ~10µA | 3.3V | Minimal power, wake on GPIO |

## 💾 Memory Map

### Flash Partitions

Default partition table ([partitions.csv](partitions.csv)):

| Name | Type | SubType | Offset | Size | Description |
|------|------|---------|--------|------|-------------|
| nvs | data | nvs | 0x9000 | 24KB | Non-volatile storage |
| phy_init | data | phy | 0xF000 | 4KB | PHY initialization data |
| factory | app | factory | 0x10000 | 8MB | Application firmware |
| storage | data | spiffs | - | 7MB | File system (SPIFFS) |

**Total Flash**: 16MB

### RAM

| Type | Size | Usage |
|------|------|-------|
| **SRAM** | 512KB | Main system RAM |
| **LP-SRAM** | 16KB | Low-power SRAM (RTC) |
| **Cache** | 32KB | Instruction cache |
| **PSRAM (External)** | 8MB | Extended memory via SPIRAM |

**PSRAM Configuration**:
- **Mode**: Octal SPI (8-bit data lines)
- **Speed**: 80 MHz
- **Access**: Can store instructions and data
- **Usage**: LVGL buffers, large arrays, audio buffers

## 🔊 Audio Specifications

### Microphones

| Parameter | Specification |
|-----------|--------------|
| **Type** | Digital MEMS microphones |
| **Quantity** | 2 (stereo or beamforming) |
| **Interface** | I2S (ES7210 ADC) |
| **SNR** | >64 dB |
| **Sensitivity** | -26 dBFS |
| **Frequency Response** | 20 Hz - 20 kHz |

### Speaker

| Parameter | Specification |
|-----------|--------------|
| **Type** | Dynamic speaker |
| **Impedance** | 8Ω |
| **Power** | 1W max |
| **Amplifier** | Class D (on AMP_EN GPIO6) |
| **Frequency Response** | 300 Hz - 18 kHz |

## 📺 Display Specifications

### AMOLED Panel

| Parameter | Specification |
|-----------|--------------|
| **Size** | 2.06 inches (diagonal) |
| **Resolution** | 410 × 502 pixels |
| **Pixel Density** | ~300 PPI |
| **Color Depth** | 16.7M colors (24-bit RGB) |
| **Color Format** | RGB565 (16-bit per pixel in framebuffer) |
| **Brightness** | 600 nits (typical), 800 nits (peak) |
| **Contrast Ratio** | >100,000:1 |
| **Viewing Angle** | 178° (H/V) |
| **Response Time** | <10 μs |
| **Refresh Rate** | 60 Hz |
| **Active Area** | 24.48mm × 30.12mm |

### Display Driver (SH8601)

| Parameter | Specification |
|-----------|--------------|
| **Interface** | QSPI (4-bit data) |
| **Max Clock** | 40 MHz |
| **RAM** | 410×502×18 bits (GRAM) |
| **Commands** | MIPI DCS compatible |
| **Color Modes** | RGB565, RGB666, RGB888 |

## 📡 Wireless Specifications

### WiFi

| Feature | Specification |
|---------|--------------|
| **Standards** | 802.11 b/g/n/ax (WiFi 6) |
| **Bands** | 2.4 GHz only |
| **Modes** | Station, SoftAP, Station+SoftAP |
| **TX Power** | 21 dBm max |
| **RX Sensitivity** | -98 dBm @ 11b, -76 dBm @ 11n |
| **Security** | WPA/WPA2/WPA3, WEP, WPS |
| **Features** | OFDMA, TWT (Target Wake Time) |

### Bluetooth

| Feature | Specification |
|---------|--------------|
| **Version** | Bluetooth 5.0 LE |
| **TX Power** | 21 dBm max |
| **RX Sensitivity** | -98 dBm |
| **Modes** | Central, Peripheral, Broadcaster, Observer |
| **Data Rate** | 2 Mbps (LE 2M PHY) |
| **Range** | ~100m (line of sight) |
| **Features** | BLE Mesh, Long Range (LE Coded PHY) |

### Thread/Zigbee

| Feature | Specification |
|---------|--------------|
| **Standard** | IEEE 802.15.4 |
| **Frequency** | 2.4 GHz |
| **Protocols** | Thread 1.3, Zigbee 3.0 |
| **TX Power** | +21 dBm max |
| **RX Sensitivity** | -105 dBm |

## 🌡️ Environmental Specifications

| Parameter | Min | Typical | Max | Unit |
|-----------|-----|---------|-----|------|
| **Operating Temperature** | -20 | 25 | 70 | °C |
| **Storage Temperature** | -40 | - | 85 | °C |
| **Operating Humidity** | 10 | - | 90 | % RH |
| **Operating Voltage** | 3.0 | 3.7 | 4.2 | V |

## 📐 Mechanical Specifications

### Board Dimensions

| Parameter | Measurement |
|-----------|-------------|
| **Length** | 38.0 mm |
| **Width** | 70.0 mm |
| **Thickness** | ~6.0 mm (with battery) |
| **Weight** | ~15g (without battery) |

### Display Active Area

| Parameter | Measurement |
|-----------|-------------|
| **Width** | 24.48 mm |
| **Height** | 30.12 mm |
| **Diagonal** | 2.06 inches (52.3 mm) |

### Mounting

- **Connector**: USB-C (top edge)
- **Battery Connector**: JST 1.25mm (bottom edge)
- **Buttons**: BOOT and RESET (side)
- **Mounting Holes**: None (designed for enclosure or direct use)

## 🔍 Additional Hardware Details

### RTC (PCF85063)

| Feature | Specification |
|---------|--------------|
| **Accuracy** | ±3 ppm @ 25°C |
| **Battery Backup** | Yes (separate coin cell or main battery) |
| **Alarm** | Single alarm with interrupt |
| **Timer** | Countdown timer with interrupt |
| **Calibration** | ±126 ppm (2 ppm steps) |
| **I2C Address** | 0x51 |

### IMU (QMI8658)

**Accelerometer**:

| Parameter | Range | Resolution |
|-----------|-------|-----------|
| **Full Scale** | ±2G, ±4G, ±8G, ±16G | 16-bit |
| **Sensitivity** | 16384, 8192, 4096, 2048 LSB/g | - |
| **Noise Density** | 60 μg/√Hz | @ ±2G |

**Gyroscope**:

| Parameter | Range | Resolution |
|-----------|-------|-----------|
| **Full Scale** | ±16, ±32, ±64, ±128, ±256, ±512, ±1024, ±2048 dps | 16-bit |
| **Sensitivity** | 2048 to 16 LSB/dps | - |
| **Noise Density** | 0.005 dps/√Hz | @ ±512 dps |

**Common**:
- **Interface**: I2C (0x6A or 0x6B)
- **ODR Range**: 8 Hz to 1000 Hz
- **FIFO**: 32-level for both sensors
- **Interrupts**: 2 programmable interrupt pins
- **Temperature Sensor**: Yes (±1°C accuracy)

## ⚡ Electrical Characteristics

### Absolute Maximum Ratings

| Parameter | Min | Max | Unit |
|-----------|-----|-----|------|
| **Supply Voltage** | -0.3 | 6.0 | V |
| **GPIO Voltage** | -0.3 | 3.6 | V |
| **Storage Temperature** | -40 | 125 | °C |
| **ESD (HBM)** | - | 2000 | V |

**WARNING**: Exceeding absolute maximum ratings may damage the device permanently.

### Recommended Operating Conditions

| Parameter | Min | Typical | Max | Unit |
|-----------|-----|---------|-----|------|
| **VDD** | 3.0 | 3.3 | 3.6 | V |
| **IO Voltage** | - | 3.3 | - | V |
| **Operating Temp** | -20 | 25 | 70 | °C |

### GPIO Specifications

| Parameter | Value | Unit |
|-----------|-------|------|
| **Output High (VOH)** | 2.4 (typ 3.3) | V |
| **Output Low (VOL)** | 0.4 | V |
| **Input High (VIH)** | 2.0 | V |
| **Input Low (VIL)** | 0.8 | V |
| **Source Current (IOH)** | 40 | mA |
| **Sink Current (IOL)** | 28 | mA |
| **Pull-up Resistor** | 45 | kΩ |
| **Pull-down Resistor** | 45 | kΩ |

## 📋 Quick Reference Summary

### I2C Device Summary

```
I2C Bus 0 (GPIO7=SCL, GPIO8=SDA):
├── 0x18 - ES8311 (Audio Codec Control)
├── 0x34 - AXP2101 (Power Management)
├── 0x38 - FT3168 (Touch Controller)
├── 0x51 - PCF85063 (Real-Time Clock)
└── 0x6B - QMI8658 (IMU)
```

### SPI/QSPI Device Summary

```
QSPI (Display):
├── CS: GPIO5
├── CLK: GPIO0
├── D0: GPIO1
├── D1: GPIO2
├── D2: GPIO3
├── D3: GPIO4
└── RST: GPIO11
```

### I2S Audio Summary

```
I2S Bus (Audio):
├── MCLK: GPIO19
├── BCLK: GPIO20
├── LRCK: GPIO22
├── DOUT: GPIO23 (to speaker)
├── DIN: GPIO21 (from mic)
└── AMP_EN: GPIO6
```

---

**For software usage, see [README.md](README.md)**
**For driver API documentation, see [DRIVERS.md](DRIVERS.md)**
