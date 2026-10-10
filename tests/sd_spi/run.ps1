$ErrorActionPreference = 'Stop'
$sdTestRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$sdTestBuild = Join-Path $sdTestRepo 'build/sd_spi_tests'
$sdTestExe = Join-Path $sdTestBuild 'sd_spi_tests.exe'
$sdTestCompiler = (Get-Command gcc -ErrorAction Stop).Source

New-Item -ItemType Directory -Path $sdTestBuild -Force | Out-Null
$sdTestArguments = @(
    '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
    ('-I' + $PSScriptRoot),
    ('-I' + (Join-Path $sdTestRepo 'lib/drivers/sd_spi')),
    ('-I' + (Join-Path $sdTestRepo 'lib')),
    ('-I' + (Join-Path $sdTestRepo 'lib/fatfs')),
    ('-I' + (Join-Path $sdTestRepo 'firmware/App/storage')),
    ('-I' + (Join-Path $sdTestRepo 'firmware/App/logging')),
    ('-I' + (Join-Path $sdTestRepo 'firmware/BSP/b_u585i_iot02a')),
    ('-I' + (Join-Path $sdTestRepo 'firmware/BSP')),
    (Join-Path $sdTestRepo 'lib/drivers/sd_spi/sd_spi.c'),
    (Join-Path $sdTestRepo 'firmware/BSP/sd_spi_port.c'),
    (Join-Path $sdTestRepo 'firmware/BSP/b_u585i_iot02a/board_sd.c'),
    (Join-Path $sdTestRepo 'firmware/App/storage/sd_diskio.c'),
    (Join-Path $sdTestRepo 'firmware/App/storage/storage.c'),
    (Join-Path $sdTestRepo 'firmware/App/logging/log_file.c'),
    (Join-Path $sdTestRepo 'lib/fatfs/ff.c'),
    (Join-Path $sdTestRepo 'lib/fatfs/diskio.c'),
    (Join-Path $sdTestRepo 'lib/fatfs/ff_gen_drv.c'),
    (Join-Path $PSScriptRoot 'test_sd_spi.c'),
    '-o', $sdTestExe
)
& $sdTestCompiler @sdTestArguments
if ($LASTEXITCODE -ne 0) { throw 'SD SPI test compilation failed.' }
& $sdTestExe
if ($LASTEXITCODE -ne 0) { throw 'SD SPI tests failed.' }
