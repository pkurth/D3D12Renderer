@ECHO OFF
setlocal

SET "turing_gpu_found="

FOR /F "usebackq tokens=1*" %%i in (`wmic path win32_VideoController get name`) DO (
	if /I "%%i" == "NVIDIA" (
		echo NVIDIA GPU found.
		FOR %%a in (%%j) DO (
			SET "var="&for /f "delims=0123456789" %%f in ("%%a") do set var=%%f
			if not defined var (
				if %%a geq 2060 (
					SET "turing_gpu_found=y"
				)
			)
		)
	)
)

if defined turing_gpu_found (
	echo Turing GPU found.
	call premake\premake5.exe vs2019
) else (
	echo No Turing GPU found. Certain features, e.g. mesh shaders, will not be available.
	call premake\premake5.exe --no-turing vs2019
)

PAUSE
