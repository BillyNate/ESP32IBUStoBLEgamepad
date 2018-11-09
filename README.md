# ESP32 FlySky iBus to Bluetooth Gamepad
#### Turn any FlySky transmitter/controller into a gamepad for your favorite simulator  
Let your ESP32 manifest itself as a Bluetooth gamepad to connect to any compatible device (pc, smartphone...) while listening to the serial output (iBus) of a receiver module for FlySky transmitters  

### Prerequisites
- An ESP32 (preferably on a dev board)
- iBus receiver module
- FlySky compatible transmitter
- A pc with the Arduino IDE

### Setup
1. `git clone` this repository or download and unpack [the zip](https://github.com/BillyNate/ESP32IBUStoBLEgamepad/archive/master.zip) to the `Arduino` directory in your user's folder
2. Connect your ESP32 dev board to a pc and make sure the Arduino IDE is set up correctly to program the ESP
3. Connect the iBus output wire of the receiver module to gpio16 of the ESP and power the receiver module (_be careful! 3v3 only or you'll blow up the ESP_)
4. Upload the sketch
5. Connect the FlySky transmitter to the receiver module
6. Connect your pc, smartphone or something else to `IBUS2BLE` on Bluetooth
7. Enjoy!

### Tested on
- Transmitter: [Turnigy Evolution](https://hobbyking.com/en_us/fpv-racer-radio-mode-2-black.html)
- Receiver module: TGY-iA6C (says 4.0-6.5V, but 3V3 works fine)
- [ESP32](http://s.click.aliexpress.com/e/MFuoQh6)
- [ESP32 development board](http://s.click.aliexpress.com/e/cYnEOFGk)
- Windows 10 PC and Android 8.1 Oreo device
- [FPV Freerider](https://play.google.com/store/apps/details?id=com.Freeride.Freerider_FREE) (using [VR goggles](http://s.click.aliexpress.com/e/G00xf3S) on the Android device)

### Credits and many thanks to:
- [Neil Kolban](https://github.com/nkolban) for his great contributions to the ESP32 FW (in particular the BLE support)
- [Chegewara](https://github.com/chegewara) for his contributions to Kolban's repository and his helpfull support to anyone asking
- [Tim Wilkinson](https://gitlab.com/timwilkinson) for his FlySkyIbus library
 
and to Espressif for providing the HID implementation within the esp-idf.
