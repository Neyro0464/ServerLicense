cmake_minimum_required(VERSION 3.15)
project(LicenseServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ============================================================
# Внешние зависимости
# ============================================================
find_package(Drogon REQUIRED)
# Замени Qt6 на Qt5, если библиотека собрана под пятую версию
find_package(Qt6 COMPONENTS Core Sql REQUIRED)
find_package(GTest QUIET)

# ============================================================
# Пути к заголовкам
# ============================================================
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Migration
    ${CMAKE_CURRENT_SOURCE_DIR}/include/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/include/database
    ${CMAKE_CURRENT_SOURCE_DIR}/include/models
)

# ============================================================
# Исходники (общие для сервера и тестов)
# ============================================================
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "include/*.h")

# ============================================================
# Динамическая библиотека LicenseLib (из папки Bin)
# ============================================================
find_library(LICENSE_LIB
    NAMES LicenseLib
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/Bin
    NO_DEFAULT_PATH
)

# ============================================================
# Основной исполняемый файл сервера
# ============================================================
add_executable(${PROJECT_NAME} main.cpp ${SRC_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
    Qt6::Core
    Qt6::Sql
    ${LICENSE_LIB}
)

# Копируем migration.json и config.json в build-директорию
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/migration.json
    ${CMAKE_CURRENT_BINARY_DIR}/migration.json COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY
)

# ============================================================
# Unit-тесты (ServerLicenseTests)
# ============================================================
if(GTest_FOUND)
    enable_testing()

    # Исходники для тестирования: только те классы, которые не зависят от Drogon HTTP.
    # Контроллеры (AuthController, LicenseController, AuthFilter) тестируются
    # интеграционно через API, а не unit-тестами здесь.
    set(TESTABLE_SRC_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/models/AppConfig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/database/DatabaseController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/models/LicenseData.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/models/LicenseValidator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Migration/AbstractConfig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Migration/JsonConfig.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Migration/QtDBMigration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Migration/SchemaVersion.cpp
    )

    file(GLOB TEST_SRC_FILES "tests/*.cpp")

    add_executable(ServerLicenseTests
        ${TEST_SRC_FILES}
        ${TESTABLE_SRC_FILES}
    )

    target_include_directories(ServerLicenseTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Migration
        ${CMAKE_CURRENT_SOURCE_DIR}/include/controllers
        ${CMAKE_CURRENT_SOURCE_DIR}/include/database
        ${CMAKE_CURRENT_SOURCE_DIR}/include/models
    )

    target_link_libraries(ServerLicenseTests PRIVATE
        GTest::GTest
        Qt6::Core
        Qt6::Sql
        ${LICENSE_LIB}
    )

    # Конфиги (config.json, migration.json) уже копируются в ${CMAKE_CURRENT_BINARY_DIR}
    # через configure_file выше — тестовый бинарник их найдёт автоматически.

    # Регистрируем тесты в CTest
    add_test(NAME ServerLicenseTests COMMAND ServerLicenseTests)

    message(STATUS "GTest found — Unit Tests target 'ServerLicenseTests' configured.")
else()
    message(WARNING
        "GTest NOT found — Unit Tests will NOT be built.\n"
        "Install with: sudo apt install libgtest-dev"
    )
endif()
