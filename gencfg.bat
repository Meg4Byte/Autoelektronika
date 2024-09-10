REM Automatically generated batch file to generate Microsoft Visual Studio compiler and project configurations for PC-lint Plus

REM Temporary filename for imposter log
SET "IMPOSTER_LOG=C:\Users\Cain\AppData\Local\Temp\tmp905.tmp"
REM Clear temporary file
BREAK > "%IMPOSTER_LOG%"
REM Activate Visual Studio developer tools environment
CALL "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
@echo on
REM Set path to cl.exe
FOR /F "tokens=* USEBACKQ" %%F IN (`where cl`) DO (SET "PCLP_CFG_CL_PATH=%%F" & GOTO AFTER_FIRST_CL)
:AFTER_FIRST_CL
REM Generate compiler configuration
python "C:\Users\Cain\Downloads\pclp.windows.2.2\pclp\config\pclp_config.py" --compiler=vs2022_64 --compiler-bin="%PCLP_CFG_CL_PATH%" --config-output-lnt-file="C:\Users\Cain\source\repos\maybe\Autoelektronika-main(1)\Autoelektronika-main\Autoelektronika-main\co_vs2022_x64.lnt" --config-output-header-file="C:\Users\Cain\source\repos\maybe\Autoelektronika-main(1)\Autoelektronika-main\Autoelektronika-main\co_vs2022_x64.h" --generate-compiler-config
REM build imposter.c
cl "/FeC:\Users\Cain\Downloads\pclp.windows.2.2\pclp\config\\" "C:\Users\Cain\Downloads\pclp.windows.2.2\pclp\config\imposter.c"
REM generate project configuration
msbuild "C:\Users\Cain\source\repos\maybe\Autoelektronika-main(1)\Autoelektronika-main\Autoelektronika-main\Starter.sln" /t:clean
SET IMPOSTER_MODULES_IN_WORKING_DIR=1
SET IMPOSTER_PATH_ARGUMENT_RELATIVE_TO_WORKING_DIR_OPTION_INTRODUCERS=/I;-I
SET "IMPOSTER_COMPILER=%PCLP_CFG_CL_PATH%"
REM Clear temporary file
BREAK > "%IMPOSTER_LOG%"
msbuild "C:\Users\Cain\source\repos\maybe\Autoelektronika-main(1)\Autoelektronika-main\Autoelektronika-main\Starter.sln" /p:CLToolEXE=imposter.exe /p:CLToolPath="C:\Users\Cain\Downloads\pclp.windows.2.2\pclp\config"
python "C:\Users\Cain\Downloads\pclp.windows.2.2\pclp\config\pclp_config.py" --compiler=vs2022_64 --imposter-file="%IMPOSTER_LOG%" --config-output-lnt-file="C:\Users\Cain\source\repos\maybe\Autoelektronika-main(1)\Autoelektronika-main\Autoelektronika-main\pr_Starter.lnt" --generate-project-config
REM done

