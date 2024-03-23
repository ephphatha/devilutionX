include(functions/FetchContent_MakeAvailableExcludeFromAll)

include(FetchContent)
FetchContent_Declare(fpm
    URL https://github.com/MikeLankamp/fpm/archive/22bab8ff3abe0acfb00bc20853ed6b74e9959fd7.tar.gz
    URL_HASH MD5=0ed6ba362c989d92c9e891de0a8db8ad
)
FetchContent_MakeAvailableExcludeFromAll(fpm)
