#include <Arduino.h>
#include "ur4qbp_si5351.h"
#include <LiquidCrystal.h>



String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
boolean dataReady = false;  // whether the string is complete
unsigned long f1 = 1000000;
unsigned long f2 = 30000000;
unsigned int N = 32;
char data[256];
char testdata[256];

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

unsigned long time;


void setup()
{
Serial.begin(19200, SERIAL_8N1);
// reserve 200 bytes for the inputString:
inputString.reserve(200);
si5351_Init();

si5351aSetFrequencyA(0);//CLK0 10MHz
si5351aSetFrequencyB(0); //CLK1 10MHz
si5351aSetFrequencyC(0);//clk2_freq 10MHz

}


void loop()
{

  String buf ="";
  // print the string when a newline arrives:
  if (stringComplete) {

    if (inputString.substring(0, 2) == "f1") {setF1(inputString);}//setF1(inputString);}
    if (inputString.substring(0, 2) == "f2") {setF2(inputString);}
    if (inputString.substring(0, 1) == "n") {setN(inputString);}
    // clear the string:
    inputString = "";
    stringComplete = false;
  }

//dataReady = true;//Debug
  if (dataReady)
  {

    for (byte i = 0; i < N; i++)
    {
      Serial.write(data[i]);
    }
Serial.flush();
    dataReady = false;
  }
  testData();
  //realData();

delay(40);
//lcd.setCursor(0,0);
//lcd.print(time);
}




void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n' ) {

      //Debug

      lcd.setCursor(0, 0);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print("                ");
      /*

      lcd.print(inputString);
      */
      //lcd.print(inputString);
      stringComplete = true;

    }
  }
}



void realData()
{
  unsigned long buf = 0;
  unsigned long f = f1;
  unsigned long step = (f2-f1) / N;

  for (byte i = 0; i < N; i++)
  {
  si5351aSetFrequencyB(f);
    for(byte i=0; i<=3; i++)
    {
    buf += analogRead(3);
  }
    data[i] =  (buf >> 4)-128;
    buf = 0;
    f += step;
  }
  dataReady = true;
}


void testData()
{
for (byte i = 0; i < N; i++)
{
  if ( i == 0) {data[i] = 80*log10(1023)-128;}
  if (i > 0 && i < N/2) {data[i] = round(80*log10(random(1,3)))-128;}
  if ((i >= N/2) && (i <= (N/2+N/8))) {data[i] = round(80*log10(random(900,1023)))-128;}
  if (i > (N/2+N/8) && i < N-1) {data[i] = round(80*log10(random(1,3)))-128;}
  if (i == N-1) {data[i] = 80*log10(1023)-128;}
}
dataReady = true;
}

void setF1(String input)
{
unsigned int len = input.length();
input.remove(len-1);
input.remove(0,2);
f1 = input.toInt() * 1000;
if (f1 < 9000) f1 = 9000;
lcd.setCursor(0, 0) ;lcd.print(f1);inputString = "";
}


void setF2(String input)
{
unsigned int len = input.length();
input.remove(len-1);
input.remove(0,2);
f2 = input.toInt() *1000;
if (f2 > 220000000) f2 = 220000000;
lcd.setCursor(0, 0) ;lcd.print(f2);inputString = "";
}

void setN(String input)
{
unsigned int len = input.length();
input.remove(len-1);
input.remove(0,1);
N = input.toInt();
lcd.setCursor(0,0) ;lcd.print(N);inputString = "";
}
