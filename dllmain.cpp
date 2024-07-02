// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "PCANBasic.h"
#include <vector>
#include <map>
#include <set>

//#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

#define MAX_DATA_LENGTH    8

const map<int32_t, TPCANBaudrate> baudRates = {
    { 1000000, PCAN_BAUD_1M },
    { 800000, PCAN_BAUD_800K },
    { 500000, PCAN_BAUD_500K },
    { 250000, PCAN_BAUD_250K },
    { 125000, PCAN_BAUD_125K },
    { 100000, PCAN_BAUD_100K },
    { 95000, PCAN_BAUD_95K },
    { 83000, PCAN_BAUD_83K },
    { 50000, PCAN_BAUD_50K },
    { 47000, PCAN_BAUD_47K },
    { 33000, PCAN_BAUD_33K },
    { 20000, PCAN_BAUD_20K },
    { 10000, PCAN_BAUD_10K },
    { 5000, PCAN_BAUD_5K }
};

const set<int> messageTypes = {
    PCAN_MESSAGE_STANDARD,
    PCAN_MESSAGE_RTR,
    PCAN_MESSAGE_EXTENDED,
    PCAN_MESSAGE_FD,
    PCAN_MESSAGE_BRS,
    PCAN_MESSAGE_ESI,
    PCAN_MESSAGE_ECHO,
    PCAN_MESSAGE_ERRFRAME,
    PCAN_MESSAGE_STATUS
};

const TPCANHandle pcanChannels[] = { PCAN_USBBUS1, PCAN_USBBUS2, PCAN_USBBUS3, PCAN_USBBUS4,
                                     PCAN_USBBUS5, PCAN_USBBUS6, PCAN_USBBUS7, PCAN_USBBUS8,
                                     PCAN_USBBUS9, PCAN_USBBUS10, PCAN_USBBUS11, PCAN_USBBUS12,
                                     PCAN_USBBUS13, PCAN_USBBUS14, PCAN_USBBUS15, PCAN_USBBUS16 };

const int numChannels = sizeof(pcanChannels) / sizeof(TPCANHandle);

vector<TPCANHandle> initializedChannels;
vector<uint8_t> availableChannels;


extern "C" {
    __declspec(dllexport) int32_t getNumCanChannels(void);
    __declspec(dllexport) uint8_t* getCanChannels(void);
    __declspec(dllexport) void canIdentify(uint8_t channel, bool enable);
    __declspec(dllexport) bool canInitialize(uint8_t channel, int32_t baudRate);
    __declspec(dllexport) void canDeInitialize(uint8_t channel);
    __declspec(dllexport) bool canWrite(uint8_t channel, uint8_t type, uint32_t can_id, uint8_t* data, uint8_t length);
    __declspec(dllexport) bool canRead(uint8_t channel, uint32_t* can_id, uint8_t* data, uint8_t* length);
}

__declspec(dllexport) int32_t getNumCanChannels(void)
{
    DWORD condition;
    availableChannels.clear();

    for (uint8_t i = 0; i < numChannels; i++) {
        if (CAN_GetValue(pcanChannels[i], PCAN_CHANNEL_CONDITION, &condition, sizeof(condition)) == PCAN_ERROR_OK) {
            if ((condition & PCAN_CHANNEL_AVAILABLE) == PCAN_CHANNEL_AVAILABLE) {
                availableChannels.push_back(i);
            }
        }
    }
    return availableChannels.size();
}

__declspec(dllexport) uint8_t* getCanChannels(void)
{
    return &(availableChannels[0]);
}

__declspec(dllexport) void canIdentify(uint8_t channel, bool enable)
{
    DWORD condition;
    UINT activate = enable ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;

    if (channel >= 0 and channel < numChannels) {
        if (CAN_GetValue(pcanChannels[channel], PCAN_CHANNEL_CONDITION, &condition, sizeof(condition)) == PCAN_ERROR_OK) {
            if ((condition & PCAN_CHANNEL_AVAILABLE) == PCAN_CHANNEL_AVAILABLE) {
                CAN_SetValue(pcanChannels[channel], PCAN_CHANNEL_IDENTIFYING, &activate, sizeof(activate));
            }
        }
    }
}

__declspec(dllexport) bool canInitialize(uint8_t channel, int32_t baudRate)
{
    TPCANStatus result = PCAN_ERROR_INITIALIZE;
    DWORD condition;

    auto it = baudRates.find(baudRate);

    if (it != baudRates.end()) {
        if (channel >= 0 and channel < numChannels) {
            if (CAN_GetValue(pcanChannels[channel], PCAN_CHANNEL_CONDITION, &condition, sizeof(condition)) == PCAN_ERROR_OK) {
                if ((condition & PCAN_CHANNEL_AVAILABLE) == PCAN_CHANNEL_AVAILABLE) {
                    // the second parameter in the iterator is the baud rate definition
                    result = CAN_Initialize(pcanChannels[channel], it->second);
                    if (result == PCAN_ERROR_OK) {
                        initializedChannels.push_back(pcanChannels[channel]);
                    }
                }
            }
        }
    }
    return (result == PCAN_ERROR_OK);
}

__declspec(dllexport) void canDeInitialize(uint8_t channel)
{
    if (channel < numChannels) {
        for (auto it = initializedChannels.begin(); it != initializedChannels.end(); ++it) {
            if (*it == pcanChannels[channel]) {
                CAN_Uninitialize(pcanChannels[channel]);
                initializedChannels.erase(it);
                break;
            }
        }
    }
}

__declspec(dllexport) bool canWrite(uint8_t channel, uint8_t type, uint32_t can_id, uint8_t* data, uint8_t length)
{
    TPCANStatus result = PCAN_ERROR_INITIALIZE;
    TPCANMsg msg;

    if ((messageTypes.find(type) != messageTypes.end()) && (length <= MAX_DATA_LENGTH)) {
        msg.ID = can_id;
        msg.LEN = length;
        msg.MSGTYPE = type;

        for (int i = 0; i < length; i++) {
            msg.DATA[i] = data[i];
        }
        result = CAN_Write(pcanChannels[channel], &msg);
    }
    return (result == PCAN_ERROR_OK);
}

__declspec(dllexport) bool canRead(uint8_t channel, uint32_t* can_id, uint8_t* data, uint8_t* length)
{
    TPCANStatus result = PCAN_ERROR_INITIALIZE;
    TPCANMsg received;

    if (CAN_Read(pcanChannels[channel], &received, NULL) != PCAN_ERROR_QRCVEMPTY) {
        if (received.LEN <= MAX_DATA_LENGTH) {
            *can_id = received.ID;
            *length = received.LEN;
            for (int i = 0; i < received.LEN; i++) {
                data[i] = received.DATA[i];
            }
            return true;
        }
    }
    return false;
}
