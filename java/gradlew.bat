@echo off
rem Gradle wrapper script for Windows

set DIR=%~dp0
if "%DIR%" == "" set DIR=.

set WRAPPER_JAR=%DIR%gradle/wrapper/gradle-wrapper.jar
set WRAPPER_PROPERTIES=%DIR%gradle/wrapper/gradle-wrapper.properties

if not exist "%WRAPPER_JAR%" (
    echo ERROR: Wrapper JAR file not found: %WRAPPER_JAR%
    exit /b 1
)

if not exist "%WRAPPER_PROPERTIES%" (
    echo ERROR: Wrapper properties file not found: %WRAPPER_PROPERTIES%
    exit /b 1
)

java -jar "%WRAPPER_JAR%" %*