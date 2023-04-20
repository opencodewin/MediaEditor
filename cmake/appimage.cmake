function(make_appimage)
	set(optional)
	set(args PROJECT_DIR EXE NAME OUTPUT_NAME DESKTOP_SRC ICON_SRC RESOURCE_FILES)
	set(list_args ASSETS)
	cmake_parse_arguments(
		PARSE_ARGV 0
		ARGS
		"${optional}"
		"${args}"
		"${list_args}"
	)

	if(${ARGS_UNPARSED_ARGUMENTS})
		message(WARNING "Unparsed arguments: ${ARGS_UNPARSED_ARGUMENTS}")
	endif()


    # download linuxdeploy if needed (TODO: non-x86 build machine?)
    SET(LINUX_DEPLOY_PATH "${ARGS_PROJECT_DIR}/cmake/linuxdeploy-x86_64.AppImage" CACHE INTERNAL "")
    if (NOT EXISTS "${LINUX_DEPLOY_PATH}")
        #file(DOWNLOAD https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage "${LINUX_DEPLOY_PATH}")
        execute_process(COMMAND wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage ${LINUX_DEPLOY_PATH})
        execute_process(COMMAND chmod +x ${LINUX_DEPLOY_PATH})
    endif()

    # make the AppDir
    set(APPDIR "${CMAKE_BINARY_DIR}/AppDir")
    file(REMOVE_RECURSE "${APPDIR}")       # remove if leftover
    file(MAKE_DIRECTORY "${APPDIR}")

    # copy resource files
    file(COPY "${ARGS_RESOURCE_FILES}" DESTINATION "${APPDIR}/usr/languages/")
    file(COPY "${ARGS_ICON_SRC}" DESTINATION "${APPDIR}/usr/resources/")
    #file(COPY "${ARGS_PROJECT_DIR}/xxx" DESTINATION "${APPDIR}/usr")

    # prepare desktop and icon file
    file(COPY "${ARGS_DESKTOP_SRC}" DESTINATION "${CMAKE_BINARY_DIR}")
    get_filename_component(TEMP "${ARGS_DESKTOP_SRC}" NAME)
    set(DESKTOP_FILE "${CMAKE_BINARY_DIR}/${ARGS_EXE}.desktop")
    file(RENAME "${CMAKE_BINARY_DIR}/${TEMP}" "${DESKTOP_FILE}")
    file(COPY "${ARGS_ICON_SRC}" DESTINATION "${CMAKE_BINARY_DIR}")
    get_filename_component(TEMP "${ARGS_ICON_SRC}" NAME)
    get_filename_component(ICON_EXT "${ARGS_ICON_SRC}" EXT)
    set(ICON_FILE "${CMAKE_BINARY_DIR}/${ARGS_EXE}${ICON_EXT}")
    file(RENAME "${CMAKE_BINARY_DIR}/${TEMP}" "${ICON_FILE}")

    # use linuxdeploy to generate the appimage
    set(ENV{OUTPUT} "${ARGS_OUTPUT_NAME}.AppImage")
    execute_process(COMMAND ${LINUX_DEPLOY_PATH} --appdir ${APPDIR} -e ${ARGS_EXE} -d ${DESKTOP_FILE} -i ${ICON_FILE} --output appimage)

endfunction()
