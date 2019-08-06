@ECHO OFF

SET BINDIR=C:\Program Files\7-Zip
SET EXE=%BINDIR%\7z.exe

"%EXE%" a -tzip tpmiddle CHANGELOG *.bat *.h *.cpp *.rc *.sln *.vcxproj *.filters



