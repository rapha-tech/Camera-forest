/*
 * SPDX-FileCopyrightText: 2026 esp_cam_sensor_imx contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * IMX219 register map. Register addresses and semantics were taken from the
 * public Sony IMX219 register documentation and the Linux kernel driver
 * (drivers/media/i2c/imx219.c, GPL-2.0). Only factual register numbers/values
 * are reproduced here; no GPL code is copied.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Sentinels used inside imx219_reginfo_t arrays */
#define OV64A40_REG_END             0xffff  /*!< marks end of a register array   */
#define IMX219_REG_DELAY            0xfffe  /*!< .val is a delay in milliseconds */

/* Identification */
#define OV64A40_REG_CHIP_ID         0x300a  /*!< 16-bit chip id, big endian */
#define OV64A40_CHIP_ID             0x6441

#define OV64A40_PLL1_PRE_DIV0		0x0301
#define OV64A40_PLL1_PRE_DIV		0x0303
#define OV64A40_PLL1_MULTIPLIER		0x0304
#define OV64A40_PLL1_M_DIV		    0x0307
#define OV64A40_PLL2_SEL_BAK_SA1	0x0320
#define OV64A40_PLL2_PRE_DIV		0x0323
#define OV64A40_PLL2_MULTIPLIER		0x0324
#define OV64A40_PLL2_PRE_DIV0		0x0326
#define OV64A40_PLL2_DIVDAC		    0x0329
#define OV64A40_PLL2_DIVSP		    0x032d
#define OV64A40_PLL2_DACPREDIV		0x032e

/* Helpers: split a 16-bit value into two 8-bit register writes */
#define OV64A40_HI(v)  (((v) >> 8) & 0xFF)
#define OV64A40_LO(v)  ((v) & 0xFF)

#define OV64A40_REG_TIMING_CTRL0	0x3800
#define OV64A40_REG_TIMING_CTRL1	0x3801
#define OV64A40_REG_TIMING_CTRL2	0x3802
#define OV64A40_REG_TIMING_CTRL3	0x3803
#define OV64A40_REG_TIMING_CTRL4	0x3804
#define OV64A40_REG_TIMING_CTRL5	0x3805
#define OV64A40_REG_TIMING_CTRL6	0x3806
#define OV64A40_REG_TIMING_CTRL7	0x3807
#define OV64A40_REG_TIMING_CTRL8	0x3808
#define OV64A40_REG_TIMING_CTRL9	0x3809
#define OV64A40_REG_TIMING_CTRLA	0x380a
#define OV64A40_REG_TIMING_CTRLB	0x380b
#define OV64A40_REG_TIMING_CTRLC	0x380c
#define OV64A40_REG_TIMING_CTRLD	0x380d
#define OV64A40_REG_TIMING_CTRLE	0x380e
#define OV64A40_REG_TIMING_CTRLF	0x380f
#define OV64A40_REG_TIMING_CTRL10	0x3810
#define OV64A40_REG_TIMING_CTRL11	0x3811
#define OV64A40_REG_TIMING_CTRL12	0x3812
#define OV64A40_REG_TIMING_CTRL13	0x3813

#define OV64A40_REG_TIMING_CTRL14	0x3814
#define OV64A40_REG_TIMING_CTRL15	0x3815
#define OV64A40_ODD_INC_SHIFT		4
#define OV64A40_SKIPPING_CONFIG(_odd, _even) \
				(((_odd) << OV64A40_ODD_INC_SHIFT) | (_even))

#define OV64A40_REG_TIMING_CTRL_20	  0x3820
#define OV64A40_TIMING_CTRL_20_VFLIP  (1 << 2)
#define OV64A40_TIMING_CTRL_20_VBIN	  (1 << 1)

#define OV64A40_REG_TIMING_CTRL_21	  0x3821
#define OV64A40_TIMING_CTRL_21_HBIN	  (1 << 4)
#define OV64A40_TIMING_CTRL_21_HFLIP  (1 << 2)
#define OV64A40_TIMING_CTRL_21_DSPEED (1 << 0)
#define OV64A40_TIMING_CTRL_21_HBIN_CONF \
					(OV64A40_TIMING_CTRL_21_HBIN | \
					 OV64A40_TIMING_CTRL_21_DSPEED)

#define OV64A40_REG_SMIA		    0x0100
#define OV64A40_REG_SMIA_STREAMING	(1 << 0)

#define OV64A40_REG_MEC_LONG_EXPO	0x3500 // 24 bits
#define OV64A40_EXPOSURE_MIN		16
#define OV64A40_EXPOSURE_MARGIN		32

#define OV64A40_REG_MEC_LONG_GAIN	0x3508 // 16 bits
#define OV64A40_ANA_GAIN_MIN		0x80
#define OV64A40_ANA_GAIN_MAX		0x7ff
#define OV64A40_ANA_GAIN_DEFAULT	0x80

#define OV64A40_VTS_MAX             0xFFFFFF
#define OV64A40_EXPOSURE_MAX        (OV64A40_VTS_MAX - OV64A40_EXPOSURE_MARGIN)

/* Core control */
#define OV64A40_REG_SW_RESET        0x0103  /*!< bit0 = software reset */
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
#define IMX219_EXPOSURE_DEFAULT     0x04B0

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

#ifdef __cplusplus
}
#endif
