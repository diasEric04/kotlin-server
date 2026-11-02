cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_COMPILER "C:\\msys64\\mingw64\\bin\\g++.exe")
set(CMAKE_C_COMPILER "C:\\msys64\\mingw64\\bin\\gcc.exe")
set(CMAKE_PREFIX_PATH "C:\\Qt\\6.9.1\\mingw_64")
project(TESTE LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OpenGL_GL_PREFERENCE LEGACY)

find_package(OpenGL REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network WebSockets Test)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${OpenGL_INCLUDE_DIRS} ${Qt6Core_INCLUDE_DIRS} ${Qt6Gui_INCLUDE_DIRS} ${Qt6Widgets_INCLUDE_DIRS})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

# Defina os arquivos .ui
# set(UI_FILES ${CMAKE_SOURCE_DIR}/ui/main_window.ui)

# Defina o diretório de saída para o cabeçalho gerado
# set(CMAKE_UI_HEADER_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated_ui)

# Gerar os cabeçalhos .h a partir do .ui
# qt_wrap_ui(UI_HEADERS ${UI_FILES})

set(HEADERS
    include/ApiServer.hpp
    include/MobileServer.hpp
)

# Defina os arquivos fontes do projeto
set(SOURCES
    # ${UI_HEADERS}  # Incluindo o arquivo gerado pelo uic
    src/ApiServer.cpp
    src/MobileServer.cpp
)

# Adicione o executável
add_executable(main ${SOURCES} ${HEADERS} main.cpp)

# Comando para mover o arquivo gerado ui_main_window.h para a pasta 'ui'
# add_custom_command(
#     TARGET main POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/ui_main_window.h ${CMAKE_SOURCE_DIR}/ui
# )

target_include_directories(main PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(main PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui Qt6::Network Qt6::WebSockets OpenGL::GL)

# # Ativar geração de testes
# enable_testing()

# # Arquivos do teste
# set(TEST_SOURCES
#     tests/test_calculator_ui.cpp  # Exemplo de arquivo de teste
# )

# # Adiciona o executável de teste
# add_executable(run_tests ${TEST_SOURCES} ${HEADERS} ${SOURCES})

# # Inclui diretórios necessários
# target_include_directories(run_tests PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/src
#     ${CMAKE_CURRENT_BINARY_DIR}
# )

# # Linka com Qt Test e outras dependências se necessário
# target_link_libraries(run_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui Qt6::Widgets OpenGL::GL Testes::testes)

# # Registra o teste para uso com ctest
# add_test(NAME run_tests COMMAND run_tests)