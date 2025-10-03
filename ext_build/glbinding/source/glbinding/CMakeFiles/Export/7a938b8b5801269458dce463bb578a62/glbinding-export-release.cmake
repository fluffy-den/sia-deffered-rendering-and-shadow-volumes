#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "glbinding::glbinding" for configuration "Release"
set_property(TARGET glbinding::glbinding APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(glbinding::glbinding PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libglbinding.a"
  )

list(APPEND _cmake_import_check_targets glbinding::glbinding )
list(APPEND _cmake_import_check_files_for_glbinding::glbinding "${_IMPORT_PREFIX}/lib/libglbinding.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
