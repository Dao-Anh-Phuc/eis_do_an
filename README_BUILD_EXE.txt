╔═══════════════════════════════════════════════════════════════════╗
║     HƯỚNG DẪN TẠO FILE EXE TỪ MAIN1.PY - ĐƠN GIẢN NHẤT          ║
╚═══════════════════════════════════════════════════════════════════╝

📋 YÊU CẦU:
   ✓ Đã có Python 3.7 trở lên
   ✓ Đã có 2 file ảnh logo trong cùng thư mục với main1.py:
     - dai-hoc-khoa-hoc-tu-nhien-Photoroom.png
     - Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png

═══════════════════════════════════════════════════════════════════

🚀 CÁCH SỬ DỤNG NHANH (3 BƯỚC):

BƯỚC 1: Cài đặt thư viện cần thiết 
   - Mở PowerShell trong thư mục này
   - Chạy lệnh: pip install -r requirements.txt

BƯỚC 2: Tạo file EXE
   ⭐ CÁCH ĐỂ GIẢN NHẤT: Double-click vào file "build_exe.bat"
   
   Hoặc mở PowerShell và gõ: .\build_exe.bat

BƯỚC 3: Lấy file EXE
   - File EXE nằm trong thư mục: dist\DienHoaApp.exe
   - Copy file này ra Desktop hoặc nơi bạn muốn
   - ⚠️ QUAN TRỌNG: Copy cả 2 file ảnh logo vào cùng thư mục với DienHoaApp.exe
   - Double-click DienHoaApp.exe để chạy!

═══════════════════════════════════════════════════════════════════

📁 CẤU TRÚC THƯ MỤC SAU KHI BUILD:

Thư mục hiện tại/
├── main1.py  (file code gốc)
├── build_exe.bat  (file để build EXE)
├── requirements.txt
├── dai-hoc-khoa-hoc-tu-nhien-Photoroom.png
├── Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png
└── dist/
    └── DienHoaApp.exe  ⭐ FILE EXE CỦA BẠN

═══════════════════════════════════════════════════════════════════

💡 LƯU Ý:

1. Kích thước file EXE: Khoảng 50-150MB (bình thường)

2. Logo: Nếu không thấy logo, hãy copy 2 file ảnh vào cùng thư mục với DienHoaApp.exe

3. Antivirus: Một số antivirus có thể cảnh báo, chỉ cần bỏ qua (false positive)

4. Chia sẻ: Muốn chia sẻ cho người khác, cần gửi:
   - DienHoaApp.exe
   - dai-hoc-khoa-hoc-tu-nhien-Photoroom.png  
   - Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png
   (Đặt 3 file này trong cùng 1 thư mục)

5. Build lại: Nếu sửa code main1.py, chỉ cần chạy lại build_exe.bat

═══════════════════════════════════════════════════════════════════

❓ TROUBLESHOOTING (GỠ LỖI):

LỖI: "PyInstaller không được cài đặt"
   → Chạy: pip install pyinstaller

LỖI: "ImportError" 
   → Chạy: pip install --upgrade pyinstaller

LỖI: Không thấy logo
   → Copy 2 file ảnh .png vào cùng thư mục với DienHoaApp.exe

LỖI: File EXE không chạy
   → Tắt antivirus tạm thời và thử lại
   → Hoặc chạy với quyền Administrator

═══════════════════════════════════════════════════════════════════

🎯 KẾT QUẢ:
   Sau khi hoàn tất, bạn có file DienHoaApp.exe có thể:
   ✓ Double-click để chạy ngay
   ✓ Không cần cài Python
   ✓ Không cần cài thư viện
   ✓ Copy sang máy Windows khác và chạy được luôn
   ✓ Đặt shortcut lên Desktop để dễ sử dụng

═══════════════════════════════════════════════════════════════════
