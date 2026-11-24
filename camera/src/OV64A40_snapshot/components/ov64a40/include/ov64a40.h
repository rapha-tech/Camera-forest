/*
 * SPDX-FileCopyrightText: 2026 esp_cam_sensor_imx contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV64A40_SENSOR_NAME "OV64A40"

/*
 * 7-bit SCCB/I2C address. The OV64A40 is strapped to 0x36
 */
#ifndef OV64A40_SCCB_ADDR
#define OV64A40_SCCB_ADDR   0x36
#endif

/**
 * @brief Probe and initialise an OV64A40 on the given interface.
 *
 * @param config Pointer to esp_cam_sensor_config_t.
 * @return Sensor device handle on success, NULL on failure.
 */
esp_cam_sensor_device_t *ov64a40_detect(esp_cam_sensor_config_t *config);

#ifdef __cplusplus
}
#endif
