//
// ToolkitWiFi_Client.cpp

#include "ToolkitWiFi_Client.h"
//
// static variables

uint32_t ToolkitWiFi_Client::_num_clients = 0;
ToolkitWiFi_Client ToolkitWiFi_Client::_client_list[MAX_CLIENTS];

//
// object methods

ToolkitWiFi_Client::ToolkitWiFi_Client()
{
    client = NULL;
    type = 0;
    millis_last_used = 0;
    closed = NULL;
}

ToolkitWiFi_Client::~ToolkitWiFi_Client()
{
    closeClient();
}

void ToolkitWiFi_Client::closeClient()
{
    if (client) {
        client->stop();
        delete(client);
        client = NULL;
    }
    type = ToolkitWiFi_Client::TYPE_UNUSED;
    millis_last_used = 0;
    if (closed) {
        closed();
    }
    closed = NULL;
    _num_clients--;
    //Serial.printf("Num clients (closing) = %u\n", _num_clients);
}

boolean ToolkitWiFi_Client::didClientTimeout()
{
    if (0!=millis_last_used) {
        uint32_t alive = millis() - millis_last_used;
        if (alive > ToolkitWiFi_Client::CLOSE_TIMEOUT) {
            return true;
        }
    }
    return false;
}

void ToolkitWiFi_Client::setClientTimedClose()
{
    millis_last_used = millis();  // milliseconds since Arduino started
    type = ToolkitWiFi_Client::TYPE_UNKNOWN;
}

//
// static methods

ToolkitWiFi_Client *ToolkitWiFi_Client::getAnEmptyClient()
{
    if (_num_clients < MAX_CLIENTS) {
        for (uint32_t i = 0; i < MAX_CLIENTS; i++) {
            if (ToolkitWiFi_Client::TYPE_UNUSED == _client_list[i].type) {
                _num_clients++;
                //Serial.printf("Num clients = %u\n", _num_clients);
                _client_list[i].type = ToolkitWiFi_Client::TYPE_UNKNOWN;
                return &_client_list[i];
            }
        }
    }
    return NULL;
}

void ToolkitWiFi_Client::acceptNewClient(WiFiClient client)
{ // we do it this way because accept() is passing references not pointers
    if (client) {
        ToolkitWiFi_Client *twfc = getAnEmptyClient();
        if (twfc) { // copied twfc->client has global scope
            twfc->client = new WiFiClient(client);  // clone it
        } else {
            Serial.println("Accept .. server is full.");
        }
    }
}

void ToolkitWiFi_Client::checkClientList(void(*func)(ToolkitWiFi_Client*))
{
    for (uint32_t i = 0; i < MAX_CLIENTS; i++) {
        ToolkitWiFi_Client *twfc = &_client_list[i];
        if (NULL != twfc->client) {
            if (!twfc->client->connected()) {
                twfc->closeClient();
                //Serial.println("Client closed itself");
            } else if (twfc->didClientTimeout()) {
                twfc->closeClient();
                //Serial.println("Client timeout close!");
            } else {
                func(twfc);
            } // end of connected()
        } // end of if(client)
    } // end of for()
}

//
// END OF ToolkitWiFi_Client.cpp
