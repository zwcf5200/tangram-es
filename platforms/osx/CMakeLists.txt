cmake_minimum_required(VERSION 3.0)

include(../../toolchains/utils.cmake)

check_unsupported_compiler_version()

add_definitions(-DTANGRAM_OSX)

# Build core library with dependencies.
add_subdirectory(../../core ./core)

if(TANGRAM_APPLICATION)

  if($ENV{CIRCLE_BUILD_NUM})
    add_definitions(-DBUILD_NUM_STRING="\($ENV{CIRCLE_BUILD_NUM}\)")
  endif()

  find_package(OpenGL REQUIRED)

  # Build GLFW.
  if(TANGRAM_USE_SYSTEM_GLFW_LIBS)
    include(FindPkgConfig)
    pkg_check_modules(GLFW REQUIRED glfw3)
  else()
    # configure GLFW to build only the library
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
    set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
    add_subdirectory(${PROJECT_SOURCE_DIR}/platforms/common/glfw)
  endif()

  add_bundle_resources(RESOURCES "${PROJECT_SOURCE_DIR}/scenes" "Resources")

  add_executable(tangram
    MACOSX_BUNDLE
    src/main.mm
    src/osxPlatform.mm
    ../common/platform_gl.cpp
    ../common/glfwApp.cpp
    ../common/appleAllowedFonts.mm
    ${RESOURCES}
  )

  target_include_directories(tangram
    PUBLIC
    ${GLFW_SOURCE_DIR}/include
    ../common
  )

  target_link_libraries(tangram
    tangram-core
    glfw
    ${GLFW_LIBRARIES}
    ${OPENGL_LIBRARIES}
  )

  target_compile_options(tangram
    -std=c++1y
    -stdlib=libc++
    -Wall
    -Wreturn-type
    -Wsign-compare
    -Wignored-qualifiers
    -Wtype-limits
    -Wmissing-field-initializers
  )

  # add resource files and property list
  set_target_properties(tangram PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/platforms/osx/Info.plist
  )

endif()
