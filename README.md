# Sensirion SDP8XX Zephyr driver
An out-of-tree Zephyr driver for Sensirion's SDP8xx differential pressure sensors  
Tested on SDP810-500Pa.

## Note
This is a work-in-progress and only triggered mode measurement is implemented. Feel free to contribute.

## Usage
Copy the working tree to a directory in your main project, for example ``modules`` or ``external``  
And include it in you CMAKE
```cmake
list(APPEND EXTRA_ZEPHYR_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/modules/
)
```

See ``samples/`` for usage

