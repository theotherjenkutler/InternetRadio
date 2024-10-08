// Tested: ESP8266, ESP32, M0+WINC1500

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <WiFi.h>

char* ssid     = "Wave Farm WiFi";
const char* password = "";

//  http://ice1.somafm.com/u80s-128-mp3
const char *host = "audio.wavefarm.org";
const char *path = "/wgxc.mp3";
//const char *path = "/doomed-128-mp3";
int httpPort = 8000;

// These are the pins used
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)
#define VS1053_CS      27     // VS1053 chip select pin (output)
#define VS1053_DCS     14     // VS1053 Data/command select pin (output)
#define VS1053_DREQ    13     // VS1053 Data request, ideally an Interrupt pin


#define VS1053_RESET2   -1     // VS1053 reset pin (not used!)
#define VS1053_CS2      32     // VS1053 chip select pin (output)
#define VS1053_DCS2     33     // VS1053 Data/command select pin (output)
#define VS1053_DREQ2    25     // VS1053 Data request, ideally an Interrupt pin


#define VOLUME_KNOB    A0
#define ON_OFF_SWITCH  4

int lastvol = 10;

Adafruit_VS1053 musicPlayer =  Adafruit_VS1053(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ);
Adafruit_VS1053 musicPlayer2 =  Adafruit_VS1053(VS1053_RESET2, VS1053_CS2, VS1053_DCS2, VS1053_DREQ2);


// Use WiFiClient class to create HTTP/TCP connection
WiFiClient client;
  
void setup() {
  Serial.begin(115200);

  Serial.println("\n\nAdafruit VS1053 Feather WiFi Radio");

  /************************* INITIALIZE MP3 WING */
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Hang on let me initialize the second one"));
     if(! musicPlayer2.begin()){
      Serial.println(F("well fuck it didn't work"));
      while(1){
        delay(10);
      }
     }
  }

  Serial.println(F("VS1053 found"));
  //musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(lastvol, lastvol);

  // don't use an IRQ, we'll hand-feed

  pinMode(ON_OFF_SWITCH, INPUT_PULLUP);
  /************************* INITIALIZE WIFI */
  Serial.print("Connecting to SSID "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");  Serial.println(WiFi.localIP());


  /************************* INITIALIZE STREAM */
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

  musicPlayer.setVolume(lastvol, lastvol);

}

// our little buffer of mp3 data
uint8_t mp3buff[32];   // vs1053 likes 32 bytes at a time

int loopcounter = 0;

void loop() {
  if (! digitalRead(ON_OFF_SWITCH)) {
    yield();
    return;
  }
  
  loopcounter++;

  // wait till mp3 wants more data
  if (musicPlayer.readyForData()) {
    //Serial.print("ready ");
    
    //wants more data! check we have something available from the stream
    if (client.available() > 0) {
      //Serial.print("set ");
      // yea! read up to 32 bytes
      uint8_t bytesread = client.read(mp3buff, 32);
      // push to mp3
      musicPlayer.playData(mp3buff, bytesread);

      //Serial.println("stream!");
    }
  } else {
    if (loopcounter >= 1000) {
      loopcounter = 0;
      // adjust volume!
      //int vol = 0;
      //vol = analogRead(VOLUME_KNOB);
      //vol /= 10;
      //if (abs(vol - lastvol) > 3) {
      //  Serial.println(vol);
      //  lastvol = vol;
      //  musicPlayer.setVolume(lastvol, lastvol);
      //}
    }
  }
}
