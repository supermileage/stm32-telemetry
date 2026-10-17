$ErrorActionPreference = 'Stop'
$sdTestRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$sdTestBuild = Join-Path $sdTestRepo 'build/sd_spi_tests'

# CMake compiles the driver/app as C and the production BSP as C++.
cmake -S (Join-Path $sdTestRepo 'firmware') -B $sdTestBuild -G Ninja `
    -DTELEMETRY_BUILD_FIRMWARE=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
if ($LASTEXITCODE -ne 0) { throw 'SD SPI test configuration failed.' }

cmake --build $sdTestBuild --target sd_spi_tests
if ($LASTEXITCODE -ne 0) { throw 'SD SPI test compilation failed.' }

ctest --test-dir $sdTestBuild -R '^sd_spi_and_fatfs$' --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'SD SPI tests failed.' }
