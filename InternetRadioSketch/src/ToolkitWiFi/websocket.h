//
// websocket.h
//

#include "ToolkitWiFi_Server.h"

#ifndef _LOCUS_WEBSOCKET_H_
#define _LOCUS_WEBSOCKET_H_

typedef struct {
    bool isFinalFragment;
    uint8_t opcode;
    uint32_t dataSize;
} websocket_frame_info;

//
// WS Handshake
const char *websocket_isWSHeader(char *request_buffer, int request_length);
    // the key (if found) is 24 bytes stored statically .. otherwise return is 0
const char *websocket_getClientKey();

char *websocket_handshake(const char *wsKey, uint32_t *reply_length);

//
// Toolkit - send to the browser
//void websocket_sendString(ToolkitWiFi_Client *twfc, const char *str);
void websocket_sendSettings(ToolkitWiFi_Client *twfc);

//
// Handle incoming packets
boolean websocket_handleIncoming(ToolkitWiFi_Client *twfc,
    const char *buffer, size_t size);

//
// Send a setting to all WS clients
void websocket_broadcast(const char *name, const char *value);

#endif

//
// END of websocket.h
