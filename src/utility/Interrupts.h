/*
   Interrupt handling helper functions for PulseSensors.
   See https://www.pulsesensor.com to get started.

   Copyright World Famous Electronics LLC - see LICENSE
   Contributors:
     Joel Murphy, https://pulsesensor.com
     Yury Gitman, https://pulsesensor.com
     Bradford Needham, @bneedhamia, https://bluepapertech.com

   Licensed under the MIT License, a copy of which
   should have been included with this software.

   This software is not intended for medical use.
*/

/*
   Any Sketch using the Playground must do one of two things:
   1) #define USE_ARDUINO_INTERRUPTS true - if using interrupts;
   2) #define USE_ARDUINO_INTERRUPTS false - if not using interrupts.
   
   Only the Sketch must define USE_ARDUINO_INTERRUPTS.
   If the Sketch doesn't define USE_ARDUINO_INTERRUPTS, or if some other file
   defines it as well, a link error will result.
   
   See notes in PulseSensorPlayground.h
   
   The code below is rather convoluted, with nested #if's.
   This structure is used to achieve two goals:
   1) Minimize the complexity the user has to deal with to use or
      not use interrupts to sample the PulseSensor data;
   2) Create an ISR() only if the Sketch uses interrupts.  Defining an
      ISR(), even if not used, may interfere with other libraries' use
      of interrupts.
   
   The nesting goes something like this:
     if the Sketch is being compiled...              #if defined(USE_ARDUINO_INTERRUPTS)
       if the user wants to use interrupts...        #if USE_ARDUINO_INTERRUPTS
         #if's for the various Arduino platforms...  #if defined(__AVR_ATmega328P__)...

   RULES of the constant USE_ARDUINO_INTERRUPTS:
   1) This file, interrupts.h, should be the only file that uses USE_ARDUINO_INTERRUPTS
     (although PulseSensorPlayground's comments talk about it to the user).
     If other code in the library wants to know whether interrupts are being used,
     that code should use PulseSensorPlayground::UsingInterrupts, which is true
     if the Sketch wants to use interrupts.
   1) Always use #if USE_ARDUINO_INTERRUPTS inside an #if defined(USE_ARDUINO_INTERRUPTS).
      If you don't first test the #if defined(...), a compile error will occur
      when compiling the library modules.
   2) USE_ARDUINO_INTERRUPTS is defined only when this file is being included
      by the user's Sketch; not when the rest of the library is compiled.
   3) USE_ARDUINO_INTERRUPTS is true if the user wants to use interrupts;
      it's false if they don't.
*/

#ifndef PULSE_SENSOR_INTERRUPTS_H
#define PULSE_SENSOR_INTERRUPTS_H


//TODO: if noInterrupts() and interrupts() are defined for Arduino 101,
// Use them throughout and eliminate these DISABLE/ENAGLE macros.
//
// Macros to link to interrupt disable/enable only if they exist
// The name is long to avoid collisions with Sketch and Library symbols.
#if defined(__arc__)
  // Arduino 101 doesn't have cli() and sei().
#define DISABLE_PULSE_SENSOR_INTERRUPTS
#define ENABLE_PULSE_SENSOR_INTERRUPTS
#else
#define DISABLE_PULSE_SENSOR_INTERRUPTS cli()
#define ENABLE_PULSE_SENSOR_INTERRUPTS sei()
#endif


/*
   (internal to the library)
   Sets up the sample timer interrupt for this Arduino Platform.
   
   Returns true if successful, false if we don't yet support
   the timer interrupt on this Arduino.
   
   NOTE: This is the declaration (vs. definition) of this function.
   See the definition (vs. declaration) of this function, below.
*/
boolean PulseSensorPlaygroundSetupInterrupt();


#if defined(USE_ARDUINO_INTERRUPTS) // that is, if the Sketch is including us...

/*
   (internal to the library) True if the Sketch uses interrupts to
   sample 
   We need to define US_PS_INTERRUPTS once per Sketch, whether or not
   the Sketch uses interrupts.
   Not doing this or doing it for every file that includes interrupts.h
   would cause a link error.
   
   To refer to this variable, use "PulseSensorPlayground::UsingInterrupts".
   
   See PulseSensorPlayground.h
*/
boolean PulseSensorPlayground::UsingInterrupts = USE_ARDUINO_INTERRUPTS;

boolean PulseSensorPlaygroundSetupInterrupt() {

#if !USE_ARDUINO_INTERRUPTS
  /*
     The Sketch doesn't want interrupts,
     so we won't waste Flash space and create complexity
     by adding interrupt-setup code.
  */
  return true; 
  
#else
  // This code sets up the sample timer interrupt
  // based on the type of Arduino platform.

  /*
     NOTE: when you change the #if's in this function,
     be sure to add similar #if's (if necessary) to the ISR() defined
     below.
  */
  
  #if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega16U4__)
    // Initializes Timer1 to throw an interrupt every 2mS.
    // Interferes with PWM on pins 9 and 10
    TCCR1A = 0x00; // DISABLE OUTPUTS AND PWM ON DIGITAL PINS 9 & 10
    TCCR1C = 0x00; // DON'T FORCE COMPARE
    #if F_CPU == 16000000L   //  if using 16MHz crystal
      TCCR1B = 0x0C; // GO INTO 'CTC' MODE, PRESCALER = 265
      OCR1A = 0x007C;  // TRIGGER TIMER INTERRUPT EVERY 2mS
    #elif F_CPU == 8000000L  // if using 8MHz crystal
      TCCR1B = 0x0B; // prescaler = 64
      OCR1A = 0x00F9;  // count to 249 for 2mS interrupt
    #endif
    TIMSK1 = 0x02; // ENABLE OCR1A MATCH INTERRUPT
    ENABLE_PULSE_SENSOR_INTERRUPTS;
    return true;


  #elif defined(__AVR_ATtiny85__)
    GTCCR &= 0x81;     // Disable PWM, don't connect pins to events
    OCR1C = 0x7C;      // Set the top of the count to  124 TEST VALUE
    OCR1A = 0x7C;      // Set the timer to interrupt after counting to TEST VALUE
    #if F_CPU == 16000000L
      TCCR1 = 0x88;      // Clear Timer on Compare, Set Prescaler to 128 TEST VALUE
    #elif F_CPU == 8000000L
      TCCR1 = 0x89;      // Clear Timer on Compare, Set Prescaler to 128 TEST VALUE
    #endif
    bitSet(TIMSK,6);   // Enable interrupt on match between TCNT1 and OCR1A
    ENABLE_PULSE_SENSOR_INTERRUPTS;
    return true;

  #else
    return false;      // unknown or unsupported platform.
  #endif

#endif // USE_ARDUINO_INTERRUPTS
}



#if USE_ARDUINO_INTERRUPTS
/*
   We create the Interrupt Service Routine only if the Sketch is
   using interrupts. If we defined it when we didn't use it,
   the ISR() will inappropriately intercept timer interrupts that
   we don't use when not using interrupts.

   We define the ISR that handles the timer that
   PulseSensorPlaygroundSetupInterrupt() set up.
   NOTE: Make sure that this ISR uses the appropriate timer for
   the platform detected by PulseSensorPlaygroundSetupInterrupt(), above.
*/
#if defined(__AVR__)
ISR(TIMER1_COMPA_vect)
{
  DISABLE_PULSE_SENSOR_INTERRUPTS;         // disable interrupts while we do this
  
  PulseSensorPlayground::OurThis->onSampleTime();
 
  ENABLE_PULSE_SENSOR_INTERRUPTS;          // enable interrupts when you're done
}
#endif


#endif // USE_ARDUINO_INTERRUPTS

#endif // defined(USE_ARDUINO_INTERRUPTS)

#endif // PULSE_SENSOR_INTERRUPTS_H
