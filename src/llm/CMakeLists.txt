# Find Python components
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Add Python module directory to global property
set_property(GLOBAL APPEND PROPERTY SRCS_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/chat_manager.py
)

# Install Python module
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/chat_manager.py
    DESTINATION ${INSTALL_DEFAULT_BINDIR}/llm
) 