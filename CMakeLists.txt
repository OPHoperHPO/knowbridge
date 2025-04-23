# --- File: CMakeLists.txt ---
cmake_minimum_required(VERSION 3.20) # As recommended for KF6

project(knowbridge LANGUAGES CXX)

# --- Version Settings ---
set(QT_MIN_VERSION "6.6.0") # KF6 generally requires Qt 6.6+
set(KF_MIN_VERSION "6.0.0")

# --- Find ECM (Extra CMake Modules) ---
find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# --- Include Standard KDE CMake Modules ---
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

# --- C++ Standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Qt Automations ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

# --- Find Qt6 ---
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
        Core
        Gui         # For QClipboard, QCursor, QIcon, QApplication
        Widgets     # For QMenu, QApplication (Widgets version)
        Network     # For QNetworkAccessManager, ApiClient
        # AccessibilityBridge # Might be needed for Qt's internal AT-SPI integration/init
)

# --- Find KDE Frameworks 6 ---
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
        CoreAddons
        GlobalAccel
        I18n
        XmlGui          # KActionCollection, KXMLGUIFactory
        Notifications
)

find_package(KF6Config ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6ConfigWidgets ${KF_MIN_VERSION} CONFIG REQUIRED)

if(TARGET KF6::Notifications)
    add_definitions(-DHAVE_KNOTIFICATIONS)
    set(KF6_Notifications_FOUND TRUE)
else()
    set(KF6_Notifications_FOUND FALSE)
    message(WARNING "KF6::Notifications not found. Notifications will be disabled.")
endif()

# --- Find AT-SPI and its Dependencies (GLib/GObject) ---
# Use PkgConfig to find the Accessibility Toolkit Service Provider Interface library
find_package(PkgConfig REQUIRED)

# Find AT-SPI itself
pkg_check_modules(ATSPI REQUIRED IMPORTED_TARGET atspi-2) # Checks for libatspi-2.pc

if (ATSPI_FOUND)
    message(STATUS "Found ATSPI: ${ATSPI_LIBRARIES}")
    add_definitions(-DHAVE_ATSPI) # Define if AT-SPI is found
    set(HAVE_ATSPI_FLAG TRUE)

    # --- ADDED: Find required dependencies for AT-SPI usage ---
    # GObject is needed for g_object_unref (used by AccessibilityHelper)
    pkg_check_modules(GOBJECT REQUIRED IMPORTED_TARGET gobject-2.0)
    if(NOT GOBJECT_FOUND)
        message(FATAL_ERROR "GObject (gobject-2.0) required for AT-SPI support but not found.")
    else()
        message(STATUS "Found GObject: ${GOBJECT_LIBRARIES}")
    endif()
    # GLib is usually a dependency of GObject, find it explicitly too for safety
    pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
    if(NOT GLIB_FOUND)
        message(FATAL_ERROR "GLib (glib-2.0) required for AT-SPI support but not found.")
    else()
        message(STATUS "Found GLib: ${GLIB_LIBRARIES}")
    endif()
    # --- END ADDED ---

else()
    message(WARNING "ATSPI (atspi-2) library not found. Text interaction features will be limited to clipboard.")
    set(HAVE_ATSPI_FLAG FALSE)
endif()

pkg_check_modules(ATK REQUIRED IMPORTED_TARGET atk) # Checks for atk.pc

if (ATK_FOUND)
    message(STATUS "Found ATK: ${ATK_LIBRARIES}")
    add_definitions(-DHAVE_ATK) # Define if ATK is found
    set(HAVE_ATSPI_FLAG TRUE)
else()
    message(WARNING "ATK (atk) library not found. Text interaction features will be limited to clipboard.")
    set(HAVE_ATSPI_FLAG FALSE)
endif()

# --- Include Directories ---
# Modern targets usually handle this, but explicit is okay.
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# Add include directories for AT-SPI and its dependencies if found
if(ATSPI_FOUND)
    include_directories(${ATSPI_INCLUDE_DIRS})
    include_directories(${ATK_INCLUDE_DIRS})
    include_directories(${GOBJECT_INCLUDE_DIRS}) # Added
    include_directories(${GLIB_INCLUDE_DIRS})    # Added
endif()

# --- Define the executable ---
add_executable(knowbridge) # Sources added below

# --- Specify Sources ---
target_sources(knowbridge PRIVATE
        src/main.cpp
        src/ApiClient.cpp
        src/BackgroundProcessor.cpp
        src/AccessibilityHelper.cpp
        src/ConfigManager.cpp          # NEW
        src/SettingsDialog.cpp         # NEW
        src/AccessibilityHelper.h
        src/ConfigManager.h            # NEW
        src/ActionEditorDialog.cpp
        src/ActionEditorDialog.h
        src/SettingsDialog.h)          # NEW

# --- Link Libraries ---
# Use the modern Namespace::Component syntax
set(KDEOpenAI_LINK_LIBS
        # Qt Libraries
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        # Qt6::AccessibilityBridge # Link if needed

        # KDE Frameworks Libraries (non-optional)
        KF6::CoreAddons
        KF6::GlobalAccel
        KF6::I18n
        KF6::XmlGui

        KF6::ConfigCore
        KF6::ConfigGui
        KF6::ConfigWidgets

        # Conditional KF6 Libs
        $<$<BOOL:${KF6_Notifications_FOUND}>:KF6::Notifications>

        # Conditional AT-SPI and its Dependencies
        # Use IMPORTED_TARGETS defined by pkg_check_modules
        $<$<BOOL:${ATSPI_FOUND}>:PkgConfig::ATSPI>
        $<$<BOOL:${ATSPI_FOUND}>:PkgConfig::ATK>
        $<$<BOOL:${ATSPI_FOUND}>:PkgConfig::GOBJECT> # <-- ADDED Link GObject
        $<$<BOOL:${ATSPI_FOUND}>:PkgConfig::GLIB>    # <-- ADDED Link GLib
)

# --- tranlations ---------------------------------------------------------------
add_subdirectory(po)
add_dependencies(update-po knowbridge)   # run xgettext/msgmerge after the exe
ki18n_install(po)

# --- Link Libraries to Target ---
target_link_libraries(knowbridge PRIVATE ${KDEOpenAI_LINK_LIBS})



# --- Installation ---

install(TARGETS knowbridge ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

configure_file(assets/knowbridge.desktop.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/knowbridge.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/knowbridge.desktop
        DESTINATION ${KDE_INSTALL_APPDIR})

if(DEFINED KDE_INSTALL_KNOTIFYRCDIR)
    message(STATUS "Installing notifyrc to ${KDE_INSTALL_KNOTIFYRCDIR}")
    install(FILES assets/knowbridge.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
else()
    message(WARNING "KDE_INSTALL_KNOTIFYRCDIR is not defined. Skipping installation of notifyrc file.")
endif()

# --- Feature Summary ---
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)