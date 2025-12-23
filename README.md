# Sensirion SDP8XX Zephyr driver
An out-of-tree Zephyr driver for Sensirion's SDP8xx differential pressure sensors  
Tested on SDP810-500Pa.

## Note
This is a work-in-progress and only single triggered mode measurement is implemented. Feel free to contribute.
Datasheet: https://sensirion.com/media/documents/90500156/6167E43B/Sensirion_Differential_Pressure_Datasheet_SDP8xx_Digital.pdf

## Usage
Copy the repo, or add it as submodule, to a directory in your main project, for example ``external/sdp8xx_driver``  
And include it in you CMAKE
```cmake
list(APPEND EXTRA_ZEPHYR_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/external/sdp8xx_driver/
)
```

See ``samples/`` for usage

