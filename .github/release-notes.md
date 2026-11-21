Agrosmart firmware for ESP32-C5, built with ESP-IDF v6.1 and default sdkconfig with Kconfig default.

Flash it from the browser with the web flasher on the Agromate website, or download the images and flash them with esptool (replace `PORT` with your serial port):

```sh
esptool --chip esp32c5 -p PORT -b 460800 --before default-reset --after hard-reset \
  write-flash --flash-mode dio --flash-size 4MB --flash-freq 80m \
  0x2000 agrosmart-esp32c5-bootloader.bin \
  0x8000 agrosmart-esp32c5-partition-table.bin \
  0xe000 agrosmart-esp32c5-ota-data.bin \
  0x10000 agrosmart-esp32c5-app.bin
```
