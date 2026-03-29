include_guard(GLOBAL)

include(FetchContent)
FetchContent_Declare(capi
  URL https://github.com/ascii255/capi/archive/refs/tags/v1.0.6.tar.gz
  URL_HASH MD5=895b53793f3f119b190290363bda8eb4
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(capi)
