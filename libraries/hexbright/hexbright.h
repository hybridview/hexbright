/*
Copyright (c) 2012, "David Hilton" <dhiltonp@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies, 
either expressed or implied, of the FreeBSD Project.
*/

#include <Wire.h>
#include <Arduino.h>

/// Some space-saving options
#define LED // comment out save 786 bytes if you don't use the rear LEDs
#define PRINT_NUMBER // comment out to save 626 bytes if you don't need to print numbers (but need the LEDs)
#define ACCELEROMETER //comment out to save 3500 bytes (in development, it will shrink a lot once it's finished)

// In development, api will change.
#ifdef ACCELEROMETER 
//#define DEBUG 6
#define DPIN_ACC_INT 3

#define ACC_ADDRESS             0x4C
#define ACC_REG_XOUT            0
#define ACC_REG_YOUT            1
#define ACC_REG_ZOUT            2
#define ACC_REG_TILT            3
#define ACC_REG_INTS            6
#define ACC_REG_MODE            7
#endif


// debugging related definitions
#define DEBUG 0
// Some debug modes set the light.  Your control code may reset it, causing weird flashes at startup.
#define DEBUG_OFF 0 // no extra code is compiled in
#define DEBUG_ON 1 // initialize printing
#define DEBUG_LOOP 2 // main loop
#define DEBUG_LIGHT 3 // Light control
#define DEBUG_TEMP 4  // temperature safety
#define DEBUG_BUTTON 5 // button presses - you may experience some flickering LEDs if enabled
#define DEBUG_LED 6 // rear LEDs - you may get flickering LEDs with this enabled
#define DEBUG_ACCEL 7 // accelerometer
#define DEBUG_NUMBER 8 // number printing utility
#define DEBUG_CHARGE 9 // charge state



#if (DEBUG==DEBUG_TEMP)
#define OVERHEAT_TEMPERATURE 265 // something lower, to more easily verify algorithms
#else
#define OVERHEAT_TEMPERATURE 320 // 340 in original code, 320 = 130* fahrenheit/55* celsius (with calibration)
#endif


///////////////////////////////////
// key points on the light scale //
///////////////////////////////////
#define MAX_LEVEL 1000
#define MAX_LOW_LEVEL 500
#define CURRENT_LEVEL -1

#define NOW 1


// led constants
#define RLED 0
#define GLED 1

#define LED_OFF 0
#define LED_WAIT 1
#define LED_ON 2

// charging constants
#define CHARGING 1
#define BATTERY 7
#define CHARGED 3

class hexbright {
  public: 
    // ms_delay is the time update will try to wait between runs.
    // the point of ms_delay is to provide regular update speeds,
    //   so if code takes longer to execute from one run to the next,
    //   the actual interface doesn't change (button click duration,
    //   brightness changes). Set this from 5-30. very low is generally
    //   fine (or great), BUT if you do any printing, the actual delay
    //   may be greater than the value you set.
    //   Also, the minimum value when working with the accelerometer is 9. (1000/120)
    // Don't try to use times smaller than this value in your code.
    //   (setting the on_time for less than update_delay_ms = 0)
    hexbright(int update_delay_ms);
  
    // init hardware.
    // put this in your setup().
    static void init_hardware();
    
    // Put update in your loop().  It will block until update_delay has passed.
    static void update();

    // When plugged in: turn off the light immediately, 
    //   leave the cpu running (as it cannot be stopped)
    // When on battery power: turn off the light immediately, 
    //   turn off the cpu in about .5 seconds.
    // Loop will run a few more times, and if your code turns 
    //  on the light, shutoff will be canceled. As a result, 
    //  if you do not reset your variables you may get weird 
    //  behavior after turning the light off and on again in 
    // less than .5 seconds.
    static void shutdown();

    
    // go from start_level to end_level over time (in milliseconds)
    // level is from 0-1000. 
    // 0 = no light (but still on), 500 = MAX_LOW_LEVEL, MAX_LEVEL=1000.
    // start_level can be CURRENT_LEVEL
    static void set_light(int start_level, int end_level, int time);
    // get light level (before overheat protection adjustment)
    static int get_light_level();
    // get light level (after overheat protection adjustment)
    static int get_safe_light_level();

    // Returns the duration the button has been in updates.  Keeps its value 
    //  immediately after being released, allowing for use as follows:
    // if(button_released() && button_held()>500)
    static int button_held();
    // button has been released
    static boolean button_released();
    
    // led = GLED or RLED,
    // on_time (0-MAXINT) = time in milliseconds before led goes to LED_WAIT state
    // wait_time (0-MAXINT) = time in ms before LED_WAIT state decays to LED_OFF state.
    //   Defaults to 100 ms.
    // brightness (0-255) = brightness of rear led
    //   Defaults to 255 (full brightness)
    // Takes up 16 bytes.
    static void set_led(byte led, int on_time, int wait_time=100, byte brightness=255);
    // led = GLED or RLED 
    // returns LED_OFF, LED_WAIT, or LED_ON
    // Takes up 54 bytes.
    static byte get_led_state(byte led);
    // returns the opposite color than the one passed in
    // Takes up 12 bytes.
    static byte flip_color(byte color);


    // Get the raw thermal sensor reading. Takes up 18 bytes.
    static int get_thermal_sensor();
    // Get the degrees in celsius. I suggest calibrating your sensor, as described
    //  in programs/temperature_calibration. Takes up 60 bytes
    static int get_celsius();
    // Get the degrees in fahrenheit. After calibrating your sensor, you'll need to
    //  modify this as well. Takes up 60 bytes
    static int get_fahrenheit();

    // returns CHARGING, CHARGED, or BATTERY
    // This reads the charge state twice with a small delay, then returns 
    //  the actual charge state.  BATTERY will never be returned if we are 
    //  plugged in.
    // Use this if you take actions based on the charge state (example: you
    //  turn on when you stop charging).  Takes up 56 bytes (34+22).
    static byte get_definite_charge_state();
    // returns CHARGING, CHARGED, or BATTERY
    // This reads and returns the charge state, without any verification.  
    //  As a result, it may report BATTERY when switching between CHARGED 
    //  and CHARGING.
    // Use this if you don't care if the value is sometimes wrong (charging 
    //  notification).  Takes up 34 bytes.
    static byte get_charge_state();

    
    // prints a number through the rear leds
    // 120 = 1 red flashes, 2 green flashes, one long red flash (0), 2 second delay.
    // the largest printable value is +/-999,999,999, as the left-most digit is reserved.
    // negative numbers begin with a leading long flash.
    static void print_number(long number);
    // currently printing a number
    static boolean printing_number();

#ifdef ACCELEROMETER
    // Accelerometer (in development)
    // good documentation:
    // http://cache.freescale.com/files/sensors/doc/app_note/AN3461.pdf
    // http://cache.freescale.com/files/sensors/doc/data_sheet/MMA7660FC.pdf
    static void read_accelerometer_vector();
    // accepts things like ACC_REG_TILT
    static byte read_accelerometer(byte acc_reg);

    static void print_accelerometer();

    // last two readings have had less than tolerance acceleration(in Gs)
    static boolean stationary(double tolerance=.1);
    // last reading had more than tolerance acceleration (in Gs)
    static boolean moved(double tolerance=.5);


    //returns the angle between straight down and 
    // returns 0 to 100. 0 == down, 100% == up.  Multiply by 1.8 to get degrees.
    // expect noise of about 10.
    static double difference_from_down();
    static double get_dp();
    static double get_gs(); // Gs of acceleration
    static double* get_axes_rotation();
    // lots of noise < 5*.  Most noise is <10*
    // noise varies partially based on sample rate.  120, noise <10*.  64, ~8?
    static double get_angle_change();

    static double jab_detect(float sensitivity=1);

  private:
    static double angle_difference(double dot_product, double magnitude1, double magnitude2);
    static void normalize(double* out_vector, double* in_vector, double magnitude);
    static void sum_vectors(double* out_vector, double* in_vector1, double* in_vector2);
    static double dot_product(double* vector1, double* vector2);
    static double get_magnitude(double* vector);

    static int convert_axis_number(byte value);
    static void print_vector(double* vector, char* label);

    static void enable_accelerometer();
    static void disable_accelerometer();
  public:
#endif

  private: 
    static void adjust_light();
    static void set_light_level(unsigned long level);
    static void overheat_protection();

    static void update_number();

    // controls actual led hardware set.  
    //  As such, state = HIGH or LOW
    static void _set_led(byte led, byte state);
    static void _led_on(byte led);
    static void _led_off(byte led);
    static void adjust_leds();

    static void read_thermal_sensor();
    
    static void read_button();
};


