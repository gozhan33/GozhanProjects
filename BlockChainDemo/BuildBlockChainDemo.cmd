@echo off
REM Compile BlockChainDemo with MSVC. Run this from a Developer Command Prompt (or call vcvarsall.bat first).
set "OPENSSL_DIR=C:\Program Files\OpenSSL-Win64"

echo Validating OPENSSL include path: "%OPENSSL_DIR%\include\openssl"
if not exist "%OPENSSL_DIR%\include\openssl\sha.h" (
	echo OpenSSL headers not found in %OPENSSL_DIR%\include
	echo Please adjust OPENSSL_DIR in this script or install OpenSSL for MSVC.
	exit /b 1
)

echo Validating OPENSSL library path: "%OPENSSL_DIR%\lib\VC\X64\MTd\libcrypto.lib"
if not exist "%OPENSSL_DIR%\lib\VC\X64\MTd\libcrypto.lib" (
	echo libcrypto.lib not found in %OPENSSL_DIR%\lib\VC\X64\MTd
	echo If you installed a different flavor like MD, MDd, MT change the path or install the matching OpenSSL package.
	exit /b 1
)

echo Building BlockChainDemo for x64, so make sure you run this from "x64 Native Tools Command Prompt for VS ..."
cl.exe /nologo /Zi /Od /EHsc /std:c++17 /MTd /I"%OPENSSL_DIR%\include" /FC /showIncludes BlockChainDemo.cpp /link /DEBUG /LIBPATH:"%OPENSSL_DIR%\lib\VC\x64\MTd" libcrypto.lib libssl.lib > build.log 2>&1