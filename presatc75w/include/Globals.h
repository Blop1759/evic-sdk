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

#ifndef EVICSDK_GLOBALS_H
#define EVICSDK_GLOBALS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Define type of mod : 1 = VTC MINI, 2 = PRESA TC75W
 */  
#define MODTYPE 2

/**
 * Evic Type.
 */
#define EVICVTCMINI 1

/**
 * Presa Type.
 */
#define PRESATC75W 2

/**
 * Maximum default output voltage, in millivolts.
 */                                
#define ATOMIZER_MAX_VOLTS 9000

/**
 * Maximum default output power, in milliwatts.
 */
#define ATOMIZER_MAX_WATT 75000

/**
 * Maximum default output current, in milliamps.
 */
#define ATOMIZER_MAX_CURRENT 20000


#ifdef __cplusplus
}
#endif

#endif
