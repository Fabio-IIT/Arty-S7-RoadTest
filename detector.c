/*
 * detector.c
 *
 *  Created on: 24 Apr 2018
 *      Author: fabio
 */

#include <stdio.h>
#include "xparameters.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xiic.h"
#include "xintc.h"
#include "xil_io.h"
#include <unistd.h>
#include "xgpio.h"
#include "xintc_l.h"
#include "xil_exception.h"

#include "grideye_api.h"
#include "detector.h"

//  To enable logging info via UART (115200 baud),
//  uncomment the following line
//#define TEST_MODE_

static  XGpio  gpio0;
static	XGpio  gpio1;
static	XIntc  intc;

static u8 flags=ACQ_BACKGROUND;

static UI_Info uiState={0};
static UI_Info uiInterruptInfo={0};

static u8 flashing_counter=0;
static u8 ui_led_active=0;
static u8 error_led_active=0;

static u8 current_state=PROCESS_IDLE;
static u8 background_frame_counter=BACKGROUND_AVERAGE_SIZE; // count 10 frames to calculate the average background frame

static Frame_Info frame_info={
		background_frame:	{0},
        current_frame:		{0},
		mode:				DIFFERENTIAL_MODE,
		threshold:			DIFFERENTIAL_THRESHOLD
};

static Alarm_Info alarm_info={
		alarm_status: 	ALARM_OFF,
		active_zones: 	ZONE_1|ZONE_2|ZONE_3|ZONE_4,
		zone_detected: 	NO_ZONE
};

#ifdef TEST_MODE_
void print_frame(u16* frame, char* title);
void print_frame(u16* frame, char* title){
	xil_printf("%s:\n",title);

	for(int i=1;i<=64; i++){
		printf("%.2f ",PIXEL_TEMPERATURE_U16_TO_FLOAT(frame[i-1]));
		if(i%8==0){
			printf("\n");
		}
	}
}
#endif /* TEST_MODE_ */

void gpio_int_handler(void *ctrl){

	XGpio_InterruptGlobalDisable(&gpio1);
	u32 ch = XGpio_InterruptGetStatus(&gpio1) & BTNS_SW_INT_MASK;
	if(ch==XGPIO_IR_CH1_MASK){
		// Buttons
		uiInterruptInfo.buttons=XGpio_DiscreteRead(&gpio1, ch);
		uiInterruptInfo.switches=XGpio_DiscreteRead(&gpio1, XGPIO_IR_CH2_MASK);
		flags |= UI_INTERRUPT;
	}
	//Clear the interrupt both in the Gpio instance as well as the interrupt controller
	XGpio_InterruptClear(&gpio1, BTNS_SW_INT_MASK);
	XIntc_Acknowledge(&intc,INTC_GPIO_INTERRUPT_ID);
	XGpio_InterruptGlobalEnable(&gpio1);
}

void timer_int_handler(void *ctrl){

	if(current_state == PROCESS_IDLE){
		flags |= TIMER_INTERRUPT;
	}
	XIntc_Acknowledge(&intc,INTC_TIMER_INTERRUPT_ID);

}
/*
 * Setup GPIO controller and Interrupt controller
 *
 * There are 5 interrupts defined for the system:
 *
 * int0 - I2C controller
 * int1 - UART controller
 * int2 - GPIO1 (switches & buttons)
 * int3 - QSPI Flash controller
 * int4 - Timer (0.1s interrupt)
 */

int setup_system(void){
	int Status;

	/*
	 * Setup GPIO controller for buttons and switches
	 * set the GPIO ports and their interrupts
	 */
	XGpio_Initialize(&gpio1, BTNS_SW_DEVICE_ID);
	XGpio_SetDataDirection(&gpio1,XGPIO_IR_CH1_MASK,1); //set push buttons as input port
	XGpio_SetDataDirection(&gpio1,XGPIO_IR_CH2_MASK,1); //set switches as input port
	XGpio_InterruptEnable(&gpio1, BTNS_SW_INT_MASK);
	XGpio_InterruptGlobalEnable(&gpio1);

	/*
	 * Setup GPIO controller for leds
	 * set the GPIO ports
	 */
	XGpio_Initialize(&gpio0, LEDS_DEVICE_ID);
	XGpio_SetDataDirection(&gpio0,XGPIO_IR_CH1_MASK,0); //set rgb leds as output port
	XGpio_SetDataDirection(&gpio0,XGPIO_IR_CH2_MASK,0); //set leds as output port
	//test leds on init
	for(u8 i=1;i<64;){
		XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH1_MASK,i);
		XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,i);
		usleep(100000);
		XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH1_MASK,0);
		XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,0);
		usleep(100000);
		i*=2;
	}
	//switch leds off
	XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH1_MASK,0);
	XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,0);

	/* Initialize the Interrupt controller */
	Status = XIntc_Initialize(&intc, XPAR_INTC_0_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Perform a self-test to ensure that the hardware was built correctly.*/
	Status = XIntc_SelfTest(&intc);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Connect the interrupt handler routines */

    Status = XIntc_Connect(&intc, INTC_GPIO_INTERRUPT_ID, // interrupt line from xconcat to intc
                           (XInterruptHandler)gpio_int_handler,(void *)&intc);
    if (Status != XST_SUCCESS) {
    	return XST_FAILURE;
    }

    Status = XIntc_Connect(&intc, INTC_TIMER_INTERRUPT_ID, // interrupt line from xconcat to intc
                           (XInterruptHandler)timer_int_handler,(void *)&intc);
    if (Status != XST_SUCCESS) {
    	return XST_FAILURE;
    }

    /* Enable all interrupts (5 interrupt lines -> 5 bits width -> 0x1f mask) on the Interrrupt controller */
    XIntc_EnableIntr(INTC_BASE_ADDRESS,0x1f);

    /* Start the Interrupt controller */
    XIntc_Start(&intc, XIN_REAL_MODE);

    /* Setting up exceptions handler */
    Xil_ExceptionInit();

    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_M_AXI_I_EXCEPTION,
            (XExceptionHandler)XIntc_InterruptHandler,
			&intc);
    Xil_ExceptionEnable();

    current_state = PROCESS_IDLE;

	return XST_SUCCESS;

}

/*
 * main loop
 *
 * the system can be in one of the following states:
 *
 * PROCESS_IDLE: manage the user interface while in this
 *               state, and check for interrupt conditions,
 *               selecting the next state accordingly.
 *
 * PROCESS_READ_TEMPERATURE: read the temperature from the sensor,
 *                           if threshold has been reached set
 *                           alarm flag and switch on the led
 *
 * PROCESS_UI_INTERRUPT: manage user input
 */
void loop(void){
	while(1){
		u8 next_state = current_state;
		switch(current_state){
			case PROCESS_IDLE:{
				//check flags
				// timer interrupt always take precedence
#ifdef TEST_MODE_
				xil_printf("in PROCESS_IDLE...\n\r");
#endif /* TEST_MODE_ */
				if((flags & GRIDEYE_ERROR) && (flags & TIMER_INTERRUPT)){
					error_led_active^=0x3f; // all 3 colours of rgb leds are lit
					XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,error_led_active);
					XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH1_MASK,error_led_active);
				} else if(flags & TIMER_INTERRUPT){
					// timer interrupt occurred.
					// check if there is any led flashing to be done
					if(flags & UI_CONFIRM){
						// some user interaction still processing
						// make sure it is completed
						if(flashing_counter>0){
							//toggle leds output
							flashing_counter--;
							ui_led_active ^= 1;
							if(ui_led_active) {
								XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,uiState.switches);
							} else {
								XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,0);
							}
						} else {
							// user interaction completed
							// put leds out
							flags ^= UI_CONFIRM;
							ui_led_active = 0;
							//restore alarm lights condition
							if(alarm_info.alarm_status==ALARM_ON){
								XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,alarm_info.zone_detected);
								XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH1_MASK,ALARM_COLOUR_MASK);
							}
						}
					}
					next_state=PROCESS_READ_TEMPERATURE;
					flags ^= TIMER_INTERRUPT;
				} else if(flags & UI_INTERRUPT){
					// user interrupt
					next_state = PROCESS_UI_INTERRUPT;
					flags ^= UI_INTERRUPT;
				}
				break;
			}
			case PROCESS_READ_TEMPERATURE:{
				// read the temperature from the sensor
				// and check if threshold has been reached
				// if so, set alarm flag and switch on the led
				// next state is PROCESS_IDLE
#ifdef TEST_MODE_
				xil_printf("in PROCESS_READ_TEMPERATURE\n\r");
#endif /* TEST_MODE_ */
				// read the temperature from the sensor
				if(read_frame_temperature(frame_info.current_frame)==XST_FAILURE){
					flags |= GRIDEYE_ERROR;
					break;
				} else {
					if(flags & GRIDEYE_ERROR){
						flags ^= GRIDEYE_ERROR;
					}
				}
				//check if we are acquiring background frame
				if(flags & ACQ_BACKGROUND){
#ifdef TEST_MODE_
					xil_printf("acquiring background frame %u...\n\r",BACKGROUND_AVERAGE_SIZE-background_frame_counter);
#endif /* TEST_MODE_ */
					if(background_frame_counter>0){
						// still acquiring...
						for(u8 i=0;i<64;i++){
							frame_info.background_frame[i]=(frame_info.background_frame[i]+frame_info.current_frame[i])/2;
						}
						background_frame_counter--;
					} else {
						// done loading background
#ifdef TEST_MODE_
						xil_printf("background frames acquired...\n\r");
						print_frame(frame_info.background_frame, "Backgroud");
						xil_printf("current frame...\n\r");
						print_frame(frame_info.current_frame,"Current");
#endif /* TEST_MODE_ */
						flags ^= ACQ_BACKGROUND;
						background_frame_counter=BACKGROUND_AVERAGE_SIZE;
					}
				} else if((alarm_info.alarm_status==ALARM_OFF) && (alarm_info.active_zones != NO_ZONE)){
					u32 zone_detected=check_threshold();
					if (zone_detected!=NO_ZONE){
						//hit threshold
#ifdef TEST_MODE_
						xil_printf("*******\n*********ALARM*******\n*********\n\r");
						print_frame(frame_info.background_frame, "Backgroud");
						print_frame(frame_info.current_frame, "Current");
#endif /* TEST_MODE_ */
						alarm_info.alarm_status=ALARM_ON;
						alarm_info.zone_detected=zone_detected;
						XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH1_MASK,ALARM_COLOUR_MASK);
						XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,zone_detected);
					}
				}
				next_state=PROCESS_IDLE;
				break;
			}
			case PROCESS_UI_INTERRUPT:{
				//process input
				// act accordingly
				//set flashing counter and light up the led
				u8 pressed=0;
#ifdef TEST_MODE_
				xil_printf("in PROCESS_UI_INTERRUPT...\n\r");
#endif /* TEST_MODE_ */
				u32 changed = uiInterruptInfo.buttons ^ uiState.buttons;
				pressed += ((changed & BUTTON_0) && !(uiState.buttons & BUTTON_0))?1:0;
				pressed += ((changed & BUTTON_1) && !(uiState.buttons & BUTTON_1))?1:0;
				pressed += ((changed & BUTTON_2) && !(uiState.buttons & BUTTON_2))?1:0;
				pressed += ((changed & BUTTON_3) && !(uiState.buttons & BUTTON_3))?1:0;
				if((changed!=0) && (pressed==1)){
					u32 ui_led_active=0;
					// only 1 button pressed (ignore all other cases)
					if(changed & BUTTON_0){
						// button 0 is alarm reset
						if(alarm_info.alarm_status==ALARM_ON){
							alarm_info.alarm_status=ALARM_OFF;
							alarm_info.zone_detected=NO_ZONE;
							XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH1_MASK,0);
							XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,0);
						}
#ifdef TEST_MODE_
						xil_printf("Alarm reset button pressed!!!\n\r");
#endif /* TEST_MODE_ */
						uiState.buttons=BUTTON_0;
					} else if (changed & BUTTON_1){
						// button 1 is set zones
						alarm_info.active_zones=uiInterruptInfo.switches;
						uiState.buttons=BUTTON_1;
						ui_led_active=1;
#ifdef TEST_MODE_
						xil_printf("Set Zones button pressed!!!\n\r");
						xil_printf("Zones set: %x\n\r",alarm_info.active_zones);
#endif /* TEST_MODE_ */
					} else if (changed & BUTTON_2){
						// button 2 is set mode
						frame_info.mode=(uiInterruptInfo.switches & 0x1);
						if(frame_info.mode==DIFFERENTIAL_MODE){
							frame_info.threshold=(uiInterruptInfo.switches & 0xe)>>1;
#ifdef TEST_MODE_
							xil_printf("Set Differential Mode button pressed!!!\n\r");
							xil_printf("Differential threshold = %i\n\r",frame_info.threshold);
#endif /* TEST_MODE_ */
						} else {
						    frame_info.sign=(uiInterruptInfo.switches & 0x2)?-1:1;
							frame_info.offset=(uiInterruptInfo.switches & 0xc)<<2;
#ifdef TEST_MODE_
							xil_printf("Set Absolute Mode button pressed!!!\n\r");
							xil_printf("Absolute offset = %i\n\r",frame_info.offset);
#endif /* TEST_MODE_ */
						}
						uiState.buttons=BUTTON_2;
						ui_led_active=1;
					} else if (changed & BUTTON_3){
						// button 3 is set temperature/acquire background
						if((frame_info.mode & 0x1)==DIFFERENTIAL_MODE){
							// in differential mode, this button
							// triggers ACQUIRE_BACKGROUND
							ui_led_active=0;
							flags |= ACQ_BACKGROUND;
#ifdef TEST_MODE_
							xil_printf("Acquire Background button pressed!!!\n\r");
#endif /* TEST_MODE_ */
						} else {
							//absolute mode: read temperature threshold
							ui_led_active=1;
							frame_info.threshold=(uiInterruptInfo.switches+frame_info.offset)*frame_info.sign;
#ifdef TEST_MODE_
						xil_printf("Set Threshold button pressed!!!\n\r");
						xil_printf("Absolute threshold = %i\n\r",frame_info.threshold);
#endif /* TEST_MODE_ */
						}
						uiState.buttons=BUTTON_3;
					}
					uiState.switches=uiInterruptInfo.switches;
					if(ui_led_active!=0){
						//set flashing light counter
						flashing_counter=FLASHING_COUNT;
						flags |= UI_CONFIRM;
						XGpio_DiscreteWrite(&gpio0,XGPIO_IR_CH2_MASK,uiState.switches);
					}
				} else if(pressed==0){
					//no changes
					uiState.buttons=NO_BUTTONS;
				}
				next_state=PROCESS_IDLE;
				break;
			}
			default:break;
		}
		current_state=next_state;
	}
}

/*
 * check_threshold helper function
 *
 * Check if the threshold set has been reached,
 * taking care of the operation mode (differential/absolute).
 *
 * Return value:
 * NO_ZONE          -> threshold not reached
 * zones detected   -> zones where threshold has been reached
 *                     (zones calculated as OR operator of ZONE_x masks,
 *                     ex: for zone 1 and 3 -> ZONE_1 | ZONE_3
 */
u32 check_threshold(){
	u32 detected=NO_ZONE;
	if(frame_info.mode==DIFFERENTIAL_MODE){
		for(u8 i=0;i<64;i++){
			u16 diff = frame_info.current_frame[i]-frame_info.background_frame[i];
			float temp_diff=PIXEL_TEMPERATURE_U16_TO_FLOAT(diff);
			if(temp_diff>=frame_info.threshold){
				detected|=(zone_mapping[i] & alarm_info.active_zones);
#ifdef TEST_MODE_
				xil_printf("Pixel detected:%u\n\r",i);
#endif /* TEST_MODE_ */
			}
		}
	} else {
		//absolute mode
		for(u8 i=0;i<64;i++){
			if(PIXEL_TEMPERATURE_U16_TO_FLOAT(frame_info.current_frame[i])>=frame_info.threshold){
				detected|=(zone_mapping[i] & alarm_info.active_zones);
#ifdef TEST_MODE_
				xil_printf("Pixel detected:%u\n\r",i);
#endif /* TEST_MODE_ */
			}
		}
	}
	return detected;
}

int main()
{
    int status;

	Xil_ICacheEnable();
    Xil_DCacheEnable();

#ifdef TEST_MODE_
    xil_printf("Setting up system...\r\n");
    xil_printf("flags: %x\n\rui_led_active: %x\n\r",flags,ui_led_active);
#endif /* TEST_MODE_ */

	status = setup_system();
    if (status == XST_SUCCESS) {
    	float thermTemp;
		status = read_thermistor_temperature(&thermTemp);
	    if (status == XST_SUCCESS) {
#ifdef TEST_MODE_
	    	printf("Thermistor Temperature: %3.2f C\n\n",thermTemp);
#endif /* TEST_MODE_ */
			loop();
	    }
    }
    Xil_DCacheDisable();
    Xil_ICacheDisable();

    return 0;
}


