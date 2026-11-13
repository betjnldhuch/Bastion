# Bastion UI для ПК: HTML-окна поверх GTA SA / SA-MP через CEF (Chromium Embedded).
# Собирается только 32-битным MSVC:  cmake -A Win32 -DCEF_ROOT=<папка cef_binary_*_windows32_minimal>
cmake_minimum_required(VERSION 3.21)
project(bastion_ui CXX C)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "GTA SA 32-битная: собирай с -A Win32")
endif ()
if (NOT CEF_ROOT)
  message(FATAL_ERROR "Укажи -DCEF_ROOT=<папка CEF binary distribution windows32>")
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CONFIGURATION_TYPES Release)

# --- CEF: libcef_dll_wrapper из дистрибутива ---
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# --- MinHook (перехват функций), RakNet BitStream из RakHook (оба MIT) ---
include(FetchContent)
FetchContent_Declare(minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG v1.3.4)
FetchContent_Declare(rakhook
  GIT_REPOSITORY https://github.com/imring/RakHook.git
  GIT_TAG 5be94a18d338a2d962c13c3d8a1403c9d7135db8)
FetchContent_GetProperties(minhook)
if (NOT minhook_POPULATED)
  FetchContent_Populate(minhook)
endif ()
FetchContent_GetProperties(rakhook)
if (NOT rakhook_POPULATED)
  FetchContent_Populate(rakhook)
endif ()

add_library(minhook_static STATIC
  ${minhook_SOURCE_DIR}/src/buffer.c
  ${minhook_SOURCE_DIR}/src/hook.c
  ${minhook_SOURCE_DIR}/src/trampoline.c
  ${minhook_SOURCE_DIR}/src/hde/hde32.c)
target_include_directories(minhook_static PUBLIC ${minhook_SOURCE_DIR}/include)
set_property(TARGET minhook_static PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(raknet_bs STATIC
  ${rakhook_SOURCE_DIR}/src/RakNet/BitStream.cpp)
target_include_directories(raknet_bs PUBLIC ${rakhook_SOURCE_DIR}/include)
set_property(TARGET raknet_bs PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

# --- bastion_ui.asi: плагин, который грузит ASI Loader в gta_sa.exe ---
add_library(bastion_ui SHARED
  src/plugin.cpp
  src/samp.cpp
  src/browser.cpp
  src/overlay.cpp
  src/text.cpp)
SET_LIBRARY_TARGET_PROPERTIES(bastion_ui)
target_compile_options(bastion_ui PRIVATE /WX- /utf-8)
add_dependencies(bastion_ui libcef_dll_wrapper)
target_include_directories(bastion_ui PRIVATE ${CEF_INCLUDE_PATH})
target_link_libraries(bastion_ui PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS}
  "${CEF_BINARY_DIR_RELEASE}/libcef.lib" minhook_static raknet_bs d3d9.lib delayimp.lib)
# libcef.dll лежит в папке bastion_ui\ рядом с игрой: грузим его сами после SetDllDirectory
target_link_options(bastion_ui PRIVATE /DELAYLOAD:libcef.dll)
set_target_properties(bastion_ui PROPERTIES SUFFIX ".asi" OUTPUT_NAME "bastion_ui")

# --- bastion_cef.exe: процессы Chromium (рендер, GPU, сеть) ---
add_executable(bastion_cef WIN32 src/subprocess.cpp src/bastion_cef.manifest)
SET_EXECUTABLE_TARGET_PROPERTIES(bastion_cef)
target_compile_options(bastion_cef PRIVATE /WX- /utf-8)
add_dependencies(bastion_cef libcef_dll_wrapper)
target_include_directories(bastion_cef PRIVATE ${CEF_INCLUDE_PATH})
target_link_libraries(bastion_cef PRIVATE libcef_dll_wrapper ${CEF_STANDARD_LIBS}
  "${CEF_BINARY_DIR_RELEASE}/libcef.lib")
