/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x32 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be
 included in any redistribution.
 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define CLK D5
#define DATA D6
#define ENC_ACC_THRESHOLD (3)
#define ENC_ACC_FACTOR    (2)

#define LED1 D8
#define LED2 D0 // output only
#define SW1 D3
#define SW2 D7

#define ENCODER_SW D4

static uint8_t prevNextCode = 0;
static uint16_t store=0;

#define DISPLAY_CYCLE_MS (100)
unsigned long lastDisplayUpdateMs = 0;
int8_t encoderVal = 0;

#define DISPLAY_CYCLE_MS (100)
unsigned long lastUserInputUpdateMs = 0;

float faderVal = 0.0f;
const float faderSteps = 0.5f;

// Prototypes
int8_t read_rotary();

void setup() {
  Serial.begin (115200);

  Serial.println("Setup display");
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  Serial.println("Setup encoder");
  pinMode(CLK, INPUT_PULLUP);
  pinMode(DATA, INPUT_PULLUP);

  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);

  pinMode(ENCODER_SW, INPUT_PULLUP);
}

void loop() {
 static int8_t val;

 if( val=read_rotary() ) {
    encoderVal += val;
 }

 processUserInput();

 updateDisplay();

 digitalWrite(LED1, digitalRead(SW1));
 digitalWrite(LED2, digitalRead(SW2));

 if(!digitalRead(ENCODER_SW)) {
  digitalWrite(LED1, 1);
  digitalWrite(LED2, 1);
  delay(100);
  digitalWrite(LED1, 0);
  digitalWrite(LED2, 0);
 }

#if 1
  static long lastUpdate = 0;
 if(millis() - lastUpdate >=100) {
  Serial.printf("Encoder: %d %d\n",digitalRead(DATA), digitalRead(CLK)); 
  lastUpdate = millis();
 }
#endif
}

void processUserInput() {
  long millis_passed = millis() - lastUserInputUpdateMs;
  if(millis_passed >= DISPLAY_CYCLE_MS) {
    lastUserInputUpdateMs = millis();

    if(abs(encoderVal) > ENC_ACC_THRESHOLD) {
      encoderVal *= ENC_ACC_FACTOR;
    }

    if(0 != encoderVal) {
      Serial.println(encoderVal);
      faderVal += encoderVal * faderSteps;
    }
    
    encoderVal = 0;
  }
}

void updateDisplay() {
  long millis_passed = millis() - lastDisplayUpdateMs;
  if(millis_passed >= DISPLAY_CYCLE_MS) {
    lastDisplayUpdateMs = millis();
    display.clearDisplay();

#if 0
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    
    display.setTextColor(BLACK, WHITE); // 'inverted' 
    display.print("CH1");
    display.setTextColor(WHITE); // 'inverted' 
    display.println(" Maerry");
    
    display.printf("LVL: %.1fdB\n", faderVal);
    
    display.print("REC: ");
    printTime(millis(), 3);
    display.println("");
#else
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    
    display.setTextColor(BLACK, WHITE); // 'inverted' 
    display.println("<REC|MUTE>");
    
    display.print("REC: ");
    printTime(millis(), 2);
    display.println("");
#endif
    
    display.display();
  } 
}

void printTime(long timestamp_ms, int positions) {
  long seconds_ts;
  long minutes_ts;
  long hours_ts;
  
  long seconds = timestamp_ms / 1000;
  hours_ts = seconds / 3600;
  seconds -= hours_ts * 3600;
  minutes_ts = seconds / 60;
  seconds -= minutes_ts * 60;
  seconds_ts = seconds;

  if(positions < 3) {
    if(hours_ts < 1) {
      display.printf("%02d:%02d", minutes_ts, seconds_ts);
    } else {
      display.printf("%02d:%02d", hours_ts, minutes_ts);
    }
  } else {
    display.printf("%02d:%02d:%02d", hours_ts, minutes_ts, seconds_ts);
  }
}


// A vald CW or  CCW move returns 1, invalid returns 0.
int8_t read_rotary() {
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};

  prevNextCode <<= 2;
  if (digitalRead(DATA)) prevNextCode |= 0x02;
  if (digitalRead(CLK)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

   // If valid then store as 16 bit data.
   if  (rot_enc_table[prevNextCode] ) {
      store <<= 4;
      store |= prevNextCode;
      //if (store==0xd42b) return 1;
      //if (store==0xe817) return -1;
      if ((store&0xff)==0x2b) return -1;
      if ((store&0xff)==0x17) return 1;
   }
   return 0;
}
