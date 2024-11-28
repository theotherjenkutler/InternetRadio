//
// Encoder.h

#ifndef _ENCODER_H_
#define _ENCODER_H_

#include <WiFi.h>
#include "src/ToolkitVLSI/ToolkitVLSI.h"

class Streamer : public ToolkitVS1063
{
    public:
        Streamer();
        ~Streamer();

        // read everything from settings and configure the encoder.
        // ALSO reads the "startup_auto_mode" to determine
        // listening, transmitting, or waiting state.
        void encoder_setup();
        void encoder_updateVolumes();

        // connect the listener to a remote stream and
        // clear (read) the stream headers. stream is
        // then ready to be passed to the mp3 decoder.
        boolean start_listener();

        void update_playbackVolume();
        void update_recordVolume();

        boolean run_not_wait;
        boolean listen_dont_encode;

    public:
        WiFiClient listener;

    public: // buffer stream stuff
        enum {
            BUFFER_SIZE         = 4096,
            BUFFERS_PER_SECOND  = 4,        // at 128kbps
            BUFFER_SECONDS      = 3,
            NUMBER_OF_BUFFERS   = 12        // BUFFERS_PER_SECOND * BUFFER_SECONDS
        };

        static SemaphoreHandle_t buffer_mutex;

        static uint8_t buffer[NUMBER_OF_BUFFERS][BUFFER_SIZE];

        static size_t buffer_in_index;
        static size_t buffer_in_address;
        static size_t buffer_out_index;

        static boolean initBuffers();
        static boolean mutexExists();

        static uint8_t *getNextInBuffer(size_t *remaining);
            // buffer to stream encoded audio into
        static void advanceInBuffer(size_t bytesused);
            // advances the buffer when the current buffer is full

        static uint8_t *getNextOutBuffer(size_t *bytestostream);
            // pull out the next buffer that is ready to stream
            // returns NULL if there is nothing ready to go
            // advances to the next output buffer automatically
            // SET this as ToolkitWiFi::setMP3DataStreamFunction(..)
};

#endif

//
// END OF Encoder.h
