project(akonadiclient)

cmake_minimum_required (VERSION 3.0 FATAL_ERROR)

set(QT_MIN_VERSION "5.10.0")
set(KF5_MIN_VERSION "5.18.0")
set(AKONADI_VERSION "5.19.0")
set(AKONADI_CONTACT_VERSION "5.19.0")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

    
include(FeatureSummary)
include(ECMAddAppIcon)
include(ECMInstallIcons)
include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMAddTests)
include(ECMQtDeclareLoggingCategory)

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS -DQT_NO_URL_CAST_FROM_STRING)
#add_definitions("-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII")
add_definitions(-DTRANSLATION_DOMAIN=\"akonadiclient\")

# KDE Application Version, managed by release script
set (KDE_APPLICATIONS_VERSION_MAJOR "17")
set (KDE_APPLICATIONS_VERSION_MINOR "03")
set (KDE_APPLICATIONS_VERSION_MICRO "70")

set (KDE_APPLICATIONS_VERSION "${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}.${KDE_APPLICATIONS_VERSION_MICRO}")

find_package(KPim5Akonadi ${AKONADI_VERSION} CONFIG REQUIRED)
find_package(KPim5AkonadiContact ${AKONADI_CONTACT_VERSION} CONFIG REQUIRED)
find_package(KPim5Mime ${AKONADI_VERSION} CONFIG REQUIRED)

find_package(Qt5 ${QT_MIN_VERSION} REQUIRED NO_MODULE COMPONENTS Core Widgets Test)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n DocTools)


add_custom_target(vcsversion
  ALL
  COMMENT "Checking VCS type and version"
  VERBATIM
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/genversion.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE_APPLICATIONS_VERSION}
)


add_subdirectory( src )
add_subdirectory(doc)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
