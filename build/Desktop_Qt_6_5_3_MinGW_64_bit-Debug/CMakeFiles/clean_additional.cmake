# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\SmartCampusSecondhandTradingSystemServer_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SmartCampusSecondhandTradingSystemServer_autogen.dir\\ParseCache.txt"
  "SmartCampusSecondhandTradingSystemServer_autogen"
  )
endif()
