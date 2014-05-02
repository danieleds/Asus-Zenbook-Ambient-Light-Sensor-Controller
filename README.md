Asus Zenbook Ambient Light Sensor Controller
============================================

Tested with:
------------
 * UX32VD
   * Ubuntu 14.04 + Linux 3.13.0
   * Ubuntu 13.10 + Linux 3.11.0
   * Ubuntu 13.04 + Linux 3.8.0
 * UX31A
   * Ubuntu 13.10

How to install
--------------

**Required packages:** libbsd-dev, xbacklight.

 1. Install the ALS Driver:
   1. Download the source code from [here](https://github.com/danieleds/als).
   2. Extract the archive, move into the directory, and compile with `make`.
   3. Insert the module into your current kernel with `sudo insmod als.ko`
 2. Build this controller:
   1. `cd service`
   2. `qmake als-controller.pro -r -spec linux-g++-64`, or `qmake als-controller.pro -r -spec linux-g++` if you're on a 32-bit system.
   3. `make`
   
The generated binary file, *als-controller*, is what will monitor the light sensor.

Note that, for the driver to see the sensor, you should set `acpi_osi='!Windows 2012'` (e.g. at the end of GRUB_CMDLINE_LINUX_DEFAULT in /etc/default/grub) and then reboot.

How to use
----------
 1. Launch als-controller with root privileges, for example: `sudo ./als-controller`. This will be the service that monitors the light sensor.
 2. Use the same program with user privileges, als-controller, to control the service. Some examples:
    
        ./als-controller -e     // Enable the sensor
        ./als-controller -d     // Disable the sensor
        ./als-controller -s     // Get sensor status (enabled/disabled)

Example
-------
After compiling and running als-controller, try running switch.sh from the "example" folder.
For an ideal integration with your system, the suggested idea is to start the service at boot,
and then bind some script similar to switch.sh to a key combination on your keyboard.

Thanks
------
 * Diego - https://github.com/Voskot
