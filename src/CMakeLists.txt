###############################################################################
# Add the source subdirectories
add_subdirectory(applications)
add_subdirectory(dsp)
add_subdirectory(interfaces)
add_subdirectory(qtgui)
add_subdirectory(receivers)
add_subdirectory(llm)

# Add src directory to include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/qtgui)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    if(${OSX_AUDIO_BACKEND} MATCHES "Portaudio")
        add_subdirectory(portaudio)
    else()
        add_subdirectory(osxaudio)
    endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD")
    if(${LINUX_AUDIO_BACKEND} MATCHES "Pulseaudio")
        add_subdirectory(pulseaudio)
    elseif(${LINUX_AUDIO_BACKEND} MATCHES "Portaudio")
        add_subdirectory(portaudio)
    endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
endif()

###############################################################################
# bring in the global properties
get_property(${PROJECT_NAME}_SOURCE GLOBAL PROPERTY SRCS_LIST)
get_property(${PROJECT_NAME}_UI_SOURCE GLOBAL PROPERTY UI_SRCS_LIST)

###############################################################################
# Process the UI files
if(Qt6_FOUND)
    QT_WRAP_UI(UIS_HDRS
        ${${PROJECT_NAME}_UI_SOURCE}
    )
else()
    QT5_WRAP_UI(UIS_HDRS
        ${${PROJECT_NAME}_UI_SOURCE}
    )
endif()

###############################################################################
# Process the resources
if(Qt6_FOUND)
    QT_ADD_RESOURCES(RESOURCES_LIST
        ../resources/icons.qrc
        ../resources/textfiles.qrc
    )
else()
    QT5_ADD_RESOURCES(RESOURCES_LIST
        ../resources/icons.qrc
        ../resources/textfiles.qrc
    )
endif()

###############################################################################
# Resource file - adds an icon to GQRX executable
if (WIN32)
    set(ICON_SOURCE ${CMAKE_SOURCE_DIR}/resources/icons/gqrx.ico)
    set(RES_FILES "${CMAKE_CURRENT_BINARY_DIR}/gqrx.rc")
    file(WRITE "${RES_FILES}" "id ICON \"${ICON_SOURCE}\"")
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
    list(APPEND RESOURCES_LIST ${RES_FILES})
endif(WIN32)

###############################################################################
# Build the program
add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCE} ${UIS_HDRS} ${RESOURCES_LIST})
if(Qt6_FOUND)
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
else()
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)
endif()
# The pulse libraries are only needed on Linux. On other platforms they will
# not be found, so having them here is fine.

if(Qt6_FOUND)
    target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Network
        Qt6::Widgets
        Qt6::Svg
        Qt6::SvgWidgets
        Qt6::WebEngineWidgets
        Qt6::Sql
    )
else()
    target_link_libraries(${PROJECT_NAME}
        Qt5::Core
        Qt5::Network
        Qt5::Widgets
        Qt5::Svg
        Qt5::WebEngineWidgets
        Qt5::Sql
    )
endif()

target_link_libraries(${PROJECT_NAME}
    ${GNURADIO_OSMOSDR_LIBRARIES}
    ${PULSEAUDIO_LIBRARY}
    ${PULSE-SIMPLE}
    ${PORTAUDIO_LIBRARIES}
)

if(NOT Gnuradio_VERSION VERSION_LESS "3.10")
    target_link_libraries(${PROJECT_NAME}
        gnuradio::gnuradio-analog
        gnuradio::gnuradio-blocks
        gnuradio::gnuradio-digital
        gnuradio::gnuradio-filter
        gnuradio::gnuradio-network
        gnuradio::gnuradio-audio
        Volk::volk
    )
else()
    target_link_libraries(${PROJECT_NAME}
        gnuradio::gnuradio-analog
        gnuradio::gnuradio-blocks
        gnuradio::gnuradio-digital
        gnuradio::gnuradio-filter
        gnuradio::gnuradio-audio
        Volk::volk
    )
endif()

#build a win32 app, not a console app
if (WIN32)
    if (MSVC)
        set(CMAKE_EXE_LINKER_FLAGS "/entry:mainCRTStartup ${CMAKE_EXE_LINKER_FLAGS}")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "-Wl,-e,mainCRTStartup ${CMAKE_EXE_LINKER_FLAGS}")
    endif(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON)
endif (WIN32)

set(INSTALL_DEFAULT_BINDIR "bin" CACHE STRING "Appended to CMAKE_INSTALL_PREFIX")
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${INSTALL_DEFAULT_BINDIR})
