/**
 * Application-wide hardware and feature configuration.
 */

#pragma once

#include "qmi8658.h"

/* Display Configuration */
#define APP_DISPLAY_WIDTH           410
#define APP_DISPLAY_HEIGHT          502

/* I2C Device Addresses */
#define APP_AXP2101_I2C_ADDRESS     0x34
#define APP_QMI8658_I2C_ADDRESS     QMI8658_ADDRESS_HIGH
#define APP_PCF85063_I2C_ADDRESS    0x51
#define APP_FT3168_I2C_ADDRESS      0x38
#define APP_ES8311_I2C_ADDRESS      0x18

/* IMU Configuration */
#define APP_IMU_ACCEL_RANGE         QMI8658_ACCEL_RANGE_8G
#define APP_IMU_ACCEL_ODR           QMI8658_ACCEL_ODR_500HZ
#define APP_IMU_GYRO_RANGE          QMI8658_GYRO_RANGE_512DPS
#define APP_IMU_GYRO_ODR            QMI8658_GYRO_ODR_500HZ

/* Audio Configuration */
#define APP_AUDIO_SAMPLE_RATE       16000
#define APP_AUDIO_BIT_WIDTH         16
#define APP_AUDIO_CHANNELS          2
#define APP_AUDIO_DEFAULT_VOLUME    60
