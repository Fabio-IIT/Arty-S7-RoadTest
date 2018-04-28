/*
 * grideye_api.c
 *
 *  Created on: 24 Apr 2018
 *      Author: fabio
 */
#include "grideye_api.h"
#include "xstatus.h"
#include "xil_types.h"
#include "xiic.h"

int read_iic_register(u8* reg, u8 *value);

/*
 * read_iic_register(u8* reg, u8* value)
 *
 * The function reads a register from of the GRIDEYE i2c device,
 * and store its content in the value variable
 *
 * Returns:
 * XST_SUCCESS -> all ok
 * XST_FAILURE -> error while interacting with the device
 *
 */
inline int read_iic_register(u8* reg, u8 *value){
	if(XIic_Send(XPAR_IIC_0_BASEADDR,GRIDEYE_ADDRESS,reg,1,XIIC_REPEATED_START) != 1){
		return XST_FAILURE;
	}
	if(XIic_Recv(XPAR_IIC_0_BASEADDR, GRIDEYE_ADDRESS, value, 1, XIIC_STOP) != 1){
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

// conversion macro
u16 THERMISTOR_TEMPERATURE_FLOAT_TO_U16(float float_temp){
	u16 result = ((u16)(float_temp/THERMISTOR_RESOLUTION)) & 0x0fff;
	result |= (float_temp<0)? 0x0800 : 0;
	return result;
}

// conversion macro
u16 PIXEL_TEMPERATURE_FLOAT_TO_U16(float float_temp){
	u16 result = ((u16)(float_temp/PIXEL_RESOLUTION)) & 0x0fff;
	result |= (float_temp<0)? 0x0800 : 0;
	return result;
}

// reads the Thermistor temperature from GRIDEYE
// and returns the value as float number
int read_thermistor_temperature(float *temperature){
	u8 low,high;
	int status = XST_SUCCESS;
	u8 reg_l=REG_ADDR_TH_L;
	u8 reg_h=REG_ADDR_TH_H;

	if((read_iic_register(&reg_l,&low)==XST_SUCCESS) &&
	    (read_iic_register(&reg_h,&high)==XST_SUCCESS)){

		*temperature = THERMISTOR_TEMPERATURE(low,high);

	} else {
		status = XST_FAILURE;
	}

	return status;
}

// reads the pixels temperature from GRIDEYE
// and returns the value as an array of 64
// 16-bit unsigned int.
// use the conversion macros defined above to
// calculate the float temperature value from
// the u16 integer
int read_frame_temperature(u16* temperatureArray){
	int status = XST_SUCCESS;
	u8 reg_l=REG_ADDR_PIXEL_L;
	u8 reg_h=REG_ADDR_PIXEL_H;
	for(u8 i=0;i<64;i++){
		u8 low,high;
		if((read_iic_register(&reg_l,&low)==XST_SUCCESS) &&
		    (read_iic_register(&reg_h,&high)==XST_SUCCESS)){
			// reverse order,as the pixels are read from the
			// GRIDEYE have pixel 64 at the top left corner,
			// and pixel 1 at the bottom right corner
			*(temperatureArray+63-i) = TEMPERATURE_U16(low,high);
			reg_l+=2;
			reg_h+=2;
		} else {
			status = XST_FAILURE;
			break;
		}
	}
	return status;
}

