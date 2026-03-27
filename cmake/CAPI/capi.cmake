include_guard(GLOBAL)

include(FetchContent)
FetchContent_Declare(capi
  URL https://github.com/ascii255/capi/archive/refs/tags/v1.0.5.tar.gz
  URL_HASH MD5=3168ede490ab58769964439396e24429
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(capi)
