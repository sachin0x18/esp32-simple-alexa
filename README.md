# Prerequisites
Please prepare your host system with the toolchain. Please see http://esp-idf.readthedocs.io/en/latest/get-started/index.html for the host setup.

# Supported Hardware
The current application supports ESP32 based simple_alexa board (https://www.tindie.com/products/microwavemont/esp32-simple-alexabis/)

# Prepare Images
If you already have the pre-compiled images flashed on the board, please jump to the next section (Device Configuration).

## Clone all the repositories

```
$ git clone --recursive https://github.com/espressif/esp-idf.git

$ cd esp-idf; git checkout release/v3.1

$ git clone https://github.com/espressif/esp-avs-sdk.git
```

## Build and flash the project

```
$ cp -r esp32-simple-alexa esp-avs-sdk/examples/

$ cd esp-avs-sdk/examples/simple_alexa

$ export IDF_PATH=/path/to/esp-idf

$ export ESPPORT=/dev/cu.SLAB_USBtoUART (or /dev/ttyUSB0 or /dev/ttyUSB1 on Linux)

$ make -j 8 flash monitor
```

# Device Configuration
Please use the android app available [here](https://github.com/espressif/esp-avs-sdk/wiki/Android-app-APK) to configure the WIFI credentials and to associate Amazon Login with the device.

# Enabling AWS [Optional]
To Enabling AWS IoT platform on ESP32 follow below steps:

* Enable Amazon Web Services IoT Platform
```
make menuconfig -> Component config -> Amazon Web Services IoT Platform
```
* Enter AWS IoT Endpoint Hostname
```
make menuconfig -> Component config -> Amazon Web Services IoT Platform -> AWS IoT Endpoint Hostname -> Enter the hostname
```

* Copy certificate file (certificate.pem.cert and private.pem.key) to examples/simple_alexa/main/certs/ folder appropriately to communicate with AWS endpoint.

* Subscribe to "test_topic/esp32" on AWS Console to see the messages.

# Known Issues
* When AWS is enabled, after provisioning the device takes time(15-20 sec) to connect to AWS.

# Demo
* Once the board boots up and successfully connects to the Wi-Fi network after provisioning, you can say ALEXA and speak or use "ALEXA_SW" button on Simple_Alexa board for conversation. For Tap-to-Talk, press and release the button and speak. 
* LED States:
    * WHITE: ESP32 in Provisioning Mode
    * CYAN: ESP32 in ALEXA Detection Mode
    * GREEN: Wakeword/Tap-to-Talk detected and listening
    * YELLOW: ALEXA Speaking
* You can connect any external speaker/headphone with 3.5mm connector to PHONE JACK to listen to responses.
* you can now ask any command like:
    * tell me a joke
    * how is the weather?
    * will it rain today?
    * Sing a song
    * Play TuneIn radio
    * Set volume level to 7
* Each time Alexa is detected the WakeWord count is incremented on AWS IoT console
* Press and Hold "ALEXA_SW" button for 5 seconds to reset the board to factory settings
