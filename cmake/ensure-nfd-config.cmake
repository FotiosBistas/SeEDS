if(NOT EXISTS "${NFD_CONFIG}")
    message(STATUS "Creating NFD configuration: ${NFD_CONFIG}")

    file(COPY_FILE
        "${NFD_CONFIG_SAMPLE}"
        "${NFD_CONFIG}"
    )
endif()
