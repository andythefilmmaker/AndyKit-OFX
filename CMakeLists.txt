cmake_minimum_required(VERSION 3.20)
project(Linework VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(LW_BUILD_PLUGIN "Build the OpenFX plugin" ON)
option(LW_BUILD_TOOLS  "Build the lw_render command line tool" ON)

include(FetchContent)

# ---- engine (host independent) ----
add_library(linework_core STATIC
  src/core/Engine.cpp
  src/core/Params.cpp
  src/core/Presets.cpp)
target_include_directories(linework_core PUBLIC src)
set_target_properties(linework_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_package(Threads REQUIRED)
target_link_libraries(linework_core PUBLIC Threads::Threads)
if(MSVC)
  target_compile_options(linework_core PRIVATE /O2 /fp:fast /W3)
else()
  target_compile_options(linework_core PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter)
endif()

# ---- OpenFX plugin ----
if(LW_BUILD_PLUGIN)
  FetchContent_Declare(openfx
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
    GIT_TAG        OFX_Release_1.5.1
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  do-not-add)          # headers + support sources only
  FetchContent_MakeAvailable(openfx)
  set(OFX ${openfx_SOURCE_DIR})

  add_library(Linework MODULE
    src/ofx/LineworkPlugin.cpp
    ${OFX}/Support/Library/ofxsCore.cpp
    ${OFX}/Support/Library/ofxsImageEffect.cpp
    ${OFX}/Support/Library/ofxsInteract.cpp
    ${OFX}/Support/Library/ofxsLog.cpp
    ${OFX}/Support/Library/ofxsMultiThread.cpp
    ${OFX}/Support/Library/ofxsParams.cpp
    ${OFX}/Support/Library/ofxsProperty.cpp
    ${OFX}/Support/Library/ofxsPropertyValidation.cpp)
  target_include_directories(Linework PRIVATE ${OFX}/include ${OFX}/Support/include ${OFX}/Support/Library)
  target_link_libraries(Linework PRIVATE linework_core)
  target_compile_definitions(Linework PRIVATE LW_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} LW_VERSION_MINOR=${PROJECT_VERSION_MINOR})
  if(WIN32)
    target_compile_definitions(Linework PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
  endif()
  set_target_properties(Linework PROPERTIES PREFIX "" SUFFIX ".ofx")

  # OFX bundle layout: Linework.ofx.bundle/Contents/<arch>/Linework.ofx
  if(APPLE)
    set(LW_ARCH_DIR MacOS)
    target_link_options(Linework PRIVATE "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/src/ofx/exports.macos")
  elseif(WIN32)
    set(LW_ARCH_DIR Win64)
  else()
    set(LW_ARCH_DIR Linux-x86-64)
    target_link_options(Linework PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/ofx/exports.linux" -static-libstdc++ -static-libgcc)
  endif()
  set(LW_BUNDLE ${CMAKE_BINARY_DIR}/bundle/Linework.ofx.bundle)
  foreach(cfg "" _DEBUG _RELEASE _RELWITHDEBINFO _MINSIZEREL)
    set_target_properties(Linework PROPERTIES LIBRARY_OUTPUT_DIRECTORY${cfg} ${LW_BUNDLE}/Contents/${LW_ARCH_DIR})
  endforeach()
  configure_file(src/ofx/Info.plist.in ${LW_BUNDLE}/Contents/Info.plist @ONLY)
  install(DIRECTORY ${LW_BUNDLE} DESTINATION .)
endif()

# ---- command line renderer (development / CI) ----
if(LW_BUILD_TOOLS)
  FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5
    SOURCE_SUBDIR  do-not-add)
  FetchContent_MakeAvailable(stb)
  add_executable(lw_render tools/lw_render.cpp)
  target_include_directories(lw_render PRIVATE ${stb_SOURCE_DIR})
  target_link_libraries(lw_render PRIVATE linework_core)
endif()

# ---- smoke test: load the bundle in a minimal OpenFX host and render frames ----
option(LW_BUILD_TESTS "Build the OpenFX smoke-test host" OFF)
if(LW_BUILD_TESTS AND LW_BUILD_PLUGIN AND LW_BUILD_TOOLS)
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
  add_dependencies(smoke_host Linework)
endif()
