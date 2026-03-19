@echo off
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Users\oulas\projects\tournament-trivia\vs\Tournament Trivia.sln" /p:Configuration=Release /p:Platform=x64 /t:Build /v:minimal
