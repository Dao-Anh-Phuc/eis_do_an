# Hướng Dẫn Tạo File EXE Từ main1.py

## Phương Pháp 1: Sử Dụng File BAT (Đơn Giản Nhất)

### Bước 1: Cài đặt Python
- Đảm bảo đã cài đặt Python 3.7 trở lên
- Kiểm tra: mở PowerShell và gõ `python --version`

### Bước 2: Cài đặt các thư viện cần thiết
Mở PowerShell trong thư mục này và chạy:
```powershell
pip install -r requirements.txt
```

### Bước 3: Chạy file build_exe.bat
- **Cách 1**: Double-click vào file `build_exe.bat`
- **Cách 2**: Mở PowerShell và gõ: `.\build_exe.bat`

### Bước 4: Lấy file EXE
- File EXE sẽ được tạo trong thư mục: `dist\DienHoaApp.exe`
- Copy file này ra Desktop hoặc nơi bạn muốn
- Double-click để chạy!

---

## Phương Pháp 2: Chạy Lệnh Trực Tiếp (Nâng Cao)

Mở PowerShell trong thư mục này và chạy:

```powershell
# Cài đặt PyInstaller
pip install pyinstaller

# Build file EXE (một file duy nhất, không có cửa sổ console)
pyinstaller --onefile --windowed --name="DienHoaApp" main1.py
```

---

## Phương Pháp 3: Tạo File .spec Tùy Chỉnh (Cho Logo)

Nếu bạn muốn nhúng logo vào file EXE:

### Bước 1: Tạo file spec
```powershell
pyi-makespec --onefile --windowed --name="DienHoaApp" main1.py
```

### Bước 2: Sửa file DienHoaApp.spec
Thêm đường dẫn logo vào phần `datas`:
```python
datas=[
    ('E:/Download/dai-hoc-khoa-hoc-tu-nhien-Photoroom.png', '.'),
    ('E:/Download/Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png', '.'),
],
```

### Bước 3: Build từ file spec
```powershell
pyinstaller DienHoaApp.spec
```

---

## Lưu Ý Quan Trọng

1. **Đường dẫn logo**: File main1.py đang dùng đường dẫn tuyệt đối `E:/Download/...`
   - Nếu muốn chia sẻ file EXE, nên copy ảnh vào cùng thư mục với main1.py và sửa code

2. **Kích thước file**: File EXE có thể to (50-150MB) do chứa Python runtime và các thư viện

3. **Antivirus**: Một số antivirus có thể cảnh báo file EXE do PyInstaller. Đây là False Positive

4. **Cổng COM**: File EXE vẫn cần quyền truy cập cổng COM (serial port)

---

## Sửa Code Để Logo Hoạt Động Trong EXE

Nếu muốn logo hoạt động khi build EXE, sửa đường dẫn logo trong main1.py:

```python
# Thay vì:
img1 = Image.open("E:/Download/dai-hoc-khoa-hoc-tu-nhien-Photoroom.png")

# Sửa thành:
import os
import sys

def resource_path(relative_path):
    """Get absolute path to resource, works for dev and for PyInstaller"""
    try:
        # PyInstaller creates a temp folder and stores path in _MEIPASS
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

img1 = Image.open(resource_path("dai-hoc-khoa-hoc-tu-nhien-Photoroom.png"))
```

---

## Troubleshooting

### Lỗi "ImportError"
```powershell
pip install --upgrade pyinstaller
```

### File EXE chạy chậm
- Dùng `--onedir` thay vì `--onefile` để tăng tốc độ khởi động

### Logo không hiển thị
- Copy file ảnh vào cùng thư mục với file EXE

---

## Kết Quả

Sau khi build xong, bạn sẽ có:
- **File EXE**: `dist\DienHoaApp.exe` (có thể double-click để chạy)
- Không cần cài Python hay bất kỳ thư viện nào
- Có thể copy sang máy khác (Windows) và chạy trực tiếp
