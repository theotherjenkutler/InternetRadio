//
// ToolkitSettings.h

#ifndef ToolkitSettings_H
#define ToolkitSettings_H

#include <Arduino.h>

//
// A linked list of settings that can be saved to file
// and parsed. Each setting is stored as:
// <name> = <value>\n
//

class SettingItem
{
    public:
        // (1) load settings from a buffer and create the object list
        static void loadSettingsFromBuffer(const char *buffer,
            size_t buffer_length);
            // make sure your buffer has a delimeter or NULL char at the end

        // (2) save everything in the object list to a text buffer
        static size_t saveAll(char *buffer, size_t buffer_length);
            // returns the actual bytes used

        // (3) destroy the entire object list, free all the memory
        static void destroyAll();

        // (4) fetch your settings by key name
        static char *findString(const char *name);
        static uint16_t findUInt(const char *name, uint16_t default_value);
        static float findFloat(const char *name, float default_value);
            // UInt will convert the string to an int
            // Float will convert the string to a float

        static const char *parseSetting(const char *text,
            const char **outName, const char **outValue);

        static void updateOrAdd(const char *name, const char *inValue);

        static void printSettingsToSerial();

    private:
        SettingItem(const char *name, const char *inValue);
        ~SettingItem();

    private:
        void add(const char *inName);
        void remove();

        size_t save(char *buffer, size_t buffer_length);
            // returns actual size used
            // save a single setting to the buffer

    private:
        static SettingItem *find(const char *name);

        static const char *makeSettingFromParseableText(
            const char *text, const char *end);
            // creates a new SettingItem and links it into the list
            // returns a pointer to the end of the parsed text
            // text must end with a delimeter or NULL character
            // *end is the character after the last character in *text

        enum {
            ITEM_MAX_NAME_STRING = 32,
            ITEM_MAX_VALUE_STRING = 32
        };

        static SettingItem *first, *last;
        SettingItem *next, *previous;  // linked list
        char name[ITEM_MAX_NAME_STRING];
        char value[ITEM_MAX_VALUE_STRING];  // string
};

//
// Contents of setting.txt ..
//
// wifi_router_SSID         string
// wifi_router_password     string
// wifi_toolkit_hostname    string
// wifi_toolkit_AP_SSID     string
//
// Listening:
//      listen_icecast_url              string
//      listen_icecasst_port            int
//      listen_icecast_mountpoint       string
//      listen_volume                   float
//
// Streaming:
//      remote_icecast_url              string
//      remote_icecasst_port            int
//      remote_icecast_mountpoint       string
//      remote_icecast_password         string
//
// Encoder:
//      mic_not_line        int mic_not_line
//      channels            int from enums list
//      bitrate             int from enums list
//      sample_rate         int from enums list
//      agc_not_manual      int agc_not_manual
//      manual_gain_level   float
//      agc_maximum_gain    float
//
// Auto:
//      startup_auto_mode   string "listener" | "transmitter" | "waiting"
//          // wait -> wait for instructions from afar
//

#endif

//
// END OF ToolkitSettings.h
