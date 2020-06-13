# napi_png
decode png

    mod = require("napi_png")
    fs = require("fs")
    
    src = fs.readFileSync("test/input.png")
    res = mod.png_decode_size(src)
    console.log(res)
    size_x = res[0]
    size_y = res[1]
    dst = Buffer.alloc(4*size_x*size_y)
    mod.png_decode_rgba(src, dst)

## Thanks
inspired by zhangyuanwei/node-images
