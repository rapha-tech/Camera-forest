#pragma once

#include <stdint.h>
#include "esp_cam_sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Sentinels used inside imx219_reginfo_t arrays */
#define IMX219_REG_END              0xffff  /*!< marks end of a register array   */
#define IMX219_REG_DELAY            0xfffe  /*!< .val is a delay in milliseconds */

/* Identification */
#define IMX219_REG_CHIP_ID_H        0x0000  /*!< 16-bit chip id, big endian */
#define IMX219_REG_CHIP_ID_L        0x0001
#define IMX219_CHIP_ID              0x0219

/* Core control */
#define IMX219_REG_MODE_SELECT      0x0100  /*!< 0=standby, 1=streaming */
#define IMX219_REG_SW_RESET         0x0103  /*!< bit0 = software reset */
#define IMX219_REG_CSI_LANE_MODE    0x0114  /*!< 0x01 = 2 lane, 0x03 = 4 lane */
#define IMX219_CSI_2_LANE_MODE      0x01
#define IMX219_CSI_4_LANE_MODE      0x03
#define IMX219_REG_DPHY_CTRL        0x0128  /*!< 0 = auto timing */
#define IMX219_REG_EXCK_FREQ_H      0x012a  /*!< external clock freq = MHz*256 */
#define IMX219_REG_EXCK_FREQ_L      0x012b

/* Exposure / gain */
#define IMX219_REG_ANALOG_GAIN      0x0157  /*!< 8-bit, gain = 256/(256-code)      */
#define IMX219_ANA_GAIN_MIN         0
#define IMX219_ANA_GAIN_MAX         232
#define IMX219_ANA_GAIN_DEFAULT     104     /*!< 1.684x; == imx219_ana_gain_code_map[9] */
#define IMX219_REG_DIGITAL_GAIN_H   0x0158  /*!< 16-bit, 0x0100 = 1.0x            */
#define IMX219_REG_DIGITAL_GAIN_L   0x0159
#define IMX219_DGTL_GAIN_MIN        0x0100
#define IMX219_DGTL_GAIN_MAX        0x0fff
#define IMX219_DGTL_GAIN_DEFAULT    0x0100
#define IMX219_REG_EXPOSURE_H       0x015a  /*!< 16-bit, coarse integration time in lines */
#define IMX219_REG_EXPOSURE_L       0x015b
#define IMX219_EXPOSURE_MIN         4
#define IMX219_EXPOSURE_MAX         65535   /*!< register width only; the real
                                                 ceiling is VTS - OFFSET       */
#define IMX219_EXPOSURE_OFFSET      4       /*!< coarse integration time must
                                                 stay <= VTS - 4               */
#define IMX219_EXPOSURE_DEFAULT     0x0640

/* Frame timing */
#define IMX219_REG_VTS_H            0x0160  /*!< frame length lines (VTS) */
#define IMX219_REG_VTS_L           0x0161
#define IMX219_REG_LINE_LENGTH_H   0x0162  /*!< line length pixels (HTS) */
#define IMX219_REG_LINE_LENGTH_L   0x0163

/* Windowing */
#define IMX219_REG_X_ADD_STA_H     0x0164
#define IMX219_REG_X_ADD_STA_L     0x0165
#define IMX219_REG_X_ADD_END_H     0x0166
#define IMX219_REG_X_ADD_END_L     0x0167
#define IMX219_REG_Y_ADD_STA_H     0x0168
#define IMX219_REG_Y_ADD_STA_L     0x0169
#define IMX219_REG_Y_ADD_END_H     0x016a
#define IMX219_REG_Y_ADD_END_L     0x016b
#define IMX219_REG_X_OUTPUT_SIZE_H 0x016c
#define IMX219_REG_X_OUTPUT_SIZE_L 0x016d
#define IMX219_REG_Y_OUTPUT_SIZE_H 0x016e
#define IMX219_REG_Y_OUTPUT_SIZE_L 0x016f
#define IMX219_REG_X_ODD_INC       0x0170
#define IMX219_REG_Y_ODD_INC       0x0171
#define IMX219_REG_ORIENTATION     0x0172  /*!< bit0 = h flip, bit1 = v flip */
#define IMX219_ORIENTATION_HMIRROR 0x01
#define IMX219_ORIENTATION_VFLIP   0x02
#define IMX219_REG_BINNING_MODE_H  0x0174
#define IMX219_REG_BINNING_MODE_L  0x0175
#define IMX219_BINNING_NONE        0x0000
#define IMX219_BINNING_2X2_NORMAL  0x0101
#define IMX219_BINNING_2X2_SPECIAL 0x0303

/* Output data format */
#define IMX219_REG_CSI_DATA_FORMAT_H 0x018c  /*!< 0x0a0a = RAW10, 0x0808 = RAW8 */
#define IMX219_REG_CSI_DATA_FORMAT_L 0x018d

/* PLL clock dividers */
#define IMX219_REG_VTPXCK_DIV        0x0301
#define IMX219_REG_VTSYCK_DIV        0x0303
#define IMX219_REG_PREPLLCK_VT_DIV   0x0304
#define IMX219_REG_PREPLLCK_OP_DIV   0x0305
#define IMX219_REG_PLL_VT_MPY_H      0x0306
#define IMX219_REG_PLL_VT_MPY_L      0x0307
#define IMX219_REG_OPPXCK_DIV        0x0309
#define IMX219_REG_OPSYCK_DIV        0x030b
#define IMX219_REG_PLL_OP_MPY_H      0x030c
#define IMX219_REG_PLL_OP_MPY_L      0x030d

/* Test pattern */
#define IMX219_REG_TEST_PATTERN_H    0x0600
#define IMX219_REG_TEST_PATTERN_L    0x0601
#define IMX219_TEST_PATTERN_DISABLE  0x0000
#define IMX219_TEST_PATTERN_COLORBARS 0x0002


typedef struct {
    uint16_t reg;   /*!< register address, or IMX219_REG_END / IMX219_REG_DELAY */
    uint8_t  val;   /*!< value to write, or delay in ms for IMX219_REG_DELAY   */
} imx219_reginfo_t;

/* XCLK is 24 MHz on the Raspberry Pi camera modules. EXCK_FREQ = 24 * 256. */
#define IMX219_XCLK_FREQ_HZ   24000000

/*
 * Common registers, applied before any mode. Access-enable sequence for the
 * 0x3000-0x5fff range, undocumented tuning registers, line length (HTS=3448),
 * pixel increments and D-PHY/clock setup.
 */
static const imx219_reginfo_t imx219_common_regs[] = {
    {IMX219_REG_MODE_SELECT, 0x00},          /* ensure standby */

    /* Access code to reach registers 0x3000-0x5fff */
    {0x30eb, 0x05}, {0x30eb, 0x0c},
    {0x300a, 0xff}, {0x300b, 0xff},
    {0x30eb, 0x05}, {0x30eb, 0x09},

    /* Undocumented Sony tuning registers */
    {0x455e, 0x00}, {0x471e, 0x4b}, {0x4767, 0x0f}, {0x4750, 0x14},
    {0x4540, 0x00}, {0x47b4, 0x14}, {0x4713, 0x30}, {0x478b, 0x10},
    {0x478f, 0x10}, {0x4793, 0x10}, {0x4797, 0x0e}, {0x479b, 0x0e},

    /* Line length = 3448 (0x0d78) */
    {IMX219_REG_LINE_LENGTH_H, 0x0d}, {IMX219_REG_LINE_LENGTH_L, 0x78},
    {IMX219_REG_X_ODD_INC, 0x01},
    {IMX219_REG_Y_ODD_INC, 0x01},

    /* D-PHY auto timing; EXCK_FREQ = 24MHz * 256 = 0x1800 */
    {IMX219_REG_DPHY_CTRL, 0x00},
    {IMX219_REG_EXCK_FREQ_H, 0x18}, {IMX219_REG_EXCK_FREQ_L, 0x00},

    {IMX219_REG_END, 0x00},
};

/*
 * 2-lane PLL configuration. Fixed link frequency of 456 MHz (912 Mbps/lane)
 * across all modes; pixel rate 182.4 MHz.
 */
static const imx219_reginfo_t imx219_2lane_regs[] = {
    {IMX219_REG_VTPXCK_DIV, 5},
    {IMX219_REG_VTSYCK_DIV, 1},
    {IMX219_REG_PREPLLCK_VT_DIV, 3},
    {IMX219_REG_PREPLLCK_OP_DIV, 3},
    {IMX219_REG_PLL_VT_MPY_H, 0x00}, {IMX219_REG_PLL_VT_MPY_L, 57},  /* 0x0039 */
    {IMX219_REG_OPSYCK_DIV, 1},
    {IMX219_REG_PLL_OP_MPY_H, 0x00}, {IMX219_REG_PLL_OP_MPY_L, 114}, /* 0x0072 */
    {IMX219_REG_CSI_LANE_MODE, IMX219_CSI_2_LANE_MODE},
    {IMX219_REG_END, 0x00},
};

/* RAW10 output format */
static const imx219_reginfo_t imx219_raw10_regs[] = {
    {IMX219_REG_CSI_DATA_FORMAT_H, 0x0a}, {IMX219_REG_CSI_DATA_FORMAT_L, 0x0a},
    {IMX219_REG_OPPXCK_DIV, 10},
    {IMX219_REG_END, 0x00},
};

/* RAW8 output format */
static const imx219_reginfo_t imx219_raw8_regs[] = {
    {IMX219_REG_CSI_DATA_FORMAT_H, 0x08}, {IMX219_REG_CSI_DATA_FORMAT_L, 0x08},
    {IMX219_REG_OPPXCK_DIV, 8},
    {IMX219_REG_END, 0x00},
};

/*
 * Mode: 1640x1232, 2x2 binned, RAW10, ~30fps. Full field of view, half
 * resolution — the recommended bring-up mode (lowest bandwidth full-FOV mode).
 * Crop is the full array (0..3279, 0..2463); VTS = 0x06e3.
 */
static const imx219_reginfo_t imx219_mode_1640x1232_regs[] = {
    {IMX219_REG_X_ADD_STA_H, 0x00}, {IMX219_REG_X_ADD_STA_L, 0x00},   /* 0    */
    {IMX219_REG_X_ADD_END_H, 0x0c}, {IMX219_REG_X_ADD_END_L, 0xcf},   /* 3279 */
    {IMX219_REG_Y_ADD_STA_H, 0x00}, {IMX219_REG_Y_ADD_STA_L, 0x00},   /* 0    */
    {IMX219_REG_Y_ADD_END_H, 0x09}, {IMX219_REG_Y_ADD_END_L, 0x9f},   /* 2463 */
    {IMX219_REG_X_OUTPUT_SIZE_H, 0x06}, {IMX219_REG_X_OUTPUT_SIZE_L, 0x68}, /* 1640 */
    {IMX219_REG_Y_OUTPUT_SIZE_H, 0x04}, {IMX219_REG_Y_OUTPUT_SIZE_L, 0xd0}, /* 1232 */
    {IMX219_REG_BINNING_MODE_H, 0x01}, {IMX219_REG_BINNING_MODE_L, 0x01},   /* 2x2 normal */
    {IMX219_REG_VTS_H, 0x06}, {IMX219_REG_VTS_L, 0xe3},               /* 1763 */
    {IMX219_REG_END, 0x00},
};

/* Fixed sensor timing (2-lane): pixel rate 182.4 MHz, HTS 3448 pixels. */
#define IMX219_PIXEL_RATE_2LANE   182400000
#define IMX219_HTS                3448
#define IMX219_TLINE_NS           18904   /* HTS / pixel_rate, in ns */
/* 2-lane link freq 456 MHz -> 912 Mbps per lane */
#define IMX219_MIPI_CSI_LINE_RATE 912000000

#define IMX219_LINESYNC_ENABLE    0   /* set 1 to emit line-sync short packets */


/* If you want to use the baseboard's ISP, provide ISP info */
static const esp_cam_sensor_isp_info_t custom_fmt_isp_info = {
    .isp_v1_info = {
        .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
        .pclk = IMX219_PIXEL_RATE_2LANE,
        .hts = IMX219_HTS,
        .vts = 1763,                        /* 0x06e3 */
        .exp_def = IMX219_EXPOSURE_DEFAULT,
        .gain_def = IMX219_ANA_GAIN_DEFAULT,
        .tline_ns = IMX219_TLINE_NS,
        .bayer_type = ESP_CAM_SENSOR_BAYER_RGGB,
    }
};

/*Provides the description of the initializer list.
 *Note that the description of the format must be `static const` type */
static const esp_cam_sensor_format_t custom_format_info = {
    .name = "MIPI_2lane_24Minput_RAW10_1640x1232_30fps",
    .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
    .port = ESP_CAM_SENSOR_MIPI_CSI,
    .xclk = IMX219_XCLK_FREQ_HZ,
    .width = 1640,
    .height = 1232,
    .regs = imx219_mode_1640x1232_regs,
    .regs_size = ARRAY_SIZE(imx219_mode_1640x1232_regs),
    .fps = 30,
    .isp_info = &custom_fmt_isp_info,
    .mipi_info = {
        .mipi_clk = IMX219_MIPI_CSI_LINE_RATE,
        .lane_num = 2,
        .line_sync_en = IMX219_LINESYNC_ENABLE,
    },
    .reserved = NULL,
};

#ifdef __cplusplus
}
#endif