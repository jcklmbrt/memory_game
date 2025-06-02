cmake_minimum_required(VERSION 3.5.0)
project(memory_game VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)

add_executable(memory_game)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_include_directories(memory_game PUBLIC src . 3rdparty/stb)

add_subdirectory(3rdparty/sdl2 EXCLUDE_FROM_ALL)
target_include_directories(memory_game PUBLIC 3rdparty/sdl2/include)
if(WIN32)
	target_link_libraries(memory_game SDL2::SDL2-static SDL2::SDL2main)
else()
	target_link_libraries(memory_game m SDL2::SDL2-static)
endif()

target_sources(memory_game PRIVATE 
	"src/main.c"
	"src/game.c")
