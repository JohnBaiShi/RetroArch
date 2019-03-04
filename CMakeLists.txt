cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)
project(RetroArch LANGUAGES CXX C)

add_executable(retroarch
	input/drivers/linuxraw_input.c input/common/linux_common.c input/drivers_joypad/linuxraw_joypad.c frontend/drivers/platform_unix.c version_git.c frontend/frontend.c frontend/frontend_driver.c frontend/drivers/platform_null.c ui/ui_companion_driver.c ui/drivers/ui_null.c ui/drivers/null/ui_null_window.c ui/drivers/null/ui_null_browser_window.c ui/drivers/null/ui_null_msg_window.c ui/drivers/null/ui_null_application.c core_impl.c retroarch.c dirs.c paths.c command.c msg_hash.c intl/msg_hash_us.c ./libretro-common/queues/task_queue.c tasks/task_content.c tasks/task_save.c tasks/task_file_transfer.c tasks/task_image.c tasks/task_audio_mixer.c ./libretro-common/encodings/encoding_utf.c ./libretro-common/encodings/encoding_crc32.c ./libretro-common/compat/fopen_utf8.c ./libretro-common/lists/file_list.c ./libretro-common/lists/dir_list.c ./libretro-common/file/retro_dirent.c ./libretro-common/streams/stdin_stream.c ./libretro-common/streams/file_stream.c ./libretro-common/streams/file_stream_transforms.c ./libretro-common/streams/interface_stream.c ./libretro-common/streams/memory_stream.c ./libretro-common/vfs/vfs_implementation.c ./libretro-common/lists/string_list.c ./libretro-common/string/stdstring.c ./libretro-common/memmap/memalign.c setting_list.c list_special.c ./libretro-common/file/nbio/nbio_stdio.c ./libretro-common/file/nbio/nbio_linux.c ./libretro-common/file/nbio/nbio_unixmmap.c ./libretro-common/file/nbio/nbio_windowsmmap.c ./libretro-common/file/nbio/nbio_orbis.c ./libretro-common/file/nbio/nbio_intf.c ./libretro-common/file/file_path.c file_path_special.c file_path_str.c ./libretro-common/hash/rhash.c audio/audio_driver.c ./libretro-common/audio/audio_mixer.c input/common/input_common.c input/input_driver.c input/input_mapper.c led/led_driver.c led/drivers/led_null.c gfx/video_coord_array.c gfx/video_display_server.c gfx/video_driver.c gfx/video_crt_switch.c camera/camera_driver.c wifi/wifi_driver.c location/location_driver.c configuration.c ./libretro-common/dynamic/dylib.c dynamic.c cores/dynamic_dummy.c ./libretro-common/queues/message_queue.c managers/core_manager.c managers/state_manager.c gfx/drivers_font_renderer/bitmapfont.c tasks/task_autodetect.c input/input_autodetect_builtin.c input/input_keymaps.c input/input_remapping.c ./libretro-common/queues/fifo_queue.c managers/core_option_manager.c ./libretro-common/compat/compat_fnmatch.c ./libretro-common/compat/compat_posix_string.c managers/cheat_manager.c core_info.c ./libretro-common/file/config_file.c ./libretro-common/file/config_file_userdata.c runtime_file.c tasks/task_screenshot.c tasks/task_powerstate.c ./libretro-common/gfx/scaler/scaler.c gfx/video_shader_parse.c ./libretro-common/gfx/scaler/pixconv.c ./libretro-common/gfx/scaler/scaler_int.c ./libretro-common/gfx/scaler/scaler_filter.c gfx/font_driver.c gfx/video_filter.c ./libretro-common/audio/resampler/audio_resampler.c ./libretro-common/audio/dsp_filter.c ./libretro-common/audio/resampler/drivers/sinc_resampler.c ./libretro-common/audio/resampler/drivers/nearest_resampler.c ./libretro-common/audio/resampler/drivers/null_resampler.c ./libretro-common/utils/md5.c location/drivers/nulllocation.c camera/drivers/nullcamera.c wifi/drivers/nullwifi.c gfx/drivers/nullgfx.c gfx/display_servers/dispserv_null.c audio/drivers/nullaudio.c input/drivers/nullinput.c input/drivers_hid/null_hid.c input/drivers_joypad/null_joypad.c playlist.c movie.c record/record_driver.c record/drivers/record_null.c ./libretro-common/features/features_cpu.c performance_counters.c verbosity.c midi/midi_driver.c midi/drivers/null_midi.c runahead/copy_load_info.c runahead/dirty_input.c runahead/mem_util.c runahead/mylist.c runahead/run_ahead.c runahead/secondary_core.c audio/drivers_resampler/cc_resampler.c intl/msg_hash_de.c intl/msg_hash_eo.c intl/msg_hash_es.c intl/msg_hash_fr.c intl/msg_hash_it.c intl/msg_hash_ja.c intl/msg_hash_ko.c intl/msg_hash_nl.c intl/msg_hash_pl.c intl/msg_hash_pt_br.c intl/msg_hash_pt_pt.c intl/msg_hash_ru.c intl/msg_hash_vn.c intl/msg_hash_chs.c intl/msg_hash_cht.c intl/msg_hash_ar.c intl/msg_hash_el.c ./libretro-common/compat/compat_strl.c ./libretro-common/formats/image_texture.c cores/libretro-imageviewer/image_core.c libretro-db/bintree.c libretro-db/libretrodb.c libretro-db/query.c libretro-db/rmsgpack.c libretro-db/rmsgpack_dom.c database_info.c tasks/task_database.c tasks/task_database_cue.c audio/drivers/tinyalsa.c audio/drivers/pulse.c ./libretro-common/audio/conversion/s16_to_float.c ./libretro-common/audio/conversion/float_to_s16.c ./libretro-common/audio/audio_mix.c ./libretro-common/formats/wav/rwav.c menu/drivers/rgui.c menu/drivers/materialui.c menu/drivers/xmb.c menu/drivers/ozone/ozone.c menu/drivers/ozone/ozone_entries.c menu/drivers/ozone/ozone_display.c menu/drivers/ozone/ozone_texture.c menu/drivers/ozone/ozone_theme.c menu/drivers/ozone/ozone_sidebar.c menu/menu_shader.c menu/menu_driver.c menu/menu_content.c menu/menu_input.c menu/menu_entries.c menu/menu_setting.c menu/menu_networking.c menu/widgets/menu_filebrowser.c menu/widgets/menu_dialog.c menu/widgets/menu_input_dialog.c menu/widgets/menu_input_bind_dialog.c menu/widgets/menu_entry.c menu/widgets/menu_osk.c menu/menu_cbs.c menu/cbs/menu_cbs_ok.c menu/cbs/menu_cbs_cancel.c menu/cbs/menu_cbs_select.c menu/cbs/menu_cbs_start.c menu/cbs/menu_cbs_info.c menu/cbs/menu_cbs_refresh.c menu/cbs/menu_cbs_left.c menu/cbs/menu_cbs_right.c menu/cbs/menu_cbs_deferred_push.c menu/cbs/menu_cbs_scan.c menu/cbs/menu_cbs_get_value.c menu/cbs/menu_cbs_label.c menu/cbs/menu_cbs_sublabel.c menu/cbs/menu_cbs_title.c menu/cbs/menu_cbs_up.c menu/cbs/menu_cbs_down.c menu/cbs/menu_cbs_contentlist_switch.c menu/menu_displaylist.c menu/menu_animation.c menu/drivers/menu_generic.c menu/drivers/null.c menu/drivers_display/menu_display_null.c tasks/task_overlay.c input/input_overlay.c led/drivers/led_overlay.c gfx/drivers_font_renderer/stb.c gfx/drivers_font_renderer/stb_unicode.c gfx/drivers_font_renderer/freetype.c ./libretro-common/rthreads/rthreads.c gfx/video_thread_wrapper.c audio/audio_thread_wrapper.c input/common/input_x11_common.c input/drivers/x11_input.c gfx/common/x11_common.c gfx/common/xinerama_common.c gfx/display_servers/dispserv_x11.c gfx/drivers_context/x_ctx.c input/drivers_keyboard/keyboard_event_xkb.c input/drivers/udev_input.c input/drivers_joypad/udev_joypad.c input/drivers_hid/libusb_hid.c input/drivers_joypad/hid_joypad.c input/connect/joypad_connection.c input/connect/connect_ps2adapter.c input/connect/connect_psxadapter.c input/connect/connect_ps3.c input/connect/connect_ps4.c input/connect/connect_wii.c input/connect/connect_nesusb.c input/connect/connect_snesusb.c input/connect/connect_wiiupro.c input/connect/connect_wiiugca.c input/drivers_joypad/parport_joypad.c gfx/drivers_context/gfx_null_ctx.c gfx/video_state_tracker.c gfx/drivers_context/drm_ctx.c gfx/drivers/gl.c gfx/drivers/gl_core.c ./libretro-common/gfx/gl_capabilities.c gfx/drivers_font/gl_raster_font.c gfx/drivers_font/gl_core_raster_font.c ./libretro-common/glsym/rglgen.c menu/drivers_display/menu_display_gl.c menu/drivers_display/menu_display_gl_core.c gfx/drivers_context/xegl_ctx.c ./libretro-common/glsym/glsym_es3.c gfx/drivers_shader/shader_glsl.c gfx/common/egl_common.c gfx/drivers/vulkan.c gfx/common/vulkan_common.c ./libretro-common/vulkan/vulkan_symbol_wrapper.c gfx/drivers_font/vulkan_raster_font.c gfx/drivers_shader/shader_vulkan.cpp gfx/drivers_shader/shader_gl_core.cpp gfx/drivers_shader/shader_gl_core.h gfx/drivers_context/khr_display_ctx.c menu/drivers_display/menu_display_vulkan.c gfx/common/drm_common.c gfx/drivers/xvideo.c gfx/drivers_shader/slang_process.cpp gfx/drivers_shader/slang_preprocess.cpp gfx/drivers_shader/glslang_util.cpp gfx/drivers_shader/slang_reflection.cpp ./deps/glslang/glslang.cpp ./deps/glslang/glslang/SPIRV/SpvBuilder.cpp ./deps/glslang/glslang/SPIRV/SPVRemapper.cpp ./deps/glslang/glslang/SPIRV/InReadableOrder.cpp ./deps/glslang/glslang/SPIRV/doc.cpp ./deps/glslang/glslang/SPIRV/Logger.cpp ./deps/glslang/glslang/SPIRV/GlslangToSpv.cpp ./deps/glslang/glslang/SPIRV/disassemble.cpp ./deps/glslang/glslang/glslang/GenericCodeGen/Link.cpp ./deps/glslang/glslang/glslang/GenericCodeGen/CodeGen.cpp ./deps/glslang/glslang/OGLCompilersDLL/InitializeDll.cpp ./deps/glslang/glslang/glslang/MachineIndependent/ParseContextBase.cpp ./deps/glslang/glslang/glslang/MachineIndependent/Intermediate.cpp ./deps/glslang/glslang/glslang/MachineIndependent/RemoveTree.cpp ./deps/glslang/glslang/glslang/MachineIndependent/iomapper.cpp ./deps/glslang/glslang/glslang/MachineIndependent/propagateNoContraction.cpp ./deps/glslang/glslang/glslang/MachineIndependent/limits.cpp ./deps/glslang/glslang/glslang/MachineIndependent/intermOut.cpp ./deps/glslang/glslang/glslang/MachineIndependent/Initialize.cpp ./deps/glslang/glslang/glslang/MachineIndependent/SymbolTable.cpp ./deps/glslang/glslang/glslang/MachineIndependent/parseConst.cpp ./deps/glslang/glslang/glslang/MachineIndependent/glslang_tab.cpp ./deps/glslang/glslang/glslang/MachineIndependent/ParseHelper.cpp ./deps/glslang/glslang/glslang/MachineIndependent/linkValidate.cpp ./deps/glslang/glslang/glslang/MachineIndependent/attribute.cpp ./deps/glslang/glslang/glslang/MachineIndependent/IntermTraverse.cpp ./deps/glslang/glslang/glslang/MachineIndependent/InfoSink.cpp ./deps/glslang/glslang/glslang/MachineIndependent/ShaderLang.cpp ./deps/glslang/glslang/glslang/MachineIndependent/Constant.cpp ./deps/glslang/glslang/glslang/MachineIndependent/Scan.cpp ./deps/glslang/glslang/glslang/MachineIndependent/reflection.cpp ./deps/glslang/glslang/glslang/MachineIndependent/Versions.cpp ./deps/glslang/glslang/glslang/MachineIndependent/PoolAlloc.cpp ./deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpAtom.cpp ./deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpContext.cpp ./deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpTokens.cpp ./deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpScanner.cpp ./deps/glslang/glslang/glslang/MachineIndependent/preprocessor/Pp.cpp ./deps/glslang/glslang/glslang/OSDependent/Unix/ossource.cpp ./deps/SPIRV-Cross/spirv_cross.cpp ./deps/SPIRV-Cross/spirv_cfg.cpp ./deps/SPIRV-Cross/spirv_glsl.cpp ./deps/SPIRV-Cross/spirv_hlsl.cpp ./deps/SPIRV-Cross/spirv_msl.cpp ./libretro-common/file/archive_file.c ./libretro-common/streams/trans_stream.c ./libretro-common/streams/trans_stream_pipe.c ./libretro-common/file/archive_file_7z.c ./deps/7zip/7zIn.c ./deps/7zip/Bra86.c ./deps/7zip/7zFile.c ./deps/7zip/7zStream.c ./deps/7zip/LzFind.c ./deps/7zip/LzmaDec.c ./deps/7zip/LzmaEnc.c ./deps/7zip/7zCrcOpt.c ./deps/7zip/Bra.c ./deps/7zip/7zDec.c ./deps/7zip/Bcj2.c ./deps/7zip/7zCrc.c ./deps/7zip/Lzma2Dec.c ./deps/7zip/7zBuf.c ./deps/ibxm/ibxm.c ./libretro-common/file/archive_file_zlib.c ./libretro-common/streams/trans_stream_zlib.c ./libretro-common/formats/libchdr/libchdr_bitstream.c ./libretro-common/formats/libchdr/libchdr_cdrom.c ./libretro-common/formats/libchdr/libchdr_chd.c ./libretro-common/formats/libchdr/libchdr_huffman.c ./libretro-common/streams/chd_stream.c ./libretro-common/formats/libchdr/libchdr_zlib.c ./libretro-common/formats/libchdr/libchdr_lzma.c ./libretro-common/formats/tga/rtga.c ./libretro-common/formats/png/rpng.c ./libretro-common/formats/png/rpng_encode.c ./libretro-common/formats/jpeg/rjpeg.c ./libretro-common/formats/bmp/rbmp.c ./libretro-common/formats/bmp/rbmp_encode.c ./libretro-common/formats/json/jsonsax.c ./libretro-common/formats/json/jsonsax_full.c ./libretro-common/formats/image_transfer.c cores/libretro-gong/gong.c camera/drivers/video4linux2.c cores/libretro-video-processor/video_processor_v4l2.c tasks/task_decompress.c led/drivers/led_rpi.c)

target_include_directories(retroarch PRIVATE
			libretro-common/include
			deps
			deps/7zip
			libretro-common/formats/libchdr
			deps/7zip
			gfx/include
			deps/glslang/glslang/glslang/OSDependent/Unix
			deps/glslang/glslang/OGLCompilersDLL
			deps/glslang/glslang
			deps/glslang/glslang/glslang/MachineIndependent
			deps/glslang/glslang/glslang/Public
			deps/glslang/glslang/SPIRV
			deps/glslang
			deps/SPIRV-Cross
			.
			deps/stb
			/usr/include/libdrm
			/usr/lib/glib-2.0/include
			/usr/include/freetype2
			/usr/include/libpng16
			/usr/include/harfbuzz)

target_compile_definitions(retroarch PRIVATE
			HAVE_CONFIG_H
			HAVE_FILE_LOGGER
			HAVE_SHADERPIPELINE
			HAVE_NETPLAYDISCOVERY
			RARCH_INTERNAL
			HAVE_HID
			HAVE_DYLIB
			HAVE_GIT_VERSION
			GIT_VERSION=389138197b
			_GNU_SOURCE
			_FILE_OFFSET_BITS=64
			HAVE_RUNAHEAD
			HAVE_CC_RESAMPLER
			HAVE_LANGEXTRA
			HAVE_MENU
			HAVE_RGUI
			HAVE_MATERIALUI
			HAVE_XMB
			HAVE_OZONE
			HAVE_OVERLAY
			HAVE_STB_FONT
			HAVE_COMPRESSION
			HAVE_IMAGEVIEWER
			HAVE_COMMAND
			HAVE_STDIN_CMD
			HAVE_TINYALSA
			HAVE_LIBUSB
			HAVE_VULKAN
			HAVE_SLANG
			HAVE_GLSLANG
			HAVE_SPIRV_CROSS
			HAVE_OPENGLES
			HAVE_OPENGLES3
			HAVE_GL_SYNC
			HAVE_GLSL
			HAVE_EGL
			HAVE_X11
			HAVE_THREAD_STORAGE
			HAVE_RPNG
			HAVE_RJPEG
			HAVE_RBMP
			HAVE_LANGEXTRA
			_REENTRANT
			HAVE_OPENGL_CORE
			HAVE_7Z
			_7ZIP_ST
			HAVE_IBXM
			HAVE_ZLIB
			HAVE_CHD
			GL_DEBUG
			WANT_SUBCODE
			WANT_RAW_DATA_SECTOR
			HAVE_RTGA
			GLOBAL_CONFIG_DIR="/etc"
			HAVE_DR_MP3)

target_compile_options(retroarch PRIVATE
	-ffast-math -Wno-unused-result -Wno-unused-variable -Wall -pthread)

target_link_libraries(retroarch PRIVATE z pulse freetype X11 -pthread dl xkbcommon udev usb-1.0 gbm drm EGL GL Xv v4l2 rt ass m Xext Xxf86vm Xinerama Xrandr X11-xcb xcb)


