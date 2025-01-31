cmake_minimum_required(VERSION 3.18.4)

project(cgre)

set(sources
	glad/gl.c
	glad/wgl.c
#    WGL_ARB_extensions_string.cpp
#    WGL_ARB_pbuffer_extension.cpp
#    WGL_ARB_pixel_format_extension.cpp
#    WGL_ARB_render_texture_extension.cpp
    gl_wrapper.h
    background_manager.cpp
    capabilities_manager.cpp
    camera.cpp
    #console.cpp
    #examinator_fly.cpp
    #examinator_glulookat.cpp
    #examinator_ground.cpp
    examinator_trackball.cpp
    #examinator_walk.cpp
    frame_buffer_object.cpp
    framerate.cpp
    gamepad2.cpp
    light_renderer.cpp
    material_renderer.cpp
    mesh_renderer.cpp
    offscreen_rendering_factory.cpp
    #pbuffer.cpp
    photographs.cpp
    projector.cpp
    projectorsManager.cpp
    shader.cpp
    shaders_manager.cpp
    skybox.cpp
    textures_manager.cpp
    vertex_buffer.cpp
    vertex_buffer_manager.cpp
    #viewer3D_core.cpp
    widgets_renderer.cpp
    window.cpp
    )
    
set(headers
	glad/gl.h
	glad/wgl.h
    cgre.h
    camera.h
    capabilities_manager.h
    #console.h
    display_list_manager.h
    examinator.h
    #examinator_fly.h
    #examinator_glulookat.h
    #examinator_ground.h
    examinator_trackball.h
    #examinator_walk.h
    frame_buffer_object.h
    framerate.h
    gamepad2.h
    light_renderer.h
    material_renderer.h
    mesh_renderer.h
    offscreen_rendering_factory.h
    #pbuffer.h
    photographs.h
    projector.h
    projectorsManager.h
    shader.h
    shaders_manager.h
    skybox.h
    textures_manager.h
    vertex_buffer.h
    vertex_buffer_manager.h
    #viewer3D_core.h
    widgets_renderer.h
    window.h
    )

include_directories(.)
add_library(cgre STATIC ${headers} ${sources})
target_link_libraries(cgmesh cgimg)
