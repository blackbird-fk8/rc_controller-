# rc_controller
RC Controller for WSU capstone, design_1, using one ESP32-S3 and two ESP32 Dev Modules. See the READ_ME.txt for more details

General Description: Using an ESP32-S3 and two ESP32 Dev Module
- ESP32 S3 WROOM U1 
   - External Antenna 
   - Two Joy cons for X & Y 
   - Capacitors 0.1uF and 10uF to filter analog noise 
   - Adafruit SSD1306 OLED screen 

Details: S3 is able to send data packets of the joycon status and data for X and Y axis inputs and sends over the internet protocol ESP_NOW for faster communication over regular WiFi and longer range. As well tell the current user on the OLED display the status of the current % of the throttle (Y axis) and steer (X axis). The display also tells the user if they are currently connected from the controller and the two receivers (Link: TX OK #Scan iterations). 

- ESP32 Dev Module (Steer Servo) 
   Details: Uses esp_now and control packets for interpreting the revived joycon commands for the X axis. Uses in code state ID, helpers, shared states, callback and as well uses the onboard led (blue) to indicate the recieve and call back to indicate its status/command for debugging. As well Fail safes in case of losing connection will stop all commands and remain at a stop state. 

Same applies for the ESP32 Dev Module (Motor Driver). Only difference is status updates and control logic for the IBT_2/BTS2960 H-Bridge to directly control the DC motor with RC style behavior. 

Additional notes: ESP32 upon earlier testing, the microcontroller does not perform well at multiple tasks at once when using multiple PWM signals. This conflict resulted to isolating the servo and motor with two different microcontrollers to rule out the theoretical conflict. Now that this works individually. Next goal is to overcome that conflict by using one esp32 and possibly a different solution. For now this testing solution will work. 

NOTE: What each file does below

1) Sender (ESP32-S3 controller) -- rc_controller.ino + joystick.h + ui_oled.h
Purpose: Reads the two joystick axes, smooths them, turns them into percent values, shows status on the OLED, and broadcasts control packets over ESP-NOW.

What it does:
- Sets WiFi to STA mode and initializes ESP-NOW
- Adds two peers using MAC addresses:
  - Motor receiver board MAC
  - Servo receiver board MAC
- Reads joystick analog values continuously
- Applies:
  - calibration (center)
  - smoothing (IIR filter)
  - deadzone + hysteresis (so it doesn’t spam state changes)
- Produces:
  - steerPct (-100..100)
  - thrPct (-100..100)
- Sends a ControlPacket about 20 times per second (heartbeat)
- Updates OLED with:
  - link status placeholder / send status
  - steer status (LEFT / NEUTRAL / RIGHT) + percent
  - throttle status (BACK / STOP / FWD) + percent

2) Motor Reciever (ESP32 DevKit + BTS7960) -- motor_driver.ino
Purpose: Receives ESP-NOW packets and drives the IBT-2 / BTS7960 motor driver using RPWM/LPWM and enable pins.

What it does: 
- Sets WiFi to STA mode and initializes ESP-NOW receive
- Validates packets using:
  - packet size check
  - magic number check (prevents random garbage from triggering motion)
- Extracts thrPct and ignores steerPct
- Converts throttle into motor output:
  - thrPct > +deadband → Forward
  - thrPct < -deadband → Reverse
  - otherwise → Stop
- Uses ESP32 hardware PWM (LEDC) on:
  - RPWM = GPIO 25
  - LPWM = GPIO 26
- Enables/disables driver via:
  - REN = GPIO 27
  - LEN = GPIO 14
- Failsafe: if no valid packet for ~250ms:
  - PWM = 0
  - disables REN/LEN (outputs off)
- Onboard LED debug:
  - STOP = 1 blink
  - REV = 2 blinks
  - FWD = 3 blinks
  - blinks only on state changes (not every packet)
 
3) Servo Receiver (ESP32 DevKit + servo on GPIO18) -- servo.ino
Purpose: Receives ESP-NOW packets and drives the steering servo.

What it does: 
- Sets WiFi to STA mode and initializes ESP-NOW receive
- Validates packets (size + magic)
- Extracts steerPct and ignores thrPct
- Maps steering percent to servo pulse width:
  - -100 → ~1000 µs
  - 0 → ~1500 µs (center)
  - +100 → ~2000 µs
- Uses ESP32Servo library (ESP32Servo.h)
- Failsafe: if no packet for ~250ms → servo centers at 1500 µs
- Onboard LED debug:
  - CENTER = 1 blink
  - LEFT = 2 blinks
  - RIGHT = 3 blinks
  - blinks on state change only
