/* -----------------------------------------------------------------------
**  ArduTest LED
** ==============
**
**  Original from https://jaycar.com.au
**
**  DPIN--39R--+--10R---TESTLED---GND
**         |      |         |
**        470u    ATOP     ABOT
**         |
**        GND
**
**  Measures LED characteristics by charging up the cap to deliver target 
**  current and find forward voltage. From target current, we can calculate 
**  R to be used with a design supply voltage.
**  
**  Revision History:
**  0.1: - take original code and port it no new Hardware
**  0.2: - optimize code and cleanup
**  0.3: - rewrite LCD display routine to minimize flicker
**  0.4: - rewrite PWM routine to avoid LED over current
**  0.5: - add voltage test routine while pressing I-Down at startup
**  0.6: - add setup for led forward voltage at measure-start (be careful what you are doing here!)
**
** Inputs:
** --------
** A0 = analog Voltage ATOP
** A1 = analog Voltage ABOT
** A2 = Key I Down
** A3 = Key I Up
** A4 = Key V Down
** A5 = Key V Up
** A6 = analog Voltage Battery
**
** Outputs:
** ---------
** D8 = LCD RS 
** D7 = LCD Enable
** D6 = LCD Data 4
** D5 = LCD Data 5
** D4 = LCD Data 6
** D2 = LCD Data 7
** D3 = PWM output
** 
** EEPROM data:
** -------------
** 0 - values stored? (255 = no / 0 = yes)
** 1 - LED voltage start PWM vlaue (8bit)
**
** Board:
** -------
** Arduino Nano V3 | Print: ArduTest LED V0.2
** 
** Author: M.Stoffers
** Year: 2022
** License: CC (BY|NC|SA)
** -------------------------------------------------------------------------*/  
#include <LiquidCrystal.h>
#include <avr/eeprom.h>

// -----------------------------
// PIN-defines
// -----------------------------
#define Version "V0.6"
#define LCD_D2 2
#define LCD_D4 4
#define LCD_D5 5
#define LCD_D6 6
#define LCD_D7 7
#define LCD_D8 8
#define DPIN 3
#define ATOP A0
#define ABOT A1
#define ABAT A6
#define KEY_I_UP A3
#define KEY_I_DW A2
#define KEY_V_UP A5
#define KEY_V_DW A4

// -----------------------------
// System-defines
// -----------------------------
#define OSAMP 16                                                  // Number of oversamples
#define LCDINT 250                                                // LCD update interval
#define KEYINT 150                                                // Key check interval
#define VCCINT 5000                                               // VCC read interval

#define between(x, a, b)  (((a) <= (x)) && ((x) <= (b)))

// -----------------------------
// declares
// -----------------------------
LiquidCrystal lcd(LCD_D8, LCD_D7, LCD_D6, LCD_D5, LCD_D4, LCD_D2);

// -----------------------------
// variables
// -----------------------------
uint8_t pins[]={KEY_I_UP, KEY_I_DW, KEY_V_UP, KEY_V_DW};          // array of Key pins

uint8_t ma_Char[] = { 0x0A, 0x15, 0x15, 0x00, 0x0E, 0x11, 0x1F, 0x11 };
uint8_t set1_Char[] = { 0x09, 0x15, 0x11, 0x09, 0x05, 0x15, 0x09, 0x00 };
uint8_t set2_Char[] = { 0x17, 0x02, 0x02, 0x12, 0x02, 0x02, 0x12, 0x00 };
uint8_t led1_Char[] = { 0x00, 0x0C, 0x0A, 0x09, 0x18, 0x09, 0x0A, 0x0C };
uint8_t led2_Char[] = { 0x03, 0x13, 0x14, 0x10, 0x1F, 0x10, 0x10, 0x10 };
uint8_t bat1_Char[] = { 0x00, 0x0C, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E };
uint8_t bat2_Char[] = { 0x00, 0x0C, 0x1E, 0x12, 0x12, 0x1E, 0x1E, 0x1E };
uint8_t bat3_Char[] = { 0x00, 0x0C, 0x1E, 0x12, 0x12, 0x12, 0x12, 0x1E };

uint16_t VCC;                                                     // exact VCC
uint32_t vbat;                                                    // battery voltage
uint8_t pwmout=0;                                                 // pwm output of current driver
long atop, abot, arr, abat;                                       // analog sample values
long vled, vrr, irr, pset;                                        // LED voltage, Resistor voltage, resistor current, display power
uint8_t itest=10;                                                 // test current, starting at 10mA
uint32_t vset =12000;                                             // display voltage, start at 12V
uint8_t battery_status;                                           // battery status to display bat symbol

uint32_t r_calc, rval, r24[]={10,11,12,13,15,16,18,20,22,24,27,30,33,36,39,43,47,51,56,62,68,75,82,91};
uint16_t r24_mul;                                                 // variables to determine R24 value
uint8_t r24_loop;
boolean r24_found;
boolean rvalid=false;                                             // flag if resistor value is valid 

boolean lcdflash = false;                                         // lcd flashing phase variable
long lastlcd=0, lastkey=0, lastvcc=0;                             // time of last lcd & key update

uint16_t LED_forw_v_start;                                        // LED measure startup up voltage
uint16_t vsetup=0;
boolean while_loop = true;

// -----------------------------
// Setup
// -----------------------------
void setup() {
  pinMode(DPIN,OUTPUT);                                           // Set PWM pin aus Output
  for(uint8_t i=0; i<4; i++) pinMode(pins[i], INPUT_PULLUP);      // Define button Pins as Input with Pullup resistors

  readVcc();                                                      // Read VCC ...

  analogReference(DEFAULT);                                       // ... set ADC Reference back to default
  delay(2);                                                       // ... and wait for ADC to become stable
  
  lcd.begin(16, 2);                                               // LCD initialize 
  
  lcd.createChar(1, ma_Char);                                     // create special character#1 : mA
  lcd.createChar(2, set1_Char);                                   // create special character#2 : SE
  lcd.createChar(3, set2_Char);                                   // create special character#3 : T
  lcd.createChar(4, led1_Char);                                   // create special character#4 : LED symbol left
  lcd.createChar(5, led2_Char);                                   // create special character#5 : LED symbol right
  lcd.createChar(6, bat1_Char);                                   // create special character#6 : Battery symbol full
  lcd.createChar(7, bat2_Char);                                   // create special character#6 : Battery symbol half
  lcd.createChar(8, bat3_Char);                                   // create special character#6 : Battery symbol empty

  abat = analogoversample(ABAT,OSAMP);                            // read battery voltage and calc to mV
  vbat = (abat * VCC/1023)*2;

  if(between(vbat,8700,9900)) battery_status = 1;                 // decide to show which battery symbol
  else if(between(vbat,8000,8699)) battery_status = 2;            // 1=full, 2=half, 3=empty
  else battery_status = 3;                                        // if voltage to low, show empty battery symbol

  Check_EEPROM();                                                 // check if EEPROM is already initialized and read values
  
  StartupScreen();                                                // Show Startup Screen

  if(!digitalRead(KEY_I_DW)) Voltage_Test();                      // If key "current down" is pressed at startup, call Sub voltage test
  if(!digitalRead(KEY_I_UP)) Voltage_Setup();                     // If key "current up" is pressed at startup, call Sub voltage setup
  
  Draw_Init_LCD();
}

// -----------------------------
// Mainprogram
// -----------------------------
void loop() {
  rvalid=false;                                                   // set flag to not valid
  pset=0;                                                         // set power consumption var to zero
  
  atop = analogoversample(ATOP,OSAMP);                            // read voltage ATOP
  abot = analogoversample(ABOT,OSAMP);                            // and ABOT including oversample
  
  arr = atop - abot;                                              // this is the analog value across the 10R sense resistor
  if(arr < 0) arr = 0;                                            // sanity check
  
  vled = abot * VCC/1023;                                         // calc voltage across LED
  vrr = arr * VCC/1023;                                           // calc voltage across sense resistor
  irr = vrr / 10;                                                 // led and resistor current in mA

  if(irr < 1) pwmout = LED_forw_v_start;                          // if test LED is not present put PWM to given startup voltage 
  else {
    if(irr < itest) {                                             // ramp up current if too low
      pwmout++;
      if(pwmout > 255) pwmout = 255;
    }    
    if(irr > itest){                                              // ramp down if too high
      pwmout--;
      if(pwmout < 0) pwmout = 0;
    }
    if(irr > 34){                                                 // ramp down quick if too too high
      pwmout = pwmout - 5;
      if(pwmout < 0) pwmout = 0;
    }
    if(irr > itest * 3){                                          // ramp down quick if too too high
      pwmout = pwmout - 5;
      if(pwmout < 0) pwmout = 0;
    }
  }
  analogWrite(DPIN,pwmout);                                       // output new PWM

  if(irr < 1) rvalid = false;
  else if(vled > vset) rvalid = false;                            //if vled>vset, no valid resistor exists
  else {
    r_calc = (vset - vled) / itest;                               // calc needed resistor

    r24_mul = 1;                                                  // set all var for R24 search to start
    r24_loop = 0;
    r24_found = false;
  
    while(r24_loop < 4) {                                         // find next highest E24 value
      for(int i=0;i<24;i++){                                      
        if(!r24_found && ((r24[i] * r24_mul) > r_calc)){
          rval = r24[i]*r24_mul;
          rvalid=true;
          r24_found=true;
          r24_loop = 4;
       }
      }
      if(!rvalid) {                                               // if R24 search before has not matched
       r24_loop ++;                                                
       r24_mul *= 10;                                             // try next decade
      }
    }
  }
  if(abs(irr-itest)>(itest/5)+1){rvalid=false;}                   // has current settled within 20%?
  
  if(rvalid){pset=itest*itest*rval;}                              // this will be in microwatts (milliamps squared)
  
  if(millis()-lastlcd>LCDINT){                                    // check if display needs to be updated
    lastlcd=millis();
    dolcd();                                                      // update display
    lcdflash=!lcdflash;                                           // toggle flash variable
  }
  if(millis()-lastkey>KEYINT){                                    // do keys needs to be checked?
    lastkey=millis();
    dobuttons();
  }
  if(millis()-lastvcc>VCCINT){                                    // do VCC needs to be re-read?
    lastvcc=millis();
    readVcc();                                                    // Read VCC ...

    analogReference(DEFAULT);                                     // ... set ADC Reference back to default  
    delay(2);                                                     // ... and wait for ADC to become stable
  }
  delay(1);
} 

// -----------------------------
// Subroutine to update LCD
// -----------------------------
void dolcd(){
  lcd.setCursor(3,0);                                             // set postion first line|4th character
  if(itest>9) lcd.write(((itest/10)%10)+'0');                     // write tens of I-Test ...
  else lcd.write(' ');                                            // ... blank tens if zero
  lcd.write((itest%10)+'0');                                      // write ones of I-Test
  lcd.write(1);                                                   // write mA symbol and spacer
  lcd.write('|');
  
  if(vset>9999) lcd.write(((vset/10000)%10)+'0');                 // write tens of V-Set ..
  else lcd.write(' ');                                            // ... blank if zero
  lcd.write(((vset/1000)%10)+'0');                                // write ones of V-Set
  lcd.write('v');lcd.write(' ');                                  // and the electrical characteristic
  
  
  if((pset>249999)&&(lcdflash)){                                  // if power is above 1/4 watt
    lcd.setCursor(15,1);                                          // blink a 'P' on position 16 in line 2
    lcd.write('P');
  }
  else {
    lcd.setCursor(15,1);
    lcd.write(' ');
  } 
  
  lcd.setCursor(3,1);                                             // set postion second line|4th character
  
  if(irr < 1){                                                    // if test LED is not present ...
    lcd.print("-- ");                                             // print only dashes
  }
  else {                                                          // otherwise show the LED voltage
    lcd.write(((vled/1000)%10)+'0');
    lcd.write('.');
    lcd.write(((vled/100)%10)+'0');
  }
  lcd.write('v');lcd.print(" | ");                                // write 'V' char and spacer

  //actual LED current
  if(irr < 1) {                                                   // if test LED is not present ...
    lcd.print("- ");                                              // print only a dash
  }
  else {                                                          // otherwise show LED current
    if(irr>9) lcd.write(((irr/10)%10)+'0');
    else lcd.write(' ');
    lcd.write((irr%10)+'0');
  }
  lcd.write(1);                                                   // write 'ma' symbol
   
  if(rvalid){                                                     // if there is a calculated R24 value
    lcdprintrval(rval);                                           // call subroutine to print on LCD
    lcd.write(' ');
  }
  else {                                                          // otherwise leave field blank
    lcd.setCursor(11,0);
    lcd.print("     ");
  }
}

// -----------------------------
// Subroutine
// -----------------------------
void lcdprintrval(uint32_t rval){                                 // print a value in 10k0 format, always outputs 4 characters
  uint32_t mult=1;
  uint32_t modval;
  
  lcd.setCursor(11,0);                                            // go to position 12 in line one
  lcd.write(126);                                                 // and write the result-arrow
  
  if(rval>999) mult=1000;                                         // set multiplier
  if(rval>999999) mult=1000000;
  modval=(10*rval)/mult;                                          // convert to final format, save a decimal place
  if(modval>999){                                                 // print nnMn format
    lcd.write(((modval/1000)%10)+'0');
    lcd.write(((modval/100)%10)+'0');
    lcd.write(((modval/10)%10)+'0');
    lcdprintmult(mult);
  }
  else if(modval>99){                                             // print nnMn format
    lcd.write(((modval/100)%10)+'0');
    lcd.write(((modval/10)%10)+'0');
    lcdprintmult(mult);
    lcd.write(((modval)%10)+'0');
      }
  else{                                                           // print _nMn format
    lcd.write(' ');
    lcd.write(((modval/10)%10)+'0');
    lcdprintmult(mult);
    lcd.write(((modval)%10)+'0');
    }
  }

// -----------------------------
// Subroutine to print multiplier
// -----------------------------
void lcdprintmult(uint32_t mult){
  switch (mult){
    case 1: lcd.write(244); break;                                // print 'ohm' symbol
    case 1000:  lcd.print('k');break;                             // print 'kilo' ohm
    case 1000000: lcd.print('M');break;                           // print 'mega' ohm
  }
}

// -----------------------------
// Subroutine to read buttons
// -----------------------------
void dobuttons(){      
    if(!digitalRead(KEY_I_DW)) {                                  // if button I-Down is pressed
      itest=itest-1;                                              // lower test current by one
      if(itest<2) itest=2;                                        // and limit test current to 2mA
    }
    if(!digitalRead(KEY_I_UP)) {                                  // if button I-Up is pressed
      itest=itest+1;                                              // higher test current by one
      if(itest>30)itest=30;                                       // and limit test current to 30mA
    }
    if(!digitalRead(KEY_V_UP)) {                                  // if button V-Up is pressed
      vset=vset+1000;                                             // raise set voltage by 1000mV/1V
      if(vset>50000) vset=50000;                                  // and limit set voltage to 50V
    }
    if(!digitalRead(KEY_V_DW)) {                                  // if button V-Down is pressed
      vset=vset-1000;                                             // lower set voltage by 1000mV/1V
      if(vset<2000) vset=2000;                                    // and limit it to 2V
    }   
}

// -------------------------------
// Subroutine to Analog Oversample
// -------------------------------
long analogoversample(uint8_t pin,uint8_t samples){               
  long n=0;
  for(uint8_t i=0;i<samples;i++){                                 // read pin samples times ...
    n += analogRead(pin);                                         // ... add it to helper variable
  } 
  return n/samples;                                               // and return value dived by number of samples
}

// -------------------------------
// Subroutine to read VCC
// -------------------------------
void readVcc() { 
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);         // set analog reference to build in 1V15
  delay(2);                                                       // wait for Vref to settle
  ADCSRA |= _BV(ADSC);                                            // convert
  while (bit_is_set(ADCSRA,ADSC));                                // wait for conversion to become ready
  VCC = ADCL;
  VCC |= ADCH<<8;                                                 // read 16bit in total
  VCC = 1126400L / VCC;                                           // back-calculate AVcc in mV
}

// -------------------------------
// Subroutine to draw Start Screen
// -------------------------------
void StartupScreen() {
  lcd.setCursor(0,0);                                             // start in the upper left corner
  lcd.write(4);                                                   // draw the LED symbol
  lcd.write(5);
  lcd.setCursor(4,0);                                             // go to position 5 in line one
  lcd.print("ArduTest");                                          // write Text
  lcd.setCursor(14,0);                                            // go to position 15 in line one
  lcd.write(4);                                                   // and draw another LED symbol
  lcd.write(5);
  lcd.setCursor(1,1);                                             // go to position 2 in line two
  switch(battery_status) {                                        // show battery symbol according to input voltage
    case 1: lcd.write(6);                                         // battery symbol 'full'
    break;
    case 2: lcd.write(7);                                         // battery symbol 'half'
    break;
    case 3: lcd.write(8);                                         // battery symbol 'empty'
    break;
  }
  lcd.setCursor(6,1);                                             // go to position 7 in line two
  lcd.print("LED");                                               // print text
  lcd.setCursor(12,1);                                            // go to position 13 in line two
  lcd.print(Version);                                             // and print version number
  delay(5000);                                                    // wait for 5s
  lcd.clear();                                                    // and clear the LCD screen
}

// ---------------------------------
// Subroutine to draw Screen initial
// ---------------------------------
void Draw_Init_LCD() {
  lcd.setCursor(0,0);                                             // go to first position in line one
  lcd.write(2);                                                   // and draw 'SET' 
  lcd.write(3);
  lcd.setCursor(0,1);                                             // go to first posiiton in line two
  lcd.write(4);                                                   // and draw the LED symbol
  lcd.write(5);
}

// ---------------------------------
// Subroutine to draw Screen initial
// ---------------------------------
void Voltage_Test() {
  lcd.setCursor(0,0);                                             // go to position 0 line one
  lcd.print("Bat: ");                                             // and print "Bat"
  lcd.setCursor(0,1);                                             // go to position 0 line two
  lcd.print("Vcc: ");                                             // and print "Vcc"

  while(1) {                                                      // this goes forever
    readVcc();                                                    // read VCC
    analogReference(DEFAULT);                                     // ... set ADC Reference back to default
    delay(2);                                                     // ... and wait for ADC to become stable
      
    abat = analogoversample(ABAT,OSAMP);                          // read battery voltage and calc to mV
    vbat = (abat * VCC/1023)*2;

    lcd.setCursor(5,0);                                           // go to position 6 line one
    lcd.write(((vbat/1000)%10)+'0');                              // print ones of vbat
    lcd.write('.');                                               // write a dot
    lcd.write(((vbat/100)%10)+'0');                               // print first decimal of vbat
    lcd.write(((vbat/10)%10)+'0');                                // and second decimal
    lcd.print("V");                                               // write the unit "V"
    lcd.setCursor(5,1);                                           // go to position 6 line two
    lcd.write(((VCC/1000)%10)+'0');                               // print ones of vcc
    lcd.write('.');                                               // write a dot
    lcd.write(((VCC/100)%10)+'0');                                // print first decimal of vcc
    lcd.write(((VCC/10)%10)+'0');                                 // and second decimal
    lcd.print("V");                                               // write the unit "V"
    delay(250);                                                   // wait 250ms for a LCD Refresh
  }
}

// ---------------------------------
// Subroutine to check EEPROM
// ---------------------------------
void Check_EEPROM() {
  eeprom_busy_wait();                                             // wait for eeprom to become ready
  uint8_t ee_value0 = eeprom_read_byte((uint8_t*)0);              // read cell#0
  if(ee_value0 == 0xff) {                                         // if it contains 0xff the value is not set
    eeprom_busy_wait();                                           // wait
    eeprom_write_byte((uint8_t*)1,128);                           // write value 128 to cell#1
    LED_forw_v_start = 128;                                       // store this value also in variable
    eeprom_busy_wait();                                           // and wait
    eeprom_write_byte((uint8_t*)0,0);                             // and write a 0x00 to cell#0 -> value set
    lcd.setCursor(0,0);                                           // set LCD position 0 in line one
    lcd.print("EEPROM init OK!");                                 // write label
    delay(1500);                                                  // wait for 1,5s
    lcd.clear();                                                  // and clear LCD screen
  }
  else{
    eeprom_busy_wait();                                           // if cell#0 contains any other value
    LED_forw_v_start = eeprom_read_byte((uint8_t*)1);             // read cell#1 and store it in variable
  }
}

// ---------------------------------
// Subroutine to setup LED voltage
// ---------------------------------
void Voltage_Setup() {
  lcd.setCursor(0,0);                                             // go to position 0 line one
  lcd.write(4);                                                   // draw the LED symbol
  lcd.write(5);
  lcd.print(" Vstart:");                                          // and add Label
  lcd.setCursor(0,1);                                             // go to position 0 line two
  lcd.print("[I-:save]");                                         // and add store-Label
  
  while(while_loop) {                                             // this goes forever

    vsetup=(5000/255)*LED_forw_v_start;                           // convert 8bit pwm value to mV
    lcd.setCursor(11,0);                                          // go to position 13 line one
    lcd.write(((vsetup/1000)%10)+'0');                            // print ones of vsetup
    lcd.write('.');                                               // write a dot
    lcd.write(((vsetup/100)%10)+'0');                             // print first decimal of vsetup
    lcd.write(((vsetup/10)%10)+'0');                              // and second decimal
    lcd.print("V");                                               // write the unit "V"

    if(!digitalRead(KEY_V_UP)) {                                  // if button V-Up is pressed
      LED_forw_v_start++;                                         // increase  voltage by 1
      if(LED_forw_v_start>250) LED_forw_v_start=250;              // and limit set voltage to 250
      delay(250);                                                 // wait 250ms for debounce & LCD Refresh
    }
    if(!digitalRead(KEY_V_DW)) {                                  // if button V-Down is pressed
      LED_forw_v_start--;                                         // decrease voltage by 1
      if(LED_forw_v_start<100) LED_forw_v_start=100;              // and limit it to 100
      delay(250);                                                 // wait 250ms for debounce & LCD Refresh
    }
    if(!digitalRead(KEY_I_DW)) {                                  // if button I-Down is pressed
      eeprom_busy_wait();                                         // wait for eeprom to become ready
      eeprom_write_byte((uint8_t*)1,LED_forw_v_start);            // and store pwm value in cell#1
      while_loop=false;                                           // set while loop to end
      delay(250);                                                 // wait 250ms for debounce & LCD Refresh
    }
  }
}
