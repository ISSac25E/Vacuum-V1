# Vacuum V2 - Amir Gorkovchenko (Late 2021 - Early 2022)

### !! Deprecated Version. See V3 !!

Blinds V1 was quickly replaced by V2.\
V2 is the refreshed version, redocumented, and fewer bugs.
V1 was used primarily as a test platform.

This project works off of an MQTT server. The MQTT server we used was Home Assistant (back when it was free and local).

### Note
Core libraries and needed files can be found in Core branch

## Design
This project is centered around the esp8266 with builtin wifi 2.4ghz capabilities.

A later revision added an mpu6050 gyroscope/accelerometer which was used to determine the robot vacuum movement.

The esp8266 was also attached with the vacuum's internal button and LED status to be able to give a full status report as well as simulate button presses to control the vacuum.

The esp8266, mpu6050, and buck converter were all located on a breadboard on top of the vacuum cleaner, later relocated to the inside of the vacuum.

## Photos
![alt text](image-1.png)

## Videos
Showing a version where the board is hidden internally \
[Vacuum V2 Demo](https://youtube.com/shorts/B811tByo_kI?feature=share)