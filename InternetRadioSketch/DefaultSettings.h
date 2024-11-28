//
// DefaultSettings.h

#ifndef _DEFAULT_SETTINGS_H_
#define _DEFAULT_SETTINGS_H_

const char *default_settings =
    "wifi_router_SSID = YourLocalSSID\n"
    "wifi_router_password = YourLocalPassword\n"
    "wifi_toolkit_hostname = WaveFarmToolkit\n"
    "wifi_toolkit_AP_SSID = WaveFarmToolkit_AP\n"
    "listen_icecast_url = audio.wavefarm.org\n"
    "listen_icecast_port = 8000\n"
    "listen_icecast_mountpoint = /wgxc.mp3\n"
    "listen_volume = 0.88\n"
    "remote_icecast_url = somewhereinspace.net\n"
    "remote_icecast_port = 8080\n"
    "remote_icecast_user = source\n"
    "remote_icecast_password = YourIcecastPassword\n"
    "remote_icecast_mountpoint = live\n"
    "mic_not_line = 0\n"
    "channels = 1\n"
    "bitrate = 128\n"
    "sample_rate = 44100\n"
    "agc_not_manual = 1\n"
    "manual_gain_level = 32.0\n"
    "agc_maximum_gain = 24.0\n"
    "startup_auto_mode = waiting\n\0";

#define DEFAULT_SETTINGS_SIZE (strlen(default_settings)+1)

#endif

//
// END OF DefaultSettings.h
