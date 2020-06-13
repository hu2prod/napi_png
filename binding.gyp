{
  "targets": [
    {
      "target_name": "module",
      "sources": [ "./src/module.c" ],
      "cflags": [
        "-std=c99"
      ],
      "link_settings": {
        "libraries": [
          "-lpng"
        ]
      }
    }
  ]
}