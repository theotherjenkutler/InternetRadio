//
// websocket.cpp - for locus 2024
//

#include "websocket.h"
#include "sha1.h"

#include "../ToolkitFiles/ToolkitFiles.h"

//------------------------------------------------------------------------
//
// HANDSHAKE - opening a websocket connection with a browser
//

enum {
    WS_KEY_LEN = 24,
    WS_MS_LEN = 36,
    WS_REPLY_KEY_LEN = 30,
    WS_HS_ACCEPTLEN = 130
};

#define MAGIC_STRING   "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

#define WS_HS_ACCEPT                       \
    "HTTP/1.1 101 Switching Protocols\r\n" \
    "Upgrade: websocket\r\n"               \
    "Connection: Upgrade\r\n"              \
    "Sec-WebSocket-Accept: "


static char ws_key_from_client[WS_KEY_LEN];

const char *websocket_isWSHeader(char *request_buffer, int request_length)
{
    char *header = strstr(request_buffer, "Sec-WebSocket-Key");
    if (header) {
        header += 19;   // length of search string + 2 for the : and space
        int remaining = request_length - (header - request_buffer);
        if (remaining < WS_KEY_LEN) {
            return 0;   // some kind of incomplete header packet error
        }
        memcpy(ws_key_from_client, header, WS_KEY_LEN);
        return ws_key_from_client;
    }
    return 0;
}
// the key (if found) is 24 bytes stored statically .. otherwise return is 0

const char *websocket_getClientKey()
{
    return ws_key_from_client;
}

static const unsigned char base64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode20to30(uint8_t *dst, uint8_t *src)
{
	unsigned char *out, *pos;
	const unsigned char *end, *in;
    size_t len = SHA1HashSize;
//	size_t olen = WS_REPLY_KEY_LENGTH;
//	int line_len;

	out = dst;

	end = src + len;
	in = src;
	pos = out;
//	line_len = 0;
	while (end - in >= 3) {
		*pos++ = base64_table[in[0] >> 2];
		*pos++ = base64_table[((in[0] & 0x03) << 4) | (in[1] >> 4)];
		*pos++ = base64_table[((in[1] & 0x0f) << 2) | (in[2] >> 6)];
		*pos++ = base64_table[in[2] & 0x3f];
		in += 3;
//		line_len += 4;
//		if (line_len >= 72) {
//			*pos++ = '\n';
//			line_len = 0;
//		}
	}

	if (end - in) {
		*pos++ = base64_table[in[0] >> 2];
		if (end - in == 1) {
			*pos++ = base64_table[(in[0] & 0x03) << 4];
			*pos++ = '=';
		} else {
			*pos++ = base64_table[((in[0] & 0x03) << 4) |
					      (in[1] >> 4)];
			*pos++ = base64_table[(in[1] & 0x0f) << 2];
		}
		*pos++ = '=';
//		line_len += 4;
	}

//	if (line_len)
	*pos = '\0';

//	*pos = '\0';
//	if (out_len)
//		*out_len = pos - out;
//    return out;
}

static const char *websocket_replyKey(const char *wsKey)
{
    static uint8_t reply_key[WS_REPLY_KEY_LEN];
    uint8_t hash[SHA1HashSize];
    SHA1Context ctx;
    char *str = (char *) calloc(WS_KEY_LEN + WS_MS_LEN + 1, 1);

    strncpy(str, wsKey, WS_KEY_LEN);
    strcat(str, MAGIC_STRING);

    SHA1Reset(&ctx);
    SHA1Input(&ctx, (const uint8_t *)str, WS_KEY_LEN + WS_MS_LEN);
    SHA1Result(&ctx, hash);

    base64_encode20to30(reply_key, hash);
    free(str);
//    printf("reply: %.30s", reply_key);
    return (const char *) reply_key;
}
// the reply key is 28 bytes stored statically

char *websocket_handshake(const char *wsKey, uint32_t *reply_length)
{
    char *hsresponse = (char *) calloc(WS_HS_ACCEPTLEN, 1);
	strcpy(hsresponse, WS_HS_ACCEPT);
	strcat(hsresponse, websocket_replyKey(wsKey));
	strcat(hsresponse, "\r\n\r\n");
    *reply_length = strlen(hsresponse);
    return hsresponse;
}

//------------------------------------------------------------------------
//
// RECEIVE messages
//

static void mask(uint8_t *data, uint8_t *masks, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        data[i] = data[i] ^ masks[i%4];
//        printf("%02x ", data[i]);
    }
//    printf("\n");
}

// returns total length of frame (headers + data) or 0 for error
static uint32_t websocket_parse_frame(websocket_frame_info *wfi,
    const char *buffer, uint32_t buffer_length)
{
    uint8_t *b = (uint8_t *) buffer;
// printf("websocket incoming buffer length = %d\n", buffer_length);
    if (buffer_length < 6) {
//        printf("websocket frame .. too short!\n");
        return 0;
    }
    uint32_t headerSize = 2;
    wfi->isFinalFragment = b[0] & 0x80;
    wfi->opcode = b[0] & 0x0f;
// printf("isFinal %d, opcode %d\n", wfi->isFinalFragment, wfi->opcode);
    // opcode 0x0 continuation, 0x1 text, 0x2 binary, 0x8 close

    if (0x80 != (b[1] & 0x80)) {
        printf("websocket frame .. no mask bit!\n");
        return 0;
    }
    wfi->dataSize = b[1] & 0x7f;
// printf("datasize 7 bits %u\n", wfi->dataSize);
    if (126 == wfi->dataSize) {
// printf("datasize 16 %u %u\n", b[2], b[3]);
        headerSize += 2;
        wfi->dataSize = b[2];
        wfi->dataSize <<= 8;
        wfi->dataSize += b[3];
    } else if (127 == wfi->dataSize) {
        printf("websocket frame .. payload is too big!");
        return 0;
    }
// printf("data size = %u\n", wfi->dataSize);

    if (buffer_length < (wfi->dataSize + 4 + headerSize)) {
        printf("websocket frame .. to short after parsing payload length!\n");
        return 0;
    }

    uint8_t *masks = &b[headerSize];
    uint8_t *data = &b[headerSize + 4];
    mask(data, masks, wfi->dataSize);
// printf("\n");
    return headerSize + 4 + wfi->dataSize;
}

//------------------------------------------------------------------------
//
// SEND messages
//

// We actually want to send all the settings at once
// and then maybe some control messages once in a while
// So we need to be able to handle payloads up to 1200 bytes
//
#define PACKET_MAX_LENGTH 2048

static char packet[PACKET_MAX_LENGTH];

static void websocket_sendString(ToolkitWiFi_Client *twfc, const char *str)
{
    size_t payload_len = strlen(str);
    if (payload_len > (PACKET_MAX_LENGTH-4)) {
        payload_len = PACKET_MAX_LENGTH-4; }
    size_t header_len = 2;
    packet[0] = 0x81;   // FIN | TEXT
    if (payload_len <= 125) {
        packet[1] = payload_len;
    } else {
        packet[1] = 126; // magic number for a 16 bit payload length
        size_t msB = payload_len >> 8;
        size_t lsB = payload_len & 0xff;
        packet[2] = (char) msB;
        packet[3] = (char) lsB;
        header_len = 4;
    }

    memcpy(&packet[header_len], str, payload_len);
    size_t packet_len = header_len + payload_len;
    twfc->client->write(packet, packet_len);
}

void websocket_sendSettings(ToolkitWiFi_Client *twfc)
{
    size_t payload_len = SettingItem::saveAll(&packet[4], PACKET_MAX_LENGTH-4);
    packet[0] = 0x81;
    packet[1] = 126; // 16 bit payload length
    size_t msB = payload_len >> 8;
    size_t lsB = payload_len & 0xff;
    packet[2] = (char) msB;
    packet[3] = (char) lsB;
    twfc->client->write(packet, 4 + payload_len);
}

//------------------------------------------------------------------------
//
// HANDLE incoming packets
//

static void websocket_echoToOthers(ToolkitWiFi_Client *twfc,
    const char *name, const char *value)
{
    for (uint16_t i = 0; i < ToolkitWiFi_Client::MAX_CLIENTS; i++) {
        ToolkitWiFi_Client *t = &ToolkitWiFi_Client::_client_list[i];
        if (t != twfc) {
            if (NULL != t->client) {
                if (!t->client->connected()) {
                    t->closeClient();
                    //    Serial.println("Client closed itself");
                } else if (ToolkitWiFi_Client::TYPE_WEBSOCKET==t->type) {
                    static char s[64];
                    sprintf(s,"%s %s\n", name, value);
                    websocket_sendString(t, s);
                }
            }
        }
    } // end of for()
}

boolean websocket_handleIncoming(
    ToolkitWiFi_Client *twfc,
    const char *buffer, size_t size)
{
    static websocket_frame_info wfi;
    int remaining = size;

    uint32_t framesize = websocket_parse_frame(&wfi, buffer, remaining);
    while ((framesize) && (remaining > 0)) {
        if (8 == wfi.opcode) {
            framesize = 0;
            remaining = 0;
            // Serial.println("WebSocket has closed");
            return false;
        } else {
            if (1 == wfi.opcode) { // text
                if (framesize <= 126) {
                    const char *msg = &buffer[2+4]; // masks are 4 bytes
                    size_t msg_length = wfi.dataSize;
                    char *m = (char *) msg;
                    m[msg_length-1] = 0;
                    // Serial.println(m);

                    const char *name = NULL;
                    const char *value = NULL;
                    SettingItem::parseSetting(m, &name, &value);
                    if (name) {
                        if ('$' == name[0]) { // command
                            //Serial.println(name);
                            if (0==strcmp("$RESET", name)) {
                                ESP.restart();
                            }
                        } else { // setting
                            SettingItem::updateOrAdd(name, value);
                            ToolkitFiles::saveSettings();
                            websocket_echoToOthers(twfc, name, value);
                            ToolkitWiFi_Server::handleWSLiveChanges(name, value);
                        }
                    }
                }
            }
            buffer = buffer + framesize;
            remaining = remaining - framesize;
            framesize = websocket_parse_frame(&wfi, buffer, remaining);
        }
    } // end while()
    return true;
}

//
// END OF websocket.cpp
