cmake_minimum_required(VERSION 3.16)

project(game_2048 VERSION 1.0.0 LANGUAGES C)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# prevent installing to system directories. 
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

if ((APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin") OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")    # Disable shared builds on platforms where it does not make sense to use them
    set(SDL_SHARED OFF)
else()
    set(SDL_SHARED ON)
endif()

if(MSVC)
    if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
        add_definitions(/MP)				# parallelize each target, unless Ninja is the generator
    endif()
endif()

# Set the name of the executable
set(EXECUTABLE_NAME ${PROJECT_NAME})

# Create an executable or a shared library based on the platform and add our sources to it
if (ANDROID)
	# The SDL java code is hardcoded to load libmain.so on android, so we need to change EXECUTABLE_NAME
	set(EXECUTABLE_NAME main)
	add_library(${EXECUTABLE_NAME} SHARED)
else()
	add_executable(${EXECUTABLE_NAME})
endif()

# Add your sources to the target
target_sources(${EXECUTABLE_NAME} 
PRIVATE 
    src/main.c
    src/engine.c
    src/iosLaunchScreen.storyboard
)
# What is iosLaunchScreen.storyboard? This file describes what Apple's mobile platforms
# should show the user while the application is starting up. If you don't include one,
# then you get placed in a compatibility mode that does not allow HighDPI.
# This file is referenced inside Info.plist.in, where it is marked as the launch screen file.
# It is also ignored on non-Apple platforms. 

# To get an app icon on Apple platforms, add it to your executable.
# Afterward, the image file in Info.plist.in.
if(APPLE)
    target_sources("${EXECUTABLE_NAME}" PRIVATE "src/logo.png")
endif()

# Set C version
target_compile_features(${EXECUTABLE_NAME} PUBLIC c_std_11)

# on Web targets, we need CMake to generate a HTML webpage. 
if(EMSCRIPTEN)
	set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()


# Create an option to switch between a system sdl library and a vendored SDL library
option(MYGAME_VENDORED "Use vendored libraries" ON)


if(MYGAME_VENDORED)
    include(FetchContent)
    FetchContent_Declare(
      SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.4.2
    )
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif()


# Link SDL to our executable. This also makes its include directory available to us. 
if(EMSCRIPTEN)
    # Emscripten web build options for audio/worker compatibility
    target_link_options(${EXECUTABLE_NAME} PRIVATE "SHELL: -sUSE_PTHREADS=1 -sPROXY_TO_PTHREAD=1 -sPTHREAD_POOL_SIZE=4 -sALLOW_MEMORY_GROWTH=1 -sFETCH --shell-file ../shell.html")
endif()
target_link_libraries(${EXECUTABLE_NAME} PUBLIC
    SDL3::SDL3              # If using satelite libraries, SDL must be the last item in the list. 
)

# Dealing with assets
# We have some non-code resources that our application needs in order to work. How we deal with those differs per platform.
if (APPLE)
    # on Apple targets, the application bundle has a "resources" subfolder where we can place our assets.
    # SDL_GetBasePath() gives us easy access to that location.
    set(input_root "${CMAKE_CURRENT_LIST_DIR}/src")
    macro(add_resource FILE)
        file(RELATIVE_PATH relpath "${input_root}" "${FILE}")
        get_filename_component(relpath "${relpath}" DIRECTORY)
        target_sources(${EXECUTABLE_NAME} PRIVATE ${FILE})
        set_property(SOURCE ${FILE} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${relpath}")
    endmacro()
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/Text.png")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/Tiles.png")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/Tile_Background.png")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/Background.png")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/move.wav")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/win.wav")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/lose.wav")
    add_resource("${CMAKE_CURRENT_LIST_DIR}/src/sample.wav")
elseif(EMSCRIPTEN)
    # on the web, we have to put the files inside of the webassembly
    # somewhat unintuitively, this is done via a linker argument.
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE 
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/Text.png@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/Tiles.png@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/Tile_Background.png@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/Background.png@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/move.wav@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/win.wav@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/lose.wav@/\""
        "--preload-file \"${CMAKE_CURRENT_LIST_DIR}/src/sample.wav@/\""
    )
else()
    if (ANDROID)
        if (NOT MOBILE_ASSETS_DIR)
            message(FATAL_ERROR "Building for Android, but MOBILE_ASSETS_DIR is not set")
        endif()
        file(MAKE_DIRECTORY "${MOBILE_ASSETS_DIR}")    # we need to create the project Assets dir otherwise CMake won't copy our assets.
    endif()

    
    macro(copy_helper filename)
        if (ANDROID)
            # MOBILE_ASSETS_DIR is set in the gradle file via the cmake command line and points to the Android Studio Assets folder. 
            # when we copy assets there, the Android build pipeline knows to add them to the apk. 
            set(outname "${MOBILE_ASSETS_DIR}/${filename}")    
        else()
            # for platforms that do not have a good packaging format, all we can do is copy the assets to the process working directory.
            set(outname "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${filename}")
        endif()
        add_custom_command(POST_BUILD
            TARGET "${EXECUTABLE_NAME}"
		    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/src/${filename}" "${outname}"
	    )
    endmacro()
    copy_helper("Text.png")
    copy_helper("Tiles.png")
    copy_helper("Tile_Background.png")
    copy_helper("Background.png")
    copy_helper("move.wav")
    copy_helper("win.wav")
    copy_helper("lose.wav")
    copy_helper("sample.wav")
endif()

# set some extra configs for each platform
set(RESOURCE_FILES "src/logo.png")
if(IOS)
    set(RESOURCE_FILES "src/iosLaunchScreen.storyboard;src/logo.png") # make the splash screen show up on iOS
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES 
    # On macOS, make a proper .app bundle instead of a bare executable
    MACOSX_BUNDLE TRUE
    # Set the Info.plist file for Apple Mobile platforms. Without this file, your app
    # will not launch. 
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in"

    # in Xcode, create a Scheme in the schemes dropdown for the app.
    XCODE_GENERATE_SCHEME TRUE
    # Identification for Xcode
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "ru.qwertdim.game_2048"
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "ru.qwertdim.game_2048"
	XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
    RESOURCE "${RESOURCE_FILES}"
)

# on Visual Studio, set our app as the default project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")

# On macOS Platforms, ensure that the bundle is valid for distribution by calling fixup_bundle.
# note that fixup_bundle does not work on iOS, so you will want to use static libraries 
# or manually copy dylibs and set rpaths
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    # tell Install about the target, otherwise fixup won't know about the transitive dependencies
    install(TARGETS ${EXECUTABLE_NAME}
    	BUNDLE DESTINATION ./install COMPONENT Runtime
   	    RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )
	
    set(DEP_DIR "${CMAKE_BINARY_DIR}")  # where to look for dependencies when fixing up
    INSTALL(CODE 
        "include(BundleUtilities)
        fixup_bundle(\"$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>\" \"\" \"${DEP_DIR}\")
        " 
    )
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()
