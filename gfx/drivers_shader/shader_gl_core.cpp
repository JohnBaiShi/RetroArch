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

#include "shader_gl_core.h"
#include "glslang_util.h"

#include <vector>
#include <memory>
#include <functional>
#include <utility>
#include <algorithm>
#include <string.h>

#include <compat/strl.h>
#include <formats/image.h>
#include <retro_miscellaneous.h>

#include "slang_reflection.h"
#include "slang_reflection.hpp"

#include "../video_driver.h"
#include "../../verbosity.h"
#include "../../msg_hash.h"

using namespace std;

namespace gl_core
{
static const uint32_t opaque_vert[] =
#include "../drivers/vulkan_shaders/opaque.vert.inc"
;

static const uint32_t opaque_frag[] =
#include "../drivers/vulkan_shaders/opaque.frag.inc"
;

struct ConfigDeleter
{
   void operator()(config_file_t *conf)
   {
      if (conf)
         config_file_free(conf);
   }
};

static unsigned num_miplevels(unsigned width, unsigned height)
{
   unsigned size = MAX(width, height);
   unsigned levels = 0;
   while (size) {
      levels++;
      size >>= 1;
   }
   return levels;
}

static void build_identity_matrix(float *data)
{
   data[0] = 1.0f;
   data[1] = 0.0f;
   data[2] = 0.0f;
   data[3] = 0.0f;
   data[4] = 0.0f;
   data[5] = 1.0f;
   data[6] = 0.0f;
   data[7] = 0.0f;
   data[8] = 0.0f;
   data[9] = 0.0f;
   data[10] = 1.0f;
   data[11] = 0.0f;
   data[12] = 0.0f;
   data[13] = 0.0f;
   data[14] = 0.0f;
   data[15] = 1.0f;
}

static void build_vec4(float *data, unsigned width, unsigned height)
{
   data[0] = float(width);
   data[1] = float(height);
   data[2] = 1.0f / float(width);
   data[3] = 1.0f / float(height);
}

struct Size2D
{
   unsigned width, height;
};

struct Texture
{
   gl_core_filter_chain_texture texture;
   gl_core_filter_chain_filter filter;
   gl_core_filter_chain_filter mip_filter;
   gl_core_filter_chain_address address;
};

static gl_core_filter_chain_address wrap_to_address(gfx_wrap_type type)
{
   switch (type)
   {
      default:
      case RARCH_WRAP_EDGE:
         return GL_CORE_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;

      case RARCH_WRAP_BORDER:
         return GL_CORE_FILTER_CHAIN_ADDRESS_CLAMP_TO_BORDER;

      case RARCH_WRAP_REPEAT:
         return GL_CORE_FILTER_CHAIN_ADDRESS_REPEAT;

      case RARCH_WRAP_MIRRORED_REPEAT:
         return GL_CORE_FILTER_CHAIN_ADDRESS_MIRRORED_REPEAT;
   }
}

static GLenum address_to_gl(gl_core_filter_chain_address type)
{
   switch (type)
   {
      default:
      case GL_CORE_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE:
         return GL_CLAMP_TO_EDGE;

      case GL_CORE_FILTER_CHAIN_ADDRESS_CLAMP_TO_BORDER:
         return GL_CLAMP_TO_BORDER;

      case GL_CORE_FILTER_CHAIN_ADDRESS_REPEAT:
         return GL_REPEAT;

      case GL_CORE_FILTER_CHAIN_ADDRESS_MIRRORED_REPEAT:
         return GL_MIRRORED_REPEAT;
   }
}

static GLenum convert_glslang_format(glslang_format fmt)
{
#undef FMT
#define FMT(x, r) case SLANG_FORMAT_##x: return GL_##r
   switch (fmt)
   {
      FMT(R8_UNORM, R8);
      FMT(R8_SINT, R8I);
      FMT(R8_UINT, R8UI);
      FMT(R8G8_UNORM, RG8);
      FMT(R8G8_SINT, RG8I);
      FMT(R8G8_UINT, RG8UI);
      FMT(R8G8B8A8_UNORM, RGBA8);
      FMT(R8G8B8A8_SINT, RGBA8I);
      FMT(R8G8B8A8_UINT, RGBA8UI);
      FMT(R8G8B8A8_SRGB, SRGB8);

      FMT(A2B10G10R10_UNORM_PACK32, RGB10_A2);
      FMT(A2B10G10R10_UINT_PACK32, RGB10_A2UI);

      FMT(R16_UINT, R16UI);
      FMT(R16_SINT, R16I);
      FMT(R16_SFLOAT, R16F);
      FMT(R16G16_UINT, RG16UI);
      FMT(R16G16_SINT, RG16I);
      FMT(R16G16_SFLOAT, RG16F);
      FMT(R16G16B16A16_UINT, RGBA16UI);
      FMT(R16G16B16A16_SINT, RGBA16I);
      FMT(R16G16B16A16_SFLOAT, RGBA16F);

      FMT(R32_UINT, R32UI);
      FMT(R32_SINT, R32I);
      FMT(R32_SFLOAT, R32F);
      FMT(R32G32_UINT, RG32UI);
      FMT(R32G32_SINT, RG32I);
      FMT(R32G32_SFLOAT, RG32F);
      FMT(R32G32B32A32_UINT, RGBA32UI);
      FMT(R32G32B32A32_SINT, RGBA32I);
      FMT(R32G32B32A32_SFLOAT, RGBA32F);

      default:
         return 0;
   }
}

class StaticTexture
{
public:
   StaticTexture(string id,
                 GLuint image,
                 unsigned width, unsigned height,
                 bool linear,
                 bool mipmap,
                 GLenum address);
   ~StaticTexture();

   StaticTexture(StaticTexture&&) = delete;
   void operator=(StaticTexture&&) = delete;

   void set_id(string name)
   {
      id = move(name);
   }

   const string &get_id() const
   {
      return id;
   }

   const Texture &get_texture() const
   {
      return texture;
   }

private:
   GLuint image;
   string id;
   Texture texture;
};

struct CommonResources
{
   ~CommonResources();

   vector<Texture> original_history;
   vector<Texture> framebuffer_feedback;
   vector<Texture> pass_outputs;
   vector<unique_ptr<StaticTexture>> luts;

   unordered_map<string, slang_texture_semantic_map> texture_semantic_map;
   unordered_map<string, slang_texture_semantic_map> texture_semantic_uniform_map;
   unique_ptr<video_shader> shader_preset;
};

class Framebuffer
{
public:
   Framebuffer(const Size2D &max_size, GLenum format, unsigned max_levels);

   ~Framebuffer();
   Framebuffer(Framebuffer&&) = delete;
   void operator=(Framebuffer&&) = delete;

   void set_size(const Size2D &size, GLenum format = 0);

   const Size2D &get_size() const { return size; }
   GLenum get_format() const { return format; }
   GLuint get_image() const { return image; }
   GLuint get_framebuffer() const { return framebuffer; }

   void clear();
   void copy(GLuint image);

   unsigned get_levels() const { return levels; }
   void generate_mips();

private:
   GLuint image = 0;
   Size2D size;
   GLenum format;
   unsigned max_levels;
   unsigned levels           = 0;

   GLuint framebuffer = 0;

   void init();
   void init_framebuffer();
   void init_render_pass();
};

class Pass
{
public:
   Pass(bool final_pass) :
         final_pass(final_pass)
   {}

   ~Pass();

   Pass(Pass&&) = delete;
   void operator=(Pass&&) = delete;

   const Framebuffer &get_framebuffer() const
   {
      return *framebuffer;
   }

   Framebuffer *get_feedback_framebuffer()
   {
      return framebuffer_feedback.get();
   }

   Size2D set_pass_info(
         const Size2D &max_original,
         const Size2D &max_source,
         const gl_core_filter_chain_pass_info &info);

   void set_shader(GLenum stage,
                   const uint32_t *spirv,
                   size_t spirv_words);

   bool build();
   bool init_feedback();

   void build_commands(
         const Texture &original,
         const Texture &source,
         const gl_core_viewport &vp,
         const float *mvp);

   void set_frame_count(uint64_t count)
   {
      frame_count = count;
   }

   void set_frame_count_period(unsigned period)
   {
      frame_count_period = period;
   }

   void set_name(const char *name)
   {
      pass_name = name;
   }

   const string &get_name() const
   {
      return pass_name;
   }

   gl_core_filter_chain_filter get_source_filter() const
   {
      return pass_info.source_filter;
   }

   gl_core_filter_chain_filter get_mip_filter() const
   {
      return pass_info.mip_filter;
   }

   gl_core_filter_chain_address get_address_mode() const
   {
      return pass_info.address;
   }

   void set_common_resources(CommonResources *common)
   {
      this->common = common;
   }

   const slang_reflection &get_reflection() const
   {
      return reflection;
   }

   void set_pass_number(unsigned pass)
   {
      pass_number = pass;
   }

   void add_parameter(unsigned parameter_index, const std::string &id);

   void end_frame();
   void allocate_buffers();

private:
   bool final_pass;

   Size2D get_output_size(const Size2D &original_size,
                          const Size2D &max_source) const;

   GLuint pipeline = 0;
   CommonResources *common = nullptr;

   Size2D current_framebuffer_size;
   gl_core_viewport current_viewport;
   gl_core_filter_chain_pass_info pass_info;

   vector<uint32_t> vertex_shader;
   vector<uint32_t> fragment_shader;
   unique_ptr<Framebuffer> framebuffer;
   unique_ptr<Framebuffer> framebuffer_feedback;

   void clear_vk();
   bool init_pipeline();
   bool init_pipeline_layout();

   void set_texture(unsigned binding,
                    const Texture &texture);

   void set_semantic_texture(slang_texture_semantic semantic,
                             const Texture &texture);
   void set_semantic_texture_array(slang_texture_semantic semantic, unsigned index,
                                   const Texture &texture);

   void set_uniform_buffer(unsigned binding, const void *data, size_t range);

   slang_reflection reflection;
   void build_semantics(uint8_t *buffer,
                        const float *mvp, const Texture &original, const Texture &source);
   void build_semantic_vec4(uint8_t *data, slang_semantic semantic,
                            unsigned width, unsigned height);
   void build_semantic_uint(uint8_t *data, slang_semantic semantic, uint32_t value);
   void build_semantic_parameter(uint8_t *data, unsigned index, float value);
   void build_semantic_texture_vec4(uint8_t *data,
                                    slang_texture_semantic semantic,
                                    unsigned width, unsigned height);
   void build_semantic_texture_array_vec4(uint8_t *data,
                                          slang_texture_semantic semantic, unsigned index,
                                          unsigned width, unsigned height);
   void build_semantic_texture(uint8_t *buffer,
                               slang_texture_semantic semantic, const Texture &texture);
   void build_semantic_texture_array(uint8_t *buffer,
                                     slang_texture_semantic semantic, unsigned index, const Texture &texture);

   uint64_t frame_count = 0;
   unsigned frame_count_period = 0;
   unsigned pass_number = 0;

   size_t ubo_offset = 0;
   string pass_name;

   struct Parameter
   {
      string id;
      unsigned index;
      unsigned semantic_index;
   };

   vector<Parameter> parameters;
   vector<Parameter> filtered_parameters;
   vector<uint32_t> push_constant_buffer;
};
}

struct gl_core_filter_chain
{
public:
   gl_core_filter_chain();
   ~gl_core_filter_chain();

   inline void set_shader_preset(unique_ptr<video_shader> shader)
   {
      common.shader_preset = move(shader);
   }

   inline video_shader *get_shader_preset()
   {
      return common.shader_preset.get();
   }

   void set_pass_info(unsigned pass,
                      const gl_core_filter_chain_pass_info &info);
   void set_shader(unsigned pass, GLenum stage,
                   const uint32_t *spirv, size_t spirv_words);

   bool init();

   void notify_sync_index(unsigned index);
   void set_input_texture(const gl_core_filter_chain_texture &texture);
   void build_offscreen_passes(const gl_core_viewport &vp);
   void build_viewport_pass(const gl_core_viewport &vp, const float *mvp);
   void end_frame();

   void set_frame_count(uint64_t count);
   void set_frame_count_period(unsigned pass, unsigned period);
   void set_pass_name(unsigned pass, const char *name);

   void add_static_texture(unique_ptr<gl_core::StaticTexture> texture);
   void add_parameter(unsigned pass, unsigned parameter_index, const std::string &id);
   void release_staging_buffers();

private:
   vector<unique_ptr<gl_core::Pass>> passes;
   vector<gl_core_filter_chain_pass_info> pass_info;
   vector<vector<function<void ()>>> deferred_calls;
   gl_core::CommonResources common;
   GLenum original_format;

   gl_core_filter_chain_texture input_texture;

   gl_core::Size2D max_input_size;
   unsigned current_sync_index;

   void flush();

   void set_num_passes(unsigned passes);
   void execute_deferred();
   void set_num_sync_indices(unsigned num_indices);

   bool init_ubo();
   bool init_history();
   bool init_feedback();
   bool init_alias();
   vector<unique_ptr<gl_core::Framebuffer>> original_history;
   bool require_clear = false;
   void clear_history_and_feedback();
   void update_feedback_info();
   void update_history_info();
};

static unique_ptr<gl_core::StaticTexture> gl_core_filter_chain_load_lut(
      gl_core_filter_chain *chain,
      const video_shader_lut *shader)
{
   texture_image image = {};
   image.supports_rgba = true;

   if (!image_texture_load(&image, shader->path))
      return {};

   unsigned levels = shader->mipmap ? gl_core::num_miplevels(image.width, image.height) : 1;
   GLuint tex = 0;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexStorage2D(GL_TEXTURE_2D, levels,
                  GL_RGBA8, image.width, image.height);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                   image.width, image.height,
                   GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);

   if (levels > 1)
      glGenerateMipmap(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, 0);

   if (image.pixels)
      image_texture_free(&image);

   return unique_ptr<gl_core::StaticTexture>(new gl_core::StaticTexture(shader->id,
                                                                        tex, image.width, image.height,
                                                                        shader->filter != RARCH_FILTER_NEAREST,
                                                                        levels > 1,
                                                                        gl_core::address_to_gl(gl_core::wrap_to_address(shader->wrap))));
}

static bool gl_core_filter_chain_load_luts(
      gl_core_filter_chain *chain,
      video_shader *shader)
{
   for (unsigned i = 0; i < shader->luts; i++)
   {
      auto image = gl_core_filter_chain_load_lut(chain, &shader->lut[i]);
      if (!image)
      {
         RARCH_ERR("[Vulkan]: Failed to load LUT \"%s\".\n", shader->lut[i].path);
         return false;
      }

      chain->add_static_texture(move(image));
   }

   return true;
}


gl_core_filter_chain_t *gl_core_filter_chain_create_from_preset(
      const struct gl_core_filter_chain_create_info *info,
      const char *path, gl_core_filter_chain_filter filter)
{
   unsigned i;
   unique_ptr<video_shader> shader{ new video_shader() };
   if (!shader)
      return nullptr;

   unique_ptr<config_file_t, gl_core::ConfigDeleter> conf{ config_file_new(path) };
   if (!conf)
      return nullptr;

   if (!video_shader_read_conf_cgp(conf.get(), shader.get()))
      return nullptr;

   video_shader_resolve_relative(shader.get(), path);

   bool last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.valid;

   unique_ptr<gl_core_filter_chain> chain{ new gl_core_filter_chain() };
   if (!chain)
      return nullptr;

   if (shader->luts && !gl_core_filter_chain_load_luts(chain.get(), shader.get()))
      return nullptr;

   shader->num_parameters = 0;

   for (i = 0; i < shader->passes; i++)
   {
      glslang_output output;
      struct gl_core_filter_chain_pass_info pass_info;
      const video_shader_pass *pass      = &shader->pass[i];
      const video_shader_pass *next_pass =
         i + 1 < shader->passes ? &shader->pass[i + 1] : nullptr;

      pass_info.scale_type_x  = GL_CORE_FILTER_CHAIN_SCALE_ORIGINAL;
      pass_info.scale_type_y  = GL_CORE_FILTER_CHAIN_SCALE_ORIGINAL;
      pass_info.scale_x       = 0.0f;
      pass_info.scale_y       = 0.0f;
      pass_info.rt_format     = 0;
      pass_info.source_filter = GL_CORE_FILTER_CHAIN_LINEAR;
      pass_info.mip_filter    = GL_CORE_FILTER_CHAIN_LINEAR;
      pass_info.address       = GL_CORE_FILTER_CHAIN_ADDRESS_REPEAT;
      pass_info.max_levels    = 0;

      if (!glslang_compile_shader(pass->source.path, &output))
      {
         RARCH_ERR("Failed to compile shader: \"%s\".\n",
               pass->source.path);
         return nullptr;
      }

      for (auto &meta_param : output.meta.parameters)
      {
         if (shader->num_parameters >= GFX_MAX_PARAMETERS)
         {
            RARCH_ERR("[Vulkan]: Exceeded maximum number of parameters.\n");
            return nullptr;
         }

         auto itr = find_if(shader->parameters, shader->parameters + shader->num_parameters,
               [&](const video_shader_parameter &param)
               {
                  return meta_param.id == param.id;
               });

         if (itr != shader->parameters + shader->num_parameters)
         {
            /* Allow duplicate #pragma parameter, but
             * only if they are exactly the same. */
            if (meta_param.desc != itr->desc ||
                meta_param.initial != itr->initial ||
                meta_param.minimum != itr->minimum ||
                meta_param.maximum != itr->maximum ||
                meta_param.step != itr->step)
            {
               RARCH_ERR("[Vulkan]: Duplicate parameters found for \"%s\", but arguments do not match.\n",
                     itr->id);
               return nullptr;
            }
            chain->add_parameter(i, itr - shader->parameters, meta_param.id);
         }
         else
         {
            auto &param = shader->parameters[shader->num_parameters];
            strlcpy(param.id, meta_param.id.c_str(), sizeof(param.id));
            strlcpy(param.desc, meta_param.desc.c_str(), sizeof(param.desc));
            param.current = meta_param.initial;
            param.initial = meta_param.initial;
            param.minimum = meta_param.minimum;
            param.maximum = meta_param.maximum;
            param.step = meta_param.step;
            chain->add_parameter(i, shader->num_parameters, meta_param.id);
            shader->num_parameters++;
         }
      }

      chain->set_shader(i,
            GL_VERTEX_SHADER,
            output.vertex.data(),
            output.vertex.size());

      chain->set_shader(i,
            GL_FRAGMENT_SHADER,
            output.fragment.data(),
            output.fragment.size());

      chain->set_frame_count_period(i, pass->frame_count_mod);

      if (!output.meta.name.empty())
         chain->set_pass_name(i, output.meta.name.c_str());

      /* Preset overrides. */
      if (*pass->alias)
         chain->set_pass_name(i, pass->alias);

      if (pass->filter == RARCH_FILTER_UNSPEC)
         pass_info.source_filter = filter;
      else
      {
         pass_info.source_filter =
            pass->filter == RARCH_FILTER_LINEAR ? GL_CORE_FILTER_CHAIN_LINEAR :
            GL_CORE_FILTER_CHAIN_NEAREST;
      }
      pass_info.address    = gl_core::wrap_to_address(pass->wrap);
      pass_info.max_levels = 1;

      /* TODO: Expose max_levels in slangp.
       * CGP format is a bit awkward in that it uses mipmap_input,
       * so we much check if next pass needs the mipmapping.
       */
      if (next_pass && next_pass->mipmap)
         pass_info.max_levels = ~0u;

      pass_info.mip_filter = pass->filter != RARCH_FILTER_NEAREST && pass_info.max_levels > 1
         ? GL_CORE_FILTER_CHAIN_LINEAR : GL_CORE_FILTER_CHAIN_NEAREST;

      bool explicit_format = output.meta.rt_format != SLANG_FORMAT_UNKNOWN;

      /* Set a reasonable default. */
      if (output.meta.rt_format == SLANG_FORMAT_UNKNOWN)
         output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_UNORM;

      if (!pass->fbo.valid)
      {
         pass_info.scale_type_x = i + 1 == shader->passes
            ? GL_CORE_FILTER_CHAIN_SCALE_VIEWPORT
            : GL_CORE_FILTER_CHAIN_SCALE_SOURCE;
         pass_info.scale_type_y = i + 1 == shader->passes
            ? GL_CORE_FILTER_CHAIN_SCALE_VIEWPORT
            : GL_CORE_FILTER_CHAIN_SCALE_SOURCE;
         pass_info.scale_x = 1.0f;
         pass_info.scale_y = 1.0f;

         if (i + 1 == shader->passes)
         {
            pass_info.rt_format = 0;

            if (explicit_format)
               RARCH_WARN("[slang]: Using explicit format for last pass in chain,"
                     " but it is not rendered to framebuffer, using swapchain format instead.\n");
         }
         else
         {
            pass_info.rt_format = gl_core::convert_glslang_format(output.meta.rt_format);
            RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
                  glslang_format_to_string(output.meta.rt_format), i);
         }
      }
      else
      {
         /* Preset overrides shader.
          * Kinda ugly ... */
         if (pass->fbo.srgb_fbo)
            output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_SRGB;
         else if (pass->fbo.fp_fbo)
            output.meta.rt_format = SLANG_FORMAT_R16G16B16A16_SFLOAT;

         pass_info.rt_format = gl_core::convert_glslang_format(output.meta.rt_format);
         RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
               glslang_format_to_string(output.meta.rt_format), i);

         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_x = pass->fbo.scale_x;
               pass_info.scale_type_x = GL_CORE_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_x = float(pass->fbo.abs_x);
               pass_info.scale_type_x = GL_CORE_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_x = pass->fbo.scale_x;
               pass_info.scale_type_x = GL_CORE_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_y = pass->fbo.scale_y;
               pass_info.scale_type_y = GL_CORE_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_y = float(pass->fbo.abs_y);
               pass_info.scale_type_y = GL_CORE_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_y = pass->fbo.scale_y;
               pass_info.scale_type_y = GL_CORE_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }
      }

      chain->set_pass_info(i, pass_info);
   }

   if (last_pass_is_fbo)
   {
      struct gl_core_filter_chain_pass_info pass_info;

      pass_info.scale_type_x  = GL_CORE_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_type_y  = GL_CORE_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_x       = 1.0f;
      pass_info.scale_y       = 1.0f;

      pass_info.rt_format     = 0;

      pass_info.source_filter = filter;
      pass_info.mip_filter    = GL_CORE_FILTER_CHAIN_NEAREST;
      pass_info.address       = GL_CORE_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;

      pass_info.max_levels    = 0;

      chain->set_pass_info(shader->passes, pass_info);

      chain->set_shader(shader->passes,
            GL_VERTEX_SHADER,
            gl_core::opaque_vert,
            sizeof(gl_core::opaque_vert) / sizeof(uint32_t));

      chain->set_shader(shader->passes,
            GL_FRAGMENT_SHADER,
            gl_core::opaque_frag,
            sizeof(gl_core::opaque_frag) / sizeof(uint32_t));
   }

   if (!video_shader_resolve_current_parameters(conf.get(), shader.get()))
      return nullptr;

   chain->set_shader_preset(move(shader));

   if (!chain->init())
      return nullptr;

   return chain.release();
}

struct video_shader *gl_core_filter_chain_get_preset(
      gl_core_filter_chain_t *chain)
{
   return chain->get_shader_preset();
}

void gl_core_filter_chain_free(
      gl_core_filter_chain_t *chain)
{
   delete chain;
}

void gl_core_filter_chain_set_shader(
      gl_core_filter_chain_t *chain,
      unsigned pass,
      GLenum shader_stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   chain->set_shader(pass, shader_stage, spirv, spirv_words);
}

void gl_core_filter_chain_set_pass_info(
      gl_core_filter_chain_t *chain,
      unsigned pass,
      const struct gl_core_filter_chain_pass_info *info)
{
   chain->set_pass_info(pass, *info);
}

bool gl_core_filter_chain_init(gl_core_filter_chain_t *chain)
{
   return chain->init();
}

void gl_core_filter_chain_set_input_texture(
      gl_core_filter_chain_t *chain,
      const struct gl_core_filter_chain_texture *texture)
{
   chain->set_input_texture(*texture);
}

void gl_core_filter_chain_set_frame_count(
      gl_core_filter_chain_t *chain,
      uint64_t count)
{
   chain->set_frame_count(count);
}

void gl_core_filter_chain_set_frame_count_period(
      gl_core_filter_chain_t *chain,
      unsigned pass,
      unsigned period)
{
   chain->set_frame_count_period(pass, period);
}

void gl_core_filter_chain_set_pass_name(
      gl_core_filter_chain_t *chain,
      unsigned pass,
      const char *name)
{
   chain->set_pass_name(pass, name);
}

void gl_core_filter_chain_build_offscreen_passes(
      gl_core_filter_chain_t *chain,
      const gl_core_viewport *vp)
{
   chain->build_offscreen_passes(*vp);
}

void gl_core_filter_chain_build_viewport_pass(
      gl_core_filter_chain_t *chain,
      const gl_core_viewport *vp, const float *mvp)
{
   chain->build_viewport_pass(*vp, mvp);
}

void gl_core_filter_chain_end_frame(gl_core_filter_chain_t *chain)
{
   chain->end_frame();
}
