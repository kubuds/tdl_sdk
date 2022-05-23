# Copyright 2018 Bitmain Inc.
# License
# Author Yangwen Huang <yangwen.huang@bitmain.com>


if("${MIDDLEWARE_SDK_ROOT}" STREQUAL "")
  message(FATAL_ERROR "You must set MIDDLEWARE_SDK_ROOT before building IVE library.")
elseif(EXISTS "${MIDDLEWARE_SDK_ROOT}")
  message("-- Found MIDDLEWARE_SDK_ROOT (directory: ${MIDDLEWARE_SDK_ROOT})")
else()
  message(FATAL_ERROR "${MIDDLEWARE_SDK_ROOT} is not a valid folder.")
endif()

if(MIDDLEWARE_SDK_ROOT MATCHES ".*middleware/v2")
  add_definitions(-D_MIDDLEWARE_V2_)
endif()

if("${CVI_PLATFORM}" STREQUAL "CV183X")
  set(ISP_HEADER_PATH ${MIDDLEWARE_SDK_ROOT}/include/isp/cv183x/)
elseif ("${CVI_PLATFORM}" STREQUAL "CV182X")
  set(ISP_HEADER_PATH ${MIDDLEWARE_SDK_ROOT}/include/isp/cv182x/)
elseif ("${CVI_PLATFORM}" STREQUAL "MARS")
  set(ISP_HEADER_PATH ${MIDDLEWARE_SDK_ROOT}/include/isp/mars/)
endif()

set(MIDDLEWARE_INCLUDES
    ${ISP_HEADER_PATH}
    ${MIDDLEWARE_SDK_ROOT}/include/
    ${KERNEL_ROOT}/include/
)

# add cvitracer path into cflags/cxxflags
# TODO: add these in more elegant way
if("${CVI_PLATFORM}" STREQUAL "MARS")
  set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CVI_MIDDLEWARE_3RD_LDFLAGS} ${CVI_MIDDLEWARE_3RD_INCCLAGS}")
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CVI_MIDDLEWARE_3RD_LDFLAGS} ${CVI_MIDDLEWARE_3RD_INCCLAGS}")
else()
  set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CVI_MIDDLEWARE_3RD_LDFLAGS} ${CVI_MIDDLEWARE_3RD_INCCLAGS} -lcvitracer")
  set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CVI_MIDDLEWARE_3RD_LDFLAGS} ${CVI_MIDDLEWARE_3RD_INCCLAGS} -lcvitracer")
endif()
set(MIDDLEWARE_LIBS
	            ${MIDDLEWARE_SDK_ROOT}/lib/libsys.so
	            ${MIDDLEWARE_SDK_ROOT}/lib/libvpu.so)

if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "SDKRelease")
  install(DIRECTORY ${MIDDLEWARE_SDK_ROOT}/include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include/middleware)
  install(FILES ${MIDDLEWARE_LIBS} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/)
endif()
