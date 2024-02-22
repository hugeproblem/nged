add_defines('SPDLOG_COMPILED_LIB=1')
set_languages('cxx17', 'c11')
add_rules("mode.release")
add_rules("mode.debug")
add_rules("mode.profile")
add_rules("mode.check")

if is_mode('debug') then
  add_defines('DEBUG')
  set_symbols('debug')
  set_optimize('none')
end

option('backend')
  if is_host('windows') then
    set_default('dx11')
  else
    set_default('gl2')
  end
  set_description('renderer backend, default to dx11 on windows, otherwise default to gl2')
  set_showmenu(true)
  set_values('dx11', 'dx12', 'vulkan', 'gl2', 'gl3')
option_end()
local backend = get_config('backend')
if is_plat('windows') and (backend=='vulkan' or backend=='gl2' or backend=='gl3') then
  add_requires('vcpkg::glfw3')
  if backend=='gl2' or backend=='gl3' then
    add_requires('vcpkg::gl3w')
    add_defines('IMGUI_IMPL_OPENGL_LOADER_GL3W=1')
  end
elseif backend=='gl3' then
  add_defines('IMGUI_IMPL_OPENGL_LOADER_GLEW=1')
  add_links('GLEW')
end
if backend=='dx11' then
  add_defines('NGED_BACKEND_DX11')
elseif backend=='dx12' then
  add_defines('NGED_BACKEND_DX12')
elseif backend=='vulkan' then
  add_defines('NGED_BACKEND_VULKAN')
elseif backend=='gl2' then
  add_defines('NGED_BACKEND_GL2')
elseif backend=='gl3' then
  add_defines('NGED_BACKEND_GL3')
end

option('python')
  set_description('python executable path, or "no" if python binding is not needed')
  set_showmenu(true)
  set_default('no')
option_end()

option('pyextension_fullpath')
option('pyincludedirs')
option('pylibdirs')
option('pylib')

option('vulkan-sdk')
  set_description('vulkan sdk path')
  set_showmenu(true)
  set_default('')
local vulkan_sdk = get_config('vulkan-sdk')

rule('pythonlib')
on_config(function(target)
  for _,p in ipairs(string.split(get_config('pyincludedirs'), path.envsep())) do
    if os.isdir(p) then
      target:add('includedirs', p)
    end
  end
  for _,p in ipairs(string.split(get_config('pylibdirs'), path.envsep())) do
    if os.isdir(p) then
      target:add('linkdirs', p)
    end
  end
  target:add('links', get_config('pylib'))
end)
rule_end()

target('imgui')
  set_kind('static')
  add_includedirs('deps/imgui', 'deps/imgui/backends', 'deps/imgui/misc/cpp', {public=true})
  add_headerfiles('deps/imgui/*.h', 'deps/imgui/misc/cpp/*.h')
  add_files('deps/imgui/*.cpp', 'deps/imgui/misc/cpp/*.cpp')
  remove_files('deps/imgui/imgui_demo.cpp')
  if backend=='dx11' then
    add_files('deps/imgui/backends/imgui_impl_win32.cpp', 'deps/imgui/backends/imgui_impl_dx11.cpp')
    add_links('d3d11', 'dxgi', 'd3dcompiler')
  elseif backend=='dx12' then
    add_files('deps/imgui/backends/imgui_impl_win32.cpp', 'deps/imgui/backends/imgui_impl_dx12.cpp')
    add_links('d3d12', 'dxgi', 'd3dcompiler')
  elseif backend=='vulkan' then
    add_files('deps/imgui/backends/imgui_impl_glfw.cpp', 'deps/imgui/backends/imgui_impl_vulkan.cpp')
    if backend=='vulkan' then
      add_includedirs(vulkan_sdk..'/Include', {public=true})
      add_linkdirs(vulkan_sdk..'/Lib', {public=true})
    end
    add_links('glfw3', 'vulkan-1')
  elseif backend=='gl2' then
    add_files('deps/imgui/backends/imgui_impl_glfw.cpp', 'deps/imgui/backends/imgui_impl_opengl2.cpp')
  elseif backend=='gl3' then
    add_files('deps/imgui/backends/imgui_impl_glfw.cpp', 'deps/imgui/backends/imgui_impl_opengl3.cpp')
  end
  if is_plat('windows') or is_plat('msys') then
    add_links('ole32', 'uuid', 'gdi32', 'comctl32', 'dwmapi')
    if backend=='gl2' or backend=='gl3' then
      add_links('glfw3', 'opengl32')
      add_packages('vcpkg::glfw3')
    elseif backend=='vulkan' then
      add_packages('vcpkg::glfw3')
    end
  else
    add_links('glfw', 'GL', 'dl', 'pthread')
  end

target('spdlog')
  set_kind('static')
  add_includedirs('deps/spdlog/include')
  add_headerfiles('deps/spdlog/include/**.h')
  add_files('deps/spdlog/src/*.cpp')
  if is_plat('linux') then
    add_links('pthread')
  end

target('nfd')
  set_kind('static')
  add_includedirs('deps/nativefiledialog/src/include', {public=true})
  add_headerfiles('deps/nativefiledialog/src/include/*.h')
  add_files('deps/nativefiledialog/src/nfd_common.c')
  local nfdimp = 'nfd_win.cpp'
  if is_plat('macosx') then
    nfdimp = 'nfd_cocoa.m'
  elseif is_plat('linux') then
    nfdimp = 'nfd_zenity.c'
  end
  add_files('deps/nativefiledialog/src/'..nfdimp)
  if is_plat('windows') or is_plat('msys') then
    add_links('ole32', 'uuid', 'gdi32', 'comctl32')
  end

target('boxer')
  set_kind('static')
  add_includedirs('deps/boxer/include')
  add_headerfiles('deps/boxer/include/**')
  if is_plat('macosx') then
    add_files('deps/boxer/src/boxer_osx.mm')
  elseif is_plat('linux') then
    add_files('deps/boxer/src/boxer_zenity.cpp')
  else
    add_files('deps/boxer/src/boxer_win.cpp')
  end

target('s7')
  set_kind('static')
  add_includedirs('deps/s7', {public=true})
  add_headerfiles('deps/s7/s7.h', 'ngs7/s7-extensions.h')
  add_files      ('deps/s7/s7.c', 'ngs7/s7-extensions.cpp')
  if is_plat('linux') then
    add_links('dl')
  end

target('s7e')
  set_kind('binary')
  add_files('ngs7/s7e.cpp')
  add_deps('s7')

target('miniz')
  set_kind('static')
  add_headerfiles('deps/miniz/miniz.h')
  add_files('deps/miniz/miniz.c')
  add_includedirs('deps/miniz', {public=true})

target('ngdoc')
  set_kind('static')
  add_headerfiles('ngdoc.h')
  add_files('ngdoc.cpp', 'ngdraw.cpp', 'style.cpp')
  add_deps('spdlog', 'miniz')
  add_includedirs(
    'deps/nlohmann',
    'deps/spdlog/include',
    'deps/stduuid/include',
    'deps/stduuid', -- for gsl
    'deps/parallel_hashmap/parallel_hashmap',
    {public=true})

target('nged')
  set_kind('static')
  add_headerfiles('*.h|ngdoc.h')
  add_files('nged.cpp', 'nged_imgui.cpp', 'nged_imgui_fonts.cpp')
  add_deps('spdlog', 'nfd', 'imgui', 'boxer', 'ngdoc')
  add_cxflags('/bigobj', {tools='cl'})
  add_includedirs(
    'deps/boxer/include',
    'deps/nlohmann',
    'deps/spdlog/include',
    'deps/parallel_hashmap/parallel_hashmap'
  )

target('tests')
  set_kind('binary')
  add_deps('ngdoc', 'spdlog')
  add_files('tests/*.cpp')
  add_includedirs(
    '.',
    'deps/doctest')

target('lua')
  set_kind('static')
  add_includedirs('deps/lua')
  add_files('deps/lua/*.c|lua.c|luac.c|onelua.c')

target('entry')
  set_kind('static')
  add_deps('imgui')
  add_files('entry/entry.cpp')
  if backend=='dx11' then
    add_files('entry/dx11_main.cpp')
    add_files('entry/dx11_texture.cpp')
  elseif backend=='dx12' then
    add_files('entry/dx12_main.cpp')
    add_files('entry/dx12_texture.cpp')
  elseif backend=='vulkan' then
    add_files('entry/vulkan_main.cpp')
    add_files('entry/vulkan_texture.cpp')
  elseif backend=='gl2' then
    add_files('entry/gl2_main.cpp')
    add_files('entry/gl_texture.cpp')
  elseif backend=='gl3' then
    add_files('entry/gl3_main.cpp')
    add_files('entry/gl_texture.cpp')
  end
  if is_plat('windows') then
    add_links('ws2_32', 'advapi32', 'gdi32', 'shell32', 'version')
    if backend~='dx11' and backend~='dx12' then
      add_packages('vcpkg::glfw3')
    end
    if backend=='gl2' or backend=='gl3' then
      add_packages('vcpkg::gl3w')
    end
  end

target('demo')
  set_kind('binary')
  add_deps('nged', 'entry')
  add_files('demo/main.cpp')
  add_includedirs('.', 'deps/boxer/include')

target('typed_demo')
  set_kind('binary')
  add_deps('nged', 'entry')
  add_files('typed_demo/main.cpp')
  add_includedirs('.', 'deps/boxer/include')

target('ngs7')
  set_kind('binary')
  add_deps('nged', 's7', 'entry')
  add_files('ngs7/ngs7.cpp', 'ngs7/main.cpp')
  add_includedirs('deps/imgui/backends')
  if is_plat('windows') then
    add_files('ngs7/icon.rc')
  end
  add_includedirs('.', 'deps/boxer/include', 'deps/imgui', 'deps/nlohmann', 'deps/spdlog/include', 'deps/subprocess.h')

if get_config('python') and get_config('python')~='no' then
  target('parmscript')
    add_rules('pythonlib')
    set_kind('static')
    add_deps('lua', 'imgui', 'nfd')
    add_includedirs('deps/lua', 'deps/sol2/include', 'deps/nlohmann')
    add_includedirs('deps/pybind11/include')
    add_files('deps/parmscript/*.cpp')
    add_files('deps/parmscript/parmexpr.lua', {rule='utils.bin2c'})

  target('ngpy')
    add_rules('pythonlib')
    set_kind('shared')
    add_headerfiles('ngpy.h', 'pybind11_imgui.h')
    add_files('ngpy.cpp', 'pybind11_imgui.cpp')
    add_deps('nged', 'entry', 'parmscript')
    add_includedirs('.', 'deps/boxer/include', 'deps/imgui', 'deps/nlohmann', 'deps/spdlog/include', 'deps/parallel_hashmap/parallel_hashmap', 'deps/subprocess.h', 'deps/parmscript')
    add_includedirs('deps/pybind11/include')
    add_cxflags('/bigobj', {tools='cl'})

    after_build(function(target)
      os.cp(target:targetfile(), get_config('pyextension_fullpath'))
    end)
    if get_config('python') ~= '' then
      add_runenvs('PYTHONPATH', path.directory(get_config('pyextension_fullpath')))
      on_run(function(target)
        os.run('%s %s', get_config('python'), 'pydemo/main.py')
      end)
    end
end -- python enabled

