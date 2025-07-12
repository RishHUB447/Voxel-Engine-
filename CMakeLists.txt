cmake_minimum_required(VERSION 3.15)
project(MinecraftClone)

set(CMAKE_CXX_STANDARD 17)

# Set correct CRT linkage for MSVC: /MDd in Debug, /MD in Release
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/wd4715 /wd4172 /wd4098)
endif()

include_directories(include)
link_directories(${CMAKE_SOURCE_DIR}/lib)

file(GLOB_RECURSE SRC_FILES src/*.cpp)
file(GLOB IMGUI_SRC include/imgui/*.cpp)
set(ALL_SRC ${SRC_FILES} ${IMGUI_SRC})

set(RESOURCE_FILE ${CMAKE_SOURCE_DIR}/include/resource.rc)
add_executable(Minecraft ${ALL_SRC} ${RESOURCE_FILE})


# ✅ Set output location AFTER target creation
set_target_properties(Minecraft PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
)

target_link_libraries(Minecraft
    glfw3
    glew32
    freetype
    assimp-vc143-mtd  # <- switch this to mt for Release later
    opengl32
    user32
    gdi32
    shell32
    ole32
)

set(DLL_FILES
    ${CMAKE_SOURCE_DIR}/lib/glew32.dll
    ${CMAKE_SOURCE_DIR}/lib/glfw3.dll
    ${CMAKE_SOURCE_DIR}/lib/assimp-vc143-mtd.dll
    ${CMAKE_SOURCE_DIR}/lib/freetype.dll
)

foreach(DLL ${DLL_FILES})
    if(EXISTS "${DLL}")
        add_custom_command(TARGET Minecraft POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DLL}"
                "${CMAKE_SOURCE_DIR}"
        )
    else()
        message(WARNING "WARNING: ${DLL} not found. Skipping copy.")
    endif()
endforeach()
