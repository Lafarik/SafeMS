#pragma once
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1276Wrapper.h>
#include "SenseBoxEyeBoard.h"
#include "EyeRadioWrapper.h"
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/sensors/EnvironmentSensorManager.h>

#define DISPLAY_CLASS SSD1306Display

#include <helpers/ui/SSD1306Display.h>

extern SenseBoxEyeBoard board;
extern EyeRadioWrapper radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();
