# Licenta
SISTEM INDUSTRIAL DE OPERARE ȘI MONITORIZARE A UNEI MACARALE SI A UNEI MASINI

Demo of the system can be found here: https://uptro29158-my.sharepoint.com/:f:/g/personal/eduard_bran_student_upt_ro/IgBmqxKYsb3kSIT-fPaT6OXNAZT_Sz6NkMsVwO4GGzrFpAk?e=UNJgDJ

Environments used: Arduin IDE v2, VSCode, Node-RED, Ignition v8.3.2, Timebase Historian, TIA Portal

Configuration steps:

**IOT2050:**

_SET_UP_

1. Download industrial OS for IOT2050 from https://support.industry.siemens.com/cs/document/109741799/downloads-for-simatic-iot20x0?dti=0&lc=en-RO
2. Burn the USB drive (with balenaEtcher)
3. Flash IOT2050 with a usb drive (.wic file)
4. Install docker, docker containers and node-red (activate automatic run at startup)
5. Set up putty using the instructions from https://cache.industry.siemens.com/dl/files/259/109963259/att_1275665/v1/iot2050_operating_instructions_en_en-US.pdf
6. Additional resource: https://github.com/SIMATICmeetsLinux/IOT2050-Setting-Up-Example-Image?tab=readme-ov-file#skip-emmc-as-boot-device
7. Additional resource: https://www.youtube.com/watch?v=vwtHLQE1z8o&list=PLeUt5cH7zxRpvqNZmlvFskB-XCcn-892j&index=2

_WIFI MODULE_

1. plug in wifi dongle
2. Connect IOT2050 to internet through the local connection with the PC
   - Configure ethernet port of IOT2050 as DHCP
   - on PC: view network connections -> wifi -> share
   - on PC: ethernet -> advanced ipv4 settings -> add LAN ip (to be able to use Putty)
3. on IOT2050 write the following commands:
   - git clone "https://github.com/RinCat/RTL88x2BU-Linux-Driver.git" /usr/src/rtl88x2bu-git
   - cd /usr/src/rtl88x2bu-git
   - make
   - make install
   - modprobe 88x2bu
4. on IOT2050 activate the Wifi connection using the instructions from https://github.com/SIMATICmeetsLinux/IOT2050-SmartFarming-Application/blob/main/docs/SIMATIC_IOT2050_setting_up.md#32-first-commissioning-of-the-simatic-iot2050

_NODE-RED_

check logs: sudo journalctl -u node-red -n 20
to run zenoh nodes the following must be done:
    https://flows.nodered.org/node/@freol35241/nodered-contrib-zenoh
    
    Edit settings.js :
    // At the top of the file, before module.exports
    if (!process.execArgv.includes('--experimental-wasm-modules')) {
        console.log('Note: WASM support not enabled. Some Zenoh features may not work.');
        console.log('Consider setting NODE_OPTIONS="--experimental-wasm-modules --no-warnings"');
    }
    
    Edit auto start:
    sudo systemctl edit node-red
    Add :
    [Service]
    Environment="NODE_OPTIONS=--experimental-wasm-modules --no-warnings"
    
    Save and exit: Press Ctrl+O, then Enter, then Ctrl+X.
    Prove working: cat /proc/$(pgrep -f node-red | head -n 1)/environ | tr '\0' '\n' | grep NODE_OPTIONS

_ZENOH ROUTER_

1. cd /root
2. mkdir -p zenoh_plugins/lib (create folder)
3. cd zenoh_plugins (move to folder)
4. wget "https://download.eclipse.org/zenoh/zenoh-plugin-remote-api/1.9.0/zenoh-ts-1.9.0-aarch64-unknown-linux-musl-standalone.zip" -O zenoh-ts-1.9.0.zip (download Remote API plugin v1.9.0 for ARM64)
5. python3 -m zipfile -e zenoh-ts-1.9.0.zip . (unzip file)
6. wget "https://download.eclipse.org/zenoh/zenoh-plugin-mqtt/1.9.0/zenoh-plugin-mqtt-1.9.0-aarch64-unknown-linux-musl-standalone.zip" -O zenoh-mqtt-1.9.0-musl.zip (download MQTT plugin v1.9.0 for ARM64)
7. python3 -m zipfile -e zenoh-mqtt-1.9.0-musl.zip . (unzip file)
8. mv libzenoh_plugin_mqtt.so lib/ (move file to lib folder)
9. ls -la (check that .SO files are present in lib folder)

_DOCKER_

run the container using the following command:

      docker run -d \
        --name zenoh-router \
        --restart unless-stopped \
        --network host \
        -v /root/zenoh_plugins/lib:/root/.zenoh/lib \
        eclipse/zenoh:1.9.0 \
        -l tcp/0.0.0.0:7447 \
        --cfg='plugins/remote_api/websocket_port:"0.0.0.0:10000"' \
        --cfg='plugins/storage_manager/volumes/memory:{}' \
        --cfg='plugins/storage_manager/storages/iot_storage/key_expr:"**"' \
        --cfg='plugins/storage_manager/storages/iot_storage/volume:"memory"' \
        --cfg='plugins/mqtt/port:1884'

 commands:
 - check logs: docker logs zenoh-router
 - see if docker is running: sudo systemctl is-active docker
 - see active containers: docker ps
 - delete container: docker rm -f zenoh-router

**Telematic_ECU**

1. install platformIO add-on in VSCode
2. install CMake and Visual Studio, also have "Desktop development with C++" Workload enabled
3. start project from project root directory using the "Developer PowerShell" with the command: code . (the Developer PowerShell has all the dev tools already enabled in the path such as "nmake" (required for build)

**Main_ECU**

1. download CORE library from https://github.com/maxgerhardt/pio-framework-bluepad32/archive/refs/heads/main.zip
2. in the platformio.ini file link the CORE library as the framework to be used by the project 

**RestAPI**

1. add python extension in VSCode
2. create virtual env: python -m venv venv
3. to bypass power-shell security: Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
4. activate env: .\venv\Scripts\activate
5. pip install fastapi uvicorn httpx openai websockets google-genai
6. create account at "google ai studio" for token access
7. change the TIMEBASE_URL and GEMINI_API_KEY variables with your configuration values
8. start application: python main.py

**SCADA**

in Ignition Gateway:
1. add modules: MQTT Engine, MQTT Transmission, Timebase Historian
2. connections -> settings MQTT Engine -> Namespaces -> custom -> add a subscription and the name of the folder where to generate the tags that are received
3. connections -> settings MQTT Engine -> Sets -> add new set with primary host disabled
4. connections -> settings MQTT Engine -> Servers -> settings -> add new server: give the url of the Zenoh router with the MQTT accepting port and link the created Server set
5. connections -> settings MQTT Transmission -> Sets -> create a new Server set
6. connections -> settings MQTT Transmission -> Transmitters -> add MQTT Engine as tag provider and set the Server set that was created
7. connections -> settings MQTT Transmission -> Servers -> add a new server with the URL of the Zenoh router with the MQTT accepting port and link the Server set created
8. services -> Historians -> create Historian and add the Timebase URL (add the port for http)

_**Additional resources:**_
1. https://github.com/eclipse-zenoh/zenoh-pico
2. https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/#3
3. https://www.youtube.com/watch?v=rvGo-P4qOn8&list=PLeUt5cH7zxRpvqNZmlvFskB-XCcn-892j&index=4
4. https://gist.github.com/julianlam/0ce7692ca10fb91970b6693bc02587ce
5. https://askubuntu.com/questions/919054/how-do-i-run-a-single-command-at-startup-using-systemd
6. https://www.youtube.com/watch?v=OrOtKirwS5w
7. https://download.eclipse.org/zenoh/zenoh-plugin-remote-api/1.9.0/
8. https://flows.nodered.org/node/@freol35241/nodered-contrib-zenoh
9. https://github.com/SIMATICmeetsLinux/IOT2050-SmartFarming-Application/blob/main/docs/SIMATIC_IOT2050_setting_up.md#32-first-commissioning-of-the-simatic-iot2050
10. https://github.com/SIMATICmeetsLinux/IOT2050-NodeRed-OPCUA-Server/blob/main/docs/README_IOT2050SETUP_NODEREDFLOW.md#required-packages
11. https://cache.industry.siemens.com/dl/files/073/109974073/att_1295970/v1/iot2050_operating_instructions_en_en-US.pdf
12. https://conferences2.sigcomm.org/acm-icn/2022/assets/zenoh-4-Zenoh-and-Zenoh-Flow-Hands-on-e8cbd760e0b88b74417fb1c14d1d373b5ce2a094bc29b5f1a0bfd8e52030c151.pdf
13. https://ardushop.ro/ro/electronica/2418-1350-senzor-greutate.html
14. https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads
15. https://community.platformio.org/t/use-bluepad32-library-in-pio/46745
16. https://bluepad32.readthedocs.io/en/latest/plat_arduino/
17. https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
18. https://docs.arduino.cc/resources/datasheets/ABX00083-datasheet.pdf
19. https://docs.arduino.cc/language-reference/en/variables/data-types/stringObject/
20. https://www.figma.com/colors/mustard-yellow/
21. https://inductiveautomation.com/downloads/third-party-modules/8.3.2
22. https://localhost:4510/
23. https://docs.timebase.flow-software.com/knowledge-base/timebase-module-for-ignition
24. https://github.com/ace-technologies-inc/i3X-Explorer/releases
25. https://docs.timebase.flow-software.com/knowledge-base/timebase-module-for-ignition
26. https://inductiveautomation.com/downloads/third-party-modules/8.3.2
27. https://docs.timebase.flow-software.com/knowledge-base/i3x-interoperability-api-guide-
28. https://www.geeksforgeeks.org/python/fastapi-uvicorn/
29. https://www.geeksforgeeks.org/python/fastapi-uvicorn/
30. https://aistudio.google.com/rate-limit?timeRange=last-hour&project=gen-lang-client-0819173646
