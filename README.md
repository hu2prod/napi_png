# napi_png
decode png

    mod = require("napi_png")
    fs = require("fs")
    
    src = fs.readFileSync("test/input.png")
    dst = Buffer.alloc(4*1920*1080)
    [size_x, size_y, channel_count] = mod.png_decode_rgba(src, dst)

## Thanks
inspired by zhangyuanwei/node-images
