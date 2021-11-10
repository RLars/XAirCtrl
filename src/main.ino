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
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>          // For WiFi

// Include the MIDI stuff
#include <OSCMessage.h>       // For OSC support

// SEE https://behringerwiki.musictribe.com/index.php?title=OSC_Remote_Protocol for OSC protocol
// AND https://wiki.munichmakerlab.de/images/1/17/UNOFFICIAL_X32_OSC_REMOTE_PROTOCOL_%281%29.pdf
// AND https://sourceforge.net/projects/sysexoscgen/

#include "wifi_passwd.h"

// Devmode (no mixer available)
#define DEV_MODE_NO_MIXER (0)

// TYPES
typedef enum {
  STATE_STARTUP,
  STATE_CONNECTING,
  STATE_SYNCING,
  STATE_RUNNING
} eSystemState;

// Constants
const int numChannels = 16;

// WiFi setup
char ssid[] = "XR16-5E-E5-AC";                     // your network SSID (name)
// Define as follows in wifi_passwd.h: char passwd[] = "0011223344";

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define CLK D5
#define DATA D6
#define ENC_ACC_THRESHOLD (3)
#define ENC_ACC_FACTOR    (2)

#define LED1 D0 // output only
#define LED2 D8
#define SW1 D7
#define SW2 D3

#define ENCODER_SW D4

// Variables

static uint8_t prevNextCode = 0;
static uint16_t store=0;

#define DISPLAY_CYCLE_MS (100)
unsigned long lastDisplayUpdateMs = 0;
int8_t encoderVal = 0;

#define INPUT_CYCLE_MS (100)
unsigned long lastUserInputUpdateMs = 0;

#define LED_CYCLE_MS (500)
unsigned long lastLedUpdateMs = 0;

#define REMOTE_STATUS_UPDATE_CYCLE_MS (500)
unsigned long lastRemoteStatusUpdateMs = 0;

float faderVal = 0.0f;
const float faderSteps = 0.1f;

int8_t read_rotary();
WiFiUDP Udp;                                // A UDP instance to let us send and receive packets over UDP
const IPAddress outIp(192,168,1,1);       // IP of the XR18 in Comma Separated Octets, NOT dots!
const unsigned int outPort = 10024;         // remote port to receive OSC X-AIR is 10024, X32 is 10023
const unsigned int localPort = 8888;        // local port to listen for OSC packets (actually not used for sending)
char chNames[numChannels][13];

bool isRecording = false;
unsigned long recStart = 0;
bool vocalsMuted = false;

eSystemState state = STATE_STARTUP;

// Prototypes
int extractChId(OSCMessage &msg);
void sendMsg(const char* msg_str);
template<typename T>
void sendMsgWithParameter(const char* msg_str, T param);
void receiveMsg();
void updateLeds();
static void displayState(eSystemState state);

/* Mixer */
static void updateRemoteStatus();
static void muteVocals(bool mute);
static void startRecording();
static void stopRecording();


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

  enterState(STATE_CONNECTING);
#if !(DEV_MODE_NO_MIXER)
  Serial.println();
  Serial.print("Connecting to ");     // to DEBUG screen
  Serial.println(ssid);               // to DEBUG screen
  WiFi.enableInsecureWEP(); 
  WiFi.begin(ssid, passwd);

  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");

  Serial.println("IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(localPort);
  Serial.println("");

  enterState(STATE_SYNCING);

  Serial.println("Reading channel names");
  char msgStr[20];
  for(int i=0; i<numChannels; i++) {
    sprintf(msgStr, "/ch/%02d/config/name", i+1);
    sendMsg(msgStr);
    delay(50);
    receiveMsg();
  }
#endif

  enterState(STATE_RUNNING);
}

void loop() {
  updateRemoteStatus();

  processEncoder();
  processUserInput();

  updateDisplay();

  updateLeds();

#if 0
  static long lastUpdate = 0;
 if(millis() - lastUpdate >=100) {
  Serial.printf("Encoder: %d %d\n",digitalRead(DATA), digitalRead(CLK)); 
  lastUpdate = millis();
 }
#endif
}

static void enterState(eSystemState newState) {
  displayState(newState);
  state = newState;
}

// ============ HMI =================

static void displayState(eSystemState state) {
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  switch (state)
  {
  case STATE_STARTUP:
    display.println("Starting...");
    break;
  case STATE_CONNECTING:
    display.println("Connecting...");
    break;
  case STATE_SYNCING:
    display.println("Syncing...");
    break;
  case STATE_RUNNING:
    display.println("Running...");
    break;
  
  default:
    break;
  }

  display.display();
}

void processEncoder() {
    encoderVal += read_rotary();
}

void processUserInput() {
  long millis_passed = millis() - lastUserInputUpdateMs;
  if(millis_passed >= INPUT_CYCLE_MS) {
    lastUserInputUpdateMs = millis();

    int sw1_val = digitalRead(SW1);
    int sw2_val = digitalRead(SW2);

    static bool init = true;
    static int last_sw1_val = 0;
    static int last_sw2_val = 0;
    if(!init && last_sw1_val != sw1_val && sw1_val) {
      if(isRecording) {
        stopRecording();
      } else {
        startRecording();
      }
    }

    if(!init && last_sw2_val != sw2_val && sw2_val) {
      muteVocals(!vocalsMuted);
    }

    last_sw1_val = sw1_val;
    last_sw2_val = sw2_val;

    
#if 0
    if(!digitalRead(ENCODER_SW)) {
      digitalWrite(LED1, 1);
      digitalWrite(LED2, 1);
      delay(100);
      digitalWrite(LED1, 0);
      digitalWrite(LED2, 0);
    }
#endif

  // ignore encoder for the moment
#if 0
    if(abs(encoderVal) > ENC_ACC_THRESHOLD) {
      encoderVal *= ENC_ACC_FACTOR;
    }

    if(0 != encoderVal) {
      Serial.println(encoderVal);
      faderVal += encoderVal * faderSteps;
    }
    
    encoderVal = 0;
#endif

    init = false;
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
    display.setCursor(0, 0);
    
    display.setTextColor(WHITE);
    display.print("<");

    if(isRecording) {
      display.setTextColor(BLACK, WHITE); // 'inverted' 
    } else {
      display.setTextColor(WHITE);
    }
    display.print("REC");

    display.setTextColor(WHITE);
    display.print("|");
    
    if(vocalsMuted) {
      display.setTextColor(BLACK, WHITE); // 'inverted' 
    } else {
      display.setTextColor(WHITE);
    }
    display.print("MUTE");

    display.setTextColor(WHITE);
    display.println(">");
    
    if(isRecording) {
      display.print("REC: ");
      printTime(millis() - recStart, 2);
      display.println("");
    }
#endif
    
    display.display();
  } 
}

void updateLeds() {
  long millis_passed = millis() - lastLedUpdateMs;
  if(millis_passed >= LED_CYCLE_MS) {
    lastLedUpdateMs = millis();
    if(isRecording) {
      digitalWrite(LED2, !digitalRead(LED2));
    } else {
      digitalWrite(LED2, 0);  
    }
    
    digitalWrite(LED1, vocalsMuted);
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

// =============== COM ======================
static void updateRemoteStatus()
{
  long millis_passed = millis() - lastRemoteStatusUpdateMs;
  if(millis_passed >= REMOTE_STATUS_UPDATE_CYCLE_MS) {
    lastRemoteStatusUpdateMs = millis();

#if !(DEV_MODE_NO_MIXER)
    getCommonData();

#if 0
    getChannelData(0);
#endif

    delay(50);
    receiveMsg();
#endif
  }
}

void getChannelData(int chId) {
  char msgStr[20];

  if(0 == chId) {
    sprintf(msgStr, "/lr/mix/on", chId);
  } else {
    sprintf(msgStr, "/ch/%02d/mix/on", chId);
  }
  sendMsg(msgStr);

  if(0 == chId) {
    sprintf(msgStr, "/lr/mix/fader", chId);
  } else {
    sprintf(msgStr, "/ch/%02d/mix/fader", chId);
  }
  sendMsg(msgStr);
}

void getCommonData() {
  sendMsg("/-stat/tape/state");
  sendMsg("/config/mute/1");
}

void sendMsg(const char* msg_str) {
  Serial.print("Sending msg: ");
  Serial.println(msg_str);
  
  OSCMessage msg(msg_str);
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
}

template<typename T>
void sendMsgWithParameter(const char* msg_str, T param) {
  Serial.print("Sending msg: ");
  Serial.print(msg_str);
  Serial.print(" (");
  Serial.print(param);
  Serial.println(")");
  
  OSCMessage msg(msg_str);
  msg.add(param);
  Udp.beginPacket(outIp, outPort);
  msg.send(Udp);
  Udp.endPacket();
}

static void startRecording() {
  Serial.println("startRecording");
#if !(DEV_MODE_NO_MIXER)
  sendMsgWithParameter("/-stat/tape/state", 4);
#else
  isRecording = true;
  recStart = millis();
#endif
}

static void stopRecording() {
  Serial.println("stopRecording");
#if !(DEV_MODE_NO_MIXER)
  sendMsgWithParameter("/-stat/tape/state", 0);
#else
  isRecording = false;
#endif
}

static void muteVocals(bool mute) {
  Serial.print("Set mute: ");
  Serial.println(mute);
#if !(DEV_MODE_NO_MIXER)
  sendMsgWithParameter("/config/mute/1", (int)mute);
#else
  vocalsMuted = mute;
#endif
}

void printMsgAddress(OSCMessage &msg) {
  char addr[50];
  msg.getAddress(addr);
  Serial.print(addr);
}

int extractChId(OSCMessage &msg) {
  char addr[50];
  msg.getAddress(addr);
  return atoi(&addr[4]);
}

void fader_notify(OSCMessage &msg) {
  Serial.print("Type: ");
  Serial.println(msg.getType(0));
  
  float fader_val = msg.getFloat(0);
  printMsgAddress(msg);
  Serial.print(": ");
  Serial.println(fader_val);
}

void mute_notify(OSCMessage &msg) {
  Serial.print("Type: ");
  Serial.println(msg.getType(0));
  
  int mute = msg.getInt(0);
  printMsgAddress(msg);
  Serial.print(": ");
  Serial.println(mute);
}

void mute_group_notify(OSCMessage &msg) {
  Serial.print("MuteGroup received. Type: ");
  Serial.println(msg.getType(0));
  
  int mute = msg.getInt(0);
  printMsgAddress(msg);
  Serial.print(": ");
  Serial.println(mute);

  vocalsMuted = mute;
}

void name_notify(OSCMessage &msg) {
  /*Serial.print("Type: ");
  Serial.println(msg.getType(0));*/

  int chId = extractChId(msg);
  
  msg.getString(0, chNames[chId - 1]);
  printMsgAddress(msg);
  Serial.print(": ");
  Serial.println(chNames[chId - 1]);
}

void rec_state_notify(OSCMessage &msg) {
  Serial.print("Type: ");
  Serial.println(msg.getType(0));
  
  int val = msg.getInt(0);
  printMsgAddress(msg);
  Serial.print(" (4==rec): ");
  Serial.println(val);

  bool wasRecording = isRecording;
  isRecording = (4 == val);

  if(!wasRecording && isRecording) {
    recStart = millis();
  }
}

void receiveMsg() {
 int size;

  // parse all available packets
  while (size = Udp.parsePacket()) {
    OSCMessage msg;
    while (size--) {
      msg.fill(Udp.read());
    }
    if (!msg.hasError()) {
      msg.dispatch("/mix/fader", fader_notify, 6);
      msg.dispatch("/mix/on", mute_notify, 6);
      msg.dispatch("/lr/mix/fader", fader_notify);
      msg.dispatch("/lr/mix/on", mute_notify);
      msg.dispatch("/config/name", name_notify, 6);
      msg.dispatch("/-stat/tape/state", rec_state_notify);
      msg.dispatch("/config/mute/1", mute_group_notify);
    } else {
      OSCErrorCode error = msg.getError();
      Serial.print("error: ");
      Serial.println(error);
    }
  }  
}