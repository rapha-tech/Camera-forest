/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Omnivision OV64A40 (Arducam Owlsight) driver for the ESP32-P4
 * esp_cam_sensor framework.
 *
 * Structure follows the upstream esp_cam_sensor drivers (e.g. ov5647). The
 * OV64A40-specific register values are the standard Omnivision bring-up values, cross
 * checked against the Linux kernel driver (GPL-2.0) but not copied from it.
 */
#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "ov64a40_settings.h"
#include "ov64a40.h"

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms) vTaskDelay((ms > portTICK_PERIOD_MS ? ms / portTICK_PERIOD_MS : 1))

/* ISP timing for the 1920x1080 mode; sensor PPL=880, VTS=1397 at 60 fps. */
#define OV64A40_PIXEL_RATE         300000000 // from linux driver
#define OV64A40_HTS                3520
#define OV64A40_TLINE_NS           11733   /* HTS / pixel_rate, in ns */
/* 2-lane link freq 456 MHz -> 912 Mbps per lane */
#define OV64A40_MIPI_CSI_LINE_RATE 912000000

#define IMX219_LINESYNC_ENABLE    0   /* set 1 to emit line-sync short packets */

static const char *TAG = "ov64a40";

/* ------------------------------------------------------------------ */
/* Supported output formats                                            */
/* ------------------------------------------------------------------ */
enum {
    OV64A40_FMT_1920x1080 = 0,
};
#define OV64A40_FORMAT_COUNT 1

static const esp_cam_sensor_isp_info_t ov64a40_isp_info[] = {
    [OV64A40_FMT_1920x1080] = {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = OV64A40_PIXEL_RATE,
            .hts = OV64A40_HTS,
            .vts = 1397, // taken from linux OV64A40 driver
            .exp_def = IMX219_EXPOSURE_DEFAULT,
            .gain_def = IMX219_ANA_GAIN_DEFAULT,
            .tline_ns = OV64A40_TLINE_NS,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    }
};

static const esp_cam_sensor_format_t imx219_format_info[] = {
    [OV64A40_FMT_1920x1080] = {
        .name = "MIPI_2lane_24Minput_1920x1080",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = IMX219_XCLK_FREQ_HZ,
        .width = 1920,
        .height = 1080,
        .regs = ov64a40_mode_1920x1080_regs,
        .regs_size = ARRAY_SIZE(ov64a40_mode_1920x1080_regs),
        .fps = 60,
        .isp_info = &ov64a40_isp_info[OV64A40_FMT_1920x1080],
        .mipi_info = {
            .mipi_clk = OV64A40_MIPI_CSI_LINE_RATE,
            .lane_num = 2,
            .line_sync_en = IMX219_LINESYNC_ENABLE,
        },
        .reserved = NULL,
    }
};

/*
 * Which mode the sensor comes up in.
 *
 * Index 2 (1632x1232) exists because H.264 codes in 16x16 macroblocks and 1640
 * is 102.5 of them. Video builds want it; everything else is better off with
 * the full 1640-wide field of view, so the default stays index 0 and the video
 * example selects index 2 in its sdkconfig.defaults.
 */
/*
 * Fall back to mode 0 if the Kconfig symbol is absent. It will be, in any build
 * tree whose sdkconfig predates this option and has not been reconfigured -
 * and without this the failure is "CONFIG_..._INDEX_DEFAULT undeclared" from
 * inside the driver, which points at the wrong thing entirely. This way a stale
 * tree quietly keeps the behaviour it already had.
 */
#ifndef CONFIG_CAMERA_OV64A40_MIPI_IF_FORMAT_INDEX_DEFAULT
#define CONFIG_CAMERA_OV64A40_MIPI_IF_FORMAT_INDEX_DEFAULT 0
#endif

#define IMX219_DEFAULT_FORMAT_INDEX CONFIG_CAMERA_OV64A40_MIPI_IF_FORMAT_INDEX_DEFAULT

#if IMX219_DEFAULT_FORMAT_INDEX >= OV64A40_FORMAT_COUNT
#error "CAMERA_OV64A40_MIPI_IF_FORMAT_INDEX_DEFAULT selects an unsupported mode"
#endif

/* ------------------------------------------------------------------ */
/* Gain table                                                          */
/* ------------------------------------------------------------------ */
/*
 * esp_video drives AE gain as a *menu* control: it calls VIDIOC_QUERYMENU to
 * read each entry's total gain and binary-searches for the one nearest the
 * target, then sets the winning INDEX. So ESP_CAM_SENSOR_GAIN has to be an
 * enumeration of gain values in milli-units (1000 = 1.00x) - declaring it as a
 * plain number makes esp_video_cam_query_menu() reject it and the AE then
 * silently drives exposure only, which looks like a dark, grainy picture
 * rather than like an error.
 *
 * IMX219 analog gain is gain = 256 / (256 - code) for code 0..232, i.e. 1.0x
 * to 10.667x. These 42 entries step it at roughly 1/12 stop; the values are
 * the gains the codes actually achieve, not the ideal 1/12-stop targets, so
 * what the AE reads back is what the sensor is really doing.
 */
static const uint32_t imx219_total_gain_val_map[] = {
      1000,   1058,   1123,   1191,   1261,   1333,   1414,   1497,
      1590,   1684,   1778,   1882,   2000,   2116,   2246,   2370,
      2510,   2667,   2813,   3012,   3160,   3368,   3556,   3765,
      4000,   4267,   4491,   4741,   5020,   5333,   5689,   5953,
      6400,   6737,   7111,   7529,   8000,   8533,   8828,   9481,
     10240,  10667,
};

// made by chatgpt
static const uint16_t ov64a40_ana_gain_code_map[] = {
    0x0080, 0x008A, 0x0095, 0x00A1, 0x00AE, 0x00BC,
    0x00CB, 0x00DC, 0x00EE, 0x0102, 0x0118, 0x0130,
    0x014A, 0x0167, 0x0186, 0x01A8, 0x01CE, 0x01F7,
    0x0225, 0x0257, 0x028F, 0x02CD, 0x0312, 0x035E,
    0x03B3, 0x0412, 0x047C, 0x04F3, 0x0578, 0x060D,
    0x06B4, 0x0770, 0x0843, 0x092F, 0x0A37, 0x0B5F,
    0x0CA9, 0x0E1C, 0x0FBB, 0x118C, 0x1394, 0x17FF
};

/* Index whose code (104) is the nearest entry to the gain this driver has
   always written at set_format time, so the menu does not change the picture
   the no-AE path produces. */
#define IMX219_DEFAULT_GAIN_INDEX 9

/* ------------------------------------------------------------------ */
/* Bayer phase                                                         */
/* ------------------------------------------------------------------ */
/*
 * Bayer phase by flip state, indexed (hmirror << 1) | vflip, matching the
 * Linux OV64A40 driver's media-bus code selection.
 *
 * A flip does not mirror the picture behind a fixed colour mosaic. It reverses
 * the order pixels leave the array in, so the 2x2 tile the ISP has to
 * demosaic rotates with it. Report the wrong one and red and blue swap while
 * the greens zipper - and nothing anywhere returns an error, the frame is
 * simply wrong. Raspberry Pi's driver does the same thing by swapping the
 * media-bus code across four entries.
 *
 * The Linux driver reports SBGGR10 with no flip, then SGRBG, SGBRG, and SRGGB
 * for indices 1 through 3. The 1920x1080 readout starts at even x/y offsets,
 * so its CFA phase agrees with that base order.
 */
static const esp_cam_sensor_bayer_pattern_t imx219_bayer_by_flip[4] = {
    [0] = ESP_CAM_SENSOR_BAYER_BGGR,    /* no flip */
    [1] = ESP_CAM_SENSOR_BAYER_GRBG,    /* vflip   */
    [2] = ESP_CAM_SENSOR_BAYER_GBRG,    /* hmirror */
    [3] = ESP_CAM_SENSOR_BAYER_RGGB,    /* both    */
};

/*
 * Per-device state.
 *
 * `format` and `isp_info` shadow whichever entry of imx219_format_info[] is in
 * use, and dev->cur_format points at the shadow rather than into the table.
 * That indirection is what lets bayer_type follow the flip bits: the static
 * table is const and shared between every device, while esp_video reads
 * bayer_type straight out of whatever cur_format points at. Every other field
 * is a verbatim copy of the static entry.
 *
 * The shadow lives in the same allocation as the device (see ov64a40_detect),
 * so it is valid for exactly as long as anything can hold a pointer to it.
 */
typedef struct {
    uint32_t exposure_val;      /*!< current exposure, in lines */
    uint32_t gain_index;        /*!< index into imx219_total_gain_val_map */
    uint8_t  hmirror;           /*!< 0x3821 bit 2 (active low) */
    uint8_t  vflip;             /*!< 0x3820 bit 2 */
    esp_cam_sensor_format_t   format;
    esp_cam_sensor_isp_info_t isp_info;
} imx219_para_t;

/* ------------------------------------------------------------------ */
/* SCCB helpers (16-bit register address, 8-bit value)                 */
/* ------------------------------------------------------------------ */
static esp_err_t ov64a40_read(esp_sccb_io_handle_t sccb, uint16_t reg, uint8_t *val)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb, reg, val);
}

static esp_err_t ov64a40_read_array(esp_sccb_io_handle_t sccb, uint16_t reg, uint8_t *val, size_t reg_val_size)
{
    return esp_sccb_transmit_receive_reg_a16(sccb, reg, val, reg_val_size);
}

static esp_err_t ov64a40_write(esp_sccb_io_handle_t sccb, uint16_t reg, uint8_t val)
{
    return esp_sccb_transmit_reg_a16v8(sccb, reg, val);
}

static esp_err_t ov64a40_write_array_manual(esp_sccb_io_handle_t sccb, uint16_t reg, uint8_t *val, size_t reg_val_size)
{
    return esp_sccb_transmit_reg_a16(sccb, reg, val, reg_val_size);
}

/* Write a 16-bit value big-endian to reg / reg+1 */
static esp_err_t ov64a40_write16(esp_sccb_io_handle_t sccb, uint16_t reg, uint16_t val)
{
    esp_err_t ret = ov64a40_write(sccb, reg, (val >> 8) & 0xff);
    if (ret == ESP_OK) {
        ret = ov64a40_write(sccb, reg + 1, val & 0xff);
    }
    return ret;
}

static esp_err_t ov64a40_write_array(esp_sccb_io_handle_t sccb, const ov64a40_reginfo_t *regs)
{
    esp_err_t ret = ESP_OK;
    int i = 0;
    while (ret == ESP_OK && regs[i].reg != OV64A40_REG_END) {
        if (regs[i].reg == IMX219_REG_DELAY) {
            delay_ms(regs[i].val);
        } else {
            ret = ov64a40_write(sccb, regs[i].reg, regs[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "wrote %d regs", i);
    return ret;
}

static esp_err_t ov64a40_set_reg_bits(esp_sccb_io_handle_t sccb, uint16_t reg,
                                     uint8_t offset, uint8_t length, uint8_t value)
{
    uint8_t reg_val = 0;
    esp_err_t ret = ov64a40_read(sccb, reg, &reg_val);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    reg_val = (reg_val & ~mask) | ((value << offset) & mask);
    return ov64a40_write(sccb, reg, reg_val);
}

/* ------------------------------------------------------------------ */
/* Sensor operations                                                   */
/* ------------------------------------------------------------------ */
static esp_err_t ov64a40_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    uint8_t buff_id[3];
    esp_err_t ret = ov64a40_read_array(dev->sccb_handle, OV64A40_REG_CHIP_ID, buff_id, 3);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read chip id failed");

    id->pid = (buff_id[1] << 8) | buff_id[2];
    return ESP_OK;
}

static esp_err_t ov64a40_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ov64a40_write(dev->sccb_handle, OV64A40_REG_SMIA, enable ? OV64A40_REG_SMIA_STREAMING : 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "set stream failed");
    dev->stream_status = enable;
    ESP_LOGD(TAG, "stream=%d", enable);
    return ret;
}

static esp_err_t ov64a40_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ov64a40_set_reg_bits(dev->sccb_handle, OV64A40_REG_SW_RESET, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t imx219_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);   /* datasheet t4+t5 ~6.2ms; be generous */
    }
    return ESP_OK;
}

/*
 * Push the cached flip state to the sensor and to the shadow isp_info.
 *
 * Update only the orientation bits: the neighboring bits control binning and
 * pixel timing. OV64A40 horizontal flip is active-low; vertical flip is active-high.
 *
 * Note what the ISP does and does not see. esp_video latches bayer_type into
 * its own CSI state inside update_format_config(), which runs at device open
 * and again on VIDIOC_S_SENSOR_FMT - not at STREAMON, and not when a control
 * is set. So the sensor obeys a flip immediately, while the ISP keeps
 * demosaicing on the phase it latched. Set flips before opening the device or
 * before the S_SENSOR_FMT that selects the mode; imx219_set_orientation()
 * warns when it is too late for that.
 */
static esp_err_t imx219_apply_orientation(esp_cam_sensor_device_t *dev)
{
    imx219_para_t *para = (imx219_para_t *)dev->priv;
    if (para == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ov64a40_set_reg_bits(dev->sccb_handle, OV64A40_REG_TIMING_CTRL_21,
                                         2, 1, para->hmirror ? 0 : 1);
    if (ret == ESP_OK) {
        ret = ov64a40_set_reg_bits(dev->sccb_handle, OV64A40_REG_TIMING_CTRL_20,
                                   2, 1, para->vflip);
    }
    if (ret != ESP_OK) {
        return ret;
    }

    para->isp_info.isp_v1_info.bayer_type =
        imx219_bayer_by_flip[(para->hmirror ? 2 : 0) | (para->vflip ? 1 : 0)];
    return ESP_OK;
}

static esp_err_t imx219_set_orientation(esp_cam_sensor_device_t *dev, int hmirror, int vflip)
{
    imx219_para_t *para = (imx219_para_t *)dev->priv;

    if (para == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    para->hmirror = hmirror ? 1 : 0;
    para->vflip = vflip ? 1 : 0;

    if (dev->stream_status) {
        ESP_LOGW(TAG, "flip changed while streaming: the sensor obeys now, but the ISP keeps "
                 "the Bayer phase it latched - re-set the sensor format to pick it up");
    }
    return imx219_apply_orientation(dev);
}

/*
 * Largest legal coarse integration time for the mode currently selected.
 *
 * Unlike the IMX708, whose VTS is fixed, the IMX219's frame length differs per
 * mode (1763 binned, 3526 full), so the ceiling has to come from the mode
 * rather than from a constant. Exposing 65535 - the register width - would let
 * the AE drive integration time past the frame length, which the sensor cannot
 * honour: the value silently does not take effect and the AE loop then sees no
 * response to its own request.
 */
static uint32_t ov64a40_exposure_max(esp_cam_sensor_device_t *dev)
{
    uint32_t max = OV64A40_EXPOSURE_MAX;

    if (dev->cur_format && dev->cur_format->isp_info) {
        uint32_t vts = dev->cur_format->isp_info->isp_v1_info.vts;

        if (vts > (OV64A40_EXPOSURE_MARGIN + OV64A40_EXPOSURE_MIN)) {
            max = vts - OV64A40_EXPOSURE_MARGIN;
        }
    }

    return max > OV64A40_EXPOSURE_MAX ?
           OV64A40_EXPOSURE_MAX : max;
}

/* Exposure in lines (coarse integration time). */
static esp_err_t ov64a40_set_exposure(esp_cam_sensor_device_t *dev, uint32_t lines)
{
    uint32_t max = ov64a40_exposure_max(dev);
    if (lines < OV64A40_EXPOSURE_MIN) {
        lines = OV64A40_EXPOSURE_MIN;
    }
    if (lines > max) {
        lines = max;
    }
    uint8_t buff[3] = {
        (uint8_t)(lines >> 16),
        (uint8_t)(lines >> 8),
        (uint8_t)lines
    };

    esp_err_t ret = ov64a40_write_array_manual(dev->sccb_handle, OV64A40_REG_MEC_LONG_EXPO, buff, 3);
    if (ret == ESP_OK && dev->priv) {
        ((imx219_para_t *)dev->priv)->exposure_val = lines;
    }
    return ret;
}

/* Analog gain: 8-bit code, gain = 256 / (256 - code). */
static esp_err_t imx219_set_analog_gain(esp_cam_sensor_device_t *dev, uint32_t code)
{
    if (code > IMX219_ANA_GAIN_MAX) {
        code = IMX219_ANA_GAIN_MAX;
    }
    return ov64a40_write(dev->sccb_handle, IMX219_REG_ANALOG_GAIN, (uint8_t)code);
}

/* Total gain by menu index - what the ISP's AE actually calls. */
static esp_err_t ov64a40_set_gain_index(esp_cam_sensor_device_t *dev, uint32_t index)
{
    if (index >= ARRAY_SIZE(ov64a40_ana_gain_code_map)) {
        index = ARRAY_SIZE(ov64a40_ana_gain_code_map) - 1;
    }

    uint16_t gain = ov64a40_ana_gain_code_map[index];

    if (gain < OV64A40_ANA_GAIN_MIN) {
        gain = OV64A40_ANA_GAIN_MIN;
    }

    if (gain > OV64A40_ANA_GAIN_MAX) {
        gain = OV64A40_ANA_GAIN_MAX;
    }
    
    esp_err_t ret = ov64a40_write16(dev->sccb_handle, OV64A40_REG_MEC_LONG_GAIN, gain);
    if (ret == ESP_OK && dev->priv) {
        ((imx219_para_t *)dev->priv)->gain_index = index;
    }
    return ret;
}

/* Digital gain: 16-bit, 0x0100 = 1.0x. */
static esp_err_t imx219_set_digital_gain(esp_cam_sensor_device_t *dev, uint32_t val)
{
    if (val < IMX219_DGTL_GAIN_MIN) {
        val = IMX219_DGTL_GAIN_MIN;
    }
    if (val > IMX219_DGTL_GAIN_MAX) {
        val = IMX219_DGTL_GAIN_MAX;
    }
    return ov64a40_write16(dev->sccb_handle, IMX219_REG_DIGITAL_GAIN_H, (uint16_t)val);
}

static esp_err_t imx219_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return ov64a40_write(dev->sccb_handle, OV64A40_REG_TEST_PATTERN,
                         enable ? OV64A40_TEST_PATTERN_TYPE1 : OV64A40_TEST_PATTERN_DISABLED);
}

static esp_err_t imx219_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = IMX219_EXPOSURE_MIN;
        /* Report what set_exposure will actually accept, so the AE's model of
           the control matches the sensor's behaviour. */
        qdesc->number.maximum = ov64a40_exposure_max(dev);
        qdesc->number.step = 1;
        qdesc->default_value = IMX219_EXPOSURE_DEFAULT;
        break;
    case ESP_CAM_SENSOR_GAIN:
        /* Menu control: elements are total gain in milli-units, and the value
           set later is an INDEX into this table, not a register code. */
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = ARRAY_SIZE(imx219_total_gain_val_map);
        qdesc->enumeration.elements = imx219_total_gain_val_map;
        qdesc->default_value = IMX219_DEFAULT_GAIN_INDEX;
        break;
    case ESP_CAM_SENSOR_ANGAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = IMX219_ANA_GAIN_MIN;
        qdesc->number.maximum = IMX219_ANA_GAIN_MAX;
        qdesc->number.step = 1;
        qdesc->default_value = IMX219_ANA_GAIN_DEFAULT;
        break;
    case ESP_CAM_SENSOR_DGAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = IMX219_DGTL_GAIN_MIN;
        qdesc->number.maximum = IMX219_DGTL_GAIN_MAX;
        qdesc->number.step = 1;
        qdesc->default_value = IMX219_DGTL_GAIN_DEFAULT;
        break;
    default:
        ESP_LOGD(TAG, "id=%" PRIx32 " not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ret;
}

static esp_err_t imx219_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    imx219_para_t *para = (imx219_para_t *)dev->priv;

    if (para == NULL || arg == NULL || size < sizeof(uint32_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        *(uint32_t *)arg = para->exposure_val;
        break;
    case ESP_CAM_SENSOR_GAIN:
        *(uint32_t *)arg = para->gain_index;
        break;
    case ESP_CAM_SENSOR_HMIRROR:
        *(uint32_t *)arg = para->hmirror;
        break;
    case ESP_CAM_SENSOR_VFLIP:
        *(uint32_t *)arg = para->vflip;
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t imx219_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    if (arg == NULL || size < sizeof(uint32_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "set para value id: %d", id);
    switch(id){
    case ESP_CAM_SENSOR_VFLIP: {
        const imx219_para_t *para = (const imx219_para_t *)dev->priv;
        ret = imx219_set_orientation(dev, para ? para->hmirror : 0, *(const int *)arg);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        const imx219_para_t *para = (const imx219_para_t *)dev->priv;
        ret = imx219_set_orientation(dev, *(const int *)arg, para ? para->vflip : 0);
        break;
    }
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        ret = ov64a40_set_exposure(dev, *(const uint32_t *)arg);
        break;
    case ESP_CAM_SENSOR_EXPOSURE_US: {
        uint32_t us = *(const uint32_t *)arg;
        uint32_t lines = (uint32_t)(((uint64_t)us * 1000) / OV64A40_TLINE_NS);
        ret = ov64a40_set_exposure(dev, lines);
        break;
    }
    case ESP_CAM_SENSOR_GAIN:
        ret = ov64a40_set_gain_index(dev, *(const uint32_t *)arg);
        break;
    case ESP_CAM_SENSOR_ANGAIN:
        ret = imx219_set_analog_gain(dev, *(const uint32_t *)arg);
        break;
    case ESP_CAM_SENSOR_GROUP_EXP_GAIN: {
        /* The ISP prefers this: exposure and gain applied together, so a frame
           never lands with one updated and the other not. */
        const esp_cam_sensor_gh_exp_gain_t *g = (const esp_cam_sensor_gh_exp_gain_t *)arg;
        uint32_t lines;
        if (g->exposure_val != 0) {
            lines = g->exposure_val;
        } else if (g->exposure_us != 0) {
            lines = (uint32_t)(((uint64_t)g->exposure_us * 1000) / OV64A40_TLINE_NS);
        } else {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        ret = ov64a40_set_exposure(dev, lines);
        if (ret == ESP_OK) {
            ret = ov64a40_set_gain_index(dev, g->gain_index);
        }
        break;
    }
    case ESP_CAM_SENSOR_DGAIN:
        ret = imx219_set_digital_gain(dev, *(const uint32_t *)arg);
        break;
    default:
        ESP_LOGE(TAG, "set id=%" PRIx32 " not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ret;
}

static esp_err_t imx219_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);
    formats->count = ARRAY_SIZE(imx219_format_info);
    formats->format_array = &imx219_format_info[0];
    return ESP_OK;
}

static esp_err_t imx219_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *caps)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, caps);
    caps->fmt_raw = 1;
    return ESP_OK;
}

/*
 * Point dev->cur_format at the per-device shadow of `format` rather than into
 * imx219_format_info[], so that imx219_apply_orientation() has a bayer_type it
 * is allowed to write. Callers hold this pointer indefinitely - esp_video
 * caches it in its own device state - so the shadow has to outlive them, which
 * it does by sitting in the device's own allocation.
 *
 * The static table stays the menu: imx219_format_by_index() and
 * query_support_formats() still hand out entries from it, and those entries
 * are still what a caller passes back in here.
 */
static esp_err_t ov64a40_select_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    imx219_para_t *para = (imx219_para_t *)dev->priv;

    if (para == NULL) {
        dev->cur_format = format;
        return ESP_ERR_INVALID_STATE;
    }

    para->format = *format;
    if (format->isp_info) {
        /*
         * memcpy, not assignment: isp_v1_info.version is declared const, which
         * makes the whole union non-assignable. The destination is inside the
         * device's calloc'd block, so it has no declared type of its own and
         * the copy is what gives it one.
         *
         * Skipped when the source is already the shadow. An application that
         * reads the current format back with VIDIOC_G_SENSOR_FMT and hands it
         * straight to VIDIOC_S_SENSOR_FMT - the way to make esp_video re-latch
         * the Bayer phase after a flip - arrives here with isp_info pointing at
         * the very object being written, and memcpy may not overlap.
         */
        if (format->isp_info != &para->isp_info) {
            memcpy(&para->isp_info, format->isp_info, sizeof(para->isp_info));
        }
        para->format.isp_info = &para->isp_info;
    }
    dev->cur_format = &para->format;
    return ESP_OK;
}

static esp_err_t imx219_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;

    if (format == NULL) {
        format = &imx219_format_info[IMX219_DEFAULT_FORMAT_INDEX];
    }

    /* Software reset, then apply: common -> 2-lane PLL -> pixel format -> mode. */
    ret = ov64a40_soft_reset(dev);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "soft reset failed");
    ret = ov64a40_write_array(dev->sccb_handle, ov64a40_common_regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write common regs failed");
    ret = ov64a40_write_array(dev->sccb_handle, ov64a40_2lane_regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write 2-lane regs failed");
    ret = ov64a40_write_array(dev->sccb_handle, (const ov64a40_reginfo_t *)format->regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write mode regs failed");

    /* Match Linux's masked subsampling updates; preserve flip and timing bits. */
    ret = ov64a40_set_reg_bits(dev->sccb_handle, OV64A40_REG_TIMING_CTRL_20, 1, 1, 1);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "set vertical binning failed");
    ret = ov64a40_set_reg_bits(dev->sccb_handle, OV64A40_REG_TIMING_CTRL_21, 4, 1, 1);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "set horizontal binning failed");
    ret = ov64a40_set_reg_bits(dev->sccb_handle, OV64A40_REG_TIMING_CTRL_21, 0, 1, 1);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "set horizontal data speed failed");

    /* Before the exposure write: its ceiling is derived from the mode's VTS,
       so cur_format has to already be the mode whose registers just went in. */
    ret = ov64a40_select_format(dev, format);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "no device state to shadow the format into");

    /*
     * After the tables, because the soft reset above clears the orientation
     * register, and after the shadow is rebuilt, because rebuilding it from the
     * const table puts bayer_type back to the unflipped phase.
     */
    ret = imx219_apply_orientation(dev);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "apply orientation failed");

    /* Sensible default exposure so the first frames are not black. */
    ov64a40_set_exposure(dev, IMX219_EXPOSURE_DEFAULT);
    ov64a40_set_gain_index(dev, IMX219_DEFAULT_GAIN_INDEX);

    ESP_LOGI(TAG, "set format: %s", format->name);
    return ret;
}

static esp_err_t imx219_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, format);
    if (dev->cur_format == NULL) {
        return ESP_FAIL;
    }
    memcpy(format, dev->cur_format, sizeof(esp_cam_sensor_format_t));
    return ESP_OK;
}

static esp_err_t imx219_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;
    uint8_t regval = 0;
    esp_cam_sensor_reg_val_t *sensor_reg;

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = imx219_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = ov64a40_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = ov64a40_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = imx219_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov64a40_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov64a40_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = ov64a40_get_sensor_id(dev, (esp_cam_sensor_id_t *)arg);
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ret;
}

static esp_err_t ov64a40_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pwdn pin config failed");
        gpio_set_level(dev->pwdn_pin, 1);   /* power up */
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "reset pin config failed");
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);   /* >= 6.2ms before first I2C access */
    }

    return ret;
}

static esp_err_t imx219_power_off(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
    }
    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
    }
    return ESP_OK;
}

static esp_err_t imx219_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del imx219 (%p)", dev);
    if (dev) {
        free(dev);
    }
    return ESP_OK;
}

static const esp_cam_sensor_ops_t imx219_ops = {
    .query_para_desc = imx219_query_para_desc,
    .get_para_value = imx219_get_para_value,
    .set_para_value = imx219_set_para_value,
    .query_support_formats = imx219_query_support_formats,
    .query_support_capability = imx219_query_support_capability,
    .set_format = imx219_set_format,
    .get_format = imx219_get_format,
    .priv_ioctl = imx219_priv_ioctl,
    .del = imx219_delete,
};

esp_cam_sensor_device_t *ov64a40_detect(esp_cam_sensor_config_t *config)
{
    ESP_LOGI(TAG, "starting init");

    if (config == NULL) {
        return NULL;
    }

    esp_cam_sensor_device_t *dev = calloc(1, sizeof(esp_cam_sensor_device_t) + sizeof(imx219_para_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "no memory for camera device");
        return NULL;
    }
    dev->priv = (uint8_t *)dev + sizeof(esp_cam_sensor_device_t);

    dev->name = (char *)OV64A40_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &imx219_ops;
    /* Shadow the default mode straight away: esp_video reads bayer_type out of
       cur_format at device open, which can happen without a set_format call. */
    ov64a40_select_format(dev, &imx219_format_info[IMX219_DEFAULT_FORMAT_INDEX]);

    ESP_LOGI(TAG, "format selected");

    if (config->sensor_port != ESP_CAM_SENSOR_MIPI_CSI) {
        ESP_LOGE(TAG, "only MIPI-CSI is supported");
        goto err;
    }

    if (ov64a40_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "power on failed");
        goto err;
    }

    ESP_LOGI(TAG, "sensor powered on");

    if (ov64a40_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "get chip id failed");
        goto err;
    }
    if (dev->id.pid != OV64A40_CHIP_ID) {
        ESP_LOGE(TAG, "sensor is not OV64A40, PID=0x%04x", dev->id.pid);
        goto err;
    }
    ESP_LOGI(TAG, "detected OV64A40, PID=0x%04x", dev->id.pid);
    return dev;

err:
    imx219_power_off(dev);
    free(dev);
    return NULL;
}

#if CONFIG_CAMERA_OV64A40_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(ov64a40_detect, ESP_CAM_SENSOR_MIPI_CSI, OV64A40_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return ov64a40_detect(config);
}
#endif
