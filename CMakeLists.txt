cmake_minimum_required(VERSION 3.20)
project(AndyKit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(AK_BUILD_PLUGIN "Build the OpenFX plugin" ON)
option(AK_BUILD_TOOLS  "Build the ak_render command line tool" ON)

include(FetchContent)

# ---- engine (host independent) ----
add_library(andykit_core STATIC
  src/core/Engine.cpp
  src/core/Params.cpp
  src/core/Presets.cpp)
target_include_directories(andykit_core PUBLIC src)
set_target_properties(andykit_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_package(Threads REQUIRED)
target_link_libraries(andykit_core PUBLIC Threads::Threads)
if(MSVC)
  target_compile_options(andykit_core PRIVATE /O2 /fp:fast /W3)
else()
  target_compile_options(andykit_core PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter)
endif()

# ---- OpenFX plugin ----
if(AK_BUILD_PLUGIN)
  FetchContent_Declare(openfx
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
    GIT_TAG        OFX_Release_1.5.1
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  do-not-add)          # headers + support sources only
  FetchContent_MakeAvailable(openfx)
  set(OFX ${openfx_SOURCE_DIR})

  add_library(AndyKit MODULE
    src/ofx/AndyKitPlugin.cpp
    ${OFX}/Support/Library/ofxsCore.cpp
    ${OFX}/Support/Library/ofxsImageEffect.cpp
    ${OFX}/Support/Library/ofxsInteract.cpp
    ${OFX}/Support/Library/ofxsLog.cpp
    ${OFX}/Support/Library/ofxsMultiThread.cpp
    ${OFX}/Support/Library/ofxsParams.cpp
    ${OFX}/Support/Library/ofxsProperty.cpp
    ${OFX}/Support/Library/ofxsPropertyValidation.cpp)
  target_include_directories(AndyKit PRIVATE ${OFX}/include ${OFX}/Support/include ${OFX}/Support/Library)
  target_link_libraries(AndyKit PRIVATE andykit_core)
  target_compile_definitions(AndyKit PRIVATE AK_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} AK_VERSION_MINOR=${PROJECT_VERSION_MINOR})
  if(WIN32)
    target_compile_definitions(AndyKit PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
  endif()
  set_target_properties(AndyKit PROPERTIES PREFIX "" SUFFIX ".ofx")

  # OFX bundle layout: AndyKit.ofx.bundle/Contents/<arch>/AndyKit.ofx
  if(APPLE)
    set(AK_ARCH_DIR MacOS)
    target_link_options(AndyKit PRIVATE "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/src/ofx/exports.macos")
  elseif(WIN32)
    set(AK_ARCH_DIR Win64)
  else()
    set(AK_ARCH_DIR Linux-x86-64)
    target_link_options(AndyKit PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/ofx/exports.linux" -static-libstdc++ -static-libgcc)
  endif()
  set(AK_BUNDLE ${CMAKE_BINARY_DIR}/bundle/AndyKit.ofx.bundle)
  foreach(cfg "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    set_target_properties(AndyKit PROPERTIES LIBRARY_OUTPUT_DIRECTORY${cfg} ${AK_BUNDLE}/Contents/${AK_ARCH_DIR})
  endforeach()
  configure_file(src/ofx/Info.plist.in ${AK_BUNDLE}/Contents/Info.plist @ONLY)
  install(DIRECTORY ${AK_BUNDLE} DESTINATION .)
endif()

# ---- command line renderer (development / CI) ----
if(AK_BUILD_TOOLS)
  FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5
    SOURCE_SUBDIR  do-not-add)
  FetchContent_MakeAvailable(stb)
  add_executable(ak_render tools/ak_render.cpp)
  target_include_directories(ak_render PRIVATE ${stb_SOURCE_DIR})
  target_link_libraries(ak_render PRIVATE andykit_core)
endif()

# ---- smoke test: load the bundle in a minimal OpenFX host and render frames ----
option(AK_BUILD_TESTS "Build the OpenFX smoke-test host" OFF)
if(AK_BUILD_TESTS AND AK_BUILD_PLUGIN AND AK_BUILD_TOOLS)
  find_package(EXPAT REQUIRED)
  file(GLOB OFXHOST_SOURCES ${OFX}/HostSupport/src/*.cpp)
  add_executable(smoke_host
    tests/ofxhost/smoke_host.cpp
    tests/ofxhost/hostDemoClipInstance.cpp
    tests/ofxhost/hostDemoEffectInstance.cpp
    tests/ofxhost/hostDemoHostDescriptor.cpp
    ${OFXHOST_SOURCES})
  target_include_directories(smoke_host PRIVATE tests/ofxhost ${OFX}/include ${OFX}/HostSupport/include ${stb_SOURCE_DIR})
  target_link_libraries(smoke_host PRIVATE EXPAT::EXPAT ${CMAKE_DL_LIBS})
  add_dependencies(smoke_host AndyKit)
endif()
