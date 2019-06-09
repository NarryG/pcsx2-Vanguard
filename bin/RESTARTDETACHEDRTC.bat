@echo off

taskkill /F /IM pcsx2.exe > nul 2>&1
taskkill /F /IM WerFault.exe > nul 2>&1

start pcsx2.exe
exit