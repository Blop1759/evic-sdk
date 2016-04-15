/*
 * This file is part of eVic SDK.
 *
 * eVic SDK is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * eVic SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eVic SDK.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2016 Blop1759
 * simple test for TC handling : TFR for 316L, not TCR, 
 *  based on atomizer example of sdk
 */

#include <stdio.h>
#include <math.h>
#include <M451Series.h>
#include <Display.h>
#include <Font.h>
#include <Atomizer.h>
#include <Button.h>
#include <TimerUtils.h>
#include <Battery.h>
#include <Globals.h>

#define TC_TMRFLAG_PID (1 << 1)
#define TC_TIMER_PID_RESET() do { __set_PRIMASK(1); \
	TC_timerCountPID = 0; \
	TC_timerFlag &= ~TC_TMRFLAG_PID; \
	__set_PRIMASK(0); } while(0)


/**
 * TC regulation timer. Each tick is 40us.
 * Counts up to 26 (1ms) and stops.
 * used to collect errors for PID 
 */
static volatile uint8_t TC_timerCountPID;

/**
 * Bitwise combination of TC_TMRFLAG_*.
 */
static volatile uint8_t TC_timerFlag;




//temperature table for 316LSS
/**
 * Atomiseur resistance temperature lookup table.
 * maps the 50°C range starting at 0 °C.
 * 0, 50, 100, 150, 200, 250, 300, 350, 400
 * Linear interpolation is done inside the range.
 */

static const uint16_t Atomizer_TempTable[9] = {
	978, 1030, 1080, 1126, 1168, 1207, 
  1246, 1283, 1318 
};


static void TC_timer_PID(uint32_t unused) {
	if(TC_timerCountPID != 51) {
		TC_timerCountPID++;
	}
	else {
		TC_timerFlag |= TC_TMRFLAG_PID;
	}
}

uint16_t wattsToVolts(uint32_t watts, uint16_t res) {
	// Units: mV, mW, mOhm
	// V = sqrt(P * R)
	// Round to nearest multiple of 10
	uint16_t volts = (sqrt(watts * res) + 5) / 10;
	return volts * 10;
}

uint16_t Atomizer_ReadTemp(uint16_t atoRes, uint16_t atoResRef, uint16_t oldTemp) {
	uint8_t i;
	uint16_t lowerBound, higherBound;
	uint16_t thermRes;

  if (atoRes == 0) {
    return oldTemp;
  }
  if (atoResRef == 0) {
    return 20;
  }
	
	if (atoRes < atoResRef) {
    return 0;
  }
	thermRes = 1000L*atoRes/atoResRef;// reference at 20°C is 1ohm in the table;
  
  //around 20°C, we assume its 20°C
  if (((atoRes>atoResRef) && (atoRes - atoResRef) < 5) || ((atoRes<atoResRef) && (atoResRef - atoRes)<5)) {
    return 20;
  }

	// Handle corner cases
	if(thermRes <= Atomizer_TempTable[0]) {
		return 0;
	}
	else if(thermRes >= Atomizer_TempTable[8]) {  //ie > 400°C
  	lowerBound = Atomizer_TempTable[7];
  	higherBound = Atomizer_TempTable[8];
  	return 50 * (7) + (thermRes - lowerBound) * 50 / (higherBound - lowerBound);
	}

	// Look up lower resistance bound (higher temperature bound)
	for(i = 0; i < 9 && thermRes > Atomizer_TempTable[i]; i++);

	// Interpolate
	lowerBound = Atomizer_TempTable[i];
	higherBound = Atomizer_TempTable[i+1];
	return 50 * (i) + (thermRes - lowerBound) * 50 / (higherBound - lowerBound);
}


int main() {
	char buf[100];
	const char *atomState;
  const char *dcState;
	uint16_t volts, newVolts, battVolts, displayVolts, atoTemp, tempTC;
  int16_t errTemp, lastErrTemp, sumErrTemp;
  int cons;
	uint32_t watts, wattsDef;
	uint16_t atoResDef;//, atoRes;
	uint8_t btnState, battPerc, boardTemp, mode;
	Atomizer_Info_t atomInfo;

	if (Timer_CreateTimer(25000, 1, TC_timer_PID, 0) < 0) {
    return 1;
  }


  //just in case
	Atomizer_Control(0);
	Atomizer_ReadInfo(&atomInfo);
	atoResDef = 0;
	// Let's start with 10.0W as the initial value
	// We keep watts as mW
	watts = 10000;
  mode = 1;//power
  cons = 0;
	atoTemp = 20; //assuming 20°C is the start temp
	wattsDef = watts;
  errTemp = 0;
  lastErrTemp = 0;
  sumErrTemp = 0;
  tempTC = 200;//hardcoded temp desired ...	
	volts = wattsToVolts(watts, atomInfo.resistance);
	Atomizer_SetOutputVoltage(volts);

	while(1) {
		btnState = Button_GetState();

		// Handle fire button
		if(!Atomizer_IsOn() && (btnState & BUTTON_MASK_FIRE) 
        && atomInfo.resistance != 0 && Atomizer_GetError() == OK) {
			// Power on
      cons = 0;
      errTemp = 0;
      lastErrTemp = 0;
      sumErrTemp = 0;
			Atomizer_Control(1);
		}
		else if(Atomizer_IsOn() && !(btnState & BUTTON_MASK_FIRE)) {
			// Power off
			Atomizer_Control(0);
			watts = wattsDef;//return to watts setting by user
		}

		// Handle plus/minus keys : not allowed while firing
		if((btnState & BUTTON_MASK_RIGHT) && !(btnState & BUTTON_MASK_LEFT) && !Atomizer_IsOn()) {
			if (mode==1) {
        newVolts = wattsToVolts(watts + 100, atomInfo.resistance);
        if (newVolts > 0) {
          //don't allow over spec
    			if(newVolts <= ATOMIZER_MAX_VOLTS && watts < ATOMIZER_MAX_WATT && (watts/newVolts < ATOMIZER_MAX_CURRENT/1000)) {
    				watts += 100;
    				volts = newVolts;
    
    				// Set voltage
    				Atomizer_SetOutputVoltage(volts);
    			}
    			wattsDef = watts;	
        }
        // Slow down increment
			 Timer_DelayMs(25);
      } else {
        if (tempTC < 315) {
          tempTC += 5;
        }
        // Slow down increment
			 Timer_DelayMs(100);
      }
			
 		}

		if((btnState & BUTTON_MASK_LEFT) && !(btnState & BUTTON_MASK_RIGHT) && watts >= 100 && !Atomizer_IsOn()) {
			if (mode == 1) {
        watts -= 100;
  			volts = wattsToVolts(watts, atomInfo.resistance);
  
  			// Set voltage
  			Atomizer_SetOutputVoltage(volts);
  			wattsDef = watts;
        // Slow down decrement
  			Timer_DelayMs(25);
      } else {
        if (tempTC > 100) {
          tempTC -= 5;
        }      
        // Slow down decrement
			 Timer_DelayMs(100);      
      }
			
		}

		if(!Atomizer_IsOn() && (btnState & BUTTON_MASK_RIGHT) && (btnState & BUTTON_MASK_LEFT)) {
			//we authorize reset of resistance with left and right
      //reset if not 0
      if (atoResDef != 0) {
        atoResDef = 0;
        //switch to power if button still pressed after 1s
        Timer_DelayMs(1000);
        btnState = Button_GetState();
        if (!Atomizer_IsOn() && (btnState & BUTTON_MASK_RIGHT) && (btnState & BUTTON_MASK_LEFT)) {
          mode = 1;
        } 
      } else { 
  			Atomizer_ReadInfo(&atomInfo);
        //tcRes is just to show a live resistance, baseRes is used here
        atoResDef = atomInfo.resistance;
        //switch to power if button still pressed after 1s
        Timer_DelayMs(1000);
        btnState = Button_GetState();
        if (!Atomizer_IsOn() && (btnState & BUTTON_MASK_RIGHT) && (btnState & BUTTON_MASK_LEFT)) {
          mode = 2;
        } 
      }
      
		}


		// Update info
		// If resistance is zero voltage will be zero
		Atomizer_ReadInfo(&atomInfo);
    if (atoResDef > 0) {
      if (Atomizer_IsOn() ) {
      
      if (TC_timerFlag & TC_TMRFLAG_PID) {
			    TC_TIMER_PID_RESET();
          //really simple TC, activated only if default res is set
          //watts are used, could be volts but as users are used to set watts to set ramp up
          //it more comfortable to read
          //using PID, with simple testing coeff		
          //we are using unsigned int, so must see if current temp is more or less than temp def
          //calculate PID
          atoTemp = Atomizer_ReadTemp(atomInfo.resistance, atoResDef, atoTemp);
          lastErrTemp = errTemp;
          errTemp = atoTemp - tempTC;
          sumErrTemp += errTemp; 
          //we're going to go in two steps :
          //first : ramp up with power by user : he wants something, let's make it
          //we don't want the temperature to go higher than the selected one
          cons = wattsDef*100*errTemp/ATOMIZER_MAX_WATT + 4*sumErrTemp/100 + (lastErrTemp - errTemp);
          if (atoTemp < (tempTC - 10*tempTC/100)) { //ramp up till 10%
            watts = wattsDef;
            //cons = watts;
          } else {
            //last control : don't go higher 10%
            if (atoTemp > (tempTC + 10*tempTC/100)) { 
              watts = 0;
              //cons = watts;
            } else {   
              watts += cons;
            }
          }

          if (watts < 100) {
            //cons = 100;
            watts = 100;//to read a resistance at least
          } else if (watts > wattsDef) {
            watts = wattsDef;
          }
          
          newVolts = wattsToVolts(watts, atomInfo.resistance);
        } else {
          newVolts = volts;
        }
        if (volts == 0) {
          newVolts = 100;//to keep it reading a res
        }
      } else {
  		  newVolts = wattsToVolts(watts, atomInfo.resistance);
      }
    } else {
		  newVolts = wattsToVolts(watts, atomInfo.resistance);
    }

		if(newVolts != volts) {
			if(Atomizer_IsOn()) {
				// Update output voltage to correct res variations:
				// If the new voltage is lower, we only correct it in
				// 10mV steps, otherwise a flake res reading might
				// make the voltage plummet to zero and stop.
				// If the new voltage is higher, we push it up by 100mV
				// to make it hit harder on TC coils, but still keep it
				// under control.
        if (atoResDef == 0) {	
  				if(newVolts < volts) {
  					newVolts = volts - (volts >= 10 ? 10 : 0);
  				}
  				else {
  					newVolts = volts + 100;
  				}
        }
			}	
      //not over max voltage limitation
			if(newVolts > ATOMIZER_MAX_VOLTS) {
				newVolts = ATOMIZER_MAX_VOLTS;
			}
			volts = newVolts;
      //not over watts specified by user
			if (watts > wattsDef) {
				watts=wattsDef;
		 		volts = wattsToVolts(watts, atomInfo.resistance);
			}
      
			//not over max current limitation
			if (volts>0) {
				if ((watts/volts) > ATOMIZER_MAX_CURRENT/1000) {
					volts = 1000*watts/ATOMIZER_MAX_CURRENT;	
				}
			}
			Atomizer_SetOutputVoltage(volts);
			
		}

		// Get battery voltage and charge
		battVolts = Battery_IsPresent() ? Battery_GetVoltage() : 0;
		battPerc = Battery_VoltageToPercent(battVolts);

		// Get board temperature
		boardTemp = Atomizer_ReadBoardTemp();

		// Display info
		displayVolts = Atomizer_IsOn() ? atomInfo.voltage : volts;
	 switch(Atomizer_GetError()) {
			case SHORT:
				atomState = "SHORT";
				break;
			case OPEN:
				atomState = "NO ATOM";
				break;
			case WEAK_BATT:
				atomState = "WEAK BAT";
				break;
			case OVER_TEMP:
				atomState = "TOO HOT";
				break;
			default:
				atomState = Atomizer_IsOn() ? "FIRING" : "";
				break;
		}
    if (atomInfo.state == POWERON_BOOST) {
      dcState = "BOOST";
    } else if (atomInfo.state == POWERON_BUCK) {
      dcState = "BUCK";
    } else {
      dcState = "";
    }
    
		if (atoResDef > 0) {
      if (Atomizer_IsOn() &&  atomInfo.tcRes < atoResDef && atomInfo.tcRes>50) {
        atoResDef = atomInfo.tcRes;
      }
			atoTemp = Atomizer_ReadTemp(atomInfo.tcRes, atoResDef, atoTemp);
		}
		siprintf(buf, "%s:%3lu.%lu%s\nV:%2d.%02dV\nL:%1d.%03do\nR:%d.%03do\nA:%2d.%02dA\nC:%5d\nTC:%4dC\n%s\nB:%d%%%s%2lu%s\n%s",
      mode == 1 ? "P" : "T",
			mode == 1 ? watts / 1000 : tempTC, mode == 1 ? watts % 1000 / 100 : 0,
      mode == 1 ? "W" : "T",
			displayVolts / 1000, displayVolts % 1000 / 10,
			atoResDef / 1000, atoResDef % 1000,
			atomInfo.tcRes / 1000, atomInfo.tcRes % 1000,
			atomInfo.current / 1000, atomInfo.current % 1000 / 10,
			cons,//boardTemp,
			atoTemp,
			atomState,
			battPerc,
      Battery_IsCharging() ? "CHARG" : "",
      mode == 1 ? boardTemp : watts / 1000,
      mode == 1 ? "T" : "W", 
      dcState);
		Display_Clear();
		Display_PutText(0, 0, buf, FONT_DEJAVU_8PT);
		Display_Update();
    
  }	
}
