## What's this?
This is updater app for XBMC 4.0+ dashboard which is build using open-source toolchain NXDK

## How to build
- Make sure you have NXDK installed. You can follow official tutorial from [here](https://github.com/XboxDev/nxdk/wiki/Getting-Started)
- Make sure you have added `NXDK_DIR` environment variable which will point to root of NXDK repository
- Make sure you have clang v20 or above installed on your system

After everything is properly configured, you can build with following commands:
```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=${NXDK_DIR}/share/toolchain-nxdk.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Attribution
Thanks developers of NXDK
Thanks Ryzee119 for helping me with HTTPS downloads
Thanks Team Kodi for all helpers like StringUtils etc. which I borrowed from Kodi
