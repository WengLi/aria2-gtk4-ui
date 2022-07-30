# 默认情况下，meson使用调试编译类型，该模式下会打开debug和警告，并不会进行优化。–buildtype命令可以更改编译类型：
# plain 用户指定编译选项，一般用于发行包
# debug 默认模式，生成调试和警告并不做任何优化
# debugoptimized 生成调试和警告，但优化编译 (-g -O2)
# release 全体优化编译，没有调试信息
project('app',
'c',
license: 'LGPL2.1+',
meson_version: '>= 0.61',
default_options: [ 'buildtype=debug'],
version: '1.0.0',)

cc = meson.get_compiler('c')

gtkdep = dependency('gtk4')

gnome=import('gnome')
resources = gnome.compile_resources('resources','app.gresource.xml')

# libsocket = cc.find_library('socket', required: false)
# libinet = cc.find_library('inet', required: false)

sourcefiles=files('app.c',
'sha1.c',
'base64.c',
'cJSON.c',
'uuid4.c',
'socketobject.c',
'utilities.c',
'websocketclient.c',
'aria2_entity.c',
'aria2.c',
'menuwindow.c'
)

executable('app','app.c', 
sourcefiles, resources, 
dependencies: gtkdep,
# dependencies: [gtkdep,libsocket,libinet],
export_dynamic: true, 
win_subsystem: 'console',
# win_subsystem: 'windows',
link_args:'-lwsock32 -lws2_32 -lssl'.split())