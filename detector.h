/*
 * detector.h
 *
 *  Created on: 25 Apr 2018
 *      Author: fabio
 */

#ifndef SRC_DETECTOR_H_
#define SRC_DETECTOR_H_

#define INTC_DEVICE_ID XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID
#define INTC_BASE_ADDRESS XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR
#define LEDS_DEVICE_ID XPAR_GPIO_0_DEVICE_ID
#define BTNS_SW_DEVICE_ID XPAR_GPIO_1_DEVICE_ID
#define INTC_GPIO_INTERRUPT_ID XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_1_IP2INTC_IRPT_INTR
#define INTC_TIMER_INTERRUPT_ID XPAR_MICROBLAZE_0_AXI_INTC_FIT_TIMER_0_INTERRUPT_INTR
#define BTNS_SW_INT_MASK XGPIO_IR_MASK

#define NO_BUTTONS  0x00
#define BUTTON_0 	0x01   // alarm reset
#define BUTTON_1 	0x02   // set zones
#define BUTTON_2 	0x04   // set mode
#define BUTTON_3 	0x08   // set temperature (mode absolute) / acquire background frame (mode differential)

#define NO_SWITCHES  0x00
#define SWITCH_0 	0x01
#define SWITCH_1 	0x02
#define SWITCH_2 	0x04
#define SWITCH_3 	0x08

#define ABSOLUTE_THRESHOLD     24
#define DIFFERENTIAL_THRESHOLD  4  // differential is set to 4C

// states
#define PROCESS_IDLE					0x00   //do nothing
#define PROCESS_MONITOR					0x01   //check the current frame against threshold (mode dependent)
#define PROCESS_READ_TEMPERATURE 		0x02
#define PROCESS_ACQUIRE_BACKGROUND		0x03
#define PROCESS_UI_SET_MODE  			0x04
#define PROCESS_UI_SET_TEMPERATURE      0x05
#define PROCESS_UI_SET_ZONES            0x06
#define PROCESS_UI_RESET_ALARM			0x07
#define PROCESS_UI_INTERRUPT            0x09 // user input detected
#define PROCESS_UI_CONFIRM              0x08 // flashes leds to confirm selection

#define FLASHING_COUNT	8 // nummber of times LEDs flash to confirm selection

//monitoring zones
#define NO_ZONE	0x00
#define ZONE_1  0x01
#define ZONE_2  0x02
#define ZONE_3  0x04
#define ZONE_4  0x08

//mode
#define DIFFERENTIAL_MODE 0x0
#define ABSOLUTE_MODE	  0x1

#define BACKGROUND_AVERAGE_SIZE 10

#define TIMER_INTERRUPT 0x01
#define UI_INTERRUPT	0x02
#define ACQ_BACKGROUND  0x04
#define UI_CONFIRM      0x08
#define GRIDEYE_ERROR   0x10

#define ALARM_ON	0x01
#define ALARM_OFF   0x00
#define ALARM_COLOUR_MASK 0x09 //red-red

typedef struct{
	u32 buttons;
	u32 switches;
} UI_Info;

typedef struct{
	u8 alarm_status;
	u32 active_zones;
	u32 zone_detected;
} Alarm_Info;

typedef struct{
	u16 background_frame[64];
	u16 current_frame[64];
	u32 mode;
	u32 offset;
	u8 sign;
	int32_t threshold;
} Frame_Info;

const u8 zone_mapping[64]={	ZONE_1,ZONE_1,ZONE_1,ZONE_1,ZONE_2,ZONE_2,ZONE_2,ZONE_2,
							ZONE_1,ZONE_1,ZONE_1,ZONE_1,ZONE_2,ZONE_2,ZONE_2,ZONE_2,
							ZONE_1,ZONE_1,ZONE_1,ZONE_1,ZONE_2,ZONE_2,ZONE_2,ZONE_2,
							ZONE_1,ZONE_1,ZONE_1,ZONE_1,ZONE_2,ZONE_2,ZONE_2,ZONE_2,
							ZONE_4,ZONE_4,ZONE_4,ZONE_4,ZONE_3,ZONE_3,ZONE_3,ZONE_3,
							ZONE_4,ZONE_4,ZONE_4,ZONE_4,ZONE_3,ZONE_3,ZONE_3,ZONE_3,
							ZONE_4,ZONE_4,ZONE_4,ZONE_4,ZONE_3,ZONE_3,ZONE_3,ZONE_3,
							ZONE_4,ZONE_4,ZONE_4,ZONE_4,ZONE_3,ZONE_3,ZONE_3,ZONE_3};


int setup_system(void);
void gpio_int_handler(void *baseaddr);
void loop(void);

u32 check_threshold(void);

#endif /* SRC_DETECTOR_H_ */
