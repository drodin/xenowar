cmake_minimum_required(VERSION 3.9.2)

# For Android Studio we will force generated toolchain
if(ANDROID_STUDIO)
    set(gen_toolchain "${CMAKE_CURRENT_BINARY_DIR}/generated/toolchain.cmake")
    configure_file(
            "${CMAKE_CURRENT_LIST_DIR}/cmake/template/toolchain.cmake.in"
            "${gen_toolchain}"
            @ONLY
    )
    set(CMAKE_TOOLCHAIN_FILE "${gen_toolchain}" CACHE PATH "" FORCE)
endif()

option(CMAKE_VERBOSE_MAKEFILE "Verbose makefile" OFF)

option(HUNTER_KEEP_PACKAGE_SOURCES "Keep third party sources" ON)
option(HUNTER_STATUS_DEBUG "Print debug info" OFF)

include(cmake/HunterGate.cmake)
HunterGate(
    URL "https://github.com/drodin/hunter/archive/refs/heads/pr.sdl.zip"
    SHA1 "7146bee57e5e8df23e392b509091eff3468f1c80"
    LOCAL
)

project(xenowar DESCRIPTION "Xenowar" VERSION 2.10.1)

hunter_add_package(SDL2)
hunter_add_package(SDL_image)
hunter_add_package(tinyxml2)
hunter_add_package(ZLIB)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL_image CONFIG REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)
find_package(ZLIB CONFIG REQUIRED)

if(ANDROID)
    set(OPENGL_LIBRARIES -lGLESv1_CM)
elseif(IOS)
    set(OPENGL_LIBRARIES "-framework OpenGLES")
else()
    find_package(OpenGL MODULE REQUIRED)

    hunter_add_package(glew)
    find_package(glew CONFIG REQUIRED)

    list(APPEND OPENGL_LIBRARIES glew::glew)
endif()

set(SYSTEM_LIBRARIES)

# Fix MinGW / MSYS build
if(MINGW OR MSYS)
list(APPEND SYSTEM_LIBRARIES
    mingw32
)
endif()

if(WIN32)
list(APPEND SYSTEM_LIBRARIES
    winhttp
)
endif()

set(CMAKE_CXX_FLAGS "-Wno-narrowing -Wno-shorten-64-to-32 -Wno-endif-labels ${CMAKE_CXX_FLAGS}")

set(SRC
    engine/camera.cpp
    engine/engine.cpp
    engine/fixedgeom.cpp
    engine/gpustatemanager.cpp
    engine/loosequadtree.cpp
    engine/map.cpp
    engine/model.cpp
    engine/particle.cpp
    engine/particleeffect.cpp
    engine/renderqueue.cpp
    engine/screenport.cpp
    engine/serialize.cpp
    engine/settings.cpp
    engine/surface.cpp
    engine/text.cpp
    engine/texture.cpp
    engine/ufoutil.cpp
    engine/uirendering.cpp
    faces/faces.cpp
    game/ai.cpp
    game/areawidget.cpp
    game/basetradescene.cpp
    game/battledata.cpp
    game/battlescene.cpp
    game/battlevisibility.cpp
    game/buildbasescene.cpp
    game/cgame.cpp
    game/characterscene.cpp
    game/chits.cpp
    game/dialogscene.cpp
    game/fastbattlescene.cpp
    game/firewidget.cpp
    game/game.cpp
    game/gameresources.cpp
    game/gamesettings.cpp
    game/geoai.cpp
    game/geoendscene.cpp
    game/geomap.cpp
    game/geoscene.cpp
    game/helpscene.cpp
    game/inventory.cpp
    game/inventoryWidget.cpp
    game/item.cpp
    game/newgeooptions.cpp
    game/newtacticaloptions.cpp
    game/research.cpp
    game/researchscene.cpp
    game/saveloadscene.cpp
    game/scene.cpp
    game/settingscene.cpp
    game/stats.cpp
    game/storageWidget.cpp
    game/tacmap.cpp
    game/tacticalendscene.cpp
    game/tacticalintroscene.cpp
    game/tacticalunitscorescene.cpp
    game/ufosound.cpp
    game/unit.cpp
    gamui/gamui.cpp
    grinliz/glcontainer.cpp
    grinliz/gldebug.cpp
    grinliz/glfixed.cpp
    grinliz/glgeometry.cpp
    grinliz/glmatrix.cpp
    grinliz/glmemorypool.cpp
    grinliz/glperformance.cpp
    grinliz/glprime.cpp
    grinliz/glrandom.cpp
    grinliz/glstringutil.cpp
    grinliz/glutil.cpp
    grinliz/glvector.cpp
    micropather/micropather.cpp
    shared/gamedbreader.cpp
    shared/glmap.cpp
    sdl/main.cpp
    sdl/audio.cpp
)

if(ANDROID OR IOS)
    add_definitions(-D__MOBILE__=1)
endif()

if(ANDROID)
    add_definitions(-DANDROID_NDK=1)
	add_definitions(-DGRINLIZ_NO_STL=1)

    add_library(${PROJECT_NAME} SHARED ${SRC})

    set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "")
elseif(APPLE)

    add_definitions(-DGL_SILENCE_DEPRECATION=1)
if(IOS)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SRC} uforesource.db apple/ios/Assets.xcassets apple/ios/LaunchScreen.storyboard)

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/cmake/template/iOSBundleInfo.plist.in"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    )

    set_source_files_properties(apple/ios/Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set_source_files_properties(apple/ios/LaunchScreen.storyboard PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
else()
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SRC} uforesource.db apple/macos/Assets.xcassets apple/macos/app.entitlements)

    set_target_properties(${PROJECT_NAME} PROPERTIES
        OUTPUT_NAME "${PROJECT_DESCRIPTION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/cmake/template/MacOSXBundleInfo.plist.in"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2021 Dmitry Rodin. All rights reserved."
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Mac Developer"
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "apple/macos/app.entitlements"
    )
 
    set(MACOSX_BUNDLE_APP_CATEGORY "public.app-category.board-games")

    set_source_files_properties(apple/macos/Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

    set_target_properties(${PROJECT_NAME} PROPERTIES
        RESOURCE uforesource.db
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_DESCRIPTION}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.drodin.${PROJECT_NAME}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_PATCH}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
        XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
        XCODE_ATTRIBUTE_SKIP_INSTALL "No"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "$ENV{CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM}"
    )

else()
    add_executable(${PROJECT_NAME} ${SRC})
endif()

add_definitions(-DPROJECT_NAME="${PROJECT_NAME}")
add_definitions(-DPROJECT_DESCRIPTION="${PROJECT_DESCRIPTION}")

set(LIBS
    ${SYSTEM_LIBRARIES}
    ${OPENGL_LIBRARIES}
    SDL2::SDL2
    SDL2::SDL2main
    tinyxml2::tinyxml2
    ZLIB::ZLIB
)

target_link_libraries(${PROJECT_NAME} ${LIBS})


if(NOT ANDROID AND NOT IOS)
hunter_add_package(sqlite3)
find_package(sqlite3 CONFIG REQUIRED)

set(ufobuilder-SRC
    grinliz/glcontainer.cpp
    grinliz/gldebug.cpp
    grinliz/gldynamic.cpp
    grinliz/glgeometry.cpp
    grinliz/glmatrix.cpp
    grinliz/glmemorypool.cpp
    grinliz/glperformance.cpp
    grinliz/glprime.cpp
    grinliz/glrandom.cpp
    grinliz/glstringutil.cpp
    grinliz/glutil.cpp
    grinliz/glvector.cpp
    importers/ac3d.c
    importers/import.cpp
    importers/off.cpp
    shared/gamedbreader.cpp
    shared/gamedbwriter.cpp
    shared/gldatabase.cpp
    ufobuilder/builder.cpp
    ufobuilder/dither.cpp
    ufobuilder/modelbuilder.cpp
)

add_executable(ufobuilder ${ufobuilder-SRC})

target_link_libraries(ufobuilder
    ${SYSTEM_LIBRARIES}
    SDL2::SDL2
    SDL2::SDL2main
    SDL_image::SDL_image
    sqlite3::sqlite3
    tinyxml2::tinyxml2
)
endif()
