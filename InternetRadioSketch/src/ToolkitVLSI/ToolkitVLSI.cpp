/*!
 * @file ToolkitVLSI.cpp
*/

#include "../../config.h"
#include "ToolkitVLSI.h"

//
// VLSI VS10xx

#define VS10xx_SCI_READ         0x03 // SCI direction bits for read from VS10xx
#define VS10xx_SCI_WRITE        0x02 // SCI direction bits for write to VS10xx

#define VS10xx_SCI_MODE         0x00 // RW Mode control
#define VS10xx_SCI_STATUS       0x01 // RW Status
#define VS10xx_SCI_BASS         0x02 // RW Built-in bass/treble control
#define VS10xx_SCI_CLOCKF       0x03 // RW Clock frequency + multiplier
#define VS10xx_SCI_DECODE_TIME  0x04 // RW Decode time in seconds
#define VS10xx_SCI_AUDATA       0x05 // RW Misc. audio data
#define VS10xx_SCI_WRAM         0x06 // RW RAM write/read
#define VS10xx_SCI_WRAMADDR     0x07 // RW Set address for RAM write/read
#define VS10xx_SCI_HDAT0        0x08 // R  encoding: 16-bit big endian sample data
#define VS10xx_SCI_HDAT1        0x09 // R  encoding: number of samples available
#define VS10xx_SCI_AIADDR       0x0A // RW start address to application to jump to
#define VS10xx_SCI_VOLUME       0x0B // RW volume (8bit-left, 8-bit right)
#define VS10xx_SCI_AICTRL0      0x0C // RW application control
#define VS10xx_SCI_AICTRL1      0x0D // RW application control
#define VS10xx_SCI_AICTRL2      0x0E // RW application control
#define VS10xx_SCI_AICTRL3      0x0F // RW application control

#define VS10xx_SM_DIFF      0x0001 // differential output phase (inverts left out)
#define VS10xx_SM_LAYER12   0x0002 // allow MPEG layers I & II
#define VS10xx_SM_RESET     0x0004 // Soft reset
#define VS10xx_SM_CANCEL    0x0008 // Cancel decoding current file
#define VS10xx_SM_EAR_LO    0x0010 // VS1053b only **
#define VS10xx_SM_TESTS     0x0020 // Allow SDI tests
#define VS10xx_SM_STREAM    0x0040 // Stream mode VS1053b only **
#define VS10xx_SM_EAR_HI    0x0080 // VS1053b only **
#define VS10xx_SM_DACT      0x0100 // DCLK active edge        0
#define VS10xx_SM_SDIORD    0x0200 // SDI bit order           0
#define VS10xx_SM_SDISHARE  0x0400 // Share SPI chip select   0
#define VS10xx_SM_SDINEW    0x0800 // VS1002 native SPI modes 0
#define VS10xx_SM_ADPCM     0x1000 // PCM/ADPCM recording active VS1053b
#define VS10xx_SM_ENCODE    0x1000 // Activate encoding VS1063
#define VS10xx_SM_RSRV      0x2000 // 0
#define VS10xx_SM_LINE1     0x4000 // MIC/LINE1 selector, 0: MICP, 1: LINE1
#define VS10xx_SM_CLKRANGE  0x8000 // clock range, 0: 12..13 MHz, 1: 24..26 MHz

#define VS10xx_GPIO_DDR     0xC017 //!< Direction
#define VS10xx_GPIO_IDATA   0xC018 //!< Values read from pins
#define VS10xx_GPIO_ODATA   0xC019 //!< Values set to the pins

#define VS10xx_INT_ENABLE   0xC01A //!< Interrupt enable

#define VS10xx_DATABUFFERLEN 32 //!< Length of the data buffer

//------------------------------------------------------------------------
//
// ToolkitVLSI
//

//
// Constructor .. stores low level pin definitions and inits audio volumes
ToolkitVLSI::ToolkitVLSI(int8_t cs, int8_t dcs, int8_t dreq)
{
  _cs = cs;
  _dcs = dcs;
  _dreq = dreq;
  spi_dev_ctrl = NULL;
  spi_dev_data = NULL;
}

ToolkitVLSI::~ToolkitVLSI()
{
    if (spi_dev_ctrl) {
        delete spi_dev_ctrl;
    }
    if (spi_dev_data) {
        delete spi_dev_data;
    }
}

//
// Create and initialize the SPI device
// According to the VLSI Manual, the highest speed for SPI C and D
// is CLKI/7 which would be 6.857MHz for 4x speed and 7.899Mhz for 4.5x
// ALSO: you should check DREQ before sending anything!
// FOR 192kbps mp3 is 24000 bytes/sec or 12000 words/sec
// It takes 32 clocks to read a word .. so minimum clock for 192kbps is 384000
//
void ToolkitVLSI::begin()
{
 pinMode(_dreq, INPUT);
/*
 // NOTE: changed Adafruit SCI clock from 250000
 spi_dev_ctrl = new Adafruit_SPIDevice(_cs, 3000000, SPI_BITORDER_MSBFIRST,
                                         SPI_MODE0, &SPI);
 // NOTE: changed Adafruit SDI clock from 8000000
 spi_dev_data = new Adafruit_SPIDevice(_dcs, 6000000, SPI_BITORDER_MSBFIRST,
                                         SPI_MODE0, &SPI);
*/
 spi_dev_ctrl = new ToolkitSPI(_cs, 3000000);
 spi_dev_data = new ToolkitSPI(_dcs, 3000000);

 spi_dev_ctrl->begin();
 spi_dev_data->begin();
}

//
// Get the status code of the VLSI device
// bits 7-4 are SS_VER: 0=1001,1=1011,2=1002,3=1003,4=1053+8053,
//  5=1033,6=1063/1163,7=1103
uint8_t ToolkitVLSI::getStatus()
{
    return (sciRead(VS10xx_SCI_STATUS) >> 4) & 0x0F;
}

//
// Send data over the data interface
void ToolkitVLSI::playData(uint8_t *buffer, uint8_t buffersize) {
  spi_dev_data->write(buffer, buffersize);
}

//
// Check DREQ
boolean ToolkitVLSI::readyForData()
{
  return digitalRead(_dreq);
}

//
// Soft Reset over SPI
void ToolkitVLSI::reset()
{
  sciWrite(VS10xx_SCI_MODE, VS10xx_SM_SDINEW | VS10xx_SM_RESET);
  delay(100);
  sciWrite(VS10xx_SCI_CLOCKF, 0x6000);
}

//
// level is 0.0 to 1.0. 0.1 is roughly 5 dB
void ToolkitVLSI::setPlaybackVolume(float level)
{
    if (level>1.0) { level=1.0; } else if (level<0.0) { level=0.0; }
    float attenuation = 1.0 - level;
    float decibels = 0.0;
    if (attenuation <= 0.9) {
        decibels = 100.0 * attenuation;   // 0.0 to 90.0
    } else { // 90.0 down to 255.0
        decibels = 90.0 + (attenuation-0.9) * 1650.0;
    }
    uint8_t vlsi_volume = (uint8_t) decibels;
    setVolume(vlsi_volume, vlsi_volume);
    Serial.printf("volume %3.2f -> 1/2 dBs -%3.2f -> vlsi %u\n",
        level, decibels, vlsi_volume);
}

//
// Set Playback Volume (0-255 per channel, smaller is louder)
// -0.5 dB per increment (i.e. 10 is -5dB)
void ToolkitVLSI::setVolume(uint8_t left, uint8_t right)
{ // accepts values between 0 and 255 for left and right.
  uint16_t v;
  v = left;
  v <<= 8;
  v |= right;

  noInterrupts(); // cli();
  sciWrite(VS10xx_SCI_VOLUME, v);
  interrupts(); // sei();
}

//
// Read a register from the VLSI device
// VS10xx SCI Read
//  send 8-bits READ (0x03) then 8-bits address
//  receive 16-bit (big endian) response
uint16_t ToolkitVLSI::sciRead(uint8_t addr)
{
  while (!digitalRead(_dreq)) {  // DANGER !!!
#if USE_RTOS_TASKS & USE_RTOS_DREQ_DELAY
    vTaskDelay(portTICK_PERIOD_MS);
#else
    ;
#endif
  }
  uint8_t buffer[2] = {VS10xx_SCI_READ, addr};
//  spi_dev_ctrl->write_then_read(buffer, 2, buffer, 2);
//  return (uint16_t(buffer[0]) << 8) | uint16_t(buffer[1]);

  return spi_dev_ctrl->write_then_read(buffer, 2);
}

//
// Write to a register on the VLSI device
// VS10xx SCI Write
//  send 8-bits WRITE(0x20) then 8-bits address
//  send 16-bits (big endian) data
void ToolkitVLSI::sciWrite(uint8_t addr, uint16_t data)
{
  while (!digitalRead(_dreq)) {  // DANGER !!!
#if USE_RTOS_TASKS & USE_RTOS_DREQ_DELAY
    vTaskDelay(portTICK_PERIOD_MS);
#else
    ;
#endif
  }
  uint8_t buffer[4] = {VS10xx_SCI_WRITE, addr, uint8_t(data >> 8),
                       uint8_t(data & 0xFF)};
  spi_dev_ctrl->write(buffer, 4);
}

//------------------------------------------------------------------------
//
// ToolkitVS1063
//

ToolkitVS1063::ToolkitVS1063(int8_t cs, int8_t dcs, int8_t dreq)
    : ToolkitVLSI(cs, dcs, dreq)
{
    // nothing to do here .. yet!
}

#include "VS1063a_patches.h"

void ToolkitVS1063::loadPatches()
{
    int plugin_size = sizeof(vs1063a_plugin)/sizeof(vs1063a_plugin[0]);
    int i = 0;
    while (i < plugin_size) {
        uint16_t addr, n, val;
        addr = vs1063a_plugin[i++];
        n = vs1063a_plugin[i++];
        if (n & 0x8000U) { /* RLE run, replicate n samples */
            n &= 0x7FFF;
            val = vs1063a_plugin[i++];
            while (n--) {
                sciWrite(addr, val);
            }
        } else {           /* Copy run, copy n samples */
            while (n--) {
                val = vs1063a_plugin[i++];
                sciWrite(addr, val);
            }
        }
    }
    Serial.print("Loaded ");
    Serial.print(i);
    Serial.println(" words of patches code");
}

uint16_t ToolkitVS1063::getClock()
{
    return sciRead(VS10xx_SCI_CLOCKF);
}

uint16_t ToolkitVS1063::setClock(float multiplier)
{
    uint16_t clock = sciRead(VS10xx_SCI_CLOCKF);
    // modify the clock
    // there are 8 values of SC_MULT from 0-7 matching 1x to 5x
    // there are 4 values of SC_ADD from 0 (add 0) to 4
    //      1 +1, 2+1.5, 3+2.0
    if (multiplier < 1.0) { multiplier = 1.0; }
    if (multiplier > 5.5) { multiplier = 5.5; }
    multiplier = multiplier * 2.0 - 2.0;  // 0.0 to 9.0
    uint16_t m = (uint16_t) multiplier;    // 0 to 9
    // special cases are 1 (add 1.5) and 9 (4.0 + 1.5)
    const uint16_t clock_mask = 0xf800; // top 5 bits
    uint16_t clock_bits = 0;
    switch (m) {
        case 0 :
            break;
        case 1 :
            clock_bits = 0x1000;    // add 1.5
            break;
        case 9 :
            clock_bits = 0xb000;    // mul 4.0 add 1.5
            break;
        default :
            clock_bits = 0x2000 * (m-1);
            break;
    }
    clock &= (~clock_mask);
    clock |= clock_bits;
    sciWrite(VS10xx_SCI_CLOCKF, clock);
    delay(2);
    return clock;
}

void ToolkitVS1063::setSPISpeed(uint32_t freq)
{
    spi_dev_ctrl->changeFrequency(freq);
    spi_dev_data->changeFrequency(freq);
}

//------------------------------------------------------------------------
//
// VU Meters
//

uint16_t ToolkitVS1063::enableVUMeter(bool on_not_off)
{   // WRAMADDR 0x1e09 bit 2 meter enable
    sciWrite(VS10xx_SCI_WRAMADDR, 0x1e09);  // set VLSI pointer to this address
    uint16_t playMode = sciRead(VS10xx_SCI_WRAM);
    if (on_not_off) {
        playMode |= 0x0004; // bit 2 on
    } else {
        playMode &= 0xfffb; // bit 2 off
    }
    sciWrite(VS10xx_SCI_WRAMADDR, 0x1e09);  // set VLSI pointer to this address
    sciWrite(VS10xx_SCI_WRAM, playMode);
    return playMode;
}

uint16_t ToolkitVS1063::readVUMeter()
{   //WRAMADDR 0x1e0c 8 bits left 8 bits right, 0-32 in 3dB steps
    sciWrite(VS10xx_SCI_WRAMADDR, 0x1e0c);  // set VLSI pointer to this address
    uint16_t vuMeter = sciRead(VS10xx_SCI_WRAM);
    return vuMeter;
}

//------------------------------------------------------------------------
//
// Encoder basics
//

void ToolkitVS1063::encoder_start()
{   // configure the VS10xx /w CBR and start encoding
    sciWrite(VS10xx_SCI_AICTRL0, _sample_rate);

    // gain and AGC
    float g = _gain * 1024.0;
    uint16_t gu16 = (uint16_t) g;
    Serial.print("Encoder gain: ");
    Serial.println(gu16);
    sciWrite(VS10xx_SCI_AICTRL1, gu16);     // manual gain 1024 = 1.0x

    g = _agc_max_gain * 1024.0;
    gu16 = (uint16_t) g;
    Serial.print("Encoder agc max gain: ");
    Serial.println(gu16);
    sciWrite(VS10xx_SCI_AICTRL2, gu16);     // AGC max gain

    // 0x60 is mp3, _channel is stereo or mono modes
    sciWrite(VS10xx_SCI_AICTRL3, 0x60 | _channel);

    // 0xE000 is CBR bit rate x1000
    sciWrite(VS10xx_SCI_WRAMADDR, 0xE000 | _kbps);
    // NOTE: WRAMADDR is just being used as a general parameter register here.
    // It is not used as an address into the VS1063 RAM

    uint16_t mode = sciRead(VS10xx_SCI_MODE);
    mode |= VS10xx_SM_ENCODE;
    if (_mic_not_line) { // make sure the LINE1 bit is clear
        uint16_t mask = ~VS10xx_SM_LINE1;
        mode &= mask;
    } else { // set the Line1 mask .. which gives us stereo line input
        mode |= VS10xx_SM_LINE1;
    }
    sciWrite(VS10xx_SCI_MODE, mode);

    // jump to the start of the patches program .. which reads all of the
    // previously set parameters and then start's encoding
    sciWrite(VS10xx_SCI_AIADDR, 0x50);      // start recording
}

void ToolkitVS1063::encoder_stop()
{
    uint16_t mode = sciRead(VS10xx_SCI_MODE);
    mode |= VS10xx_SM_CANCEL;
    sciWrite(VS10xx_SCI_MODE, mode);
    delay(100); //wait 100 ms
    // read all remaining words from SCI_HDAT0/SCI_HDAT1
    // wait 100ms
    // soft reset
}

uint16_t ToolkitVS1063::encoder_available()
{
    uint16_t words = sciRead(VS10xx_SCI_HDAT1);
    return words * 2; // bytes
}

uint16_t ToolkitVS1063::encoder_getData(uint8_t *where, uint16_t where_size)
{
    // The encoding data buffer is 3712 16-bit words
    // We have to pull everything out of the buffer as soon
    // as it is available so that the encoder can reuse the space
    // for the next frame
    uint16_t words = sciRead(VS10xx_SCI_HDAT1); // sciRead is dreq safe!!
    if (0 == words) {
        return 0;
    }
    uint16_t where_words = where_size / 2;
    if (where_words > words) {
        where_words = words;
    }
#define VLSI_READ_BYTES
#ifdef VLSI_READ_BYTES
    uint16_t byte_index = 0;
    uint16_t data0, data1;
    uint16_t bytes_read = where_words * 2;
    while (where_words) {
        // fetch 2 bytes and decrement where_words
        data1 = sciRead(VS10xx_SCI_HDAT0);
        data0 = (data1 >> 8) & 0xff;
        data1 &= 0x00ff;
        where[byte_index++] = data0;
        where[byte_index++] = data1;
        where_words--;
    }
#else
    uint16_t bytes_read = where_words * 2;
    uint16_t *W = (uint16_t *) where;
    while (where_words) {
        *W = sciRead(VS10xx_SCI_HDAT0);
        W++;
        where_words--;
    }
#endif
    return bytes_read;
}

void ToolkitVS1063::encoder_updateVolume()
{
    // gain and AGC
    float g = _gain * 1024.0;
    uint16_t gu16 = (uint16_t) g;
    Serial.print("Encoder gain: ");
    Serial.println(gu16);
    sciWrite(VS10xx_SCI_AICTRL1, gu16);     // manual gain 1024 = 1.0x

    g = _agc_max_gain * 1024.0;
    gu16 = (uint16_t) g;
    Serial.print("Encoder agc max gain: ");
    Serial.println(gu16);
    sciWrite(VS10xx_SCI_AICTRL2, gu16);     // AGC max gain
}

//------------------------------------------------------------------------
//
// Encoder settings
//

void ToolkitVS1063::encoder_setAGC(float maximum_gain)
{   // hardware max is 64.0
    if (maximum_gain < 0.001) { maximum_gain = 0.001; }
    else if (maximum_gain > 63.99) { maximum_gain = 63.99; }
    _agc_max_gain = maximum_gain;
    _gain = 0.0; // which sets AGC mode ON
}

void ToolkitVS1063::encoder_setBitrate(uint16_t kbps_enum)
{   // use the BITRATE_xxK enums
    _kbps = kbps_enum;
}

void ToolkitVS1063::encoder_setChannels(uint16_t channel_enum)
{   // JOINT_STEREO .. etc
    _channel = channel_enum;
}

void ToolkitVS1063::encoder_setManualGain(float gain)
{   // 0.001 to 64.0
    if (gain < 0.001) { gain = 0.001; }
    else if (gain > 63.99) { gain = 63.99; }
    _gain = gain;
}

void ToolkitVS1063::encoder_setMicNotLine(boolean mic_not_line)
{   // mic is mono
    _mic_not_line = mic_not_line;
}

void ToolkitVS1063::encoder_setSamplerate(uint16_t rate_enum)
{   // use SAMPLE_ enums
    _sample_rate = rate_enum;
}

//
// END OF ToolkitVLSI.cpp
