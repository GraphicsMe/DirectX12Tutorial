cmake -G "Visual Studio 15 2017" -A x64 -B build

IF %ERRORLEVEL% NEQ 0 (
    PAUSE
) ELSE (
    START build/DirectX12Tutorial.sln
)