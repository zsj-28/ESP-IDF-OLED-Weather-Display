This repo implemented a weather display with a [ESP32 Feather board](https://www.adafruit.com/product/3405), [SSD1306 I2C OLED display](https://www.amazon.com/HiLetgo-Serial-128X64-Display-Color/dp/B06XRBYJR8/ref=asc_df_B06XRBYJR8/?tag=hyprod-20&linkCode=df0&hvadid=693675076785&hvpos=&hvnetw=g&hvrand=10350043509659898637&hvpone=&hvptwo=&hvqmt=&hvdev=c&hvdvcmdl=&hvlocint=&hvlocphy=9005925&hvtargid=pla-587536972937&psc=1&mcid=358a859600a53706877a6f6cd9ac95f2&gad_source=1), and [OpenWeatherMap](https://openweathermap.org/) API.
The SSD1306 Driver is contributed by [nopnop2002](https://github.com/nopnop2002/esp-idf-ssd1306). To run this application, please make sure the driver in the component directory is on the path for ESP-IDF.
Run the following to set target and open the configuration menu, in the Top > SSD1306 Configuration, the pin and communication protocols can be modified.
```
idf.py target esp32
idf.py menuconfig
```
After select and connect the correct pinout, modify the URL in weather-display.c with your own API-Key and desire location for weather information.
Also please remember to change the SSID and password to your desire local network
Then run 
```
idf.py build
idf.py flash
```
to flash the application to the board.

Below is a sample output:
![alt text](https://github.com/zsj-28/ESP-IDF-OLED-Weather-Display/blob/main/sample_display.jpg)

