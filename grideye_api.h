/*
 * grideye_api.h
 *
 *  Created on: 24 Apr 2018
 *      Author: fabio
 */

#ifndef SRC_GRIDEYE_API_H_
#define SRC_GRIDEYE_API_H_

#include "xil_types.h"

/* define AMG8833 specific constant */
#define GRIDEYE_ADDRESS 0x68

/* Thermistor Temperature Register Address */
#define REG_ADDR_TH_L	(u8)0x0e
#define REG_ADDR_TH_H	(u8)0x0f

/* Pixel (0-63) Temperature Register Address (0x80-0xff) macros*/
#define REG_ADDR_PIXEL_L 0x80
#define REG_ADDR_PIXEL_H 0x81

/* GridEye Thermistor temperature resolution */
#define THERMISTOR_RESOLUTION 0.0625f
/* GridEye Pixel temperature resolution */
#define PIXEL_RESOLUTION 0.25f

/* Thermistor temperature conversion macro */
#define THERMISTOR_TEMPERATURE(low,high) ((low+((0x7 & high)<<8))*0.0625f*((high & 0x8)?-1:1))

/* Temperature conversion macro */
#define TEMPERATURE_U16(low,high) (low+((0xf & high)<<8))

#define THERMISTOR_TEMPERATURE_U16_TO_FLOAT(u16_temp) ((0x07ff & u16_temp)*THERMISTOR_RESOLUTION*((u16_temp & 0x8000)?-1:1))
#define PIXEL_TEMPERATURE_U16_TO_FLOAT(u16_temp)      ((0x07ff & u16_temp)*PIXEL_RESOLUTION*((u16_temp & 0x8000)?-1:1))
u16 THERMISTOR_TEMPERATURE_FLOAT_TO_U16(float float_temp);
u16 PIXEL_TEMPERATURE_FLOAT_TO_U16(float float_temp);

int read_thermistor_temperature(float *temperature);
int read_frame_temperature(u16* temperatureArray);

#endif /* SRC_GRIDEYE_API_H_ */
