/*
 * Copyright 2007 VMware, Inc.
 * Copyright 2016 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file
 *
 * Common helper functions for PBO up- and downloads.
 */

#include "state_tracker/st_context.h"
#include "state_tracker/st_nir.h"
#include "state_tracker/st_pbo.h"
#include "state_tracker/st_cb_bufferobjects.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "cso_cache/cso_context.h"
#include "tgsi/tgsi_ureg.h"
#include "util/format/u_format.h"
#include "util/u_inlines.h"
#include "util/u_upload_mgr.h"

#include "compiler/nir/nir_builder.h"

/* Conversion to apply in the fragment shader. */
enum st_pbo_conversion {
   ST_PBO_CONVERT_NONE = 0,
   ST_PBO_CONVERT_UINT_TO_SINT,
   ST_PBO_CONVERT_SINT_TO_UINT,

   ST_NUM_PBO_CONVERSIONS
};

/* Final setup of buffer addressing information.
 *
 * buf_offset is in pixels.
 *
 * Returns false if something (e.g. alignment) prevents PBO upload/download.
 */
bool
st_pbo_addresses_setup(struct st_context *st,
                       struct pipe_resource *buf, intptr_t buf_offset,
                       struct st_pbo_addresses *addr)
{
   unsigned skip_pixels;

   /* Check alignment against texture buffer requirements. */
   {
      unsigned ofs = (buf_offset * addr->bytes_per_pixel) % st->ctx->Const.TextureBufferOffsetAlignment;
      if (ofs != 0) {
         if (ofs % addr->bytes_per_pixel != 0)
            return false;

         skip_pixels = ofs / addr->bytes_per_pixel;
         buf_offset -= skip_pixels;
      } else {
         skip_pixels = 0;
      }
   }

   assert(buf_offset >= 0);

   addr->buffer = buf;
   addr->first_element = buf_offset;
   addr->last_element = buf_offset + skip_pixels + addr->width - 1
         + (addr->height - 1 + (addr->depth - 1) * addr->image_height) * addr->pixels_per_row;

   if (addr->last_element - addr->first_element > st->ctx->Const.MaxTextureBufferSize - 1)
      return false;

   /* This should be ensured by Mesa before calling our callbacks */
   assert((addr->last_element + 1) * addr->bytes_per_pixel <= buf->width0);

   addr->constants.xoffset = -addr->xoffset + skip_pixels;
   addr->constants.yoffset = -addr->yoffset;
   addr->constants.stride = addr->pixels_per_row;
   addr->constants.image_size = addr->pixels_per_row * addr->image_height;
   addr->constants.layer_offset = 0;

   return true;
}

/* Validate and fill buffer addressing information based on GL pixelstore
 * attributes.
 *
 * Returns false if some aspect of the addressing (e.g. alignment) prevents
 * PBO upload/download.
 */
bool
st_pbo_addresses_pixelstore(struct st_context *st,
                            GLenum gl_target, bool skip_images,
                            const struct gl_pixelstore_attrib *store,
                            const void *pixels,
                            struct st_pbo_addresses *addr)
{
   struct pipe_resource *buf = st_buffer_object(store->BufferObj)->buffer;
   intptr_t buf_offset = (intptr_t) pixels;

   if (buf_offset % addr->bytes_per_pixel)
      return false;

   /* Convert to texels */
   buf_offset = buf_offset / addr->bytes_per_pixel;

   /* Determine image height */
   if (gl_target == GL_TEXTURE_1D_ARRAY) {
      addr->image_height = 1;
   } else {
      addr->image_height = store->ImageHeight > 0 ? store->ImageHeight : addr->height;
   }

   /* Compute the stride, taking store->Alignment into account */
   {
       unsigned pixels_per_row = store->RowLength > 0 ?
                           store->RowLength : addr->width;
       unsigned bytes_per_row = pixels_per_row * addr->bytes_per_pixel;
       unsigned remainder = bytes_per_row % store->Alignment;
       unsigned offset_rows;

       if (remainder > 0)
          bytes_per_row += store->Alignment - remainder;

       if (bytes_per_row % addr->bytes_per_pixel)
          return false;

       addr->pixels_per_row = bytes_per_row / addr->bytes_per_pixel;

       offset_rows = store->SkipRows;
       if (skip_images)
          offset_rows += addr->image_height * store->SkipImages;

       buf_offset += store->SkipPixels + addr->pixels_per_row * offset_rows;
   }

   if (!st_pbo_addresses_setup(st, buf, buf_offset, addr))
      return false;

   /* Support GL_PACK_INVERT_MESA */
   if (store->Invert) {
      addr->constants.xoffset += (addr->height - 1) * addr->constants.stride;
      addr->constants.stride = -addr->constants.stride;
   }

   return true;
}

/* For download from a framebuffer, we may have to invert the Y axis. The
 * setup is as follows:
 * - set viewport to inverted, so that the position sysval is correct for
 *   texel fetches
 * - this function adjusts the fragment shader's constant buffer to compute
 *   the correct destination addresses.
 */
void
st_pbo_addresses_invert_y(struct st_pbo_addresses *addr,
                          unsigned viewport_height)
{
   addr->constants.xoffset +=
      (viewport_height - 1 + 2 * addr->constants.yoffset) * addr->constants.stride;
   addr->constants.stride = -addr->constants.stride;
}

/* Setup all vertex pipeline state, rasterizer state, and fragment shader
 * constants, and issue the draw call for PBO upload/download.
 *
 * The caller is responsible for saving and restoring state, as well as for
 * setting other fragment shader state (fragment shader, samplers), and
 * framebuffer/viewport/DSA/blend state.
 */
bool
st_pbo_draw(struct st_context *st, const struct st_pbo_addresses *addr,
            unsigned surface_width, unsigned surface_height)
{
   struct cso_context *cso = st->cso_context;

   /* Setup vertex and geometry shaders */
   if (!st->pbo.vs) {
      st->pbo.vs = st_pbo_create_vs(st);
      if (!st->pbo.vs)
         return false;
   }

   if (addr->depth != 1 && st->pbo.use_gs && !st->pbo.gs) {
      st->pbo.gs = st_pbo_create_gs(st);
      if (!st->pbo.gs)
         return false;
   }

   cso_set_vertex_shader_handle(cso, st->pbo.vs);

   cso_set_geometry_shader_handle(cso, addr->depth != 1 ? st->pbo.gs : NULL);

   cso_set_tessctrl_shader_handle(cso, NULL);

   cso_set_tesseval_shader_handle(cso, NULL);

   /* Upload vertices */
   {
      struct pipe_vertex_buffer vbo = {0};
      struct cso_velems_state velem;

      float x0 = (float) addr->xoffset / surface_width * 2.0f - 1.0f;
      float y0 = (float) addr->yoffset / surface_height * 2.0f - 1.0f;
      float x1 = (float) (addr->xoffset + addr->width) / surface_width * 2.0f - 1.0f;
      float y1 = (float) (addr->yoffset + addr->height) / surface_height * 2.0f - 1.0f;

      float *verts = NULL;

      vbo.stride = 2 * sizeof(float);

      u_upload_alloc(st->pipe->stream_uploader, 0, 8 * sizeof(float), 4,
                     &vbo.buffer_offset, &vbo.buffer.resource, (void **) &verts);
      if (!verts)
         return false;

      verts[0] = x0;
      verts[1] = y0;
      verts[2] = x0;
      verts[3] = y1;
      verts[4] = x1;
      verts[5] = y0;
      verts[6] = x1;
      verts[7] = y1;

      u_upload_unmap(st->pipe->stream_uploader);

      velem.count = 1;
      velem.velems[0].src_offset = 0;
      velem.velems[0].instance_divisor = 0;
      velem.velems[0].vertex_buffer_index = 0;
      velem.velems[0].src_format = PIPE_FORMAT_R32G32_FLOAT;

      cso_set_vertex_elements(cso, &velem);

      cso_set_vertex_buffers(cso, 0, 1, &vbo);

      pipe_resource_reference(&vbo.buffer.resource, NULL);
   }

   /* Upload constants */
   {
      struct pipe_constant_buffer cb;

      cb.buffer = NULL;
      cb.user_buffer = &addr->constants;
      cb.buffer_offset = 0;
      cb.buffer_size = sizeof(addr->constants);

      cso_set_constant_buffer(cso, PIPE_SHADER_FRAGMENT, 0, &cb);

      pipe_resource_reference(&cb.buffer, NULL);
   }

   /* Rasterizer state */
   cso_set_rasterizer(cso, &st->pbo.raster);

   /* Disable stream output */
   cso_set_stream_outputs(cso, 0, NULL, 0);

   if (addr->depth == 1) {
      cso_draw_arrays(cso, PIPE_PRIM_TRIANGLE_STRIP, 0, 4);
   } else {
      cso_draw_arrays_instanced(cso, PIPE_PRIM_TRIANGLE_STRIP,
                                0, 4, 0, addr->depth);
   }

   return true;
}

void *
st_pbo_create_vs(struct st_context *st)
{
   const struct glsl_type *vec4 = glsl_vec4_type();
   const nir_shader_compiler_options *options =
      st_get_nir_compiler_options(st, MESA_SHADER_VERTEX);

   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_VERTEX, options,
                                                  "st/pbo VS");

   nir_variable *in_pos = nir_variable_create(b.shader, nir_var_shader_in,
                                              vec4, "in_pos");
   in_pos->data.location = VERT_ATTRIB_POS;

   nir_variable *out_pos = nir_variable_create(b.shader, nir_var_shader_out,
                                               vec4, "out_pos");
   out_pos->data.location = VARYING_SLOT_POS;
   out_pos->data.interpolation = INTERP_MODE_NONE;

   nir_copy_var(&b, out_pos, in_pos);

   if (st->pbo.layers) {
      nir_variable *instance_id = nir_variable_create(b.shader,
                                                      nir_var_system_value,
                                                      glsl_int_type(),
                                                      "instance_id");
      instance_id->data.location = SYSTEM_VALUE_INSTANCE_ID;

      if (st->pbo.use_gs) {
         unsigned swiz_x[4] = {0, 0, 0, 0};
         nir_store_var(&b, out_pos,
                       nir_swizzle(&b, nir_i2f32(&b, nir_load_var(&b, instance_id)), swiz_x, 4),
                       (1 << 2));
      } else {
         nir_variable *out_layer = nir_variable_create(b.shader,
                                                     nir_var_shader_out,
                                                     glsl_int_type(),
                                                     "out_layer");
         out_layer->data.location = VARYING_SLOT_LAYER;
         out_layer->data.interpolation = INTERP_MODE_NONE;
         nir_copy_var(&b, out_layer, instance_id);
      }
   }

   return st_nir_finish_builtin_shader(st, b.shader);
}

void *
st_pbo_create_gs(struct st_context *st)
{
   static const int zero = 0;
   struct ureg_program *ureg;
   struct ureg_dst out_pos;
   struct ureg_dst out_layer;
   struct ureg_src in_pos;
   struct ureg_src imm;
   unsigned i;

   ureg = ureg_create(PIPE_SHADER_GEOMETRY);
   if (!ureg)
      return NULL;

   ureg_property(ureg, TGSI_PROPERTY_GS_INPUT_PRIM, PIPE_PRIM_TRIANGLES);
   ureg_property(ureg, TGSI_PROPERTY_GS_OUTPUT_PRIM, PIPE_PRIM_TRIANGLE_STRIP);
   ureg_property(ureg, TGSI_PROPERTY_GS_MAX_OUTPUT_VERTICES, 3);

   out_pos = ureg_DECL_output(ureg, TGSI_SEMANTIC_POSITION, 0);
   out_layer = ureg_DECL_output(ureg, TGSI_SEMANTIC_LAYER, 0);

   in_pos = ureg_DECL_input(ureg, TGSI_SEMANTIC_POSITION, 0, 0, 1);

   imm = ureg_DECL_immediate_int(ureg, &zero, 1);

   for (i = 0; i < 3; ++i) {
      struct ureg_src in_pos_vertex = ureg_src_dimension(in_pos, i);

      /* out_pos = in_pos[i] */
      ureg_MOV(ureg, out_pos, in_pos_vertex);

      /* out_layer.x = f2i(in_pos[i].z) */
      ureg_F2I(ureg, ureg_writemask(out_layer, TGSI_WRITEMASK_X),
                     ureg_scalar(in_pos_vertex, TGSI_SWIZZLE_Z));

      ureg_EMIT(ureg, ureg_scalar(imm, TGSI_SWIZZLE_X));
   }

   ureg_END(ureg);

   return ureg_create_shader_and_destroy(ureg, st->pipe);
}

static const struct glsl_type *
sampler_type_for_target(enum pipe_texture_target target)
{
   bool is_array = target >= PIPE_TEXTURE_1D_ARRAY;
   static const enum glsl_sampler_dim dim[] = {
      [PIPE_BUFFER]             = GLSL_SAMPLER_DIM_BUF,
      [PIPE_TEXTURE_1D]         = GLSL_SAMPLER_DIM_1D,
      [PIPE_TEXTURE_2D]         = GLSL_SAMPLER_DIM_2D,
      [PIPE_TEXTURE_3D]         = GLSL_SAMPLER_DIM_3D,
      [PIPE_TEXTURE_CUBE]       = GLSL_SAMPLER_DIM_CUBE,
      [PIPE_TEXTURE_RECT]       = GLSL_SAMPLER_DIM_RECT,
      [PIPE_TEXTURE_1D_ARRAY]   = GLSL_SAMPLER_DIM_1D,
      [PIPE_TEXTURE_2D_ARRAY]   = GLSL_SAMPLER_DIM_2D,
      [PIPE_TEXTURE_CUBE_ARRAY] = GLSL_SAMPLER_DIM_CUBE,
   };

   return glsl_sampler_type(dim[target], false, is_array, GLSL_TYPE_FLOAT);
}


static void *
create_fs(struct st_context *st, bool download,
          enum pipe_texture_target target,
          enum st_pbo_conversion conversion)
{
   struct pipe_screen *screen = st->pipe->screen;
   const nir_shader_compiler_options *options =
      st_get_nir_compiler_options(st, MESA_SHADER_FRAGMENT);
   bool pos_is_sysval =
      screen->get_param(screen, PIPE_CAP_TGSI_FS_POSITION_IS_SYSVAL);

   nir_builder b = nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT, options,
                                                  download ?
                                                  "st/pbo download FS" :
                                                  "st/pbo upload FS");

   nir_ssa_def *zero = nir_imm_int(&b, 0);

   /* param = [ -xoffset + skip_pixels, -yoffset, stride, image_height ] */
   nir_variable *param_var =
      nir_variable_create(b.shader, nir_var_uniform, glsl_vec4_type(), "param");
   b.shader->num_uniforms += 4;
   nir_ssa_def *param = nir_load_var(&b, param_var);

   nir_variable *fragcoord =
      nir_variable_create(b.shader, pos_is_sysval ? nir_var_system_value :
                          nir_var_shader_in, glsl_vec4_type(), "gl_FragCoord");
   fragcoord->data.location = pos_is_sysval ? SYSTEM_VALUE_FRAG_COORD
                                            : VARYING_SLOT_POS;
   nir_ssa_def *coord = nir_load_var(&b, fragcoord);

   nir_ssa_def *layer = NULL;
   if (st->pbo.layers && (!download || target == PIPE_TEXTURE_1D_ARRAY ||
                                       target == PIPE_TEXTURE_2D_ARRAY ||
                                       target == PIPE_TEXTURE_3D ||
                                       target == PIPE_TEXTURE_CUBE ||
                                       target == PIPE_TEXTURE_CUBE_ARRAY)) {
      nir_variable *var = nir_variable_create(b.shader, nir_var_shader_in,
                                              glsl_int_type(), "gl_Layer");
      var->data.location = VARYING_SLOT_LAYER;
      var->data.interpolation = INTERP_MODE_FLAT;
      layer = nir_load_var(&b, var);
   }

   /* offset_pos = param.xy + f2i(coord.xy) */
   nir_ssa_def *offset_pos =
      nir_iadd(&b, nir_channels(&b, param, TGSI_WRITEMASK_XY),
               nir_f2i32(&b, nir_channels(&b, coord, TGSI_WRITEMASK_XY)));

   /* addr = offset_pos.x + offset_pos.y * stride */
   nir_ssa_def *pbo_addr =
      nir_iadd(&b, nir_channel(&b, offset_pos, 0),
               nir_imul(&b, nir_channel(&b, offset_pos, 1),
                        nir_channel(&b, param, 2)));
   if (layer) {
      /* pbo_addr += image_height * layer */
      pbo_addr = nir_iadd(&b, pbo_addr,
                          nir_imul(&b, layer, nir_channel(&b, param, 3)));
   }

   nir_ssa_def *texcoord;
   if (download) {
      texcoord = nir_f2i32(&b, nir_channels(&b, coord, TGSI_WRITEMASK_XY));

      if (layer) {
         nir_ssa_def *src_layer = layer;

         if (target == PIPE_TEXTURE_3D) {
            nir_variable *layer_offset_var =
               nir_variable_create(b.shader, nir_var_uniform,
                                   glsl_int_type(), "layer_offset");
            b.shader->num_uniforms += 1;
            layer_offset_var->data.driver_location = 4;
            nir_ssa_def *layer_offset = nir_load_var(&b, layer_offset_var);

            src_layer = nir_iadd(&b, layer, layer_offset);
         }

         texcoord = nir_vec3(&b, nir_channel(&b, texcoord, 0),
                                 nir_channel(&b, texcoord, 1),
                                 src_layer);
      }
   } else {
      texcoord = pbo_addr;
   }

   nir_variable *tex_var =
      nir_variable_create(b.shader, nir_var_uniform,
                          sampler_type_for_target(target), "tex");
   tex_var->data.explicit_binding = true;
   tex_var->data.binding = 0;

   nir_deref_instr *tex_deref = nir_build_deref_var(&b, tex_var);

   nir_tex_instr *tex = nir_tex_instr_create(b.shader, 3);
   tex->op = nir_texop_txf;
   tex->sampler_dim = glsl_get_sampler_dim(tex_var->type);
   tex->coord_components =
      glsl_get_sampler_coordinate_components(tex_var->type);
   tex->dest_type = nir_type_float;
   tex->src[0].src_type = nir_tex_src_texture_deref;
   tex->src[0].src = nir_src_for_ssa(&tex_deref->dest.ssa);
   tex->src[1].src_type = nir_tex_src_sampler_deref;
   tex->src[1].src = nir_src_for_ssa(&tex_deref->dest.ssa);
   tex->src[2].src_type = nir_tex_src_coord;
   tex->src[2].src = nir_src_for_ssa(texcoord);
   nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, NULL);
   nir_builder_instr_insert(&b, &tex->instr);
   nir_ssa_def *result = &tex->dest.ssa;

   if (conversion == ST_PBO_CONVERT_SINT_TO_UINT)
      result = nir_imax(&b, result, zero);
   else if (conversion == ST_PBO_CONVERT_UINT_TO_SINT)
      result = nir_umin(&b, result, nir_imm_int(&b, (1u << 31) - 1));

   if (download) {
      nir_variable *img_var =
         nir_variable_create(b.shader, nir_var_uniform,
                             glsl_image_type(GLSL_SAMPLER_DIM_BUF, false,
                                             GLSL_TYPE_FLOAT), "img");
      img_var->data.access = ACCESS_NON_READABLE;
      img_var->data.explicit_binding = true;
      img_var->data.binding = 0;
      nir_deref_instr *img_deref = nir_build_deref_var(&b, img_var);
      nir_intrinsic_instr *intrin =
         nir_intrinsic_instr_create(b.shader, nir_intrinsic_image_deref_store);
      intrin->src[0] = nir_src_for_ssa(&img_deref->dest.ssa);
      intrin->src[1] =
         nir_src_for_ssa(nir_vec4(&b, pbo_addr, zero, zero, zero));
      intrin->src[2] = nir_src_for_ssa(zero);
      intrin->src[3] = nir_src_for_ssa(result);
      intrin->src[4] = nir_src_for_ssa(nir_imm_int(&b, 0));
      intrin->num_components = 4;
      nir_builder_instr_insert(&b, &intrin->instr);
   } else {
      nir_variable *color =
         nir_variable_create(b.shader, nir_var_shader_out, glsl_vec4_type(),
                             "gl_FragColor");
      color->data.location = FRAG_RESULT_COLOR;

      nir_store_var(&b, color, result, TGSI_WRITEMASK_XYZW);
   }

   return st_nir_finish_builtin_shader(st, b.shader);
}

static enum st_pbo_conversion
get_pbo_conversion(enum pipe_format src_format, enum pipe_format dst_format)
{
   if (util_format_is_pure_uint(src_format)) {
      if (util_format_is_pure_sint(dst_format))
         return ST_PBO_CONVERT_UINT_TO_SINT;
   } else if (util_format_is_pure_sint(src_format)) {
      if (util_format_is_pure_uint(dst_format))
         return ST_PBO_CONVERT_SINT_TO_UINT;
   }

   return ST_PBO_CONVERT_NONE;
}

void *
st_pbo_get_upload_fs(struct st_context *st,
                     enum pipe_format src_format,
                     enum pipe_format dst_format)
{
   STATIC_ASSERT(ARRAY_SIZE(st->pbo.upload_fs) == ST_NUM_PBO_CONVERSIONS);

   enum st_pbo_conversion conversion = get_pbo_conversion(src_format, dst_format);

   if (!st->pbo.upload_fs[conversion])
      st->pbo.upload_fs[conversion] = create_fs(st, false, 0, conversion);

   return st->pbo.upload_fs[conversion];
}

void *
st_pbo_get_download_fs(struct st_context *st, enum pipe_texture_target target,
                       enum pipe_format src_format,
                       enum pipe_format dst_format)
{
   STATIC_ASSERT(ARRAY_SIZE(st->pbo.download_fs) == ST_NUM_PBO_CONVERSIONS);
   assert(target < PIPE_MAX_TEXTURE_TYPES);

   enum st_pbo_conversion conversion = get_pbo_conversion(src_format, dst_format);

   if (!st->pbo.download_fs[conversion][target])
      st->pbo.download_fs[conversion][target] = create_fs(st, true, target, conversion);

   return st->pbo.download_fs[conversion][target];
}

void
st_init_pbo_helpers(struct st_context *st)
{
   struct pipe_context *pipe = st->pipe;
   struct pipe_screen *screen = pipe->screen;

   st->pbo.upload_enabled =
      screen->get_param(screen, PIPE_CAP_TEXTURE_BUFFER_OBJECTS) &&
      screen->get_param(screen, PIPE_CAP_TEXTURE_BUFFER_OFFSET_ALIGNMENT) >= 1 &&
      screen->get_shader_param(screen, PIPE_SHADER_FRAGMENT, PIPE_SHADER_CAP_INTEGERS);
   if (!st->pbo.upload_enabled)
      return;

   st->pbo.download_enabled =
      st->pbo.upload_enabled &&
      screen->get_param(screen, PIPE_CAP_SAMPLER_VIEW_TARGET) &&
      screen->get_param(screen, PIPE_CAP_FRAMEBUFFER_NO_ATTACHMENT) &&
      screen->get_shader_param(screen, PIPE_SHADER_FRAGMENT,
                                       PIPE_SHADER_CAP_MAX_SHADER_IMAGES) >= 1;

   st->pbo.rgba_only =
      screen->get_param(screen, PIPE_CAP_BUFFER_SAMPLER_VIEW_RGBA_ONLY);

   if (screen->get_param(screen, PIPE_CAP_TGSI_INSTANCEID)) {
      if (screen->get_param(screen, PIPE_CAP_TGSI_VS_LAYER_VIEWPORT)) {
         st->pbo.layers = true;
      } else if (screen->get_param(screen, PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES) >= 3) {
         st->pbo.layers = true;
         st->pbo.use_gs = true;
      }
   }

   /* Blend state */
   memset(&st->pbo.upload_blend, 0, sizeof(struct pipe_blend_state));
   st->pbo.upload_blend.rt[0].colormask = PIPE_MASK_RGBA;

   /* Rasterizer state */
   memset(&st->pbo.raster, 0, sizeof(struct pipe_rasterizer_state));
   st->pbo.raster.half_pixel_center = 1;
}

void
st_destroy_pbo_helpers(struct st_context *st)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(st->pbo.upload_fs); ++i) {
      if (st->pbo.upload_fs[i]) {
         st->pipe->delete_fs_state(st->pipe, st->pbo.upload_fs[i]);
         st->pbo.upload_fs[i] = NULL;
      }
   }

   for (i = 0; i < ARRAY_SIZE(st->pbo.download_fs); ++i) {
      for (unsigned j = 0; j < ARRAY_SIZE(st->pbo.download_fs[0]); ++j) {
         if (st->pbo.download_fs[i][j]) {
            st->pipe->delete_fs_state(st->pipe, st->pbo.download_fs[i][j]);
            st->pbo.download_fs[i][j] = NULL;
         }
      }
   }

   if (st->pbo.gs) {
      st->pipe->delete_gs_state(st->pipe, st->pbo.gs);
      st->pbo.gs = NULL;
   }

   if (st->pbo.vs) {
      st->pipe->delete_vs_state(st->pipe, st->pbo.vs);
      st->pbo.vs = NULL;
   }
}
