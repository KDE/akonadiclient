project(akonadiclient)

cmake_minimum_required(VERSION 3.0 FATAL_ERROR)

set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.2.0")
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

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_USE_FAST_OPERATOR_PLUS -DQT_NO_URL_CAST_FROM_STRING)
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
remove_definitions(-DQT_NO_CAST_TO_ASCII)
add_definitions(-DTRANSLATION_DOMAIN=\"akonadiclient\")

# KDE Application Version, managed by release script
set(KDE_APPLICATIONS_VERSION_MAJOR "24")
set(KDE_APPLICATIONS_VERSION_MINOR "07")
set(KDE_APPLICATIONS_VERSION_MICRO "70")

set(KDE_APPLICATIONS_VERSION "${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}.${KDE_APPLICATIONS_VERSION_MICRO}")

find_package(KPim6Akonadi ${AKONADI_VERSION} CONFIG REQUIRED)
find_package(KPim6AkonadiContactCore ${AKONADI_CONTACT_VERSION} CONFIG REQUIRED)
find_package(KPim6Mime ${AKONADI_VERSION} CONFIG REQUIRED)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED NO_MODULE COMPONENTS Core Widgets Test)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons I18n DocTools)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
ecm_set_disabled_deprecation_versions(QT 6.7
    KF 6.3.0
   )
add_custom_target(vcsversion
  ALL
  COMMENT "Checking VCS type and version"
  VERBATIM
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/genversion.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE_APPLICATIONS_VERSION}
)


add_subdirectory(src)
add_subdirectory(doc)
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
