zephyr_library()

zephyr_library_sources_ifdef(CONFIG_PMW3610_ALT src/pmw3610.c)
zephyr_library_sources_ifdef(CONFIG_PMW3610_ALT_XY_UNPACK src/pmw3610_xy_unpack.c)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
