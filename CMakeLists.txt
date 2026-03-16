cmake_minimum_required(VERSION 3.15)
project(LicenseServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Ищем фреймворки
find_package(Drogon REQUIRED)
# Замени Qt6 на Qt5, если библиотека собрана под пятую версию
find_package(Qt6 COMPONENTS Core Sql REQUIRED)

# Указываем путь к заголовочным файлам твоей библиотеки
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Migration
)

file(GLOB_RECURSE SRC_FILES "src/*.cpp" "include/*.h")
add_executable(${PROJECT_NAME} main.cpp ${SRC_FILES})

# Copy migration config to the build directory so it is available at runtime
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/migration.json ${CMAKE_CURRENT_BINARY_DIR}/migration.json COPYONLY)

# Ищем твою динамическую библиотеку в папке Bin
find_library(LICENSE_LIB
    NAMES LicenseLib
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/Bin
    NO_DEFAULT_PATH
)


# Линкуем Drogon, Qt и твою библиотеку
target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
    Qt6::Core
    Qt6::Sql
    ${LICENSE_LIB}
)
