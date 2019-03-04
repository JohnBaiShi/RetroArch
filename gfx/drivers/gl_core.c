/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019 - Hans-Kristian Arntzen
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../common/gl_core_common.h"

#include <gfx/gl_capabilities.h>
#include <gfx/video_frame.h>
#include <glsym/glsym.h>
#include <string/stdstring.h>

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../record/record_driver.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../font_driver.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#ifdef HAVE_MENU_WIDGETS
#include "../../menu/widgets/menu_widgets.h"
#endif
#endif

static const struct video_ortho default_ortho = {0, 1, 0, 1, -1, 1};

static void gl_core_destroy_resources(gl_core_t *gl)
{
   int i;
   if (!gl)
      return;

   if (gl->filter_chain)
      gl_core_filter_chain_free(gl->filter_chain);
   gl->filter_chain = NULL;

   glBindVertexArray(0);
   if (gl->vao != 0)
      glDeleteVertexArrays(1, &gl->vao);

   for (i = 0; i < GL_CORE_NUM_TEXTURES; i++)
   {
      if (gl->textures[i].tex != 0)
         glDeleteTextures(1, &gl->textures[i].tex);
   }
   memset(gl->textures, 0, sizeof(gl->textures));

   if (gl->menu_texture != 0)
      glDeleteTextures(1, &gl->menu_texture);

   if (gl->pipelines.alpha_blend)
      glDeleteProgram(gl->pipelines.alpha_blend);
   if (gl->pipelines.font)
      glDeleteProgram(gl->pipelines.font);
   if (gl->pipelines.ribbon)
      glDeleteProgram(gl->pipelines.ribbon);
   if (gl->pipelines.ribbon_simple)
      glDeleteProgram(gl->pipelines.ribbon_simple);
   if (gl->pipelines.snow_simple)
      glDeleteProgram(gl->pipelines.snow_simple);
   if (gl->pipelines.snow)
      glDeleteProgram(gl->pipelines.snow);
   if (gl->pipelines.bokeh)
      glDeleteProgram(gl->pipelines.bokeh);

   free(gl);
}

static const gfx_ctx_driver_t *gl_core_get_context(gl_core_t *gl)
{
   enum gfx_ctx_api api;
   const gfx_ctx_driver_t *gfx_ctx      = NULL;
   void                      *ctx_data  = NULL;
   settings_t                 *settings = config_get_ptr();
   unsigned major;
   unsigned minor;

#ifdef HAVE_OPENGLES
   api = GFX_CTX_OPENGL_ES_API;
   major = 2;
   minor = 0;
#else
   api   = GFX_CTX_OPENGL_API;
   major = 3;
   minor = 2;
   gl_query_core_context_set(true);
#endif

   gfx_ctx = video_context_driver_init_first(gl,
         settings->arrays.video_context_driver,
         api, major, minor, false, &ctx_data);

   if (ctx_data)
      gl->ctx_data = ctx_data;

   return gfx_ctx;
}

static void gl_core_set_projection(gl_core_t *gl,
                                   const struct video_ortho *ortho, bool allow_rotate)
{
   math_matrix_4x4 rot;

   /* Calculate projection. */
   matrix_4x4_ortho(gl->mvp_no_rot, ortho->left, ortho->right,
                    ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      gl->mvp = gl->mvp_no_rot;
      return;
   }

   matrix_4x4_rotate_z(rot, M_PI * gl->rotation / 180.0f);
   matrix_4x4_multiply(gl->mvp, rot, gl->mvp_no_rot);

   memcpy(gl->mvp_no_rot_yflip.data, gl->mvp_no_rot.data, sizeof(gl->mvp_no_rot.data));
   MAT_ELEM_4X4(gl->mvp_no_rot_yflip, 1, 0) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_no_rot_yflip, 1, 1) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_no_rot_yflip, 1, 2) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_no_rot_yflip, 1, 3) *= -1.0f;

   memcpy(gl->mvp_yflip.data, gl->mvp.data, sizeof(gl->mvp.data));
   MAT_ELEM_4X4(gl->mvp_yflip, 1, 0) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_yflip, 1, 1) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_yflip, 1, 2) *= -1.0f;
   MAT_ELEM_4X4(gl->mvp_yflip, 1, 3) *= -1.0f;
}

static void gl_core_set_viewport(gl_core_t *gl,
                                 video_frame_info_t *video_info,
                                 unsigned viewport_width,
                                 unsigned viewport_height,
                                 bool force_full, bool allow_rotate)
{
   gfx_ctx_aspect_t aspect_data;
   int x                    = 0;
   int y                    = 0;
   float device_aspect      = (float)viewport_width / viewport_height;
   unsigned height          = video_info->height;

   aspect_data.aspect       = &device_aspect;
   aspect_data.width        = viewport_width;
   aspect_data.height       = viewport_height;

   video_context_driver_translate_aspect(&aspect_data);

   if (video_info->scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&gl->vp,
                                        viewport_width, viewport_height,
                                        video_driver_get_aspect_ratio(), gl->keep_aspect);
      viewport_width  = gl->vp.width;
      viewport_height = gl->vp.height;
   }
   else if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (video_info->aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         /* GL has bottom-left origin viewport. */
         x      = video_info->custom_vp_x;
         y      = height - video_info->custom_vp_y - video_info->custom_vp_height;
         viewport_width  = video_info->custom_vp_width;
         viewport_height = video_info->custom_vp_height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            x     = (int)roundf(viewport_width * (0.5f - delta));
            viewport_width = (unsigned)roundf(2.0f * viewport_width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            y      = (int)roundf(viewport_height * (0.5f - delta));
            viewport_height = (unsigned)roundf(2.0f * viewport_height * delta);
         }
      }

      gl->vp.x      = x;
      gl->vp.y      = y;
      gl->vp.width  = viewport_width;
      gl->vp.height = viewport_height;
   }
   else
   {
      gl->vp.x      = gl->vp.y = 0;
      gl->vp.width  = viewport_width;
      gl->vp.height = viewport_height;
   }

#if defined(RARCH_MOBILE)
   /* In portrait mode, we want viewport to gravitate to top of screen. */
   if (device_aspect < 1.0f)
      gl->vp.y *= 2;
#endif

   glViewport(gl->vp.x, gl->vp.y, gl->vp.width, gl->vp.height);
   gl_core_set_projection(gl, &default_ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      gl->vp_out_width  = viewport_width;
      gl->vp_out_height = viewport_height;
   }

   gl->filter_chain_vp.x = gl->vp.x;
   gl->filter_chain_vp.y = gl->vp.y;
   gl->filter_chain_vp.width = gl->vp.width;
   gl->filter_chain_vp.height = gl->vp.height;

#if 0
   RARCH_LOG("Setting viewport @ %ux%u\n", viewport_width, viewport_height);
#endif
}

static bool gl_core_init_pipelines(gl_core_t *gl)
{
   static const uint32_t alpha_blend_vert[] =
#include "vulkan_shaders/alpha_blend.vert.inc"
      ;

   static const uint32_t alpha_blend_frag[] =
#include "vulkan_shaders/alpha_blend.frag.inc"
      ;

   static const uint32_t font_frag[] =
#include "vulkan_shaders/font.frag.inc"
      ;

   static const uint32_t pipeline_ribbon_vert[] =
#include "vulkan_shaders/pipeline_ribbon.vert.inc"
      ;

   static const uint32_t pipeline_ribbon_frag[] =
#include "vulkan_shaders/pipeline_ribbon.frag.inc"
      ;

   static const uint32_t pipeline_ribbon_simple_vert[] =
#include "vulkan_shaders/pipeline_ribbon_simple.vert.inc"
      ;

   static const uint32_t pipeline_ribbon_simple_frag[] =
#include "vulkan_shaders/pipeline_ribbon_simple.frag.inc"
      ;

   static const uint32_t pipeline_snow_simple_frag[] =
#include "vulkan_shaders/pipeline_snow_simple.frag.inc"
      ;

   static const uint32_t pipeline_snow_frag[] =
#include "vulkan_shaders/pipeline_snow.frag.inc"
      ;

   static const uint32_t pipeline_bokeh_frag[] =
#include "vulkan_shaders/pipeline_bokeh.frag.inc"
      ;

   gl->pipelines.alpha_blend = gl_core_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                             alpha_blend_frag, sizeof(alpha_blend_frag),
                                                             &gl->pipelines.alpha_blend_loc, true);
   if (!gl->pipelines.alpha_blend)
      return false;

   gl->pipelines.font = gl_core_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                      font_frag, sizeof(font_frag),
                                                      &gl->pipelines.font_loc, true);
   if (!gl->pipelines.font)
      return false;

   gl->pipelines.ribbon_simple = gl_core_cross_compile_program(pipeline_ribbon_simple_vert, sizeof(pipeline_ribbon_simple_vert),
                                                               pipeline_ribbon_simple_frag, sizeof(pipeline_ribbon_simple_frag),
                                                               &gl->pipelines.ribbon_simple_loc, true);
   if (!gl->pipelines.ribbon_simple)
      return false;

   gl->pipelines.ribbon = gl_core_cross_compile_program(pipeline_ribbon_vert, sizeof(pipeline_ribbon_vert),
                                                        pipeline_ribbon_frag, sizeof(pipeline_ribbon_frag),
                                                        &gl->pipelines.ribbon_loc, true);
   if (!gl->pipelines.ribbon)
      return false;

   gl->pipelines.bokeh = gl_core_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                       pipeline_bokeh_frag, sizeof(pipeline_bokeh_frag),
                                                       &gl->pipelines.bokeh_loc, true);
   if (!gl->pipelines.bokeh)
      return false;

   gl->pipelines.snow_simple = gl_core_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                             pipeline_snow_simple_frag, sizeof(pipeline_snow_simple_frag),
                                                             &gl->pipelines.snow_simple_loc, true);
   if (!gl->pipelines.snow_simple)
      return false;

   gl->pipelines.snow = gl_core_cross_compile_program(alpha_blend_vert, sizeof(alpha_blend_vert),
                                                      pipeline_snow_frag, sizeof(pipeline_snow_frag),
                                                      &gl->pipelines.snow_loc, true);
   if (!gl->pipelines.snow)
      return false;

   return true;
}

static bool gl_core_init_default_filter_chain(gl_core_t *gl)
{
   if (!gl->ctx_driver)
      return false;

   gl->filter_chain = gl_core_filter_chain_create_default(
         gl->video_info.smooth ?
         GL_CORE_FILTER_CHAIN_LINEAR : GL_CORE_FILTER_CHAIN_NEAREST);

   if (!gl->filter_chain)
   {
      RARCH_ERR("Failed to create filter chain.\n");
      return false;
   }

   return true;
}

static bool gl_core_init_filter_chain_preset(gl_core_t *gl, const char *shader_path)
{
   gl->filter_chain = gl_core_filter_chain_create_from_preset(
         shader_path,
         gl->video_info.smooth ?
         GL_CORE_FILTER_CHAIN_LINEAR : GL_CORE_FILTER_CHAIN_NEAREST);

   if (!gl->filter_chain)
   {
      RARCH_ERR("[Vulkan]: Failed to create preset: \"%s\".\n", shader_path);
      return false;
   }

   return true;
}

static bool gl_core_init_filter_chain(gl_core_t *gl)
{
   const char *shader_path = retroarch_get_shader_preset();

   enum rarch_shader_type type = video_shader_parse_type(shader_path, RARCH_SHADER_NONE);

   if (type == RARCH_SHADER_NONE)
   {
      RARCH_LOG("[GLCore]: Loading stock shader.\n");
      return gl_core_init_default_filter_chain(gl);
   }

   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_LOG("[GLCore]: Only SLANG shaders are supported, falling back to stock.\n");
      return gl_core_init_default_filter_chain(gl);
   }

   if (!shader_path || !gl_core_init_filter_chain_preset(gl, shader_path))
      gl_core_init_default_filter_chain(gl);

   return true;
}

#ifdef GL_DEBUG
#define DEBUG_CALLBACK_TYPE APIENTRY
static void DEBUG_CALLBACK_TYPE gl_core_debug_cb(GLenum source, GLenum type,
      GLuint id, GLenum severity, GLsizei length,
      const GLchar *message, void *userParam)
{
   const char      *src = NULL;
   const char *typestr  = NULL;
   gl_core_t *gl = (gl_core_t*)userParam; /* Useful for debugger. */

   (void)gl;
   (void)id;
   (void)length;

   switch (source)
   {
      case GL_DEBUG_SOURCE_API:
         src = "API";
         break;
      case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
         src = "Window system";
         break;
      case GL_DEBUG_SOURCE_SHADER_COMPILER:
         src = "Shader compiler";
         break;
      case GL_DEBUG_SOURCE_THIRD_PARTY:
         src = "3rd party";
         break;
      case GL_DEBUG_SOURCE_APPLICATION:
         src = "Application";
         break;
      case GL_DEBUG_SOURCE_OTHER:
         src = "Other";
         break;
      default:
         src = "Unknown";
         break;
   }

   switch (type)
   {
      case GL_DEBUG_TYPE_ERROR:
         typestr = "Error";
         break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
         typestr = "Deprecated behavior";
         break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
         typestr = "Undefined behavior";
         break;
      case GL_DEBUG_TYPE_PORTABILITY:
         typestr = "Portability";
         break;
      case GL_DEBUG_TYPE_PERFORMANCE:
         typestr = "Performance";
         break;
      case GL_DEBUG_TYPE_MARKER:
         typestr = "Marker";
         break;
      case GL_DEBUG_TYPE_PUSH_GROUP:
         typestr = "Push group";
         break;
      case GL_DEBUG_TYPE_POP_GROUP:
        typestr = "Pop group";
        break;
      case GL_DEBUG_TYPE_OTHER:
        typestr = "Other";
        break;
      default:
        typestr = "Unknown";
        break;
   }

   switch (severity)
   {
      case GL_DEBUG_SEVERITY_HIGH:
         RARCH_ERR("[GL debug (High, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_MEDIUM:
         RARCH_WARN("[GL debug (Medium, %s, %s)]: %s\n", src, typestr, message);
         break;
      case GL_DEBUG_SEVERITY_LOW:
         RARCH_LOG("[GL debug (Low, %s, %s)]: %s\n", src, typestr, message);
         break;
   }
}

static void gl_core_begin_debug(gl_core_t *gl)
{
   if (gl_check_capability(GL_CAPS_DEBUG))
   {
      glDebugMessageCallback(gl_core_debug_cb, gl);
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glEnable(GL_DEBUG_OUTPUT);
   }
   else
      RARCH_ERR("[GL]: Neither GL_KHR_debug nor GL_ARB_debug_output are implemented. Cannot start GL debugging.\n");
}
#endif

static void *gl_core_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   gfx_ctx_mode_t mode;
   gfx_ctx_input_t inp;
   unsigned full_x, full_y;
   settings_t *settings                 = config_get_ptr();
   int interval                         = 0;
   unsigned win_width                   = 0;
   unsigned win_height                  = 0;
   unsigned temp_width                  = 0;
   unsigned temp_height                 = 0;
   const char *vendor                   = NULL;
   const char *renderer                 = NULL;
   const char *version                  = NULL;
   gl_core_t *gl                        = (gl_core_t*)calloc(1, sizeof(gl_core_t));
   const gfx_ctx_driver_t *ctx_driver   = gl_core_get_context(gl);

   if (!gl || !ctx_driver)
      goto error;

   video_context_driver_set(ctx_driver);

   gl->ctx_driver = ctx_driver;
   gl->video_info = *video;

   RARCH_LOG("[GLCore]: Found GL context: %s\n", ctx_driver->ident);

   video_context_driver_get_video_size(&mode);

   full_x      = mode.width;
   full_y      = mode.height;
   mode.width  = 0;
   mode.height = 0;
   interval    = 0;

   RARCH_LOG("[GLCore]: Detecting screen resolution %ux%u.\n", full_x, full_y);

   if (video->vsync)
      interval = video->swap_interval;

   video_context_driver_swap_interval(&interval);

   win_width   = video->width;
   win_height  = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   mode.width      = win_width;
   mode.height     = win_height;
   mode.fullscreen = video->fullscreen;

   if (!video_context_driver_set_video_mode(&mode))
      goto error;

   rglgen_resolve_symbols(ctx_driver->get_proc_address);

#ifdef GL_DEBUG
   gl_core_begin_debug(gl);
#endif

   /* Clear out potential error flags in case we use cached context. */
   glGetError();

   vendor   = (const char*)glGetString(GL_VENDOR);
   renderer = (const char*)glGetString(GL_RENDERER);
   version  = (const char*)glGetString(GL_VERSION);

   RARCH_LOG("[GLCore]: Vendor: %s, Renderer: %s.\n", vendor, renderer);
   RARCH_LOG("[GLCore]: Version: %s.\n", version);

   if (string_is_equal(ctx_driver->ident, "null"))
      goto error;

   if (!gl_core_init_pipelines(gl))
   {
      RARCH_ERR("[GLCore]: Failed to cross-compile menu pipelines.\n");
      goto error;
   }

   if (!string_is_empty(version))
      sscanf(version, "%d.%d", &gl->version_major, &gl->version_minor);

   gl->vsync       = video->vsync;
   gl->fullscreen  = video->fullscreen;
   gl->keep_aspect = video->force_aspect;

   mode.width     = 0;
   mode.height    = 0;

   video_context_driver_get_video_size(&mode);
   temp_width     = mode.width;
   temp_height    = mode.height;

   mode.width     = 0;
   mode.height    = 0;

   /* Get real known video size, which might have been altered by context. */

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);
   video_driver_get_size(&temp_width, &temp_height);

   RARCH_LOG("[GLCore]: Using resolution %ux%u\n", temp_width, temp_height);

   inp.input      = input;
   inp.input_data = input_data;
   video_context_driver_input_driver(&inp);

   if (!gl_core_init_filter_chain(gl))
   {
      RARCH_ERR("[GLCore]: Failed to init filter chain.\n");
      goto error;
   }

   if (video->font_enable)
   {
      font_driver_init_osd(gl, false,
                           video->is_threaded,
                           FONT_DRIVER_RENDER_OPENGL_CORE_API);
   }

   glGenVertexArrays(1, &gl->vao);
   glBindVertexArray(gl->vao);

   return gl;

error:
   video_context_driver_destroy();
   gl_core_destroy_resources(gl);
   return NULL;
}

static void gl_core_free(void *data)
{
   gl_core_t *gl = (gl_core_t*)data;
   if (!gl)
      return;

   font_driver_free_osd();
   video_context_driver_free();
   gl_core_destroy_resources(gl);
}

static bool gl_core_alive(void *data)
{
   unsigned temp_width  = 0;
   unsigned temp_height = 0;
   bool ret             = false;
   bool quit            = false;
   bool resize          = false;
   gl_core_t *gl        = (gl_core_t*)data;
   bool is_shutdown     = rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL);

   /* Needed because some context drivers don't track their sizes */
   video_driver_get_size(&temp_width, &temp_height);

   gl->ctx_driver->check_window(gl->ctx_data,
                                &quit, &resize, &temp_width, &temp_height, is_shutdown);

   if (quit)
      gl->quitting = true;
   else if (resize)
      gl->should_resize = true;

   ret = !gl->quitting;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(&temp_width, &temp_height);

   return ret;
}

static void gl_core_set_nonblock_state(void *data, bool state)
{
   int interval                = 0;
   gl_core_t         *gl       = (gl_core_t*)data;
   settings_t        *settings = config_get_ptr();

   if (!gl)
      return;

   RARCH_LOG("[GLCore]: VSync => %s\n", state ? "off" : "on");

   if (!state)
      interval = settings->uints.video_swap_interval;

   video_context_driver_swap_interval(&interval);
}

static bool gl_core_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   return video_context_driver_suppress_screensaver(&enabled);
}

static bool gl_core_set_shader(void *data,
                               enum rarch_shader_type type, const char *path)
{
   gl_core_t *gl = (gl_core_t *)data;
   if (!gl)
      return false;

   if (type != RARCH_SHADER_SLANG && path)
   {
      RARCH_WARN("[GLCore]: Only .slang or .slangp shaders are supported. Falling back to stock.\n");
      path = NULL;
   }

   if (gl->filter_chain)
      gl_core_filter_chain_free(gl->filter_chain);
   gl->filter_chain = NULL;

   if (!path)
   {
      gl_core_init_default_filter_chain(gl);
      return true;
   }

   if (!gl_core_init_filter_chain_preset(gl, path))
   {
      RARCH_ERR("[Vulkan]: Failed to create filter chain: \"%s\". Falling back to stock.\n", path);
      gl_core_init_default_filter_chain(gl);
      return false;
   }

   return true;
}

static void gl_core_set_viewport_wrapper(void *data, unsigned viewport_width,
                                         unsigned viewport_height, bool force_full, bool allow_rotate)
{
   video_frame_info_t video_info;
   gl_core_t *gl = (gl_core_t*)data;

   video_driver_build_info(&video_info);

   gl_core_set_viewport(gl, &video_info,
                        viewport_width, viewport_height, force_full, allow_rotate);
}

static void gl_core_set_rotation(void *data, unsigned rotation)
{
   gl_core_t *gl = (gl_core_t*)data;

   if (!gl)
      return;

   gl->rotation = 90 * rotation;
   gl_core_set_projection(gl, &default_ortho, true);
}

static void gl_core_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   unsigned top_y, top_dist;
   gl_core_t *gl = (gl_core_t*)data;

   video_driver_get_size(&width, &height);

   *vp             = gl->vp;
   vp->full_width  = width;
   vp->full_height = height;

   /* Adjust as GL viewport is bottom-up. */
   top_y           = vp->y + vp->height;
   top_dist        = height - top_y;
   vp->y           = top_dist;
}

static bool gl_core_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   gl_core_t *gl = (gl_core_t*)data;
   if (!gl)
      return false;

   (void)data;
   (void)buffer;
   (void)is_idle;
   return false;
}

static void gl_core_update_cpu_texture(gl_core_t *gl,
                                       struct gl_core_streamed_texture *streamed,
                                       const void *frame, unsigned width, unsigned height, unsigned pitch)
{
   if (width != streamed->width || height != streamed->height)
   {
      if (streamed->tex != 0)
         glDeleteTextures(1, &streamed->tex);
      glGenTextures(1, &streamed->tex);
      glBindTexture(GL_TEXTURE_2D, streamed->tex);
      glTexStorage2D(GL_TEXTURE_2D, 1, gl->video_info.rgb32 ? GL_RGBA8 : GL_RGB565,
                     width, height);
      streamed->width = width;
      streamed->height = height;

      if (gl->video_info.rgb32)
      {
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
      }
   }
   else
      glBindTexture(GL_TEXTURE_2D, streamed->tex);

   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   if (gl->video_info.rgb32)
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> 2);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                      width, height, GL_RGBA, GL_UNSIGNED_BYTE, frame);
   }
   else
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> 1);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                      width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
   }
}

static void gl_core_draw_menu_texture(gl_core_t *gl, video_frame_info_t *video_info)
{
   glEnable(GL_BLEND);
   glDisable(GL_CULL_FACE);
   glDisable(GL_DEPTH_TEST);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   if (gl->menu_texture_full_screen)
      glViewport(0, 0, video_info->width, video_info->height);

   glActiveTexture(GL_TEXTURE0 + 1);
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);

   glUseProgram(gl->pipelines.alpha_blend);
   if (gl->pipelines.alpha_blend_loc.flat_ubo_vertex >= 0)
      glUniform4fv(gl->pipelines.alpha_blend_loc.flat_ubo_vertex, 4, gl->mvp_no_rot_yflip.data);

   const float vbo_data[] = {
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, gl->menu_texture_alpha,
   };

   // Crude, some round-robin system might be good.
   GLuint vbo;
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(vbo_data), vbo_data, GL_STREAM_DRAW);
   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);
   glEnableVertexAttribArray(2);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(uintptr_t)0);
   glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(uintptr_t)(2 * sizeof(float)));
   glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(uintptr_t)(4 * sizeof(float)));
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glDisableVertexAttribArray(0);
   glDisableVertexAttribArray(1);
   glDisableVertexAttribArray(2);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glDeleteBuffers(1, &vbo);

   glDisable(GL_BLEND);
}

static bool gl_core_frame(void *data, const void *frame,
                          unsigned frame_width, unsigned frame_height,
                          uint64_t frame_count,
                          unsigned pitch, const char *msg,
                          video_frame_info_t *video_info)
{
   struct gl_core_filter_chain_texture texture;
   struct gl_core_streamed_texture *streamed;
   gl_core_t *gl = (gl_core_t*)data;
   if (!gl)
      return false;

   streamed = &gl->textures[gl->textures_index];
   gl_core_update_cpu_texture(gl, streamed, frame, frame_width, frame_height, pitch);

   gl_core_set_viewport(gl, video_info, video_info->width, video_info->height, false, true);

   memset(&texture, 0, sizeof(texture));
   texture.image  = streamed->tex;
   texture.width  = streamed->width;
   texture.height = streamed->height;
   texture.format = gl->video_info.rgb32 ? GL_RGBA8 : GL_RGB565;
   gl_core_filter_chain_set_input_texture(gl->filter_chain, &texture);
   gl_core_filter_chain_build_offscreen_passes(gl->filter_chain, &gl->filter_chain_vp);

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
   glClear(GL_COLOR_BUFFER_BIT);
   gl_core_filter_chain_build_viewport_pass(gl->filter_chain, &gl->filter_chain_vp, gl->mvp_yflip.data);
   gl_core_filter_chain_end_frame(gl->filter_chain);

#if defined(HAVE_MENU)
   if (gl->menu_texture_enable)
   {
      menu_driver_frame(video_info);
      if (gl->menu_texture_enable && gl->menu_texture)
         gl_core_draw_menu_texture(gl, video_info);
   }
   else if (video_info->statistics_show)
   {
      struct font_params *osd_params = (struct font_params*)
         &video_info->osd_stat_params;

      if (osd_params)
      {
         font_driver_render_msg(video_info, NULL, video_info->stat_text,
               (const struct font_params*)&video_info->osd_stat_params);
      }
   }

#ifdef HAVE_MENU_WIDGETS
   menu_widgets_frame(video_info);
#endif
#endif

   if (!string_is_empty(msg))
   {
      //if (video_info->msg_bgcolor_enable)
      //   gl_core_render_osd_background(gl, video_info, msg);
      font_driver_render_msg(video_info, NULL, msg, NULL);
   }

   video_info->cb_update_window_title(
         video_info->context_data, video_info);
   video_info->cb_swap_buffers(video_info->context_data, video_info);

   gl->textures_index = (gl->textures_index + 1) & (GL_CORE_NUM_TEXTURES - 1);
   return true;
}

static uint32_t gl_core_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);

   return flags;
}

static float gl_core_get_refresh_rate(void *data)
{
   float refresh_rate;
   if (video_context_driver_get_refresh_rate(&refresh_rate))
       return refresh_rate;
   return 0.0f;
}

static void gl_core_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   gl_core_t *gl = (gl_core_t*)data;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel();
         break;

      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(
         aspectratio_lut[aspect_ratio_idx].value);

   if (!gl)
      return;

   gl->keep_aspect = true;
   gl->should_resize = true;
}

static void gl_core_apply_state_changes(void *data)
{
   gl_core_t *gl = (gl_core_t*)data;
   if (gl)
      gl->should_resize = true;
}

static struct video_shader *gl_core_get_current_shader(void *data)
{
   gl_core_t *gl = (gl_core_t*)data;
   if (!gl || !gl->filter_chain)
      return NULL;

   return gl_core_filter_chain_get_preset(gl->filter_chain);
}

static unsigned num_miplevels(unsigned width, unsigned height)
{
   unsigned levels = 1;
   if (width < height)
      width = height;
   while (width > 1)
   {
      levels++;
      width >>= 1;
   }
   return levels;
}

static void video_texture_load_gl_core(
      struct texture_image *ti,
      enum texture_filter_type filter_type,
      uintptr_t *idptr)
{
   /* Generate the OpenGL texture object */
   GLuint id;
   unsigned levels;
   GLenum mag_filter, min_filter;

   glGenTextures(1, &id);
   *idptr = id;
   glBindTexture(GL_TEXTURE_2D, id);

   levels = 1;
   if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR || filter_type == TEXTURE_FILTER_MIPMAP_NEAREST)
      levels = num_miplevels(ti->width, ti->height);

   glTexStorage2D(GL_TEXTURE_2D, levels, GL_RGBA8, ti->width, ti->height);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   switch (filter_type)
   {
      case TEXTURE_FILTER_LINEAR:
         mag_filter = GL_LINEAR;
         min_filter = GL_LINEAR;
         break;

      case TEXTURE_FILTER_NEAREST:
         mag_filter = GL_NEAREST;
         min_filter = GL_NEAREST;
         break;

      case TEXTURE_FILTER_MIPMAP_NEAREST:
         mag_filter = GL_LINEAR;
         min_filter = GL_LINEAR_MIPMAP_NEAREST;
         break;

      case TEXTURE_FILTER_MIPMAP_LINEAR:
         mag_filter = GL_LINEAR;
         min_filter = GL_LINEAR_MIPMAP_LINEAR;
         break;
   }
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);

   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                   ti->width, ti->height, GL_RGBA, GL_UNSIGNED_BYTE, ti->pixels);

   if (levels > 1)
      glGenerateMipmap(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, 0);
}

#ifdef HAVE_THREADS
static int video_texture_load_wrap_gl_core_mipmap(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl_core((struct texture_image*)data,
         TEXTURE_FILTER_MIPMAP_LINEAR, &id);
   return (int)id;
}

static int video_texture_load_wrap_gl_core(void *data)
{
   uintptr_t id = 0;

   if (!data)
      return 0;
   video_texture_load_gl_core((struct texture_image*)data,
         TEXTURE_FILTER_LINEAR, &id);
   return (int)id;
}
#endif

static uintptr_t gl_core_load_texture(void *video_data, void *data,
                                      bool threaded, enum texture_filter_type filter_type)
{
   uintptr_t id = 0;

#ifdef HAVE_THREADS
   if (threaded)
   {
      custom_command_method_t func = video_texture_load_wrap_gl_core;

      switch (filter_type)
      {
         case TEXTURE_FILTER_MIPMAP_LINEAR:
         case TEXTURE_FILTER_MIPMAP_NEAREST:
            func = video_texture_load_wrap_gl_core_mipmap;
            break;
         default:
            break;
      }
      return video_thread_texture_load(data, func);
   }
#endif

   video_texture_load_gl_core((struct texture_image*)data, filter_type, &id);
   return id;
}

static void gl_core_unload_texture(void *data, uintptr_t id)
{
   GLuint glid;
   if (!id)
      return;

   glid = (GLuint)id;
   glDeleteTextures(1, &glid);
}

static void gl_core_set_video_mode(void *data, unsigned width, unsigned height,
                                   bool fullscreen)
{
   gfx_ctx_mode_t mode;

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   video_context_driver_set_video_mode(&mode);
}

static void gl_core_show_mouse(void *data, bool state)
{
   video_context_driver_show_mouse(&state);
}

static void gl_core_set_osd_msg(void *data,
                                video_frame_info_t *video_info,
                                const char *msg,
                                const void *params, void *font)
{
   font_driver_render_msg(video_info, font, msg, (const struct font_params *)params);
}

static void gl_core_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   GLenum menu_filter;
   settings_t *settings = config_get_ptr();
   unsigned base_size   = rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   gl_core_t *gl        = (gl_core_t*)data;
   if (!gl)
      return;

   menu_filter = settings->bools.menu_linear_filter ? GL_LINEAR : GL_NEAREST;

   if (gl->menu_texture)
      glDeleteTextures(1, &gl->menu_texture);
   glGenTextures(1, &gl->menu_texture);
   glBindTexture(GL_TEXTURE_2D, gl->menu_texture);
   glTexStorage2D(GL_TEXTURE_2D, 1, rgb32 ? GL_RGBA8 : GL_RGBA4, width, height);

   glPixelStorei(GL_UNPACK_ALIGNMENT, base_size);
   glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                   width, height, GL_RGBA, rgb32 ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT_4_4_4_4, frame);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, menu_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, menu_filter);

   if (rgb32)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   gl->menu_texture_alpha = alpha;
}

static void gl_core_set_texture_enable(void *data, bool state, bool full_screen)
{
   gl_core_t *gl = (gl_core_t*)data;

   if (!gl)
      return;

   gl->menu_texture_enable      = state;
   gl->menu_texture_full_screen = full_screen;
}

static const video_poke_interface_t gl_core_poke_interface = {
   gl_core_get_flags,
   NULL,                   /* set_coords */
   NULL,                   /* set_mvp */
   gl_core_load_texture,
   gl_core_unload_texture,
   gl_core_set_video_mode,
   gl_core_get_refresh_rate, /* get_refresh_rate */
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   gl_core_set_aspect_ratio,
   gl_core_apply_state_changes,
   gl_core_set_texture_frame,
   gl_core_set_texture_enable,
   gl_core_set_osd_msg,
   gl_core_show_mouse,
   NULL,                               /* grab_mouse_toggle */
   gl_core_get_current_shader,
   NULL,
   NULL,
};

static void gl_core_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &gl_core_poke_interface;
}

video_driver_t video_gl_core = {
   gl_core_init,
   gl_core_frame,
   gl_core_set_nonblock_state,
   gl_core_alive,
   NULL,                    /* focus */
   gl_core_suppress_screensaver,
   NULL,                    /* has_windowed */

   gl_core_set_shader,

   gl_core_free,
   "glcore",

   gl_core_set_viewport_wrapper,
   gl_core_set_rotation,

   gl_core_viewport_info,

   gl_core_read_viewport,
#if defined(READ_RAW_GL_FRAME_TEST)
   gl_core_read_frame_raw,
#else
   NULL,
#endif

#ifdef HAVE_OVERLAY
   /*gl_core_get_overlay_interface,*/NULL,
#endif
   gl_core_get_poke_interface,
   /*gl_core_wrap_type_to_enum,*/NULL,
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   gl_core_menu_widgets_enabled
#endif
};
