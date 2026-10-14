param([Parameter(Mandatory=$true)][string]$Ndk)
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$compiler = Join-Path $Ndk 'toolchains\llvm\prebuilt\windows-x86_64\bin\clang.exe'
$destination = Join-Path $project 'app\src\main\jniLibs\arm64-v8a\libsilph-loader.so'
& $compiler --target=aarch64-linux-android29 -shared -fPIC -O2 -Wall -Wextra -Werror -fvisibility=hidden -fstack-protector-strong -D_FORTIFY_SOURCE=2 '-Wl,-z,relro,-z,now,-z,max-page-size=16384' '-Wl,--no-undefined' '-Wl,-soname,libsilph-loader.so' (Join-Path $project 'native\loader-adapter.c') (Join-Path $project 'native\loader-policy.c') -ldl -o $destination
if ($LASTEXITCODE -ne 0) { throw 'Loader compilation failed' }
Get-FileHash -Algorithm SHA256 -LiteralPath $destination
