//

#include <SPI.h>
#include <Rotary.h>
/*
  Arduino pins      MAX7219 pins
  MOSI  11          DIN
  SS    10          LOAD
  sCK   13          CLK
*/
#define INTERRUPT_ROTARY 1  // that is, pin 3
#define INTERRUPT_SWITCH 0  // that is, pin 2
#define MAX7219_DIN 11
#define MAX7219_CS 10
#define MAX7219_CLK 13
#define MAX7219_REG_NOOP          (0x0)
#define MAX7219_REG_DECODEMODE    (0x9)
#define MAX7219_REG_INTENSITY     (0xA)
#define MAX7219_REG_SCANLIMIT     (0xB)
#define MAX7219_REG_SHUTDOWN      (0xC)
#define MAX7219_REG_DISPLAYTEST   (0xF)
#define MAX7219_DIGIT_REG(pos)    (pos)
#define initialJoyPositionX 52
#define initialJoyPositionY 52
#define btnNONE 52
#define LOAD    9 // Pin 9 - connect to AD9851 LOAD (FQ_UP) Pin
#define W_CLK   8 // Pin 8 = connect to AD9851 CLK Pin
#define DATA    7 // Pin 7 - connect to AD9851 DATA Pin
#define pulseHigh(pin)  {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }
 
 int ledPin = 13;
 int joyPin1 = 0;                 // slider variable connecetd to analog pin 0
 int joyPin2 = 1;                 // slider variable connecetd to analog pin 1
 int value1 = 0;                  // variable to read the value from the analog pin 0
 int value2 = 0; 
 int value1a = 0;
 int value2a = 0;// variable to read the value from the analog pin 1
 int band = 3;
 int bandOld = 3;
 int stp = 7; //10000Hz
 int_fast32_t increment = 10000;
 int_fast32_t incrementOld = 10000;
 
 int stpOld = 7;
 int adcX = 0;
 int adcY = 0;
 int adcXNew = 0;
 int adcYNew = 0;
int_fast32_t rx = 14100000; // Starting frequency of VFO 14.100 MHz
int_fast32_t rxOld;        // variable to hold the updated frequency
int_fast32_t rx_ofs;        // command frequency for DDS incl. possible IF offset 
int_fast32_t step1 = 100;
volatile boolean fired;
volatile boolean up;
int buttonState = 0;
int lastButtonState = 0;
//volatile int increment = 10;
//volatile int incrementWH = 10;
// Variables

byte dp = 0;
unsigned long num = 00000000ul;
unsigned long nbr = 99999999ul; // largest number before overflow
unsigned long i;
//const int slaveSelect = 10;
//const int encoderPinCLK = 4;
//const int encoderPinDT = 3;
//const int encoderPinSW = 2;

  



unsigned long encoderPos = 0;
boolean encoderAlast = LOW; // remembers previous pin state

Rotary r = Rotary(2, 3);

//**********************************************************************
void setup()
{

  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  // initialize the pushbutton pin as an input:
  //pinMode(encoderPinSW, INPUT);
  //pinMode(encoderPinDT, INPUT);
  //pinMode(encoderPinCLK, INPUT);
  //pinMode(slaveSelect, OUTPUT);
  pinMode(MAX7219_DIN, OUTPUT);
  pinMode(MAX7219_CS, OUTPUT);
  pinMode(MAX7219_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(LOAD, OUTPUT);
  pulseHigh(W_CLK);
  pulseHigh(LOAD);
  pulseHigh(DATA);

  //digitalWrite(encoderPinCLK, HIGH);
  //digitalWrite(encoderPinDT, HIGH);
  //digitalWrite(encoderPinSW, HIGH);

  //attachInterrupt (INTERRUPT_ROTARY, isr1, CHANGE);   // interrupt 0 is pin 2, interrupt 1 is pin 3
  
  SPI.begin();
  Serial.begin(9600);
  //clear7219();
  SPI.setBitOrder(MSBFIRST); 
  // MAX7219 requires most significant bit first
  sendByte (MAX7219_REG_SCANLIMIT, 7);      // show 8 digits zero relative
  // Change next line to 0xFF if you want all positions to display a number
  sendByte (MAX7219_REG_DECODEMODE, 0xFF);  // all digits
  sendByte (MAX7219_REG_DISPLAYTEST, 0);    // no display test
  sendByte (MAX7219_REG_INTENSITY, 0xD);    // character intensity: range: 0 to 15
  sendByte (MAX7219_REG_SHUTDOWN, 1);       // not in shutdown mode (ie. start it up)
  displayNumber(rx);
  digitalWrite(MAX7219_CS, HIGH); //deselect slave

}
//                     End of setup()

//**********************************************************************

void loop()
{
  readJoystick();
  readBandSelect();
  readStepSelect();

  if (rx != rxOld)  {    
    Serial.print("RX  frequency : "); 
    Serial.print(rx);
    //Serial.println("Hz"); 
    delay(100);
    sendFrequency(rx);
    displayNumber(rx);
    rxOld = rx;
  //rx_ofs = rx + 0;             // add IF offset frequency if required    
  //Serial.print("DDS frequency : "); 
  //Serial.print(rx_ofs);
  //Serial.println("Hz"); 
  //sendFrequency(rx_ofs);       // frequency for controlling the DDS
  }  
}


//*********************************************************************

//                      End of  loop()


//**********************************************************************
//                           FUNCTIONS
//**********************************************************************

//**********************************************************************
ISR(PCINT2_vect) {
  unsigned char result = r.process();
  if (result) {    
    if (result == DIR_CW){rx=rx+increment;}
    else {rx=rx-increment;};       
      if (rx >=54000000){rx=rxOld;}; // UPPER VFO LIMIT 40MHz
      if  (rx <= 100000){rx=rxOld;}; // LOWER VFO LIMIT 100KHz
  }
}
//**********************************************************************
// function to calculate frequency
void sendFrequency(double frequency)
{
  int32_t freq = frequency * 4294967295 / 180000000;
  for (int b=0; b<4;b++, freq >>= 8)
  {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x000);
  pulseHigh(LOAD);
}
//**********************************************************************
void tfr_byte(byte data)
{
  for (int i=0;i<8;i++, data >>= 1)
  {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(W_CLK);
  }
}
//**********************************************************************
// transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
// function to display up to eight digits on a 7-segment display
void displayNumber( unsigned long myNumber)
{
  unsigned long number = myNumber;
  for (byte digit = 1; digit < 9; digit++)
  {
    byte character = number % 10;  // get the value of the rightmost decade

    //replace leadings 0s with blanks
    if (number == 0 && digit > 1)
    {
      character = 0xF;  //value needed to blank a 7221 digit
    }

    //turn on the decimal point to act as a comma if number is > 999 or >999999
    if ((digit == 7 && myNumber > 999999) || (digit == 4 && myNumber > 999))
    {
      //add a dp
      character |= 0b10000000;
    }

    //if we have overflowed the maximum count turn on all the dp to indicate this
    if (myNumber >= nbr)
    {
      // this adds the dp at overflow
      character |= 0b10000000;
    }

    sendByte (digit, character);
    number = number / 10;            //remove the LSD for the next iteration
  }

}//                         End of displayNumber


//**********************************************************************



//**********************************************************************
// Clear the bubble display
void clear7219()
{
  SPI.setBitOrder(MSBFIRST);              // MAX7219 requires most significant bit first
  sendByte (MAX7219_REG_SCANLIMIT, 7);      // show 8 digits zero relative
  sendByte (MAX7219_REG_DECODEMODE, 0xFF);  // in the MSD use bit patterns not digits
  sendByte (MAX7219_REG_DISPLAYTEST, 0);    // no display test
  sendByte (MAX7219_REG_INTENSITY, 0x9);    // character intensity: range: 0 to 15
  sendByte (MAX7219_REG_SHUTDOWN, 1);       // not in shutdown mode (ie. start it up)
  num = 0UL;                                // clear the digits. Leading 0s will be blanked.
  displayNumber(num);

}//                         End of clear7219()

//**********************************************************************
//write a byte to the 7219 register in question
void sendByte (const byte reg, const byte data)
{
  digitalWrite (MAX7219_CS, LOW);  //enable 7219
  SPI.transfer (reg);      //select the 7219 register
  SPI.transfer (data);     //send the data for this register
  digitalWrite (MAX7219_CS, HIGH); //disable 7219

}//                         End of sendbyte()

//**********************************************************************
// convert joystick values
int treatValue(int data)
{
  return (data * 9 / 1024) + 48;
  
}//                         End of tratValue()


//**********************************************************************
int readBandSelect()
{
  if (band == 7) {band = 1;}
  if (band == 0) {band = 6;}
  if (band != bandOld)
  {
    if (band==1) { rx = 1800000L; Serial.println("selected band : 160m"); }
    else if (band==2) { rx = 3600000L; Serial.println("selected band : 80m"); }
    else if (band==3) { rx = 7100000L; Serial.println("selected band : 40m"); }
    else if (band==4) { rx = 14100000L; Serial.println("selected band : 20m"); }
    else if (band==5) { rx = 21100000L; Serial.println("selected band : 15m"); }
    else if (band==6) { rx = 28500000L; Serial.println("selected band : 10m"); } 
    delay(500); 
    bandOld = band; 
  }
}
//**********************************************************************

int readStepSelect()
{
if (stp == 10) { stp = 1;  increment = 10;}
if (stp == 0) { stp = 9;  increment = 1000000;}

 if (stp != stpOld)
 {
    if (stp == 1) {increment = 10; Serial.println("selected Step : 10Hz");}
    else if (stp == 2) {increment = 50; Serial.println("selected Step : 50Hz");}
    else if (stp == 3) {increment = 100; Serial.println("selected Step : 100Hz");}
    else if (stp == 4) {increment = 1000; Serial.println("selected Step : 1KHz");}
    else if (stp == 5) {increment = 2500; Serial.println("selected Step : 2.5KHz");}
    else if (stp == 6) {increment = 5000; Serial.println("selected Step : 5KHz");}
    else if (stp == 7) {increment = 10000; Serial.println("selected Step : 10KHz");}
    else if (stp == 8) {increment = 100000; Serial.println("selected Step : 100KHz");}
    else if (stp == 9) {increment = 1000000; Serial.println("selected Step : 1MHz");}
    delay(500);
    stpOld = stp;
  }
}
//**********************************************************************
 int readJoystick()
 {
    adcX = analogRead(A0);
    adcXNew = treatValue(adcX);
    delay(100);
    adcY = analogRead(A1);
    adcYNew = treatValue(adcY);

    if (adcXNew == initialJoyPositionX & adcYNew == initialJoyPositionY) return btnNONE;
    if (adcXNew < initialJoyPositionX) return band = band - 1;
    if (adcXNew > initialJoyPositionX) return band = band + 1;
    if (adcYNew < initialJoyPositionY) return stp = stp + 1;
    if (adcYNew > initialJoyPositionY) return stp = stp - 1;

    return btnNONE;
 }



//**********************************************************************
//                           END OF PROGRAM
//**********************************************************************








