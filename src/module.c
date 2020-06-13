#include <node_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <png.h>

typedef struct {
  uint8_t *data;
  unsigned long length;
  unsigned long position;
} Image_data;

void read_from_memory(png_structp png_ptr, png_bytep buffer, png_size_t size) {
  Image_data *img;
  size_t len;
  
  img = (Image_data *) png_get_io_ptr(png_ptr);
  if(img == NULL || img->data == NULL){
    png_error(png_ptr, "No data");
    return;
  }
  
  len = img->length - img->position;
  
  if(size > len){
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
  unsigned char *data_src;
  size_t data_src_len;
  status = napi_get_buffer_info(env, argv[0], (void**)&data_src, &data_src_len);
  
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Invalid buffer was passed as argument of data_src");
    return ret_dummy;
  }
  
  unsigned char *data_dst;
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

  if(data_src_len < PNG_BYTES_TO_CHECK) {
    napi_throw_error(env, NULL, "fail PNG_BYTES_TO_CHECK");
    return ret_dummy;
  }
  if(png_sig_cmp(data_src, 0, PNG_BYTES_TO_CHECK)) {
    napi_throw_error(env, NULL, "fail png_sig_cmp");
    return ret_dummy;
  }
  if((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL) {
    napi_throw_error(env, NULL, "fail png_create_read_struct");
    return ret_dummy;
  }
  if((info_ptr = png_create_info_struct(png_ptr)) == NULL){
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    napi_throw_error(env, NULL, "fail png_create_info_struct");
    return ret_dummy;
  }
  if (setjmp(png_jmpbuf(png_ptr))){
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    napi_throw_error(env, NULL, "fail");
    return ret_dummy;
  }
  Image_data proxy;
  proxy.data = data_src;
  proxy.length = data_src_len;
  proxy.position = 0;
  
  png_set_read_fn(png_ptr, (void *) &proxy, read_from_memory);
  
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  if (color_type == PNG_COLOR_TYPE_PALETTE){
    png_set_expand(png_ptr);
  }
  
  if(color_type != PNG_COLOR_TYPE_RGB_ALPHA && color_type != PNG_COLOR_TYPE_GRAY_ALPHA){
    png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
  }
  
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type== PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png_ptr);
  }
  
  if (bit_depth < 8) {
    png_set_packing(png_ptr);
  }
  
  if(bit_depth == 16) {
    png_set_strip_16(png_ptr);
  }
  png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  if(png_get_rowbytes(png_ptr,info_ptr) != width * 4){
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
  unsigned char *data_src;
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

  if(data_src_len < PNG_BYTES_TO_CHECK) {
    napi_throw_error(env, NULL, "fail PNG_BYTES_TO_CHECK");
    return ret_dummy;
  }
  if(png_sig_cmp(data_src, 0, PNG_BYTES_TO_CHECK)) {
    napi_throw_error(env, NULL, "fail png_sig_cmp");
    return ret_dummy;
  }
  if((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL) {
    napi_throw_error(env, NULL, "fail png_create_read_struct");
    return ret_dummy;
  }
  if((info_ptr = png_create_info_struct(png_ptr)) == NULL){
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    napi_throw_error(env, NULL, "fail png_create_info_struct");
    return ret_dummy;
  }
  if (setjmp(png_jmpbuf(png_ptr))){
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    napi_throw_error(env, NULL, "fail");
    return ret_dummy;
  }
  Image_data proxy;
  proxy.data = data_src;
  proxy.length = data_src_len;
  proxy.position = 0;
  
  png_set_read_fn(png_ptr, (void *) &proxy, read_from_memory);
  
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  if (color_type == PNG_COLOR_TYPE_PALETTE){
    png_set_expand(png_ptr);
  }
  
  if(color_type != PNG_COLOR_TYPE_RGB_ALPHA && color_type != PNG_COLOR_TYPE_GRAY_ALPHA){
    png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
  }
  
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type== PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png_ptr);
  }
  
  if (bit_depth < 8) {
    png_set_packing(png_ptr);
  }
  
  if(bit_depth == 16) {
    png_set_strip_16(png_ptr);
  }
  png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
  
  if(png_get_rowbytes(png_ptr,info_ptr) != width * 4){
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

napi_value Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value fn;
  
  status = napi_create_function(env, NULL, 0, png_decode_rgba, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "png_decode_rgba", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  
  status = napi_create_function(env, NULL, 0, png_decode_size, NULL, &fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to wrap native function");
  }
  
  status = napi_set_named_property(env, exports, "png_decode_size", fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Unable to populate exports");
  }
  
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)