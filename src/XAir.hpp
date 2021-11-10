#ifndef __XAIR_HPP
#define __XAIR_HPP

#include "Udp.h"

enum class XAirType {
    XAIR_12,
    XAIR_16,
    XAIR_18,
    X32
};

enum class XAirTapeStatus {

};

class XAir {
public:
    XAir(XAirType type, UDP &udpInstance, IPAddress outIp);
    ~XAir();

    void begin();

    float getChFaderVal(int chNum);
    void setChFaderVal(int chNum, float faderVal);

    bool getMuteGroupMute(int muteGroupNum);
    void setMuteGroupMute(int muteGroupNum, bool mute);

    XAirTapeStatus getTapeStatus();
    void setTapeStatus(XAirTapeStatus status);

private:
    void sendMsg(const char* msg_str);
    template<typename T>
    void sendMsgWithParameter(const char* msg_str, T param);

    void receiveChName(void (*callback)(OSCMessage &msg));
    static void chNameNotify(OSCMessage &msg);

    void receiveMsg(const char *pattern, void (*callback)(OSCMessage &msg), int addr_offset);

    int mNumChannels;
    int mNumAuxOuts;

    char **mChannelNames;

    UDP &mUdpInstance;

    // remote ip address to send OSC packets to
    IPAddress mOutIp;
    // remote port to send OSC packets to (varies between xair models)
    unsigned int mOutPort;
    // local port to listen for OSC packets (actually not used for sending)
    const unsigned int mLocalPort = 8888;
};

#endif