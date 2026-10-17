# How to compile an example program on ESP-IDF 6.1 for ESP32-P4?

1: Launch ESP-IDF 6.1

2: Go into your file manager and search for your ESP-IDF installation, usually `C:\Espressif\frameworks\`

3: Locate the example folder `C:\Espressif\frameworks\esp-idf-v5.3.2\examples`

4: Locate the example you want to test. Example: hello_world `C:\Espressif\frameworks\esp-idf-v5.3.2\examples\get-started\hello_world`

5: Make a copy of this example somewhere else on your PC. 

6: Copy the path of your copy of the example and cd into it on your IDF shell `cd C:\documents\hello_world`

7: To check that everything is fine you can do `dir` which should return atleast : `main & CMakeLists.txt`

8: Now you will have to specify your ESP32 target chip (here ESP32-P4): `idf.py set-target esp32p4`
>note: you only have to type this command once

9: The ESP32-P4 chip we use is an older 1.0 version, we jave to specify it. Type `idf.py menuconfig`

10: Navigate to `Component config\Hardware settings\Chip revision` and check `Select ESP32-P4 revision < 3.0`

11: Press the `Escape` key on your keyboard to exit menuconfig and when prompted press `y` to save the configuration 

12: You can now build the example firmware by typing `idf.py build`

13: Now you can flash the program to your ESP32-P4 using `idf.py flash`

14: If you want to open the Serial monitor you can type `idf.py monitor`

# Going further

You don't have to write `idf.py build`, `idf.py flash`, `idf.py monitor` each time, you can write `idf.py build flash monitor` all at once. Or a combination of two ie: `idf.py flash monitor`

If you want to configure the build you can type `idf.py menuconfig` which is going to open a bunch of menus with many different options. I will detail the most useful ones:
- `Serial flasher config\Flash size` : You can configure the flash size of your chip (16MB for ESP32-P4)
- `Compiler option\Optimization level` : You can configure the optimization for your code (Fast, Small in flash....)
- `Component config\ESP PSRAM` : Enable external RAM (ESP32-P4 has 32MB of external RAM and only 768Ko of internal RAM)