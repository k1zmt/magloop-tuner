# Magloop Controller


## Behavior

Connect to your Wi-Fi and open the IP address of the magloop
in your browser.


<a href="https://youtube.com/shorts/tR3TuS6j_TE" target="_blank">
 See video
 <br />
 <img src="http://img.youtube.com/vi/tR3TuS6j_TE/default.jpg" alt="Watch the video" width="240" height="180" border="10" />
</a>

# Development

Initializing the project

```shell script

platformio project init --board esp32dev --ide clion
```

### Flash the firmware

Before you flash you need to setup your wifi access point
SSID and password
```shell
export WIFI_SSID=YourWiFiSSID
export WIFI_PASSWORD=YourWiFiPassword
```

Now you can program the ESP
```shell
pio run --target=upload -e ota
```



## Build
- go to the directory of this project
- open a terminal
- build with : `pio run` (first time is long because it needs to download all the requirements)
- upload to the board (needs the borad connected using USB + appropriate user settings see installation) : `pio run --target upload`
- See the `Serial` output : `pio device monitor --baud 115200`


