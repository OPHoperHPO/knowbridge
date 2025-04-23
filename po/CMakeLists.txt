# po/CMakeLists.txt

# Locate the tools
find_program(XGETTEXT xgettext REQUIRED)
find_program(MSGMERGE  msgmerge  REQUIRED)

# Define POT file path
set(POTFILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pot)

# Generate POTFILES.in from actual sources
file(GLOB_RECURSE ALL_SOURCES
        RELATIVE ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/assets/*.desktop.cmake
)

# Generate POTFILES.in content
set(POTFILES_IN "${CMAKE_CURRENT_BINARY_DIR}/POTFILES.in")
file(WRITE ${POTFILES_IN} "")
foreach(f ${ALL_SOURCES})
    file(APPEND ${POTFILES_IN} "${f}\n")
endforeach()

# Custom command to generate .pot
add_custom_command(
        OUTPUT ${POTFILE}
        COMMAND ${XGETTEXT}
        --keyword=i18n --keyword=ki18n --from-code=UTF-8
        --output=${POTFILE}
        --files-from=${POTFILES_IN}
        DEPENDS ${POTFILES_IN}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating translation template: ${PROJECT_NAME}.pot"
)

# Target to generate pot file
add_custom_target(${PROJECT_NAME}-pot ALL
        DEPENDS ${POTFILE}
)

# Merge step for each .po file
file(GLOB POFILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.po)
foreach(PO ${POFILES})
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${PO}
            COMMAND ${MSGMERGE}
            --update --backup=none
            ${CMAKE_CURRENT_SOURCE_DIR}/${PO}
            ${POTFILE}
            DEPENDS ${POTFILE}
            COMMENT "Merging template into ${PO}"
    )
endforeach()

# Update target
add_custom_target(update-po ALL
        DEPENDS ${POFILES}
)
