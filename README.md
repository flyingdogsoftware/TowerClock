## Tower Clock firmware for BIGTREETECH S42B closed loop stepper motors 

Custom firmware for S42BV1.2 closed loop stepper motors (Stm-32) for Real Tower Clock project.
In back display it shows user interface for setting mode like hur/minute with the two buttons.
Also ESP-01 is needed for WiFi, Web user interface and NTP

Project page: https://hackaday.io/project/178777-real-tower-clock-version-20

ESP 01 software: https://github.com/flyingdogsoftware/Tower-Clock-Wifi

ESP 01 has to be connected to S42B via 4 cables: +, GND and two serial connection.
See https://cdn.hackaday.io/images/7688121619966256516.jpg

### How to install vscode and platformio for compile

See description at https://github.com/bigtreetech/BIGTREETECH-S42B-V1.0/tree/master/firmware/S42BV1.0

### how to update firmware
- You need a ST-Link device connected to stepper control board via 4 cables and your computer via USB

- Install ST-Link, download: https://www.st.com/en/development-tools/stsw-link004.html 

- execute   st-link_cli.exe -c SWD -p "Path to your project\firmware\S42BV1.0\.pioenvs\BIGTREE_S42B_V1_0\firmware.bin"  0x08000000 -Rst  -Run


