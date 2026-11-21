cmake_minimum_required(VERSION 3.16)
project(tfield_ui C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LVGL 9.6 marca como obsoletas funciones que sí existen en la 9.3 de device-ui: se siguen usando
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wno-deprecated-declarations)
endif()

# Pruebas: -DTFIELD_CHECKS=ON compila con ASan/UBSan y LVGL con malloc del sistema y asserts
option(TFIELD_CHECKS "Compilar con detectores de errores (pruebas)" OFF)
if(TFIELD_CHECKS AND NOT EMSCRIPTEN)
  add_compile_definitions(TFIELD_CHECKS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
  add_link_options(-fsanitize=address,undefined)
endif()

# LVGL con nuestra configuración del simulador: la de third_party/lvgl si está; si no, se
# descarga la 9.6.0 al configurar. TFIELD_LVGL_DIR permite probar con otra versión
# (device-ui usa la 9.3.0).
set(TFIELD_LVGL_DIR "" CACHE PATH "Carpeta de LVGL (vacío: third_party/lvgl o descarga)")
if(NOT TFIELD_LVGL_DIR)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl/lvgl.h)
    set(TFIELD_LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lvgl)
  else()
    include(FetchContent)
    # SOURCE_SUBDIR sin CMakeLists: solo se descarga; LVGL se compila más abajo con nuestro lv_conf.h
    FetchContent_Declare(lvgl_src GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v9.6.0
                         GIT_SHALLOW TRUE SOURCE_SUBDIR sin-cmake)
    FetchContent_MakeAvailable(lvgl_src)
    set(TFIELD_LVGL_DIR ${lvgl_src_SOURCE_DIR})
  endif()
endif()
set(TFIELD_LV_CONF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/sim CACHE PATH "Carpeta con lv_conf.h")
file(GLOB_RECURSE LVGL_SRC ${TFIELD_LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SRC})
target_include_directories(lvgl PUBLIC ${TFIELD_LVGL_DIR} ${TFIELD_LV_CONF_DIR})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

# Interfaz T-Field UI (lo que se integra en device-ui)
file(GLOB TFIELD_SRC src/*.cpp fonts/*.c)
add_library(tfield STATIC ${TFIELD_SRC})
target_include_directories(tfield PUBLIC include)
target_link_libraries(tfield PUBLIC lvgl)

if(EMSCRIPTEN)
  add_executable(tfield_web sim/sim_web.cpp sim/demo.cpp sim/demo_tiles.cpp)
  target_link_libraries(tfield_web tfield)
  set_target_properties(tfield_web PROPERTIES SUFFIX ".js")
  target_link_options(tfield_web PRIVATE
    "-sEXPORTED_FUNCTIONS=['_main','_sim_fb','_sim_dirty','_sim_pointer','_sim_key','_sim_tick','_sim_last_file']"
    "-sEXPORTED_RUNTIME_METHODS=['cwrap','UTF8ToString']"
    "-sALLOW_MEMORY_GROWTH=1" "-sINITIAL_MEMORY=33554432" "-sENVIRONMENT=web"
    "-sWASM=0" "-sSINGLE_FILE=1" "-sEXIT_RUNTIME=0" "-sDYNAMIC_EXECUTION=0")
else()
  add_executable(sim_png sim/sim_png.cpp sim/demo.cpp sim/demo_tiles.cpp)
  target_link_libraries(sim_png tfield)
  # Pruebas automáticas: funciones, situaciones y prueba al azar
  file(GLOB TEST_SRC tests/*.cpp)
  if(TEST_SRC)
    add_executable(tfield_tests ${TEST_SRC} sim/demo.cpp sim/demo_tiles.cpp)
    target_include_directories(tfield_tests PRIVATE sim tests)
    target_link_libraries(tfield_tests tfield)
  endif()
endif()
