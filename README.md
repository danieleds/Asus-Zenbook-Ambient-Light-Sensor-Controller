Asus Zenbook Ambient Light Sensor Controller
============================================

Tested with:
------------
 * UX32VD + Ubuntu 13.04 + Linux 3.8.0

How to install
--------------

 1. Install the ALS Driver:
   1. Download the source code from [here](https://github.com/victorenator/als).
   2. Extract the archive, move into the directory, and compile with `make`.
   3. Insert the module into your current kernel with `sudo insmod als.ko`
 2. Install *acpi_call*:
   1. Download the source code from [here](https://github.com/mkottman/acpi_call).
   2. Extract the archive, move into the directory, and compile with `make`.
   3. Insert the module into your current kernel with `sudo insmod acpi_call.ko`
 3. Finally, build this controller:
   1. `qmake als-controller.pro -r -spec linux-g++-64`, or `qmake als-controller.pro -r -spec linux-g++` if you're on a 32-bit system.
   2. `make`
   
The generated binary file, *als-controller*, is what will monitor the light sensor. Run it and
enable/disable the light sensor by sending SIGUSR1 to the process (e.g. `killall -s SIGUSR1 als-controller`).

Better, detailed instructions coming soon
