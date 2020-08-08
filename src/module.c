#include <node_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <png.h>
#include "type.h"

typedef struct {
  u8 *data;
  unsigned long length;
  unsigned long position;
} Image_data;

////////////////////////////////////////////////////////////////////////////////////////////////////
//    decode
////////////////////////////////////////////////////////////////////////////////////////////////////
void read_from_memory(png_structp png_ptr, png_bytep buffer, png_size_t size) {
  Image_data *img;
  size_t len;
  
  img = (Image_data *) png_get_io_ptr(png_ptr);
  if (img == NULL || img->data == NULL) {
    png_error(png_ptr, "No data");
    return;
  }
  
  len = img->length - img->position;
  
  if (size > len) {
    png_error(png_ptr, "No enough data");
    return;
  }
  
  memcpy(buffer, &(img->data[img->position]), size);
  img->position += size;
}

napi_value png_decode_rgba(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 2;
  napi_value argv[2];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  u8 *data_src;
  size_t data_src_len;
  status = napi_get_buffer_info(env, argv[0], (void**)&data_src, &data_src_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  u8 *data_dst;
  size_t data_dst_len;
  status = napi_get_buffer_info(env, argv[1], (void**)&data_dst, &data_dst_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_dst");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  #define PNG_BYTES_TO_CHECK 4
  png_structp png_ptr;
  png_infop info_ptr;
  
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;
  
  if (data_src_len < PNG_BYTES_TO_CHECK) {
    napi_throw_error(env, NULL, "fail PNG_BYTES_TO_CHECK");
    return ret_dummy;
  }
  if (png_sig_cmp(data_src, 0, PNG_BYTES_TO_CHECK)) {
    napi_throw_error(env, NULL, "fail png_sig_cmp");
    return ret_dummy;
  }
  if ((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL) {
    napi_throw_error(env, NULL, "fail png_create_read_struct");
    return ret_dummy;
  }
  if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    napi_throw_error(env, NULL, "fail png_create_info_struct");
    return ret_dummy;
  }
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    napi_throw_error(env, NULL, "fail setjmp png_jmpbuf");
    return ret_dummy;
  }
  Image_data proxy;
  proxy.data = data_src;
  proxy.length = data_src_len;
  proxy.position = 0;
  
  png_set_read_fn(png_ptr, (void *) &proxy, read_from_memory);
  
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_expand(png_ptr);
  }
  
  if (color_type != PNG_COLOR_TYPE_RGB_ALPHA && color_type != PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
  }
  
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type== PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png_ptr);
  }
  
  if (bit_depth < 8) {
    png_set_packing(png_ptr);
  }
  
  if (bit_depth == 16) {
    png_set_strip_16(png_ptr);
  }
  png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  if (png_get_rowbytes(png_ptr,info_ptr) != width * 4) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    napi_throw_error(env, NULL, "fail png_get_rowbytes");
    return ret_dummy;
  }
  
  if (data_dst_len < 4*width*height) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    printf("width = %d\n", width);
    printf("height = %d\n", height);
    printf("required = %d\n", 4*width*height);
    printf("data_dst_len = %ld\n", data_dst_len);
    napi_throw_error(env, NULL, "data_dst is too small");
    return ret_dummy;
  }
  
  size_t *row_pointer_list = (size_t*)malloc(sizeof(size_t)*height);
  for(unsigned int y=0;y<height;y++) {
    row_pointer_list[y] = (size_t)data_dst + 4*width*y;
  }
  
  png_read_image(png_ptr, (png_bytepp) row_pointer_list);
  png_read_end(png_ptr, info_ptr);
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  
  free(row_pointer_list);
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  napi_value ret_arr;
  status = napi_create_array(env, &ret_arr);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
    return ret_dummy;
  }
  
  // width
  napi_value ret_width;
  status = napi_create_int32(env, width, &ret_width);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_width");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 0, ret_width);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_width assign");
    return ret_dummy;
  }
  // height
  napi_value ret_height;
  status = napi_create_int32(env, height, &ret_height);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_height");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 1, ret_height);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_height assign");
    return ret_dummy;
  }
  
  // byte size = 4
  napi_value ret_byte_size;
  status = napi_create_int32(env, 4, &ret_byte_size);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_byte_size");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 2, ret_byte_size);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_byte_size assign");
    return ret_dummy;
  }
  
  return ret_arr;
}

napi_value png_decode_size(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 1;
  napi_value argv[1];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  u8 *data_src;
  size_t data_src_len;
  status = napi_get_buffer_info(env, argv[0], (void**)&data_src, &data_src_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  #define PNG_BYTES_TO_CHECK 4
  png_structp png_ptr;
  png_infop info_ptr;
  
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;
  
  if (data_src_len < PNG_BYTES_TO_CHECK) {
    napi_throw_error(env, NULL, "fail PNG_BYTES_TO_CHECK");
    return ret_dummy;
  }
  if (png_sig_cmp(data_src, 0, PNG_BYTES_TO_CHECK)) {
    napi_throw_error(env, NULL, "fail png_sig_cmp");
    return ret_dummy;
  }
  if ((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL) {
    napi_throw_error(env, NULL, "fail png_create_read_struct");
    return ret_dummy;
  }
  if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    napi_throw_error(env, NULL, "fail png_create_info_struct");
    return ret_dummy;
  }
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    napi_throw_error(env, NULL, "fail setjmp png_jmpbuf");
    return ret_dummy;
  }
  Image_data proxy;
  proxy.data = data_src;
  proxy.length = data_src_len;
  proxy.position = 0;
  
  png_set_read_fn(png_ptr, (void *) &proxy, read_from_memory);
  
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_expand(png_ptr);
  }
  
  if (color_type != PNG_COLOR_TYPE_RGB_ALPHA && color_type != PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
  }
  
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type== PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png_ptr);
  }
  
  if (bit_depth < 8) {
    png_set_packing(png_ptr);
  }
  
  if (bit_depth == 16) {
    png_set_strip_16(png_ptr);
  }
  png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  if (png_get_rowbytes(png_ptr,info_ptr) != width * 4) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    napi_throw_error(env, NULL, "fail png_get_rowbytes");
    return ret_dummy;
  }
  
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  
  napi_value ret_arr;
  status = napi_create_array(env, &ret_arr);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
    return ret_dummy;
  }
  
  
  // width
  napi_value ret_width;
  status = napi_create_int32(env, width, &ret_width);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_width");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 0, ret_width);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_width assign");
    return ret_dummy;
  }
  // height
  napi_value ret_height;
  status = napi_create_int32(env, height, &ret_height);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_height");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 1, ret_height);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_height assign");
    return ret_dummy;
  }
  
  // byte size = 4
  napi_value ret_byte_size;
  status = napi_create_int32(env, 4, &ret_byte_size);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_byte_size");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 2, ret_byte_size);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_byte_size assign");
    return ret_dummy;
  }
  
  
  return ret_arr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//    napi encode
////////////////////////////////////////////////////////////////////////////////////////////////////
#define PNG_INIT_FILE_SIZE 1024
#define PNG_BIG_FILE_SIZE 10240
void write_to_memory(png_structp png_ptr, png_bytep buffer, png_size_t size) {
  Image_data *img;
  size_t len;
  
  img = (Image_data *) png_get_io_ptr(png_ptr);
  if (img == NULL) {
    png_error(png_ptr, "What happend?!");
    return;
  }
  
  if (img->data == NULL) {
      len = PNG_INIT_FILE_SIZE + size;
      if ((img->data = (u8 *) malloc(len)) == NULL) {
          png_error(png_ptr, "Out of memory.");
          return;
      }
      img->length = len;
      img->position = 0;
  } else if ((len = img->position + size) > img->length) {
      len = (len < PNG_BIG_FILE_SIZE) ? (len << 1) :(len + PNG_INIT_FILE_SIZE);
      if ((img->data = (u8 *) realloc(img->data, len)) == NULL) {
          png_error(png_ptr, "Out of memory.");
          return;
      }
      img->length = len;
  }
  
  memcpy(&(img->data[img->position]), buffer, size);
  img->position += size;
}

void flush_memory(png_structp png_ptr) {printf("flush_memory d1\n");}

napi_value png_encode_rgba(napi_env env, napi_callback_info info) {
  napi_status status;
  
  napi_value ret_dummy;
  status = napi_create_int32(env, 0, &ret_dummy);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dummy");
    return ret_dummy;
  }
  
  size_t argc = 5;
  napi_value argv[5];
  status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to parse arguments");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  u8 *data_src;
  size_t data_src_len;
  status = napi_get_buffer_info(env, argv[0], (void**)&data_src, &data_src_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  i32 size_x;
  status = napi_get_value_int32(env, argv[1], &size_x);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of size_x");
    return ret_dummy;
  }
  
  i32 size_y;
  status = napi_get_value_int32(env, argv[2], &size_y);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of size_y");
    return ret_dummy;
  }
  
  u8 *data_dst;
  size_t data_dst_len;
  status = napi_get_buffer_info(env, argv[3], (void**)&data_dst, &data_dst_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_dst");
    return ret_dummy;
  }
  
  i32 data_dst_offset;
  status = napi_get_value_int32(env, argv[4], &data_dst_offset);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid i32 was passed as argument of data_dst_offset");
    return ret_dummy;
  }
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  png_structp png_ptr;
  png_infop info_ptr;
  png_bytep *row_pointer_list = NULL;
  
  if ((png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL) {
    napi_throw_error(env, NULL, "fail png_create_write_struct");
    return ret_dummy;
  }
  if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    napi_throw_error(env, NULL, "fail png_create_info_struct");
    return ret_dummy;
  }
  Image_data proxy;
  
  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    if (row_pointer_list) {
      free(row_pointer_list);
    }
    if (proxy.data) {
      free(proxy.data);
    }
    napi_throw_error(env, NULL, "fail setjmp png_jmpbuf");
    return ret_dummy;
  }
  
  proxy.data = NULL;
  proxy.length = 0;
  proxy.position = 0;
  
  png_set_write_fn(png_ptr, (void *) &proxy, write_to_memory, flush_memory);
  png_set_IHDR(png_ptr, info_ptr, size_x, size_y, 8,
                 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  
  png_write_info(png_ptr, info_ptr);
  row_pointer_list = (png_bytep *)malloc(sizeof(png_bytep *)*size_y);
  for(int i = 0; i < size_y; i++){
    row_pointer_list[i] = (data_src + i * size_x * 4);
  }
  
  png_write_image(png_ptr, (png_bytepp) row_pointer_list);
  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  
  free(row_pointer_list);
  
  napi_value realloc_buffer;
  bool realloc = false;
  unsigned long len = proxy.position;
  if (len + data_dst_offset > data_dst_len) {
    realloc = true;
    data_dst_offset = 0;
    unsigned long new_len = 1;
    // slow but whatever
    while(len > new_len) {
      new_len <<= 1;
    }
    
    status = napi_create_buffer(env, new_len, (void**)(&data_dst), &realloc_buffer);
    if (status != napi_ok) {
      free(proxy.data);
      napi_throw_error(env, NULL, "fail napi_create_buffer");
      return ret_dummy;
    }
  }
  memcpy(data_dst + data_dst_offset, proxy.data, proxy.position);
  free(proxy.data);
  
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  napi_value ret_arr;
  status = napi_create_array(env, &ret_arr);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value");
    return ret_dummy;
  }
  // data_dst_offset
  napi_value ret_data_dst_offset;
  status = napi_create_int32(env, data_dst_offset, &ret_data_dst_offset);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_data_dst_offset");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 0, ret_data_dst_offset);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_data_dst_offset assign");
    return ret_dummy;
  }
  // ret_dst_write_size
  napi_value ret_data_dst_actual_size;
  status = napi_create_int32(env, len, &ret_data_dst_actual_size);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_data_dst_actual_size");
    return ret_dummy;
  }
  
  status = napi_set_element(env, ret_arr, 1, ret_data_dst_actual_size);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to create return value ret_dst_end_offset assign");
    return ret_dummy;
  }
  //
  
  if (realloc) {
    status = napi_set_element(env, ret_arr, 2, realloc_buffer);
    
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to create return value ret_height assign");
      return ret_dummy;
    }
  } else {
    // argv[3] == data_dst
    // Я не знаю можно ли делать присваивание napi_value
    // Потому просто такой хардкод
    status = napi_set_element(env, ret_arr, 2, argv[3]);
    
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Unable to create return value ret_height assign");
      return ret_dummy;
    }
  }
  
  return ret_arr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

napi_value Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, png_decode_rgba, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "png_decode_rgba", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, png_decode_size, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "png_decode_size", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  status = napi_create_function(env, NULL, 0, png_encode_rgba, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "png_encode_rgba", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)