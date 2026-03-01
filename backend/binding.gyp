{
  "targets": [
    {
      "target_name": "rbt_scheduler",
      "sources": [
        "../cpp/src/Scheduler.cpp",
        "../cpp/src/wrapper.cpp"
      ],
      "include_dirs": [
        "../cpp/include",
        "../cpp/third_party",
        "../build/_deps/nlohmann_json-src/single_include",
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags":    [ "-fexceptions" ],
      "cflags_cc": [ "-std=c++17", "-fexceptions" ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS":   "YES",
        "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
        "MACOSX_DEPLOYMENT_TARGET":    "10.15",
        "OTHER_CPLUSPLUSFLAGS":        [ "-fexceptions" ]
      },
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1
        }
      }
    }
  ]
}
