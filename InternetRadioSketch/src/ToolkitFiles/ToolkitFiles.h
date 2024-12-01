//
// ToolkitFiles.h

#include <FS.h>
//#include <SPIFFS.h>

#ifndef ToolkitFiles_H
#define ToolkitFiles_H

#include "ToolkitSettings.h"

// read files from SPIFFS - for the http server and settings
// write files - for settings

// OKAY .. we need to ..

// (1) SPIFFS.begin(false) .. don't format the thing !!
// (2) load our settings
// (3) save our settings (in case they have been changed remotely)
// (4) fetch files for the http server

// if we use malloc() we need to check for NULL returns (i.e. failure)
// if we use free() we must pass it a valid (non-zero) block
// BUT most people say to use a pre-alloced fixed buffer so that
// you know it will always be there.

// AT this point we have 280kB of RAM to play with, so we can
// pre-allocate 2 or 3 8kB chunks for file and header stuff.

class ToolkitFiles
{
    public:
        static boolean begin();

        static boolean fileExists(const char *path);

        static char *fileReadAll(const char *path, char *buffer,
            size_t size, size_t *actual);
        static void fileReadToSerial(const char *path);
        static boolean fileWrite(const char *path, const char *buffer,
            size_t size, boolean append=false);

        static File fileOpen(const char *path, const char *mode);

        // These functions read the entire file into big_buffer[]
        // Larger files need to be streamed in chunks.
        static boolean loadSettings();
        static void saveSettings();

        // loading and saving settings uses a static buffer
        enum {
            MAX_SETTINGS_SIZE = 1024*2
        };
};

#endif

//
// END OF ToolkitFiles.h
