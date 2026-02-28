{
  "targets": [
    {
      "target_name": "rbt_scheduler",
      "sources": [
        "src/Scheduler.cpp",
        "src/wrapper.cpp"
      ],
      "include_dirs": [
        "include",
        "build/_deps/nlohmann_json-src/single_include",
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags_cc": [ "-std=c++17" ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS":   "YES",
        "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
        "MACOSX_DEPLOYMENT_TARGET":    "10.15"
      }
    }
  ]
}
