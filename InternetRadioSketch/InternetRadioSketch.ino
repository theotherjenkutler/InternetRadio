//
// Wave Farm Toolkit for audio streaming
// October/November 2024
//

// Tested with Arduino 1.8.19
// Esp32 v 1.0.6 ("ESP32 Dev Module")

// config contains compile time options for RTOS
#include "config.h"

// default settings and default index page are used if the SPIFFS
// is not setup with settings.txt and index.html
#include "DefaultSettings.h"
#include "DefaultIndexPage.h"

// Include for local/project libraires
// VLSI VS10xx
#include "Streamer.h"
#include "src/ToolkitFiles/ToolkitFiles.h"
#include "src/ToolkitWiFi/ToolkitWiFi_Server.h"

// Include hardware controls (volume knob and mode switch)
#if USE_PIN_CONTROLS
#include "PinControls.h"
PinControls pins_thing;
#endif

// These objects connect us with the Toolkit VLSI and WiFi Server
Streamer stream_thing;
ToolkitWiFi_Server wifi_thing;

//------------------------------------------------------------------------------------
//
// There are four main tasks that we need to run in the loop()
//

void server_task(void *params)
{
  const int delay_in_ms = RTOS_DELAY_SERVER_TASK;
  while (true) {
    wifi_thing.run();
    vTaskDelay(portTICK_PERIOD_MS * delay_in_ms);
  }
}

void listen()
{
  while (stream_thing.readyForData()) { // wait till mp3 wants more data
    //wants more data! check we have something available from the stream
    if (stream_thing.reconnect_listener_if_needed()) {
      if (stream_thing.listener.available() > 0) {
      
        size_t remaining = 0;
        uint8_t *buffer = stream_thing.getNextInBuffer(&remaining);
        if (remaining > 32) {
          remaining = 32;
        }
      
        uint8_t bytesused = stream_thing.listener.read(buffer, remaining);
        stream_thing.playData(buffer, bytesused); // send to the VS1063a audio output
        stream_thing.advanceInBuffer(bytesused);
  
      } // end of listener.available()
    } else { // end of reconnect_listener_if_needed()
      Serial.println("Listener has disconnected .. will try to reconnect in 10 seconds.");
      vTaskDelay(portTICK_PERIOD_MS * 10000);
    }
  } // end of while readyForData()  
}

void listener_task(void *params)
{
  const int delay_in_ms = RTOS_DELAY_LISTENER_TASK;  // 250ms per 128kbps buffer
  while (true) {
    listen();
    vTaskDelay(portTICK_PERIOD_MS * delay_in_ms);
  } // end of while (true)
}

void encode()
{
  if (stream_thing.readyForData()) {
    size_t remaining = 0;
    uint8_t *buffer = stream_thing.getNextInBuffer(&remaining);
    size_t bytesused = stream_thing.encoder_getData(buffer, remaining);  // reads as much as we ask for
    stream_thing.advanceInBuffer(bytesused);
  } // end of readyForData()  
}

void encoder_task(void *params)
{
  const int delay_in_ms = RTOS_DELAY_ENCODER_TASK;  // 250ms per 128kbps buffer, 167ms per 192kbps buffer
  while (true) {
    encode();
    vTaskDelay(portTICK_PERIOD_MS * delay_in_ms);
  }
}

void icy_task(void *params)
{
  const int delay_in_ms = 10000;  // 10 seconds
  while (true) {
    // do the delay at the top of the loop, so that we aren't checking for 
    // a lost connection as soon as the task starts
    vTaskDelay(portTICK_PERIOD_MS * delay_in_ms);
    if (!wifi_thing.isIcecastBroadcastStillConnected()) {
      Serial.println("Remote icecast server has disconnected .. trying to reconnect");
      if (wifi_thing.startIcecastBroadcast()) {
        Serial.println("ICY Broadcast started!");
      } else {
        Serial.println("ICY failed to start!");
      }
    }
  }  
}

#if USE_PIN_CONTROLS
void pins_task(void *params)
{
  const int delay_in_ms = 250;  // 4 times a second
  while (true) {
    vTaskDelay(portTICK_PERIOD_MS * delay_in_ms);
    pins_thing.updateVolume();
    pins_thing.getSwitchState();
  }
}
#endif

void start_rtos_tasks()
{
  xTaskCreatePinnedToCore(server_task, "Server Task", 4096, NULL, 1, NULL, 0); // changed to processor 0 !! from 1

  if (stream_thing.run_not_wait) {
    if (stream_thing.listen_dont_encode) {
      xTaskCreatePinnedToCore(listener_task, "Listener Task", 2048, NULL, 1+RTOS_HIPRIORITY_VLSI, NULL, 1);
    } else {
      xTaskCreatePinnedToCore(encoder_task, "Encoder Task", 2048, NULL, 1+RTOS_HIPRIORITY_VLSI, NULL, 1);
      xTaskCreatePinnedToCore(icy_task, "Icy Reconnect Task", 4096, NULL, 1, NULL, 0); // changed to processor 0 !! from 1
    }
  }

#if USE_PIN_CONTROLS
  xTaskCreatePinnedToCore(pins_task, "Hardware Pins Task", 4096, NULL, 1, NULL, 0);
#endif

  Serial.println("Toolkit started up with RTOS tasks");
  //Serial.printf("Tick timing is %u ticks per millisecond\n", portTICK_PERIOD_MS);

#if USE_RTOS_TASKS & USE_RTOS_DREQ_DELAY
  Serial.println("RTOS + RTOS DREQ DELAY are ON");
  //Serial.printf("Maximum Priority = %u\n", configMAX_PRIORITIES); // prints out 25
#endif

}

//------------------------------------------------------------------------------------
//
// Handle live changes to the playback and recording volume
//

// live updates need to look for:
//  listen_volume FLOAT .. playback volume for the VS1063a        update_playbackVolume()
//  agc_not_manual UINT .. agc on off .. set these in the encoder update_recordVolume();
//  manual_gain_level FLOAT
//  agc_maximum_gain FLOAT

const char *keys[] = {
  "listen_volume",
  "agc_not_manual",
  "manual_gain_level",
  "agc_maximum_gain",
  NULL
};

enum {
  uLISTEN_VOL = 0,
  uNONE = 4
};

static void update_volumes(const char *name, const char *value)
{
  uint32_t i = 0;
  while (NULL != keys[i]) {
    if (0==strcmp(keys[i],name)) {
      break;
    }
    i++;
  }
  switch (i) {
    case uNONE :
      break;
    case uLISTEN_VOL :
      stream_thing.update_playbackVolume();
      //Serial.println("playback volume updated");
      break;
    default :
      stream_thing.update_recordVolume();
      //Serial.println("record volume updated");
      break;
  }
}

//------------------------------------------------------------------------------------
//
// Setup / Startup
//

void setup() {
  boolean force_waiting_mode = false; // set to true if anything goes wrong
  
  // Setup serial monitor/console
  Serial.begin(115200);
  Serial.println("\n\nWave Farm Toolkit!");

  //--------------------------------------------------
  // Load settings from internal flash drive)
  //

  if (!ToolkitFiles::begin()) {
    Serial.println("ERROR starting FileSystem!");
    force_waiting_mode = true; // don't run the audio stream until we have settings
  } else { // load settings
    Serial.println("--------------------------");
    if (!ToolkitFiles::loadSettings()) {
      Serial.println("No settings to load!");
      force_waiting_mode = true; // don't run the audio stream until we have settings
      SettingItem::loadSettingsFromBuffer(default_settings, DEFAULT_SETTINGS_SIZE);
      Serial.println("Starting with builtin default settings");
    }
    Serial.println("SETTINGS:");
    SettingItem::printSettingsToSerial();
    Serial.println("--------------------------");

  }

  delay(2*1000);

  //--------------------------------------------------
  // VLSI STUFF
  
  stream_thing.encoder_setup();
  if (force_waiting_mode) {
    stream_thing.run_not_wait = false;
  }
  
  //--------------------------------------------------
  // WiFi STUFF - start the WiFi local+AP server

  wifi_thing.setDefaultIndexPage(default_index_page, DEFAULT_INDEX_PAGE_SIZE);
  wifi_thing.begin();

  //--------------------------------------------------
  // Setup the Streamer
  
  if (stream_thing.run_not_wait) {
    if (stream_thing.listen_dont_encode) {
      stream_thing.start_listener();
    } else { // startup as encoder
      Serial.println("Starting in transmitter mode.");
      if (wifi_thing.startIcecastBroadcast()) {
        Serial.println("ICY Broadcast started!");
      } else {
        Serial.println("ICY failed to start!");
      }
      stream_thing.encoder_start();
    }
  } else {
    Serial.println("Starting in Waiting mode.");
  }

  //--------------------------------------------------
  // Link the WiFi STUFF to the Streamer and the live updates function

  wifi_thing.setMP3DataStreamFunction(Streamer::getNextOutBuffer);
  wifi_thing.setWSLiveChangesFunction(update_volumes);


#if USE_PIN_CONTROLS
  pins_thing.begin();
#endif 

  //--------------------------------------------------
  // RTOS
#if USE_RTOS_TASKS
  start_rtos_tasks();
#endif
} // end of setup()

//------------------------------------------------------------------------------------
// Normal Arduino run loop
//
// The FreeRTOS idle task is used to run the loop() function whenever there is no 
// unblocked FreeRTOS task available to run. In the trivial case, where there are 
// no configured FreeRTOS tasks, the loop() function will be run exactly as normal, 
// with the exception that a short scheduler interrupt will occur every 15 ms.
//

void loop() {
#if USE_RTOS_TASKS
  // not sure if this is useful .. but it should send the scheduler back to
  // the other tasks a bit faster .. ??
  const int delay_in_ms = 10000;  // 10 seconds
  vTaskDelay(portTICK_PERIOD_MS * delay_in_ms);
#else

  wifi_thing.run();

  if (!stream_thing.run_not_wait) {
    return;
  }
  
  if (stream_thing.listen_dont_encode) { // LISTENING

    listen();

  } else { // ENCODING

    encode();

  } // end of ENCODING
  
#endif
} // end of loop()

// END OF InternetRadioSketch.ino
