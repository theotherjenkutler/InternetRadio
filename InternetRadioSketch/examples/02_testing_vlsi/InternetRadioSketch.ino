//
// Wave Farm Toolkit for audio streaming
// October/November 2024
//

const boolean listen_dont_encode = false;

// Tested with Arduino 1.8.19
// Esp32 v 1.0.6 ("ESP32 Dev Module")
// Adafruit_BusIO-1.16.1 (adafruit's SPI on top of the Esp/Arduino SPI)

// Include from packages/libraries
// SPI, WiFi, File System, SPI Flash File System
#include <SPI.h>
#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>

// Include for local/project libraires
// VLSI VS10xx
#include "src/ToolkitVLSI/ToolkitVLSI.h"

// Hardware pins
// We are using the default SPI I/O pins, so we don't have to define them
// Define the chip selects and data request pins for the VS1053 and VS1063
#define VS1053_CS      27     // VS1053 chip select pin (output)
#define VS1053_DCS     14     // VS1053 Data/command select pin (output)
#define VS1053_DREQ    13     // VS1053 Data request, ideally an Interrupt pin

#define VS1063_CS      32     // VS1063 chip select pin (output)
#define VS1063_DCS     33     // VS1063 Data/command select pin (output)
#define VS1063_DREQ    25     // VS1063 Data request, ideally an Interrupt pin

// Quick WiFi connection info
const char* ssid     = "TELUS7838-2.4G"; //"Wave Farm WiFi";
const char* password = "????";

// Icecast server link for listening
const char *host = "audio.wavefarm.org";
const char *path = "/wgxc.mp3";
const int httpPort = 8000;

// Music players for both audio chips
int lastvol = 10;

ToolkitVLSI musicPlayer1053 = ToolkitVLSI(VS1053_CS, VS1053_DCS, VS1053_DREQ);
ToolkitVS1063 musicPlayer1063 = ToolkitVS1063(VS1063_CS, VS1063_DCS, VS1063_DREQ);

//-----------
static char vuleft[34];
static char vuright[34];

void fancyVU(uint16_t vu) {
  uint16_t left = vu >> 8;
  uint16_t right = vu & 0xff;
  if (left > 32) { left = 32; }
  if (right > 32) { right = 32; }
  for (uint16_t i = 0; i <= left; i++) {
    vuleft[i] = '=';
  }
  for (uint16_t i = left+1; i < 33; i++) {
    vuleft[i] = '.';
  }
  vuleft[33] = 0;
  for (uint16_t i = 0; i <= right; i++) {
    vuright[i] = '=';
  }
  for (uint16_t i = right+1; i < 33; i++) {
    vuright[i] = '.';
  }
  vuright[33] = 0;
  Serial.print(vuleft);
  Serial.print("     ");
  Serial.println(vuright);
}
//-----------

// Use WiFiClient class to create HTTP/TCP connection
WiFiClient client;

void setup() {
  // Setup serial monitor/console
  Serial.begin(115200);
  Serial.println("\n\nWave Farm Toolkit!");

  // configure the SPI control and request lines for ALL SPI devices
  musicPlayer1053.begin();
  musicPlayer1063.begin();

  // reset and check status for the VS1053
  musicPlayer1053.reset();
  Serial.print("VS1053 status: ");
  Serial.println(musicPlayer1053.getStatus());

  // reset and check status for the VS1063
  musicPlayer1063.reset();
  Serial.print("VS1063 status: ");
  Serial.println(musicPlayer1063.getStatus());

  musicPlayer1063.loadPatches();

  // clock 5.0 is good for 128kbps, 5.5 is needed for 192kbps
  uint16_t clock = musicPlayer1063.setClock(5.5);
  Serial.print("Setting clock: ");
  Serial.println(clock, HEX);

  Serial.print("VS1063 clock: ");
  Serial.println(musicPlayer1063.getClock(), HEX);

  // Now we can ramp up the SCI speed
  musicPlayer1063.setSPISpeed(10000000);
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer1053.setVolume(lastvol, lastvol);
  musicPlayer1063.setVolume(lastvol, lastvol);

  //musicPlayer1063.enableVUMeter(true);

  musicPlayer1063.encoder_setAGC(16.0);
  musicPlayer1063.encoder_setBitrate(musicPlayer1063.BITRATE_192K);
  musicPlayer1063.encoder_setChannels(musicPlayer1063.DUAL_STEREO);
//  musicPlayer1063.encoder_setManualGain(1.0);
  musicPlayer1063.encoder_setMicNotLine(false);
  musicPlayer1063.encoder_setSamplerate(musicPlayer1063.SAMPLE_44K1);

  // don't use an IRQ, we'll hand-feed

  /************************* INITIALIZE WIFI */
  Serial.print("Connecting to SSID "); Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");  Serial.println(WiFi.localIP());


  if (listen_dont_encode) {
    /************************* INITIALIZE STREAM */
    Serial.println("Starting in listener mode");
    Serial.print("connecting to ");  Serial.println(host);
  
    if (!client.connect(host, httpPort)) {
      Serial.println("Connection failed");
      return;
    }
  
    // We now create a URI for the request
    Serial.print("Requesting URL: ");
    Serial.println(path);
  
    // This will send the request to the server
    client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
  } else { // startup as encoder
    Serial.println("Starting in transmitter mode");
    musicPlayer1063.encoder_start();
 //   while (musicPlayer1063.encoder_available() < 2048) {
 //     delay(2); // we want to gather about 15ms of data
 //   }
  }

} // end of setup()

// our little buffer of mp3 data for playback
uint8_t mp3buff[32];   // vs1053/1063 likes 32 bytes at a time

// for the encoder, we want to read ALL of the available data as
// fast as possible, so that the encoder can reuse its buffer

uint8_t encode_buffer[2][4096]; // a big double buffer for incoming encoded data
uint16_t encode_in_buffer = 0;
uint16_t encode_in_address = 0;

uint16_t decode_out_buffer = 0;
uint16_t decode_out_address = 0;
boolean decode_is_running = false;

int loopcounter = 0;

long int t1 = 0;
long int t2 = 0;
long int ta = 0;

void loop() {

  if (listen_dont_encode) { // LISTENING
    if (musicPlayer1063.readyForData()) { // wait till mp3 wants more data
      //wants more data! check we have something available from the stream
      if (client.available() > 0) {
        uint8_t bytesread = client.read(mp3buff, 32);
        // push to mp3
        musicPlayer1053.playData(mp3buff, bytesread);
        musicPlayer1063.playData(mp3buff, bytesread);
      } // end of client.available()
    } // end of if readyForData()

    // VU meter thing
    loopcounter++;
    if (loopcounter > 512*8) {
      uint16_t vu = musicPlayer1063.readVUMeter();
      fancyVU(vu);
      loopcounter = 0;
    }
  } else { // ENCODING
    // (1) read as much encoded data as we can
    if (musicPlayer1063.readyForData()) {
      uint16_t remaining = 4096 - encode_in_address;
  //    if (remaining > 128) { remaining = 128; }
      long int tsl = millis();
      uint16_t loaded = musicPlayer1063.encoder_getData(&encode_buffer[encode_in_buffer][encode_in_address], remaining);
      long int tse = millis();
      if (loaded > 0) { ta = ta + (tse - tsl); }
      encode_in_address += loaded;
      if (encode_in_address >= 4096) { // flip buffers
        encode_in_buffer = (encode_in_buffer + 1) % 2;
        encode_in_address = 0;
        if (!decode_is_running) {
          Serial.println("Turning on decode_is_running");
          decode_is_running = true;
        }
        Serial.print("Flipped encode to ");
        Serial.println(encode_in_buffer);
        t2 = millis();
        if (0 != t1) {
          Serial.print("Time to load decode buffer: ");
          Serial.println(t2-t1);
          Serial.print("Time spent in loading: ");
          Serial.println(ta);
          ta = 0;
        }
        t1 = t2;
      }
    }
    // (2) if we have loaded enough encoded data, then decode some of it
    // The decoder has a 2048 byte FIFO buffer, of which 32 bytes is guaranteed to be available
    // when DREQ is high. We don't want to send more data then we have read from the encoder!
    if (decode_is_running) {
      boolean goforit = true;
      while (goforit) {
        if (musicPlayer1053.readyForData()) { // wait till mp3 wants more data
 
          uint16_t remaining = 4096 - decode_out_address;
          if (remaining > 32) { remaining = 32; }
          musicPlayer1053.playData(&encode_buffer[decode_out_buffer][decode_out_address], remaining);
          decode_out_address += remaining;
          if (decode_out_address >= 4096) {
            decode_out_buffer = (decode_out_buffer + 1) % 2;
            decode_out_address = 0;
            if (decode_out_buffer == encode_in_buffer) {
              decode_is_running = false;
              goforit = false;
            }
            Serial.print("Flipped decode to ");
            Serial.println(decode_out_buffer);
          }
        
        } else { // end of readForData()
          goforit = false;
        }
      } // end of while(goforit)
    } // end of if(decode_is_running)
  }

} // end of loop()

// END OF InternetRadioSketch.ino
