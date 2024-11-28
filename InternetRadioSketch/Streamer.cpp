//
// Setup the MP3 Encoder - VS1063a
//

#include "Streamer.h"

#include "src/ToolkitVLSI/ToolkitVLSI.h"
#include "src/ToolkitSPIFFS/ToolkitSPIFFS.h"

// Hardware pins
// We are using the default SPI I/O pins, so we don't have to define them
// Define the chip selects and data request pins for the VS1053 and VS1063
#define VS1053_CS      27     // VS1053 chip select pin (output)
#define VS1053_DCS     14     // VS1053 Data/command select pin (output)
#define VS1053_DREQ    13     // VS1053 Data request, ideally an Interrupt pin

#define VS1063_CS      32     // VS1063 chip select pin (output)
#define VS1063_DCS     33     // VS1063 Data/command select pin (output)
#define VS1063_DREQ    25     // VS1063 Data request, ideally an Interrupt pin

Streamer::Streamer()
    : ToolkitVS1063(VS1063_CS, VS1063_DCS, VS1063_DREQ)
{
    run_not_wait = false;
    listen_dont_encode = false;
    initBuffers();
}

Streamer::~Streamer()
{

}

//
// We have to configure all SPI devices in order to use one of them.
// So we configure the VS1053 even though we don't use it.

void Streamer::encoder_setup()
{   // run mode states
    run_not_wait = (0!=strcmp(
        SettingItem::findString("startup_auto_mode"),"waiting"));
    listen_dont_encode = (0==strcmp(
        SettingItem::findString("startup_auto_mode"),"listener"));

    // configure the SPI control and request lines for ALL SPI devices
    // NOTE: player1053 might need to exists outside the scope of
    // this function. i.e. it may kill the SPI port when it is destroyed
    // SO we can try it as static, or add it as a private class variable
    ToolkitVLSI player1053 = ToolkitVLSI(VS1053_CS, VS1053_DCS, VS1053_DREQ);
    player1053.begin();

    begin(); // VLSI 1063a

    // reset and check status for the VS1053
    player1053.reset();
    Serial.print("VS1053 status: ");
    Serial.println(player1053.getStatus());
    player1053.setPlaybackVolume(
        SettingItem::findFloat("listen_volume", 0.9));

    // reset and check status for the VS1063
    reset();
    Serial.print("VS1063 status: ");
    Serial.println(getStatus());

    //
    // SETUP the VS1063 .. new patches, clock, SPI speed, playback volume
    loadPatches();

    // clock 5.0 is good for 128kbps, 5.5 is needed for 192kbps
    uint16_t clock = setClock(5.5);
    Serial.print("Setting clock: ");
    Serial.println(clock, HEX);

    Serial.print("VS1063 clock: ");
    Serial.println(getClock(), HEX);

    // Now we can ramp up the SCI speed
    delay(50);
    setSPISpeed(10000000);
    delay(50);

    setPlaybackVolume(SettingItem::findFloat("listen_volume",0.9));

    encoder_setAGC(SettingItem::findFloat("agc_maximum_gain",16.0));
    encoder_setBitrate(SettingItem::findUInt("bitrate",128));
    encoder_setChannels(SettingItem::findUInt("channels",0));
    encoder_setSamplerate(SettingItem::findUInt("sample_rate",0));
    encoder_setMicNotLine(1==SettingItem::findUInt("mic_not_line",0));
    if (0==SettingItem::findUInt("agc_not_manual",1)) {
        encoder_setManualGain(
            SettingItem::findFloat("manual_gain_level",1.0));
    }
} // end of setup()


boolean Streamer::start_listener()
{   // Connect to a remote icecast stream for listening
    Serial.println("Starting in listener mode");
    Serial.print("Connecting to ");
    Serial.println(SettingItem::findString("listen_icecast_url"));

    if (!listener.connect(
        SettingItem::findString("listen_icecast_url"),
        SettingItem::findUInt("listen_icecasst_port",8000)))
    {
        Serial.println("Connection failed");
        return false;
    }

    // We now create a URI for the request
    Serial.print("Requesting URL: ");
    Serial.println(SettingItem::findString("listen_icecast_mountpoint"));

    // This will send the request to the server
    listener.print(String("GET ") +
        SettingItem::findString("listen_icecast_mountpoint") +
        " HTTP/1.1\n" +
        "Host: " +
        SettingItem::findString("listen_icecast_url") +
        "\n" +
        "Connection: close\n\n");

    // clear the header .. wait for two \n characters in a row
    // skip \r characters
    boolean clearing = true;
    uint16_t numendlines = 0;
    uint32_t start_t = millis();
    while (clearing) {
      if (listener.available() > 0) {
        char c = listener.read();
        if ('\n'==c) { numendlines++; }
        else if ('\r'==c) { ; }
        else { numendlines=0; }
        Serial.print(c);
        if (2==numendlines) { clearing=false; }
      }
      if ((millis()-start_t) > 2000) {
          Serial.println("Icecast listener timedout waiting for header");
          listener.stop();
          return false;     // timedout waiting for header
      }
    }
    return true;
} // end of start_listener()

void Streamer::update_playbackVolume()
{
    setPlaybackVolume(SettingItem::findFloat("listen_volume",0.9));
}

void Streamer::update_recordVolume()
{
    encoder_setAGC(SettingItem::findFloat("agc_maximum_gain",16.0));
    if (0==SettingItem::findUInt("agc_not_manual",1)) {
        encoder_setManualGain(
            SettingItem::findFloat("manual_gain_level",1.0));
    }
    encoder_updateVolume(); // send volume to VS1063a
}

//
// Static rotating buffer stream

#define bufferLock(A) (xSemaphoreTake(A, portTICK_PERIOD_MS*10))
#define bufferUnlock(A) (xSemaphoreGive(A))

SemaphoreHandle_t Streamer::buffer_mutex = NULL;

uint8_t Streamer::buffer[NUMBER_OF_BUFFERS][BUFFER_SIZE];

size_t Streamer::buffer_in_index = 0;
size_t Streamer::buffer_in_address = 0;
size_t Streamer::buffer_out_index = 0;

boolean Streamer::initBuffers()
{
    if (NULL==buffer_mutex) {
        buffer_mutex = xSemaphoreCreateMutex();
    }
    return (NULL!=buffer_mutex);
}

boolean Streamer::mutexExists()
{
    return (NULL!=buffer_mutex);
}

uint8_t *Streamer::getNextInBuffer(size_t *remaining)
{
    *remaining = BUFFER_SIZE - buffer_in_address;
    return &buffer[buffer_in_index][buffer_in_address];
}

// NOTE: for tasks running on the same processor, we can
// wrap the "locked" regions with taskENTER_CRITICAL()
// and taskEXIT_CRITICAL() .. if the locks have issues.

void Streamer::advanceInBuffer(size_t bytesused)
{
    buffer_in_address += bytesused;
    if (buffer_in_address >= BUFFER_SIZE) {
        bufferLock(buffer_mutex);
        buffer_in_index = (buffer_in_index + 1) % NUMBER_OF_BUFFERS;
        bufferUnlock(buffer_mutex);
        buffer_in_address = 0;
    }
}

uint8_t *Streamer::getNextOutBuffer(size_t *bytestostream)
{
    uint8_t *result = NULL;
    *bytestostream = 0;
    boolean ready;
    size_t compare_index;
    // we only need to lock around the acces to buffer_in_index
    bufferLock(buffer_mutex);
    compare_index = buffer_in_index;
    bufferUnlock(buffer_mutex);
    if (buffer_out_index > compare_index) {
        compare_index += NUMBER_OF_BUFFERS;
    }
    ready = ((compare_index-buffer_out_index)>2);
    if (ready) {
        *bytestostream = BUFFER_SIZE;
        result = buffer[buffer_out_index];
        buffer_out_index = (buffer_out_index + 1) % NUMBER_OF_BUFFERS;
    }
    return result;
}

//
// END OF SetupEncoder.cpp
