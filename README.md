# rc_controller-
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
