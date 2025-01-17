cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

project(akonadiclient)
set(RELEASE_SERVICE_VERSION_MAJOR "24")
set(RELEASE_SERVICE_VERSION_MINOR "11")
set(RELEASE_SERVICE_VERSION_MICRO "70")
set(RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.2.3")
set(AKONADI_VERSION "6.1.0")
set(AKONADI_CONTACT_VERSION "6.1.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(FeatureSummary)
include(ECMAddAppIcon)
include(ECMInstallIcons)
include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMAddTests)
include(ECMQtDeclareLoggingCategory)
include(ECMDeprecationSettings)
include(KDEGitCommitHooks)
include(KDEClangFormat)

configure_file(config-akonadiclient.h.in ${CMAKE_CURRENT_BINARY_DIR}/config-akonadiclient.h)

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_USE_FAST_OPERATOR_PLUS -DQT_NO_URL_CAST_FROM_STRING)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_TO_ASCII)
add_definitions(-DTRANSLATION_DOMAIN=\"akonadiclient\")

find_package(KPim6Akonadi ${AKONADI_VERSION} CONFIG REQUIRED)
find_package(KPim6AkonadiContactCore ${AKONADI_CONTACT_VERSION} CONFIG REQUIRED)
find_package(KPim6Mime ${AKONADI_VERSION} CONFIG REQUIRED)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED NO_MODULE COMPONENTS Core Widgets Test)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n)

# Only 'ncurses' includes terminfo support, not classic 'curses'
set(CURSES_NEED_NCURSES TRUE)
find_package(Curses)
if (Curses_FOUND)
  set(HAVE_TERMINFO TRUE)
endif ()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
ecm_set_disabled_deprecation_versions(QT 6.8.1
    KF 6.10.0
   )

find_package(KF6DocTools ${KF_MIN_VERSION})
set_package_properties(KF6DocTools PROPERTIES DESCRIPTION
    "Tools to generate documentation"
    TYPE OPTIONAL
)


add_subdirectory(src)
ki18n_install(po)
if(KF6DocTools_FOUND)
   kdoctools_install(po)
   add_subdirectory(doc)
endif()
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
