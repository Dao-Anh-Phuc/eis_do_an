@echo off
echo ================================================
echo Building EXE from main1.py
echo ================================================
echo.

REM Kiểm tra xem PyInstaller đã được cài đặt chưa
pip show pyinstaller >nul 2>&1
if %errorlevel% neq 0 (
    echo PyInstaller chua duoc cai dat. Dang cai dat...
    pip install pyinstaller
)

echo.
echo Dang build file EXE...
echo.

REM Build file EXE với các tùy chọn
pyinstaller --onefile --windowed --icon=NONE ^
    --name="DienHoaApp" ^
    --add-data="dai-hoc-khoa-hoc-tu-nhien-Photoroom.png;." ^
    --add-data="Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png;." ^
    --hidden-import=PIL._tkinter_finder ^
    --hidden-import=scipy ^
    --hidden-import=scipy.signal ^
    --hidden-import=pandas ^
    --hidden-import=serial ^
    --hidden-import=matplotlib ^
    --hidden-import=tkinter ^
    main1.py

echo.
echo ================================================
echo Build hoan tat!
echo File EXE nam trong thu muc: dist\DienHoaApp.exe
echo ================================================
echo.
pause
