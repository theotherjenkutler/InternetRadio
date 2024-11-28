/*!
 * @file ToolkitVLSI.h
 */

#ifndef ToolkitVLSI_H
#define ToolkitVLSI_H

#include <Arduino.h>
#include "ToolkitSPI.h"

//
// What do we want to do in here?

// (1) setup the hardware config on the SPI bus with the three control lines
// (2) once we have setup the SPI for all devices sharing the bus then
//      we can handshake with the specific VS1053/VS1063 devices to make
//      sure they are both talking to us
// (3) if we are in playback mode, then connect to stream and play it
// (4) if we are in streaming mode, then load the mp3 plugin into the VS1063
//      connect to icecast server, and stream the incoming data

class ToolkitVLSI
{
  public:
    ToolkitVLSI(int8_t cs, int8_t dcs, int8_t dreq);  // store pin configurarion
    ~ToolkitVLSI();
    void begin();            // configure and open the SPI port
    uint8_t getStatus();     // get the status code from the VLSI device
    void playData(uint8_t *buffer, uint8_t buffersize); // send data bytes
    boolean readyForData();  // true when dreq is high
    void reset();            // soft reset over SPI
    void setPlaybackVolume(float level); // 0.0 to 1.0
    void setVolume(uint8_t left, uint8_t right); // 0-255,0-255
    uint16_t sciRead(uint8_t addr);
    void sciWrite(uint8_t addr, uint16_t data);

    // TODO: ADD FUNCTION TO LOAD IN A PATCH *****

  protected:
    uint8_t _dreq; // Data request pin
    int8_t _cs, _dcs;
//    Adafruit_SPIDevice *spi_dev_ctrl; // Pointer to SPI dev for control
//    Adafruit_SPIDevice *spi_dev_data; // Pointer to SPI dev for data
    ToolkitSPI *spi_dev_ctrl; // Pointer to SPI dev for control
    ToolkitSPI *spi_dev_data; // Pointer to SPI dev for data
}; // end of class ToolkitVLSI

class ToolkitVS1063 : public ToolkitVLSI
{
  public:
    ToolkitVS1063(int8_t cs, int8_t dcs, int8_t dreq);

    void loadPatches();

    uint16_t getClock();
    uint16_t setClock(float multiplier);

    void setSPISpeed(uint32_t freq);

    uint16_t enableVUMeter(bool on_not_off); // WRAMADDR 0x1e09 bit 2 meter enable
    uint16_t readVUMeter(); //WRAMADDR 0x1w0c 8 bits left 8 bits right, 0-32 in 3dB steps

    void encoder_start(); // configure the VS10xx /w CBR and start encoding
    void encoder_stop();
    uint16_t encoder_available();   // number of byte available
    uint16_t encoder_getData(uint8_t *where, uint16_t where_size); // even aligned
    void encoder_updateVolume(); // send volume changes to the VS1063a

  // ENCODER: set parameters
    enum {
        BITRATE_32K     = 32,
        BITRATE_64K     = 64,
        BITRATE_96K     = 96,
        BITRATE_128K    = 128,
        BITRATE_192K    = 192
    };

    enum {
        JOINT_STEREO    = 0,    // joint AGC
        DUAL_STEREO     = 1,    // separate AGC
        LEFT_MONO       = 2,
        RIGHT_MONO      = 3,
        MIXED_MONO      = 4
    };

    enum {
        SAMPLE_32K      = 32000,
        SAMPLE_44K1     = 44100,
        SAMPLE_48K      = 48000
    };

    void encoder_setAGC(float maximum_gain);     // hardware max is 64.0
    void encoder_setBitrate(uint16_t kbps_enum); // use the BITRATE_xxK enums
    void encoder_setChannels(uint16_t channel_enum);  // JOINT_STEREO .. etc
    void encoder_setManualGain(float gain);      // 0.001 to 64.0
    void encoder_setMicNotLine(boolean mic_not_line); // mic is mono
    void encoder_setSamplerate(uint16_t rate_enum);   // use SAMPLE_ enums

  private:
      float _agc_max_gain, _gain;
      uint16_t _channel, _kbps, _sample_rate;
      boolean _mic_not_line;
};

#endif

//
// END OF ToolkitVLSI.h
