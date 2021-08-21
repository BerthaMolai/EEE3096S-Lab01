/*
 * BinClock.c
 * Jarrod Olivier
 * Modified by Keegan Crankshaw
 * Further Modified By: Mark Njoroge 
 *
 * 
 * KBNGIV001 MLXBAT001
 * 18/08/21
*/

#include <signal.h> //for catching signals
#include <wiringPi.h>
#include <string.h> // To provide us with strerror function
#include <wiringPiI2C.h>
#include <stdio.h> //For printf functions
#include <stdlib.h> // For system functions
#include <errno.h> // For compile/runtime errors
#include "BinClock.h"
#include "CurrentTime.h"

//Global variables
int hours, mins, secs;
long lastInterruptTime = 0; //Used for button debounce
int RTC; //Holds the RTC instance

int HH,MM,SS;
int LED_status;
int B_hours;
int B_mins;

// Clean up function to avoid damaging used pins
void CleanUp(int sig){
	printf("Cleaning up\n");

	//Set LED to low then input mode	
	digitalWrite(LED, 0);
	pinMode(LED, INPUT);
	

	for (int j=0; j < sizeof(BTNS)/sizeof(BTNS[0]); j++) {
		pinMode(BTNS[j],INPUT);
	}

	exit(0);

}

void initGPIO(void){
	/* 
	 * Sets GPIO using wiringPi pins. see pinout.xyz for specific wiringPi pins
	 * You can also use "gpio readall" in the command line to get the pins
	 * Note: wiringPi does not use GPIO or board pin numbers (unless specifically set to that mode)
	 */
	printf("Setting up\n");
	wiringPiSetup(); //This is the default mode. If you want to change pinouts, be aware
	

	RTC = wiringPiI2CSetup(RTCAddr); //Set up the RTC
	
	//Set up the LED, pull it high and set as output pin
	LED_status = 1;
	pinMode(LED, OUTPUT);
	digitalWrite(LED,LED_status);

	//printf("%d\n", RTC);
	printf("LED and RTC done\n");
	
	//Set up the Buttons
	for(int j=0; j < sizeof(BTNS)/sizeof(BTNS[0]); j++){
		pinMode(BTNS[j], INPUT);
		pullUpDnControl(BTNS[j], PUD_DOWN); //buttons initially to low
	}

	//Attach interrupts to Buttons
	//Write your logic here
	// 	INT_EDGE_RISING 	INT_EDGE_FALLING	INT_EDGE_BOTH
	// 	on btn press		on btn release		both
	// 	choosing INT_EDGE_FALLING because that has shown the smoothest 
	// 	transition when button was pressed
	if ( wiringPiISR (BTNS[0], INT_EDGE_FALLING, &hourInc) < 0 ) {
     		fprintf (stderr, "Unable to setup ISR 1: %s\n", strerror (errno));
      	//	exit(0);
  	}

	if ( wiringPiISR (BTNS[1], INT_EDGE_FALLING, &minInc) < 0 ) {
     		fprintf (stderr, "Unable to setup ISR 2: %s\n", strerror (errno));
      	//	exit(0);
  	}

	//Interrupts
	//these are pretty important because they are triggered in real time
	//previously, we wrote code that checked if btn input = high,
	//but there we noticed a delay,
	//that's cos we were checking if button was pressed in main loop
	//but now with interupts, it button clicks respond in real time
	//almost as though there's a dedicated loop listening to changes in the input
	//spend time researching more on interrupts, timers, etc
	//more of great scott videos
	//the resources that helped with completing this project are:
	//
	//https://pi4j.com/1.2/apidocs/com/pi4j/wiringpi/Gpio.html
	//https://raspberrypi.stackexchange.com/questions/8544/gpio-interrupt-debounce
	//https://raspberrypi.stackexchange.com/questions/54116/gpio-command-not-found
	//https://vim.fandom.com/wiki/Copy,_cut_and_paste
	//https://pi4j.com/1.2/apidocs/com/pi4j/wiringpi/Gpio.html
	//MICRO CONTROLLER PROGRAMMING WITH C IS DOPE!


		
		

	//changing button names for clarity
	B_hours = BTNS[0];
	B_mins = BTNS[1];
	


	printf("BTNS done\n");
	printf("Setup done\n");
}


/*
 * The main function
 * This function is called, and calls all relevant functions we've written
 */
int main(void){
	signal(SIGINT,CleanUp);
	initGPIO();

	//writing current time to register
	wiringPiI2CWriteReg8(RTC, HOUR_REGISTER, decCompensation( getHours() ));
	wiringPiI2CWriteReg8(RTC, MIN_REGISTER, decCompensation( getMins() ));
	wiringPiI2CWriteReg8(RTC, SEC_REGISTER, decCompensation( getSecs() ));
	//printf("%d\n", wiringPiI2CWriteReg8(RTC, HOUR_REGISTER, 0x13));

	
	// Repeat this until we shut down
	for (;;){
		//Fetch the time from the RTC
		hours =hexCompensation(wiringPiI2CReadReg8(RTC, HOUR_REGISTER));
		mins = hexCompensation(wiringPiI2CReadReg8(RTC, MIN_REGISTER));
		secs = hexCompensation(wiringPiI2CReadReg8(RTC, SEC_REGISTER));


		//Toggle Seconds LED
		digitalWrite(LED, LED_status);
		LED_status = !LED_status; //chnaging LED status each time we run through iteration.
		//we already have a 1 second delay before next iteration so LED status will change every second
		// Print out the time we have stored on our RTC
		printf("The current time is: %d:%d:%d\n", hours, mins, secs);

		//using a delay to make our program "less CPU hungry"
		delay(1000); //milliseconds
	}
	return 0;
}

/*
 * Changes the hour format to 12 hours
 */
int hFormat(int hours){
	/*formats to 12h*/
	if (hours >= 24){
		hours = 0;
	}
	else if (hours > 12){
		hours -= 12;
	}
	return (int)hours;
}


/*
 * hexCompensation
 * This function may not be necessary if you use bit-shifting rather than decimal checking for writing out time values
 * Convert HEX or BCD value to DEC where 0x45 == 0d45 	
 */
int hexCompensation(int units){

	int unitsU = units%0x10;

	if (units >= 0x50){
		units = 50 + unitsU;
	}
	else if (units >= 0x40){
		units = 40 + unitsU;
	}
	else if (units >= 0x30){
		units = 30 + unitsU;
	}
	else if (units >= 0x20){
		units = 20 + unitsU;
	}
	else if (units >= 0x10){
		units = 10 + unitsU;
	}
	return units;
}


/*
 * decCompensation
 * This function "undoes" hexCompensation in order to write the correct base 16 value through I2C
 */
int decCompensation(int units){
	int unitsU = units%10;

	if (units >= 50){
		units = 0x50 + unitsU;
	}
	else if (units >= 40){
		units = 0x40 + unitsU;
	}
	else if (units >= 30){
		units = 0x30 + unitsU;
	}
	else if (units >= 20){
		units = 0x20 + unitsU;
	}
	else if (units >= 10){
		units = 0x10 + unitsU;
	}
	return units;
}


/*
 * hourInc
 * Fetch the hour value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 23 hours in a day
 * Software Debouncing should be used
 */
void hourInc(void){
	//Debounce
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 1 triggered, %d\n", hours);
		//Fetch RTC Time
		HH = hours;
		//printf("%d\n", HH);
		//Increase hours by 1, ensuring not to overflow
		if(HH < 23){
			HH = HH +1;
		}
		else {
			HH=0;
		}
		//Write hours back to the RTC
		//printf("%d\n", HH);
		HH = decCompensation(HH);
		//printf("%d\n", HH);
		wiringPiI2CWriteReg8(RTC, HOUR_REGISTER, HH);
	}
	lastInterruptTime = interruptTime;
}

/* 
 * minInc
 * Fetch the minute value off the RTC, increase it by 1, and write back
 * Be sure to cater for there only being 60 minutes in an hour
 * Software Debouncing should be used
 */
void minInc(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		printf("Interrupt 2 triggered, %d\n", mins);
		//Fetch RTC Time
		MM = mins;
		//Increase minutes by 1, ensuring not to overflow
		if( MM < 59){
			MM= MM + 1;
		}
		else{
			MM=0;

			HH = hours;
			if (HH<23){
				HH= HH + 1;	
			}
			else {
				HH = 0;	
			}

			HH= decCompensation(HH);

			wiringPiI2CWriteReg8(RTC, HOUR_REGISTER, HH);	


		}
		//Write minutes back to the RTC
		MM = decCompensation(MM);
		wiringPiI2CWriteReg8(RTC, MIN_REGISTER, MM);
	}
	lastInterruptTime = interruptTime;
}

//This interrupt will fetch current time from another script and write it to the clock registers
//This functions will toggle a flag that is checked in main
void toggleTime(void){
	long interruptTime = millis();

	if (interruptTime - lastInterruptTime>200){
		HH = getHours();
		MM = getMins();
		SS = getSecs();

		HH = hFormat(HH);
		HH = decCompensation(HH);
		wiringPiI2CWriteReg8(RTC, HOUR_REGISTER, HH);

		MM = decCompensation(MM);
		wiringPiI2CWriteReg8(RTC, MIN_REGISTER, MM);

		SS = decCompensation(SS);
		wiringPiI2CWriteReg8(RTC, SEC_REGISTER, 0b10000000+SS);

	}
	lastInterruptTime = interruptTime;
}
