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


//temperature table for 316SS
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
	thermRes = 1000L*atoRes/atoResRef;// + atoRes - atoResRef;
  
  if (((atoRes>atoResRef) && (atoRes - atoResRef) < 5) || ((atoRes<atoResRef) && (atoResRef - atoRes)<5)) {
    return 20;
  }

	// Handle corner cases
	if(thermRes <= Atomizer_TempTable[0]) {
		return 0;
	}
	else if(thermRes >= Atomizer_TempTable[8]) {
		return 999;
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
	uint16_t volts, newVolts, battVolts, displayVolts, atoTemp, tempTC;//, oldAtoTemp;
	uint32_t watts, wattsDef;
	uint16_t atoResDef;//, atoRes;
	uint8_t btnState, battPerc, boardTemp;
	Atomizer_Info_t atomInfo;

  //just in case
	Atomizer_Control(0);
	Atomizer_ReadInfo(&atomInfo);
	atoResDef = atomInfo.tcRes;
	// Let's start with 10.0W as the initial value
	// We keep watts as mW
	watts = 10000;
	atoTemp = 20;
	wattsDef = watts;
  tempTC = 200;//hardcoded temp desired ...	
	volts = wattsToVolts(watts, atomInfo.resistance);
	Atomizer_SetOutputVoltage(volts);

	while(1) {
		btnState = Button_GetState();

		// Handle fire button
		if(!Atomizer_IsOn() && (btnState & BUTTON_MASK_FIRE) 
        && atomInfo.resistance != 0 && Atomizer_GetError() == OK) {
			// Power on
			Atomizer_Control(1);
		}
		else if(Atomizer_IsOn() && !(btnState & BUTTON_MASK_FIRE)) {
			// Power off
			Atomizer_Control(0);
			watts = wattsDef;
		}

		// Handle plus/minus keys
		if(btnState & BUTTON_MASK_RIGHT && !Atomizer_IsOn()) {
      //Atomizer_ReadInfo(&atomInfo, 1);
			newVolts = wattsToVolts(watts + 100, atomInfo.resistance);
      if (newVolts > 0) {
  			if(newVolts <= ATOMIZER_MAX_VOLTS && watts < ATOMIZER_MAX_WATT && (watts/newVolts < ATOMIZER_MAX_CURRENT/1000)) {
  				watts += 100;
  				volts = newVolts;
  
  				// Set voltage
  				Atomizer_SetOutputVoltage(volts);
  				// Slow down increment
  				Timer_DelayMs(25);
  			}
  			wattsDef = watts;	
      }
 		}
		if(!Atomizer_IsOn() && (btnState == 0x06)) {
			//we authorize reset of resistance with left and right 
			Atomizer_ReadInfo(&atomInfo);
      atoResDef = atomInfo.tcRes;
     
				
		}
		if(btnState & BUTTON_MASK_LEFT && watts >= 100 && !Atomizer_IsOn()) {
			watts -= 100;
			volts = wattsToVolts(watts, atomInfo.resistance);

			// Set voltage
			Atomizer_SetOutputVoltage(volts);
			// Slow down decrement
			Timer_DelayMs(25);
			wattsDef = watts;
		}

		// Update info
		// If resistance is zero voltage will be zero
		Atomizer_ReadInfo(&atomInfo);
		newVolts = wattsToVolts(watts, atomInfo.resistance);

		if(newVolts != volts) {
			if(Atomizer_IsOn()) {
				// Update output voltage to correct res variations:
				// If the new voltage is lower, we only correct it in
				// 10mV steps, otherwise a flake res reading might
				// make the voltage plummet to zero and stop.
				// If the new voltage is higher, we push it up by 100mV
				// to make it hit harder on TC coils, but still keep it
				// under control.
				//atoRes = atomInfo.resistance;		
				if(newVolts < volts) {
					newVolts = volts - (volts >= 10 ? 10 : 0);
				}
				else {
					newVolts = volts + 100;
				}
			}	

			if(newVolts > ATOMIZER_MAX_VOLTS) {
				newVolts = ATOMIZER_MAX_VOLTS;
			}
			volts = newVolts;
			if (watts > wattsDef) {
				watts=wattsDef;
		 		volts = wattsToVolts(watts, atomInfo.resistance);
			}
			
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
		siprintf(buf, "P:%3lu.%luW\nV:%2d.%02dV\nL:%1d.%03do\nR:%d.%03do\nA:%2d.%02dA\nT:%5dC\nTC:%4dC\n%s\nB:%d%%%s\n%s",
			watts / 1000, watts % 1000 / 100,
			displayVolts / 1000, displayVolts % 1000 / 10,
			atoResDef / 1000, atoResDef % 1000 /*/ 10*/,
			atomInfo.tcRes / 1000, atomInfo.tcRes % 1000 /*/ 10*/,
			atomInfo.current / 1000, atomInfo.current % 1000 / 10,
			boardTemp,
			atoTemp,
			atomState,
			battPerc,
      Battery_IsCharging() ? "CHARGING" : "", 
      dcState);
		Display_Clear();
		Display_PutText(0, 0, buf, FONT_DEJAVU_8PT);
		Display_Update();
		
    if (Atomizer_IsOn() && atoResDef > 0) {	
			if (atoTemp > tempTC) {
				if (watts < 100) {
					watts = 0;
				} else {	
					watts = watts - 100;//down by 0.1W
				}		
			}
			if (atoTemp < tempTC) {
				watts = watts + 100;//up by 0.1W
        
        //very simple ramp up
        if (atoTemp<(tempTC + (5*tempTC)/100)) {
            watts += 200;
  			} else {
  				if (atoTemp-tempTC > 50) {
 						watts += 500;
  				}
  			}        
				if (watts > wattsDef) {//not over specified watts
					watts = wattsDef;
				}
			}
      //too simple limitation
			if (atoTemp>(tempTC + (5*tempTC)/100)) {
				if (watts > 200) {
          watts -= 200;
        } else {
          watts = 0;
        }
			} else {
				if (atoTemp-tempTC > 50) {
					if (watts > 500) {
						watts -= 500;
					} else {
						watts = 0;
					}
				}
			}
		}	
	}
}
