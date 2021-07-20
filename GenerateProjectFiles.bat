cmake -G "Visual Studio 16 2019" -A x64 -B build

IF %ERRORLEVEL% NEQ 0 (
    PAUSE
) ELSE (
    START build/DirectX12Tutorial.sln
)