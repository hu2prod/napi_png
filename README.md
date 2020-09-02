# napi_png
## install requirements

    apt-get install -y libpng-dev

## use

    mod = require("napi_png")
    fs = require("fs")
    
    src = fs.readFileSync("test/input.png")
    res = mod.png_decode_size(src)
    console.log(res)
    size_x = res[0]
    size_y = res[1]
    dst_rgba = Buffer.alloc(4*size_x*size_y)
    mod.png_decode_rgba(src, dst_rgba)
    dst_rgb = Buffer.alloc(3*size_x*size_y)
    mod.png_decode_rgb(src, dst_rgb)
    
    
    // encode API is really strange (but for purpose)
    // we want reuse as much buffers as possible (less allocs possible)
    png_dst = Buffer.alloc(4*1920*1080)
    res = mod.png_encode_rgba dst_rgba, size_x, size_y, png_dst, target_buffer_desired_offset = 0
    start   = res[0]
    length  = res[1]
    buf     = res[2]
    // now we have 2 different situations
    //  
    //  1. encoded_length + target_buffer_desired_offset < png_dst.length
    //  then we can reuse buffer, so we return buf == png_dst, start == target_buffer_desired_offset, length == encoded_length
    //  2. not enough space to write
    //  then we need allocate new buffer, buf != png_dst, start == 0, length == encoded_length
    
    // target_buffer_desired_offset is needed if we want use 1 big buffer for multiple images, so we can use space more effectively (theoretically for some applications)
    
    // mod.png_encode_rgb is also available

## Thanks
inspired by zhangyuanwei/node-images
