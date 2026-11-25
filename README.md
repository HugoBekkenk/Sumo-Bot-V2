# Sumo Bot V2

## Phone control (Android)

The ESP32 can be built for the Dabble app's Bluetooth Low Energy GamePad.
The existing PS4-controller build remains available separately.

1. Install **Dabble** from [Google Play](https://play.google.com/store/apps/details?id=io.dabbleapp).
2. Build and upload the phone firmware:

   ```sh
   pio run -e esp32-phone -t upload
   ```

3. Open Dabble, connect to **SumoBot**, and open the **GamePad** module in
   joystick/analog mode.
4. Move the joystick left/right to steer. Hold **Square** for full-power
   forward or **Circle** for full-power reverse; release both to stop.
   Hold **Triangle** or **Cross** to move the position servos left or right.

The bot stops and centers the position servos when the app disconnects.
To restore PS4 control, build and upload the default environment:

```sh
pio run -e esp32doit-devkit-v1 -t upload
```
