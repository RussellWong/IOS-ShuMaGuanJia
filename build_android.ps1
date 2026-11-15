# 数码管家 APK 构建脚本（不依赖 Gradle，手工完成 aapt2 / d8 / NDK 编译 / 签名）
# 用法：powershell -ExecutionPolicy Bypass -File build_android.ps1
param(
    [string[]]$Abis = @("arm64-v8a", "armeabi-v7a", "x86_64"),
    [switch]$SkipNative
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# ---------------- 工具链路径 ----------------
$cmake     = "D:\VS2026主程序\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ninjaDir  = "D:\VS2026主程序\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
$jdk       = "D:\dev\jdk-17.0.21.0.2+1"
$sdk       = "D:\dev\android-sdk"
$ndk       = "$sdk\ndk\27.2.12479018"
$bt        = "$sdk\build-tools\34.0.0"
$androidJar = "$sdk\platforms\android-34\android.jar"

foreach ($p in @($cmake, $jdk, $ndk, $bt, $androidJar)) {
    if (-not (Test-Path $p)) { throw "缺少工具链：$p" }
}

$env:JAVA_HOME = $jdk
$env:PATH = "$jdk\bin;$ninjaDir;$bt;$ndk\toolchains\llvm\prebuilt\windows-x86_64\bin;" + $env:PATH

$build  = Join-Path $root "build\android"
$stage  = Join-Path $build "stage"
$native = Join-Path $build "native"
New-Item -ItemType Directory -Force $build | Out-Null

Write-Host "== 1/6 编译原生库（$($Abis -join ', ')） ==" -ForegroundColor Cyan
$soFiles = @{}
if (-not $SkipNative) {
    foreach ($abi in $Abis) {
        $native = Join-Path $build "native\$abi"
        & $cmake -S $root -B $native -G Ninja `
            -DCMAKE_BUILD_TYPE=Release `
            -DCMAKE_TOOLCHAIN_FILE="$ndk\build\cmake\android.toolchain.cmake" `
            -DANDROID_ABI="$abi" `
            -DANDROID_PLATFORM=android-23 `
            -DANDROID_STL=c++_static
        if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败（$abi）" }

        & $cmake --build $native --target shumaguanjia
        if ($LASTEXITCODE -ne 0) { throw "原生库编译失败（$abi）" }
        $soFiles[$abi] = Join-Path $native "libshumaguanjia.so"
    }
} else {
    foreach ($abi in $Abis) {
        $soFiles[$abi] = Join-Path $build "native\$abi\libshumaguanjia.so"
    }
}
foreach ($abi in $Abis) {
    if (-not (Test-Path $soFiles[$abi])) { throw "未找到 libshumaguanjia.so（$abi）" }
}

Write-Host "== 2/6 编译 Java 桥接层 ==" -ForegroundColor Cyan
$classes = Join-Path $build "classes"
if (Test-Path $classes) { Remove-Item $classes -Recurse -Force }
New-Item -ItemType Directory -Force $classes | Out-Null
$javaFiles = Get-ChildItem (Join-Path $root "android\java") -Recurse -Filter *.java | ForEach-Object { $_.FullName }
& "$jdk\bin\javac.exe" -encoding UTF-8 -source 8 -target 8 -Xlint:-options `
    -bootclasspath $androidJar -d $classes $javaFiles
if ($LASTEXITCODE -ne 0) { throw "javac 编译失败" }

Write-Host "== 3/6 生成 classes.dex ==" -ForegroundColor Cyan
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
$classFiles = Get-ChildItem $classes -Recurse -Filter *.class | ForEach-Object { $_.FullName }
& "$bt\d8.bat" --release --lib $androidJar --min-api 23 --output $stage $classFiles
if ($LASTEXITCODE -ne 0) { throw "d8 失败" }

foreach ($abi in $Abis) {
    $abiDir = Join-Path $stage "lib\$abi"
    New-Item -ItemType Directory -Force $abiDir | Out-Null
    Copy-Item $soFiles[$abi] $abiDir -Force
}

Write-Host "== 4/6 打包 APK ==" -ForegroundColor Cyan
# aapt2 对非 ASCII 路径支持不佳，资源与清单先复制到英文临时目录处理
$ascii = "C:\dm_build"
if (Test-Path $ascii) { Remove-Item $ascii -Recurse -Force }
New-Item -ItemType Directory -Force $ascii | Out-Null
Copy-Item (Join-Path $root "android\res") (Join-Path $ascii "res") -Recurse -Force
Copy-Item (Join-Path $root "android\AndroidManifest.xml") (Join-Path $ascii "AndroidManifest.xml") -Force

$resZip = Join-Path $ascii "res.zip"
& "$bt\aapt2.exe" compile --dir (Join-Path $ascii "res") -o $resZip
if ($LASTEXITCODE -ne 0) { throw "aapt2 compile 失败" }

$baseApk = Join-Path $build "base.apk"
if (Test-Path $baseApk) { Remove-Item $baseApk -Force }
& "$bt\aapt2.exe" link -o $baseApk `
    -I $androidJar `
    --manifest (Join-Path $ascii "AndroidManifest.xml") `
    --min-sdk-version 23 --target-sdk-version 34 `
    --version-code 1 --version-name "1.0.0" `
    --auto-add-overlay `
    $resZip
if ($LASTEXITCODE -ne 0) { throw "aapt2 link 失败" }

$unsigned = Join-Path $build "apk-unsigned.apk"
if (Test-Path $unsigned) { Remove-Item $unsigned -Force }
$packScript = Join-Path $build "pack.py"
@'
import os, sys, zipfile
src, staging, out = sys.argv[1], sys.argv[2], sys.argv[3]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    if os.path.exists(src):
        with zipfile.ZipFile(src, "r") as s:
            for item in s.infolist():
                z.writestr(item, s.read(item.filename))
    for root_dir, dirs, files in os.walk(staging):
        for f in files:
            full = os.path.join(root_dir, f)
            arc = os.path.relpath(full, staging).replace("\\", "/")
            if arc == "classes.dex" or arc.startswith("lib/"):
                z.write(full, arc)
print("packed:", out)
'@ | Set-Content -Path $packScript -Encoding UTF8
python $packScript $baseApk $stage $unsigned
if ($LASTEXITCODE -ne 0) { throw "APK 打包失败" }

Write-Host "== 5/6 对齐与签名 ==" -ForegroundColor Cyan
# zipalign / apksigner 同样在英文目录执行，避免中文路径问题
$aligned = Join-Path $ascii "apk-aligned.apk"
Copy-Item $unsigned (Join-Path $ascii "apk-unsigned.apk") -Force
if (Test-Path $aligned) { Remove-Item $aligned -Force }
& "$bt\zipalign.exe" -f -p 4 (Join-Path $ascii "apk-unsigned.apk") $aligned
if ($LASTEXITCODE -ne 0) { throw "zipalign 失败" }

$ks = Join-Path $root "android\shumaguanjia.keystore"
if (-not (Test-Path $ks)) {
    & "$jdk\bin\keytool.exe" -genkeypair -keystore $ks -alias shumaguanjia `
        -keyalg RSA -keysize 2048 -validity 10000 `
        -storepass 123456 -keypass 123456 `
        -dname "CN=ShuMaGuanJia, OU=Mobile, O=ShuMaGuanJia, L=Shenzhen, ST=Guangdong, C=CN" -noprompt
    if ($LASTEXITCODE -ne 0) { throw "生成签名证书失败" }
}
$ksAscii = Join-Path $ascii "sign.keystore"
Copy-Item $ks $ksAscii -Force

$apkOutAscii = Join-Path $ascii "shumaguanjia.apk"
if (Test-Path $apkOutAscii) { Remove-Item $apkOutAscii -Force }
& "$bt\apksigner.bat" sign --ks $ksAscii --ks-key-alias shumaguanjia `
    --ks-pass pass:123456 --key-pass pass:123456 `
    --v1-signing-enabled true --v2-signing-enabled true --v3-signing-enabled true `
    --out $apkOutAscii $aligned
if ($LASTEXITCODE -ne 0) { throw "签名失败" }

$apkOut = Join-Path $root "数码管家.apk"
if (Test-Path $apkOut) { Remove-Item $apkOut -Force }
Copy-Item $apkOutAscii $apkOut -Force

Write-Host "== 6/6 校验 ==" -ForegroundColor Cyan
& "$bt\apksigner.bat" verify --verbose $apkOutAscii
if ($LASTEXITCODE -ne 0) { throw "签名校验失败" }
& "$bt\aapt2.exe" dump badging $apkOutAscii | Select-Object -First 4

$size = [math]::Round((Get-Item $apkOut).Length / 1MB, 2)
Write-Host "完成：$apkOut（$size MB）" -ForegroundColor Green
