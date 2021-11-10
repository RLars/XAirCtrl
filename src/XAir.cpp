#include "XAir.hpp"

#include <OSCMessage.h>

// ===== Defines =====
#define MAX_CH_NAME_LEN (12)

// ===== Types =====
struct XAirVariant {
    unsigned int numChannels;
    unsigned int numAuxOuts; 
    unsigned int udpOutPort;
};

// ===== Constants =====
static const XAirVariant variants[] = {
    { // XAIR 12
        .numChannels = 12,
        .numAuxOuts = 2,
        .udpOutPort = 10024,
    },
    { // XAIR 16
        .numChannels = 16,
        .numAuxOuts = 4,
        .udpOutPort = 10024,
    },
    { // XAIR 18
        .numChannels = 18,
        .numAuxOuts = 6,
        .udpOutPort = 10024,
    },
    { // X32
        .numChannels = 32,
        .numAuxOuts = 14,
        .udpOutPort = 10023
    },
};

// ===== Prototypes =====
int extractChId(OSCMessage &msg);

// ===== Method implementation =====
XAir::XAir(XAirType type, UDP &udpInstance, IPAddress outIp) : mUdpInstance(udpInstance) {
    XAirVariant variant = variants[(int)type];

    mNumChannels = variant.numChannels;
    mNumAuxOuts = variant.numAuxOuts;

    mOutPort = variant.udpOutPort;
    mOutIp = outIp;

    mChannelNames = new char*[variant.numChannels];
    for (int i = 0; i < variant.numChannels; i++) {
        mChannelNames[i] = new char[MAX_CH_NAME_LEN];
    }
}

XAir::~XAir() {
    
}

void XAir::begin() {
    mUdpInstance.begin(mLocalPort);

    // Read static data
    char msgStr[20];
    for(int i=0; i<mNumChannels; i++) {
        sprintf(msgStr, "/ch/%02d/config/name", i+1);
        sendMsg(msgStr);
        delay(50);
        receiveChName();
    }
}

float XAir::getChFaderVal(int chNum) {
    float retVal = 0.0f;
    
    if(chNum > 0 && chNum <= mNumChannels) {
        char msgStr[20];
        sprintf(msgStr, "/ch/%02d/mix/fader", chNum);
        sendMsg(msgStr);
        retVal = receiveChFaderVal();
    }

    return retVal;
}

void XAir::setChFaderVal(int chNum, float faderVal) {

}

void XAir::sendMsg(const char* msg_str) {
  Serial.print("Sending msg: ");
  Serial.println(msg_str);
  
  OSCMessage msg(msg_str);
  mUdpInstance.beginPacket(mOutIp, mOutPort);
  msg.send(mUdpInstance);
  mUdpInstance.endPacket();
}

template<typename T>
void XAir::sendMsgWithParameter(const char* msg_str, T param) {
  Serial.print("Sending msg: ");
  Serial.print(msg_str);
  Serial.print(" (");
  Serial.print(param);
  Serial.println(")");
  
  OSCMessage msg(msg_str);
  msg.add(param);
  mUdpInstance.beginPacket(mOutIp, mOutPort);
  msg.send(mUdpInstance);
  mUdpInstance.endPacket();
}

float receiveChFaderVal() {
    receiveMsg("/mix/fader", fader_notify, 6);
}

void XAir::chNameNotify(OSCMessage &msg) {
  /*Serial.print("Type: ");
  Serial.println(msg.getType(0));*/

  int chId = extractChId(msg);
  
  msg.getString(0, mChannelNames[chId - 1]);
  printMsgAddress(msg);
  Serial.print(": ");
  Serial.println(chNames[chId - 1]);
}

void XAir::receiveChName() {
    receiveMsg("/config/name", chNameNotify, 6);
}

void XAir::receiveMsg(const char *pattern, void (*callback)(OSCMessage &msg), int addr_offset) {
 int size;

  // parse all available packets
  while (size = mUdpInstance.parsePacket()) {
    OSCMessage msg;
    while (size--) {
      msg.fill(mUdpInstance.read());
    }
    if (!msg.hasError()) {
        bool msgProcessed = msg.dispatch(pattern, callback, addr_offset);

        if(!msgProcessed) {
            // TODO: Process generic messages?
        }
    } else {
      OSCErrorCode error = msg.getError();
      Serial.print("error: ");
      Serial.println(error);
    }
  }  
}

int extractChId(OSCMessage &msg) {
  char addr[50];
  msg.getAddress(addr);
  return atoi(&addr[4]); // Start at offset 4 => "/ch/XX/...."
}