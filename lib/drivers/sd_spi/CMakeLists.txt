add_library(sd_spi STATIC sd_spi.c)
target_include_directories(sd_spi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(sd_spi PUBLIC c_std_11)
