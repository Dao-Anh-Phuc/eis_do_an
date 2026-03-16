import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageTk
import serial.tools.list_ports
import serial
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import threading
import tkinter.filedialog as filedialog
import tkinter.messagebox as messagebox
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
import csv
import math
import pandas as pd
from scipy.signal import savgol_filter
import tkinter.font as tkFont
import os
import sys

# Hàm hỗ trợ tìm đường dẫn file cho cả Python và PyInstaller EXE
def resource_path(relative_path):
    """Lấy đường dẫn tuyệt đối đến resource, hoạt động cho cả dev và PyInstaller"""
    try:
        # PyInstaller tạo temp folder và lưu path trong _MEIPASS
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(os.path.dirname(__file__))
    return os.path.join(base_path, relative_path)

# Chuyển toàn bộ code SWV thành một class SWVApp đầy đủ và sẵn sàng nhúng vào frame


#ASV

# ASV (Anodic Stripping Voltammetry)
class ASVApp:
    def __init__(self, parent):
        self.root = parent
        self.serial_port = None

        # Thong so DPASV: deposition + DPV stripping
        self.start_voltage = -1000  # mV (stripping)
        self.end_voltage = 100    # mV (stripping)
        self.step_voltage = 5     # mV
        self.pulse_amp = 50        # mV
        self.pulse_width = 40      # ms
        self.dep_voltage = -1100   # mV (negative voltage)
        self.dep_time = 120000     # ms

        # Dữ liệu
        self.buffer_voltage = []
        self.buffer_current_raw = []
        self.buffer_current_filtered = []
        self.x_data = []
        self.y_data = []
        self.receiver_count = 0
        self.expected_samples = 0
        self.is_receiving = False

        self.setup_gui()
        self.setup_plot()

    def setup_gui(self):
        # ===== Top bar với logo và tiêu đề =====
        top_frame = tk.Frame(self.root, bg="#E8F5E8")  # Xanh lá nhạt cho ASV
        top_frame.pack(side="top", fill="x", padx=5, pady=5)

        logo_frame = tk.Frame(top_frame, bg="#E8F5E8")
        logo_frame.pack(side="right", padx=10)

        try:
            img1 = Image.open(resource_path("dai-hoc-khoa-hoc-tu-nhien-Photoroom.png")).resize((120, 100), Image.Resampling.LANCZOS)
            self.logo1 = ImageTk.PhotoImage(img1)
            tk.Label(logo_frame, image=self.logo1, bg="#E8F5E8").grid(row=0, column=0, padx=5)

            title_label = tk.Label(
                logo_frame,
                text="Anodic Stripping Voltammetry\nby HUS",
                font=("Segoe UI", 14, "bold"),
                bg="#E8F5E8",
                fg="#2E7D32",  # Xanh lá đậm cho tiêu đề
                justify="center"
            )
            title_label.grid(row=0, column=1, padx=10)

            img2 = Image.open(resource_path("Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png")).resize((60, 60), Image.Resampling.LANCZOS)
            self.logo2 = ImageTk.PhotoImage(img2)
            tk.Label(logo_frame, image=self.logo2, bg="#E8F5E8").grid(row=0, column=2, padx=5)
        except Exception as e:
            print("Lỗi logo:", e)

        # ===== Main content chia 2 panel =====
        main_content = tk.Frame(self.root, bg="#E8F5E8")
        main_content.pack(fill="both", expand=True)

        self.frame_left = tk.Frame(main_content, bg="#E8F5E8")
        self.frame_left.pack(side="left", padx=15, pady=10, fill="y")

        self.frame_right = tk.Frame(main_content, bg="#E8F5E8")
        self.frame_right.pack(side="right", expand=True, fill="both", padx=10, pady=10)

        # ===== Left panel: Serial, Control, Parameters, Import/Export =====
        style = ttk.Style()
        style.configure("ASV.TButton", font=("Segoe UI", 14, "bold"), padding=8)
        style.configure("ASV.TLabel", font=("Segoe UI", 14, "bold"))

        # Serial Port Control
        ttk.Label(self.frame_left, text="Serial Port Control", font=("Segoe UI", 15, "bold"), foreground="#1976D2").pack(anchor="w", pady=(5, 10))
        self.port_combo = ttk.Combobox(self.frame_left, font=("Segoe UI", 14, "bold"), postcommand=self.refresh_ports)
        self.port_combo.pack(fill="x", pady=5)
        ttk.Button(self.frame_left, text="Refresh COM", command=self.refresh_ports, style="ASV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\U0001F50C Connect", command=self.connect_serial, style="ASV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\u274C Disconnect", command=self.disconnect_serial, style="ASV.TButton").pack(fill="x", pady=3)

        # Program Control
        ttk.Label(self.frame_left, text="Program Control", font=("Segoe UI", 15, "bold"), foreground="#388E3C").pack(anchor="w", pady=(12, 5))
        ttk.Button(self.frame_left, text="\u25B6\ufe0f Measure", command=self.send_measure_command, style="ASV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="🧹 Clear Data", command=self.clear_data, style="ASV.TButton").pack(fill="x", pady=3)

        # ===== ASV Parameter Config (DPASV) =====
        ttk.Label(self.frame_left, text="ASV Parameters", font=("Segoe UI", 15, "bold"), foreground="#D84315").pack(anchor="w", pady=(12, 5))

        # Deposition Phase
        deposition_frame = tk.LabelFrame(self.frame_left, text="1. Deposition Phase", font=("Segoe UI", 12, "bold"), 
                                       bg="#E8F5E8", fg="#3F51B5", relief="groove", bd=2)
        deposition_frame.pack(fill="x", pady=5, padx=2)
        self.dep_voltage_entry = self.add_labeled_entry(deposition_frame, "Dep Voltage (mV):", self.dep_voltage, row=0)
        self.dep_time_entry = self.add_labeled_entry(deposition_frame, "Dep Time (ms):", self.dep_time, row=1)

        # Stripping Phase (DPV)
        stripping_frame = tk.LabelFrame(self.frame_left, text="2. Stripping Phase (DPV)", font=("Segoe UI", 12, "bold"), 
                                      bg="#E8F5E8", fg="#E91E63", relief="groove", bd=2)
        stripping_frame.pack(fill="x", pady=5, padx=2)
        self.start_entry = self.add_labeled_entry(stripping_frame, "Start Voltage (mV):", self.start_voltage, row=0)
        self.end_entry = self.add_labeled_entry(stripping_frame, "End Voltage (mV):", self.end_voltage, row=1)
        self.step_entry = self.add_labeled_entry(stripping_frame, "Step (mV):", self.step_voltage, row=2)
        self.pulse_amp_entry = self.add_labeled_entry(stripping_frame, "Pulse Amp (mV):", self.pulse_amp, row=3)
        self.pulse_width_entry = self.add_labeled_entry(stripping_frame, "Pulse Width (ms):", self.pulse_width, row=4)

        # Import/Export
        ttk.Label(self.frame_left, text="Data Control", font=("Segoe UI", 15, "bold"), foreground="#00838F").pack(anchor="w", pady=(12, 5))
        ttk.Button(self.frame_left, text="\U0001F4C2 Import CSV", command=self.import_file, style="ASV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\U0001F4BE Export CSV", command=self.export_file, style="ASV.TButton").pack(fill="x", pady=3)

        # Status label
        self.status_label = ttk.Label(self.frame_left, text="Ready", foreground="#2E7D32", font=("Segoe UI", 13, "bold"))
        self.status_label.pack(pady=8)

    def add_labeled_entry(self, parent, label, default, row):
        label_widget = tk.Label(parent, text=label, font=("Segoe UI", 11, "bold"), bg="#E8F5E8", anchor="w", width=16)
        label_widget.grid(row=row, column=0, sticky="w", padx=(5, 5), pady=2)
        entry = ttk.Entry(parent, width=10, font=("Segoe UI", 11, "bold"))
        entry.insert(0, str(default))
        entry.grid(row=row, column=1, sticky="ew", pady=2, padx=(0, 5))
        parent.grid_columnconfigure(1, weight=1)
        return entry

    def setup_plot(self):
        self.fig, self.ax = plt.subplots(figsize=(6, 5))
        self.fig.patch.set_facecolor("#E8F5E8")  # Xanh lá nhạt cho nền
        self.ax.set_facecolor("#F1F8E9")         # Xanh lá rất nhạt cho trục
        self.ax.set_title("ASV Data", fontsize=18, fontweight='bold', color="#2E7D32")
        self.ax.set_xlabel("Voltage (mV)", fontsize=16, color="#2E7D32")
        self.ax.set_ylabel("Current (µA)", fontsize=16, color="#2E7D32")
        self.ax.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.line, = self.ax.plot([], [], 'r-', linewidth=2)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.frame_right)
        self.canvas.draw()
        self.toolbar = NavigationToolbar2Tk(self.canvas, self.frame_right)
        self.toolbar.update()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Làm to và đậm các số trên trục
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#2E7D32")

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])

    def connect_serial(self):
        port = self.port_combo.get().strip()
        if not port:
            messagebox.showwarning("Warning", "Please select a port")
            return
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            messagebox.showinfo("Connected", f"Connected to {port}")
        except serial.SerialException as e:
            messagebox.showerror("Serial Error", f"Could not open port {port}\n\n{str(e)}")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def disconnect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            messagebox.showinfo("Disconnected", "Serial port disconnected")

    def send_measure_command(self):
        if self.serial_port and self.serial_port.is_open:
            try:
                # Doc tham so DPASV
                self.start_voltage = int(self.start_entry.get())
                self.end_voltage = int(self.end_entry.get())
                self.step_voltage = int(self.step_entry.get())
                self.pulse_amp = int(self.pulse_amp_entry.get())
                self.pulse_width = int(self.pulse_width_entry.get())
                self.dep_voltage = int(self.dep_voltage_entry.get())
                self.dep_time = int(self.dep_time_entry.get())

                if self.step_voltage <= 0:
                    raise ValueError("Step must be > 0 mV")
                if self.pulse_amp <= 0 or self.pulse_width <= 0:
                    raise ValueError("Pulse amplitude/width must be > 0")
                if self.dep_time < 0:
                    raise ValueError("Deposition time must be >= 0")

                # Neu dep_t == 0 hoac dep_v == 0 thi firmware se chay nhu DPV (khong deposition)
                use_deposition = (self.dep_time != 0 and self.dep_voltage != 0)
                if use_deposition and self.dep_voltage >= 0:
                    raise ValueError("Deposition voltage must be negative in ASV deposition mode")

                # Firmware can /step, app can so diem de doc serial
                expected_points = int(abs(self.end_voltage - self.start_voltage) // self.step_voltage) + 1
                print(f"ASV Step size: {self.step_voltage} mV, expected points: {expected_points}")

                # Format moi: 8#S?E/step|pulseAmp$pulseWidth|depV$depT!
                asv_command = (f"8#{self.start_voltage}?{self.end_voltage}/{self.step_voltage}|"
                             f"{self.pulse_amp}${self.pulse_width}|"
                             f"{self.dep_voltage}${self.dep_time}!")
                
                mode_text = "ASV (Deposition + DPV)" if use_deposition else "DPV mode (dep_t=0 or dep_v=0)"
                print(f"Send ASV: {asv_command}")
                print(f"ASV Mode: {mode_text}")
                self.serial_port.write(asv_command.encode())
                
                # Reset dữ liệu
                self.buffer_voltage.clear()
                self.buffer_current_raw.clear()
                self.receiver_count = 0
                self.expected_samples = expected_points
                self.is_receiving = True
                self.status_label.config(text=f"Measuring: {mode_text}")
                threading.Thread(target=self.read_serial, daemon=True).start()
                
            except Exception as e:
                messagebox.showerror("Send Error", f"Could not send ASV command.\n\n{str(e)}")
        else:
            messagebox.showwarning("Warning", "Serial port not connected")

    def read_serial(self):
        while self.is_receiving and self.receiver_count < self.expected_samples:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line and ";" in line:
                    direction = 1 if self.end_voltage >= self.start_voltage else -1
                    self.buffer_voltage.append(self.start_voltage + self.receiver_count * self.step_voltage * direction)
                    self.buffer_current_raw.append(float(line.split(";")[1]))
                    self.receiver_count += 1
            except Exception as e:
                print("Read error:", e)
                break
        self.is_receiving = False
        self.process_asv_data()

    def process_asv_data(self):
        # ASV processing giống LSV cho stripping phase
        self.buffer_current_filtered = self.smooth_data(self.buffer_current_raw)
        self.x_data = self.buffer_voltage
        self.y_data = self.buffer_current_filtered
        self.status_label.config(text=f"ASV Completed: {self.receiver_count} points")
        self.update_plot()

    def smooth_data(self, data):
        if len(data) < 3:
            return data
        result = [data[0], (data[0] + data[1]) / 2]
        for i in range(2, len(data)):
            result.append((data[i] + data[i - 1] + data[i - 2]) / 3)
        return result

    def update_plot(self):
        self.line.set_data(self.x_data, self.y_data)
        self.ax.relim()
        self.ax.autoscale_view()
        self.ax.set_xlim(min(self.start_voltage, self.end_voltage) - 50,
                         max(self.start_voltage, self.end_voltage) + 50)
        self.ax.set_ylabel("Current (µA)", fontsize=14)
        # Làm to và đậm các số trên trục mỗi lần cập nhật
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#2E7D32")
        self.canvas.draw()

    def clear_data(self):
        self.x_data.clear()
        self.y_data.clear()
        self.buffer_voltage.clear()
        self.buffer_current_raw.clear()
        self.buffer_current_filtered.clear()
        self.receiver_count = 0
        self.status_label.config(text="Ready")
        self.update_plot()

    def import_file(self):
        filepath = filedialog.askopenfilename(filetypes=[("CSV Files", "*.csv")])
        if filepath:
            with open(filepath, 'r') as f:
                lines = f.readlines()
                try:
                    # Ho tro ca format moi va cu
                    params = {}
                    data_start_idx = None
                    for idx, line in enumerate(lines):
                        parts = [p.strip() for p in line.strip().split(",")]
                        if len(parts) >= 2 and parts[0] == "Voltage (mV)":
                            data_start_idx = idx + 1
                            break
                        if len(parts) >= 2:
                            params[parts[0]] = parts[1]

                    self.start_voltage = int(params.get("Start Voltage", self.start_voltage))
                    self.end_voltage = int(params.get("End Voltage", self.end_voltage))
                    self.step_voltage = int(params.get("Step", self.step_voltage))
                    self.pulse_amp = int(params.get("Pulse Amplitude", self.pulse_amp))
                    self.pulse_width = int(params.get("Pulse Width", self.pulse_width))
                    self.dep_voltage = int(params.get("Deposition Voltage", self.dep_voltage))
                    self.dep_time = int(params.get("Deposition Time", self.dep_time))

                    # Update UI
                    self.start_entry.delete(0, tk.END)
                    self.start_entry.insert(0, str(self.start_voltage))
                    self.end_entry.delete(0, tk.END)
                    self.end_entry.insert(0, str(self.end_voltage))
                    self.step_entry.delete(0, tk.END)
                    self.step_entry.insert(0, str(self.step_voltage))
                    self.pulse_amp_entry.delete(0, tk.END)
                    self.pulse_amp_entry.insert(0, str(self.pulse_amp))
                    self.pulse_width_entry.delete(0, tk.END)
                    self.pulse_width_entry.insert(0, str(self.pulse_width))
                    self.dep_voltage_entry.delete(0, tk.END)
                    self.dep_voltage_entry.insert(0, str(self.dep_voltage))
                    self.dep_time_entry.delete(0, tk.END)
                    self.dep_time_entry.insert(0, str(self.dep_time))
                    
                    # Import data
                    self.buffer_voltage.clear()
                    self.buffer_current_raw.clear()
                    self.buffer_current_filtered.clear()
                    if data_start_idx is None:
                        raise ValueError("ASV file missing data header 'Voltage (mV),Current (uA)'")

                    for line in lines[data_start_idx:]:
                        parts = line.strip().split(",")
                        if len(parts) == 2:
                            self.buffer_voltage.append(float(parts[0]))
                            self.buffer_current_raw.append(float(parts[1]))
                    
                    self.x_data = self.buffer_voltage
                    self.y_data = self.smooth_data(self.buffer_current_raw)
                    self.status_label.config(text="ASV data imported successfully.")
                    self.update_plot()
                except Exception as e:
                    messagebox.showerror("Import Error", str(e))

    def export_file(self):
        filepath = filedialog.asksaveasfilename(defaultextension=".csv")
        if filepath:
            try:
                with open(filepath, 'w') as f:
                    # Export tham so DPASV
                    f.write(f"Start Voltage,{self.start_voltage},[mV]\n")
                    f.write(f"End Voltage,{self.end_voltage},[mV]\n")
                    f.write(f"Step,{self.step_voltage},[mV]\n")
                    f.write(f"Pulse Amplitude,{self.pulse_amp},[mV]\n")
                    f.write(f"Pulse Width,{self.pulse_width},[ms]\n")
                    f.write(f"Deposition Voltage,{self.dep_voltage},[mV]\n")
                    f.write(f"Deposition Time,{self.dep_time},[ms]\n")
                    f.write("Voltage (mV),Current (uA)\n")
                    for v, raw in zip(self.buffer_voltage, self.buffer_current_raw):
                        f.write(f"{v},{raw}\n")
                
                import os
                image_path = os.path.splitext(filepath)[0] + "_asv_plot.png"
                self.fig.savefig(image_path, bbox_inches='tight', dpi=300)
                messagebox.showinfo("Exported", f"ASV data and plot exported successfully!\nCSV: {filepath}\nImage: {image_path}")
            except Exception as e:
                messagebox.showerror("Export Error", str(e))

# LSV
class LSVApp:
    def __init__(self, parent):
        self.root = parent
        self.serial_port = None

        # Thông số mặc định
        self.start_voltage = -200  # mV
        self.end_voltage = 700     # mV
        self.step_voltage = 10     # mV

        # Dữ liệu
        self.buffer_voltage = []
        self.buffer_current_raw = []
        self.buffer_current_filtered = []
        self.x_data = []
        self.y_data = []
        self.receiver_count = 0
        self.expected_samples = 0
        self.is_receiving = False

        self.setup_gui()
        self.setup_plot()

    def setup_gui(self):
        # ===== Top bar với logo và tiêu đề =====
        top_frame = tk.Frame(self.root, bg="#D1C4E9")
        top_frame.pack(side="top", fill="x", padx=5, pady=5)

        logo_frame = tk.Frame(top_frame, bg="#D1C4E9")
        logo_frame.pack(side="right", padx=10)

        try:
            img1 = Image.open(resource_path("dai-hoc-khoa-hoc-tu-nhien-Photoroom.png")).resize((120, 100), Image.Resampling.LANCZOS)
            self.logo1 = ImageTk.PhotoImage(img1)
            tk.Label(logo_frame, image=self.logo1, bg="#D1C4E9").grid(row=0, column=0, padx=5)

            title_label = tk.Label(
                logo_frame,
                text="Linear Sweep Voltammetry\nby HUS",
                font=("Segoe UI", 14, "bold"),
                bg="#D1C4E9",
                fg="#6A1B9A",  # Màu tím đậm cho tiêu đề
                justify="center"
            )
            title_label.grid(row=0, column=1, padx=10)

            img2 = Image.open(resource_path("Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png")).resize((60, 60), Image.Resampling.LANCZOS)
            self.logo2 = ImageTk.PhotoImage(img2)
            tk.Label(logo_frame, image=self.logo2, bg="#D1C4E9").grid(row=0, column=2, padx=5)
        except Exception as e:
            print("Lỗi logo:", e)

        # ===== Main content chia 2 panel =====
        main_content = tk.Frame(self.root, bg="#D1C4E9")
        main_content.pack(fill="both", expand=True)

        self.frame_left = tk.Frame(main_content, bg="#D1C4E9")
        self.frame_left.pack(side="left", padx=15, pady=10, fill="y")

        self.frame_right = tk.Frame(main_content, bg="#D1C4E9")
        self.frame_right.pack(side="right", expand=True, fill="both", padx=10, pady=10)

        # ===== Left panel: Serial, Control, Parameter, Import/Export =====
        style = ttk.Style()
        style.configure("LSV.TButton", font=("Segoe UI", 14, "bold"), padding=8)
        style.configure("LSV.TLabel", font=("Segoe UI", 14, "bold"))

        # Serial Port Control
        ttk.Label(self.frame_left, text="Serial Port Control", font=("Segoe UI", 15, "bold"), foreground="#6A1B9A").pack(anchor="w", pady=(5, 10))
        self.port_combo = ttk.Combobox(self.frame_left, font=("Segoe UI", 14, "bold"), postcommand=self.refresh_ports)
        self.port_combo.pack(fill="x", pady=5)
        ttk.Button(self.frame_left, text="Refresh COM", command=self.refresh_ports, style="LSV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\U0001F50C Connect", command=self.connect_serial, style="LSV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\u274C Disconnect", command=self.disconnect_serial, style="LSV.TButton").pack(fill="x", pady=3)

        # Program Control
        ttk.Label(self.frame_left, text="Program Control", font=("Segoe UI", 15, "bold"), foreground="#388E3C").pack(anchor="w", pady=(12, 5))
        ttk.Button(self.frame_left, text="\u25B6\ufe0f Measure", command=self.send_measure_command, style="LSV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="🧹 Clear Data", command=self.clear_data, style="LSV.TButton").pack(fill="x", pady=3)

        # Parameter config
        ttk.Label(self.frame_left, text="Parameter Config", font=("Segoe UI", 15, "bold"), foreground="#D84315").pack(anchor="w", pady=(12, 5))
        param_frame = tk.Frame(self.frame_left, bg="#D1C4E9")
        param_frame.pack(fill="x", pady=2)
        self.start_entry = self.add_labeled_entry(param_frame, "Start Voltage (mV):", self.start_voltage, row=0)
        self.end_entry = self.add_labeled_entry(param_frame, "End Voltage (mV):", self.end_voltage, row=1)
        self.step_entry = self.add_labeled_entry(param_frame, "Step (mV):", self.step_voltage, row=2)

        # Import/Export
        ttk.Label(self.frame_left, text="Data Control", font=("Segoe UI", 15, "bold"), foreground="#00838F").pack(anchor="w", pady=(12, 5))
        ttk.Button(self.frame_left, text="\U0001F4C2 Import CSV", command=self.import_file, style="LSV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\U0001F4BE Export CSV", command=self.export_file, style="LSV.TButton").pack(fill="x", pady=3)

        # Status label
        self.status_label = ttk.Label(self.frame_left, text="Ready", foreground="#6A1B9A", font=("Segoe UI", 13, "bold"))
        self.status_label.pack(pady=8)

    def add_labeled_entry(self, parent, label, default, row):
        label_widget = tk.Label(parent, text=label, font=("Segoe UI", 14, "bold"), bg="#D1C4E9", anchor="w", width=18)
        label_widget.grid(row=row, column=0, sticky="w", padx=(0, 5), pady=2)
        entry = ttk.Entry(parent, width=10, font=("Segoe UI", 14, "bold"))
        entry.insert(0, str(default))
        entry.grid(row=row, column=1, sticky="ew", pady=2)
        parent.grid_columnconfigure(1, weight=1)
        return entry

    def setup_plot(self):
        self.fig, self.ax = plt.subplots(figsize=(6, 5))
        self.fig.patch.set_facecolor("#D1C4E9")
        self.ax.set_facecolor("#F3E5F5")
        self.ax.set_title("LSV Data", fontsize=18, fontweight='bold', color="#6A1B9A")
        self.ax.set_xlabel("Voltage (mV)", fontsize=16, color="#6A1B9A")
        self.ax.set_ylabel("Current (µA)", fontsize=16, color="#6A1B9A")
        self.ax.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.line, = self.ax.plot([], [], 'r-', linewidth=2)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.frame_right)
        self.canvas.draw()
        self.toolbar = NavigationToolbar2Tk(self.canvas, self.frame_right)
        self.toolbar.update()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Làm to và đậm các số trên trục
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#6A1B9A")

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])

    def connect_serial(self):
        port = self.port_combo.get().strip()
        if not port:
            messagebox.showwarning("Warning", "Please select a port")
            return
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            messagebox.showinfo("Connected", f"Connected to {port}")
        except serial.SerialException as e:
            messagebox.showerror("Serial Error", f"Could not open port {port}\n\n{str(e)}")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def disconnect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            messagebox.showinfo("Disconnected", "Serial port disconnected")

    def send_measure_command(self):
        if self.serial_port and self.serial_port.is_open:
            try:
                self.start_voltage = int(self.start_entry.get())
                self.end_voltage = int(self.end_entry.get())
                self.step_voltage = int(self.step_entry.get())
                num_step = ((self.end_voltage - self.start_voltage) // self.step_voltage + 1)
                print(num_step)
                lsv_command = f"7#{self.start_voltage}?{self.end_voltage}/{num_step}|1$0!"
                print(f"Send: {lsv_command}")
                self.serial_port.write(lsv_command.encode())
                self.buffer_voltage.clear()
                self.buffer_current_raw.clear()
                self.receiver_count = 0
                self.expected_samples = num_step
                self.is_receiving = True
                self.status_label.config(text="Receiving data...")
                threading.Thread(target=self.read_serial, daemon=True).start()
            except Exception as e:
                messagebox.showerror("Send Error", f"Could not send command.\n\n{str(e)}")
        else:
            messagebox.showwarning("Warning", "Serial port not connected")

    def read_serial(self):
        while self.is_receiving and self.receiver_count < self.expected_samples:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line and ";" in line:
                    self.buffer_voltage.append(self.start_voltage + self.receiver_count * self.step_voltage)
                    self.buffer_current_raw.append(float(line.split(";")[1]))
                    self.receiver_count += 1
            except Exception as e:
                print("Read error:", e)
                break
        self.is_receiving = False
        self.process_lsv_data()

    def process_lsv_data(self):
        # LSV chỉ một chiều, không đảo chiều
        self.buffer_current_filtered = self.smooth_data(self.buffer_current_raw)
        self.x_data = self.buffer_voltage
        self.y_data = self.buffer_current_filtered
        self.status_label.config(text=f"Received: {self.receiver_count} points")
        self.update_plot()

    def smooth_data(self, data):
        if len(data) < 3:
            return data
        result = [data[0], (data[0] + data[1]) / 2]
        for i in range(2, len(data)):
            result.append((data[i] + data[i - 1] + data[i - 2]) / 3)
        return result

    def update_plot(self):
        self.line.set_data(self.x_data, self.y_data)
        self.ax.relim()
        self.ax.autoscale_view()
        self.ax.set_xlim(self.start_voltage - 50, self.end_voltage + 50)
        self.ax.set_ylabel("Current (µA)", fontsize=14)
        # Làm to và đậm các số trên trục mỗi lần cập nhật
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#6A1B9A")
        self.canvas.draw()

    def clear_data(self):
        self.x_data.clear()
        self.y_data.clear()
        self.buffer_voltage.clear()
        self.buffer_current_raw.clear()
        self.buffer_current_filtered.clear()
        self.receiver_count = 0
        self.status_label.config(text="Ready")
        self.update_plot()

    def import_file(self):
        filepath = filedialog.askopenfilename(filetypes=[("CSV Files", "*.csv")])
        if filepath:
            with open(filepath, 'r') as f:
                lines = f.readlines()
                if len(lines) < 5:
                    messagebox.showerror("Error", "File format is incorrect!")
                    return
                try:
                    self.start_voltage = int(lines[0].split(',')[1])
                    self.end_voltage = int(lines[1].split(',')[1])
                    self.step_voltage = int(lines[2].split(',')[1])
                    self.start_entry.delete(0, tk.END)
                    self.start_entry.insert(0, str(self.start_voltage))
                    self.end_entry.delete(0, tk.END)
                    self.end_entry.insert(0, str(self.end_voltage))
                    self.step_entry.delete(0, tk.END)
                    self.step_entry.insert(0, str(self.step_voltage))
                    self.buffer_voltage.clear()
                    self.buffer_current_raw.clear()
                    self.buffer_current_filtered.clear()
                    for line in lines[4:]:
                        parts = line.strip().split(",")
                        if len(parts) == 2:
                            self.buffer_voltage.append(float(parts[0]))
                            self.buffer_current_raw.append(float(parts[1]))
                    self.x_data = self.buffer_voltage
                    self.y_data = self.smooth_data(self.buffer_current_raw)
                    self.status_label.config(text="Data imported successfully.")
                    self.update_plot()
                except Exception as e:
                    messagebox.showerror("Import Error", str(e))

    def export_file(self):
        filepath = filedialog.asksaveasfilename(defaultextension=".csv")
        if filepath:
            try:
                with open(filepath, 'w') as f:
                    f.write(f"Start Voltage,{self.start_voltage},[mV]\n")
                    f.write(f"End Voltage,{self.end_voltage},[mV]\n")
                    f.write(f"Step,{self.step_voltage},[mV]\n")
                    f.write("Voltage (mV),Current (uA)\n")
                    for v, raw in zip(self.buffer_voltage, self.buffer_current_raw):
                        f.write(f"{v},{raw}\n")
                import os
                image_path = os.path.splitext(filepath)[0] + "_plot.png"
                self.fig.savefig(image_path, bbox_inches='tight', dpi=300)
                messagebox.showinfo("Exported", f"Data and plot exported successfully!\nCSV: {filepath}\nImage: {image_path}")
            except Exception as e:
                messagebox.showerror("Export Error", str(e))
# DPV

class DPVApp:
    def __init__(self, parent):
        import tkinter as tk
        from tkinter import ttk, messagebox, filedialog
        import serial
        import serial.tools.list_ports
        import matplotlib.pyplot as plt
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
        import threading
        import csv
        from PIL import Image, ImageTk
        import os
        import time
        import numpy as np
        from scipy.signal import savgol_filter, medfilt
        from scipy.ndimage import gaussian_filter1d
        
        self.serial = serial
        self.threading = threading
        self.messagebox = messagebox
        self.filedialog = filedialog
        self.ttk = ttk
        self.time = time
        self.np = np
        self.savgol_filter = savgol_filter
        self.medfilt = medfilt
        self.gaussian_filter1d = gaussian_filter1d

        self.ser = None
        self.is_measuring = False
        self.bufferV = []
        self.bufferI = []
        self.bufferIfilter = []

        self.parent = parent
        self.root = parent

        # ✅ THÊM: Bộ lọc mặc định cho DPV
        self.filter_type = "1/7 (Moving Avg 7pt)"

        # DPV deposition parameters
        self.use_deposition = tk.BooleanVar(value=False)
        self.dep_voltage = tk.IntVar(value=-500)
        self.dep_time = tk.IntVar(value=60000)

        # Style và màu nền
        bg_color = "#E1F5FE"
        title_color = "#0288D1"

        style = ttk.Style()
        style.configure("DPV.TButton", font=("Segoe UI", 14, "bold"), padding=8)
        style.configure("DPV.TLabel", font=("Segoe UI", 14, "bold"))

        # Top Bar
        top_frame = tk.Frame(self.parent, bg=bg_color)
        top_frame.pack(side="top", fill="x", padx=5, pady=5)

        logo_frame = tk.Frame(top_frame, bg=bg_color)
        logo_frame.pack(side="right", padx=10)

        try:
            img1 = Image.open(resource_path("dai-hoc-khoa-hoc-tu-nhien-Photoroom.png")).resize((100, 80), Image.Resampling.LANCZOS)
            self.logo1 = ImageTk.PhotoImage(img1)
            logo1_label = tk.Label(logo_frame, image=self.logo1, bg=bg_color)
            logo1_label.grid(row=0, column=0, padx=5)

            title_label = tk.Label(
                logo_frame,
                text="Differential Pulse Voltammetry\nby HUS",
                font=("Segoe UI", 13, "bold"),
                bg=bg_color,
                fg=title_color,
                justify="center"
            )
            title_label.grid(row=0, column=1, padx=10)

            img2 = Image.open("E:/oE/Download/images-Photoroom.png").resize((50, 50), Image.Resampling.LANCZOS)
            self.logo2 = ImageTk.PhotoImage(img2)
            logo2_label = tk.Label(logo_frame, image=self.logo2, bg=bg_color)
            logo2_label.grid(row=0, column=2, padx=5)
        except Exception as e:
            print("Lỗi ảnh logo:", e)

        # Main Content
        main_content = tk.Frame(self.parent, bg=bg_color)
        main_content.pack(fill="both", expand=True)

        self.frame_left = tk.Frame(main_content, bg=bg_color, width=320)
        self.frame_left.pack(side="left", padx=10, pady=10, fill="y")
        self.frame_left.pack_propagate(False)

        control_panel = tk.Frame(self.frame_left, bg=bg_color)
        control_panel.pack(anchor="n", fill="x", padx=0, pady=0)

        # Serial Port Control
        tk.Label(control_panel, text="Serial Port Control", font=("Segoe UI", 15, "bold"), fg=title_color, bg=bg_color).pack(anchor="w", pady=(5, 10))
        self.port_combo = ttk.Combobox(control_panel, font=("Segoe UI", 14, "bold"), width=14, postcommand=self.refresh_ports)
        self.port_combo.pack(fill="x", pady=5)
        ttk.Button(control_panel, text="Refresh COM", command=self.refresh_ports, style="DPV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="\U0001F50C Connect", command=self.connect_serial, style="DPV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="\u274C Disconnect", command=self.disconnect_serial, style="DPV.TButton").pack(fill="x", pady=3)

        # Program Control
        tk.Label(control_panel, text="Program Control", font=("Segoe UI", 15, "bold"), fg="#388E3C", bg=bg_color).pack(anchor="w", pady=(12, 5))
        ttk.Button(control_panel, text="\u25B6\ufe0f Measure", command=self.start_measurement, style="DPV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="🧹 Clear Data", command=self.clear_all, style="DPV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="\U0001F4C2 Import CSV", command=self.import_from_csv, style="DPV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="\U0001F4BE Export CSV", command=self.export_to_csv, style="DPV.TButton").pack(fill="x", pady=3)

        # Parameter config
        tk.Label(control_panel, text="Parameter Config", font=("Segoe UI", 15, "bold"), fg="#D84315", bg=bg_color).pack(anchor="w", pady=(10, 4))
        self.start_entry = self.add_labeled_entry("Start Voltage (mV)", -200, font=("Segoe UI", 12, "bold"), parent=control_panel)
        self.end_entry = self.add_labeled_entry("End Voltage (mV)", 600, font=("Segoe UI", 12, "bold"), parent=control_panel)
        self.step_entry = self.add_labeled_entry("E_Step (mV)", 5, font=("Segoe UI", 12, "bold"), parent=control_panel)
        self.amp_entry = self.add_labeled_entry("Pulse Amplitude (mV)", 50, font=("Segoe UI", 12, "bold"), parent=control_panel)
        self.width_entry = self.add_labeled_entry("Pulse Width (ms)", 50, font=("Segoe UI", 12, "bold"), parent=control_panel)

        # ✅ THÊM: Filter Selection cho DPV
        tk.Label(control_panel, text="Filter Selection", font=("Segoe UI", 14, "bold"), fg="#9C27B0", bg=bg_color).pack(anchor="w", pady=(10, 4))
        
        filter_frame = tk.Frame(control_panel, bg=bg_color)
        filter_frame.pack(fill="x", pady=2)
        
        tk.Label(filter_frame, text="Filter Type:", font=("Segoe UI", 11, "bold"), bg=bg_color, anchor="w", width=15).grid(row=0, column=0, sticky="w", padx=(2, 3), pady=2)
        
        self.filter_combo = ttk.Combobox(
            filter_frame, 
            values=[
                "1/3 (Moving Avg 3pt)",
                "1/7 (Moving Avg 7pt)", 
                "1/9 (Moving Avg 9pt)",
                "1/11 (Moving Avg 11pt)",
                "1/13 (Moving Avg 13pt)",
                "1/15 (Moving Avg 15pt)",
                "Savitzky-Golay",
                "Gaussian Filter",
                "Median Filter",
                "Combo Filter",
                "Exponential Smoothing"
            ],
            font=("Segoe UI", 10, "bold"),
            state="readonly",
            width=18
        )
        self.filter_combo.set("1/7 (Moving Avg 7pt)")
        self.filter_combo.grid(row=0, column=1, sticky="ew", pady=2)
        self.filter_combo.bind("<<ComboboxSelected>>", self.on_filter_changed)
        filter_frame.grid_columnconfigure(1, weight=1)

        # ✅ THÊM: Filter Info Label cho DPV
        self.filter_info_label = tk.Label(
            control_panel, 
            text="Current: 7-point moving average\nBest for: General smoothing", 
            font=("Segoe UI", 9), 
            bg=bg_color, 
            fg="#666666",
            justify="left",
            anchor="w"
        )
        self.filter_info_label.pack(anchor="w", pady=(2, 8))

        # DPV Deposition (Optional)
        tk.Label(control_panel, text="DPV + Deposition (Optional)", font=("Segoe UI", 14, "bold"), fg="#E91E63", bg=bg_color).pack(anchor="w", pady=(10, 4))

        # Checkbox để bật/tắt deposition
        self.deposition_checkbox = tk.Checkbutton(
            control_panel,
            text="Enable Deposition",
            variable=self.use_deposition,
            font=("Segoe UI", 12, "bold"),
            bg=bg_color,
            fg="#E91E63",
            selectcolor=bg_color,
            command=self.toggle_deposition_controls
        )
        self.deposition_checkbox.pack(anchor="w", pady=2)

        # Frame chứa 2 controls deposition (ẩn mặc định)
        self.deposition_frame = tk.Frame(control_panel, bg=bg_color)
        self.deposition_frame.pack(fill="x", pady=2)

        # 2 thông số deposition
        self.dep_voltage_entry = self.add_labeled_entry_simple("Dep Voltage (mV)", self.dep_voltage, parent=self.deposition_frame)
        self.dep_time_entry = self.add_labeled_entry_simple("Dep Time (ms)", self.dep_time, parent=self.deposition_frame)

        # Ẩn frame ban đầu
        self.deposition_frame.pack_forget()

        # Plot Area
        self.frame_right = tk.Frame(main_content, bg=bg_color)
        self.frame_right.pack(side="right", expand=True, fill="both", padx=10, pady=10)

        self.fig, self.ax = plt.subplots(figsize=(6.5, 5))
        self.fig.patch.set_facecolor(bg_color)
        self.ax.set_facecolor('white')
        self.ax.set_title("Differential Pulse Voltammetry", fontsize=20, fontweight="bold", color=title_color)
        self.ax.set_xlabel("Voltage (mV)", fontsize=15, color=title_color)
        self.ax.set_ylabel("Current (μA)", fontsize=15, color=title_color)
        self.ax.grid(True)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.frame_right)
        self.canvas_widget = self.canvas.get_tk_widget()
        self.canvas_widget.pack(expand=True, fill="both")
        plt.tight_layout()
        plt.close(self.fig)

        # Tick label style
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#000000")
        self.canvas.draw()

    def add_labeled_entry(self, label, default, font=("Segoe UI", 14, "bold"), parent=None):
        if parent is None:
            parent = self.frame_left
        frame = tk.Frame(parent, bg="#FFFFFF")
        frame.pack(fill="x", pady=4)
        tk.Label(
            frame,
            text=label,
            font=font,
            bg="#0288D1",
            fg="white",
            anchor="w",
            width=18,
            height=2
        ).pack(side="left", padx=(0, 5))
        entry = tk.Entry(
            frame,
            width=12,
            font=font,
            justify="center",
            fg="black",
            bg="white"
        )
        entry.insert(0, str(default))
        entry.pack(side="right", fill="x", ipady=6)
        return entry

    def add_labeled_entry_simple(self, label, var, parent=None):
        """Tạo labeled entry đơn giản cho deposition"""
        if parent is None:
            parent = self.frame_left
        frame = tk.Frame(parent, bg="#E1F5FE")
        frame.pack(fill="x", pady=2)
        tk.Label(
            frame,
            text=label,
            font=("Segoe UI", 11, "bold"),
            bg="#0288D1",
            fg="white",
            anchor="w",
            width=15,
            height=1
        ).pack(side="left", padx=(2, 3))
        entry = tk.Entry(
            frame,
            textvariable=var,
            width=10,
            font=("Segoe UI", 11, "bold"),
            justify="center",
            fg="black",
            bg="white"
        )
        entry.pack(side="right", fill="x", ipady=2)
        return entry

    def toggle_deposition_controls(self):
        """Hiện/ẩn controls deposition khi checkbox được bật/tắt"""
        if self.use_deposition.get():
            self.deposition_frame.pack(fill="x", pady=2)
        else:
            self.deposition_frame.pack_forget()

    # ✅ THÊM: Xử lý khi thay đổi bộ lọc cho DPV
    def on_filter_changed(self, event=None):
        selected = self.filter_combo.get()
        
        # Cập nhật thông tin bộ lọc
        filter_info = {
            "1/3 (Moving Avg 3pt)": "Current: 3-point moving average\nBest for: Fast processing, minimal smoothing",
            "1/7 (Moving Avg 7pt)": "Current: 7-point moving average\nBest for: General smoothing, good balance",
            "1/9 (Moving Avg 9pt)": "Current: 9-point moving average\nBest for: Heavy smoothing, noisy data",
            "1/11 (Moving Avg 11pt)": "Current: 11-point moving average\nBest for: Maximum smoothing, very noisy data",
            "1/13 (Moving Avg 13pt)": "Current: 13-point moving average\nBest for: Ultra smoothing, extremely noisy data",
            "1/15 (Moving Avg 15pt)": "Current: 15-point moving average\nBest for: Maximum smoothing, extreme noise reduction",
            "Savitzky-Golay": "Current: Savitzky-Golay filter\nBest for: Preserving peaks, scientific data",
            "Gaussian Filter": "Current: Gaussian smoothing\nBest for: Natural smoothing, edge preservation",
            "Median Filter": "Current: Median filter\nBest for: Removing spikes, outlier removal",
            "Combo Filter": "Current: Median + Savitzky-Golay\nBest for: Ultimate quality, spike + noise removal",
            "Exponential Smoothing": "Current: Exponential weighted average\nBest for: Real-time data, recent emphasis"
        }
        
        self.filter_info_label.config(text=filter_info.get(selected, "Unknown filter"))
        
        # Lưu lại filter type để sử dụng trong smooth_data
        self.filter_type = selected
        
        # Nếu đã có dữ liệu, áp dụng filter mới ngay lập tức
        if self.bufferI:
            self.bufferIfilter = self.smoothing_data_dpv_advanced(self.bufferI)
            self.draw_graph()

    def refresh_ports(self):
        ports = self.serial.tools.list_ports.comports()
        self.port_combo['values'] = [port.device for port in ports]
        self.port_combo.set(ports[0].device if ports else '')

    def safe_clear_buffer(self):
        """Clear serial buffer an toàn"""
        if self.ser and self.ser.is_open:
            try:
                self.ser.reset_input_buffer()
                start_time = self.time.time()
                while self.time.time() - start_time < 0.5:
                    try:
                        self.ser.timeout = 0.1
                        remaining = self.ser.readline()
                        if not remaining:
                            break
                    except:
                        break
                self.ser.timeout = 1
                print("Serial buffer cleared successfully")
            except Exception as e:
                print(f"Error clearing buffer: {e}")

    def connect_serial(self):
        try:
            port = self.port_combo.get()
            if not port:
                self.messagebox.showwarning("Cảnh báo", "Vui lòng chọn cổng COM!")
                return
                
            if self.ser and self.ser.is_open:
                self.ser.close()
                self.time.sleep(0.2)
                
            self.ser = self.serial.Serial(
                port=port, 
                baudrate=115200, 
                timeout=1,
                bytesize=self.serial.EIGHTBITS,
                parity=self.serial.PARITY_NONE,
                stopbits=self.serial.STOPBITS_ONE,
                xonxoff=False,
                rtscts=False,
                dsrdtr=False
            )
            
            self.time.sleep(0.5)
            self.safe_clear_buffer()
            self.messagebox.showinfo("COM", f"Kết nối {port} thành công!")
            
        except self.serial.SerialException as e:
            self.messagebox.showerror("Lỗi COM", f"Không thể mở cổng {port}\n{str(e)}")
        except Exception as e:
            self.messagebox.showerror("Lỗi", f"Lỗi kết nối: {str(e)}")

    def disconnect_serial(self):
        if self.ser and self.ser.is_open:
            self.is_measuring = False
            self.ser.close()
            self.messagebox.showinfo("COM", "Đã ngắt kết nối!")

    def clear_all(self):
        self.is_measuring = False
        self.bufferV.clear()
        self.bufferI.clear()
        self.bufferIfilter.clear()
        
        # Reset plot
        self.ax.cla()
        self.ax.set_title("Differential Pulse Voltammetry", fontsize=18, fontweight="bold", color="#0288D1")
        self.ax.set_xlabel("Voltage (mV)", fontsize=18)
        self.ax.set_ylabel("Current (μA)", fontsize=18)
        self.ax.grid(True)
        self.canvas.draw()
        
        if self.ser and self.ser.is_open:
            try:
                self.safe_clear_buffer()
            except:
                pass

    def export_to_csv(self):
        if not self.bufferV:
            self.messagebox.showwarning("Cảnh báo", "Không có dữ liệu để xuất!")
            return

        file_path = self.filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV Files", "*.csv")])
        if not file_path:
            return

        try:
            with open(file_path, mode='w', newline='', encoding='utf-8') as file:
                writer = csv.writer(file)
                writer.writerow(["Start Voltage", self.start_entry.get(), "[mV]"])
                writer.writerow(["End Voltage", self.end_entry.get(), "[mV]"])
                writer.writerow(["Step", self.step_entry.get(), "[mV]"])
                writer.writerow(["Pulse Amplitude", self.amp_entry.get(), "[mV]"])
                writer.writerow(["Pulse Width", self.width_entry.get(), "[ms]"])
                
                # ✅ THÊM: Lưu thông tin filter
                writer.writerow(["Filter Type", getattr(self, 'filter_type', '1/7 (Moving Avg 7pt)'), "[type]"])
                
                # Thêm thông số deposition
                writer.writerow(["Use Deposition", self.use_deposition.get(), "[bool]"])
                if self.use_deposition.get():
                    writer.writerow(["Deposition Voltage", self.dep_voltage.get(), "[mV]"])
                    writer.writerow(["Deposition Time", self.dep_time.get(), "[ms]"])
                
                writer.writerow(["Voltage (mV)", "Current (μA)", "Filtered (μA)"])
                for i in range(len(self.bufferV)):
                    writer.writerow([
                        self.bufferV[i],
                        self.bufferI[i],
                        self.bufferIfilter[i] if i < len(self.bufferIfilter) else ""
                    ])
            import os
            image_path = os.path.splitext(file_path)[0] + "_plot.png"
            self.fig.savefig(image_path, bbox_inches='tight', dpi=300)
            self.messagebox.showinfo("Thành công", f"Xuất dữ liệu và đồ thị thành công!\nCSV: {file_path}\nẢnh: {image_path}")
        except Exception as e:
            self.messagebox.showerror("Lỗi", f"Lỗi khi ghi file:\n{str(e)}")

    def import_from_csv(self):
        file_path = self.filedialog.askopenfilename(filetypes=[("CSV Files", "*.csv")])
        if not file_path:
            return

        try:
            with open(file_path, mode='r') as file:
                reader = csv.reader(file)
                rows = list(reader)
                
                # Import basic parameters
                self.start_entry.delete(0, tk.END)
                self.start_entry.insert(0, rows[0][1])
                self.end_entry.delete(0, tk.END)
                self.end_entry.insert(0, rows[1][1])
                self.step_entry.delete(0, tk.END)
                self.step_entry.insert(0, rows[2][1])
                self.amp_entry.delete(0, tk.END)
                self.amp_entry.insert(0, rows[3][1])
                self.width_entry.delete(0, tk.END)
                self.width_entry.insert(0, rows[4][1])
                
                # Import filter info if available
                if len(rows) > 5 and "Filter Type" in rows[5][0]:
                    filter_type = rows[5][1]
                    if filter_type in self.filter_combo['values']:
                        self.filter_combo.set(filter_type)
                        self.filter_type = filter_type
                        self.on_filter_changed()
                
                # Import deposition parameters if available
                data_start_row = 9  # Default start row for data
                if len(rows) > 6 and rows[6][0] == "Use Deposition":
                    use_dep = rows[6][1].lower() == "true"
                    self.use_deposition.set(use_dep)
                    if use_dep and len(rows) > 8:
                        self.dep_voltage.set(int(rows[7][1]))
                        self.dep_time.set(int(rows[8][1]))
                        data_start_row = 10  # Data starts later due to deposition params
                    self.toggle_deposition_controls()  # Update UI
                
                self.clear_all()
                
                # Import measurement data
                for row in rows[data_start_row:]:
                    if len(row) >= 2:
                        self.bufferV.append(float(row[0]))
                        self.bufferI.append(float(row[1]))
                        if len(row) > 2 and row[2]:
                            self.bufferIfilter.append(float(row[2]))
                            
                self.draw_graph()
                self.messagebox.showinfo("Thành công", f"Đã nhập dữ liệu từ file:\n{file_path}")
        except Exception as e:
            self.messagebox.showerror("Lỗi", f"Lỗi khi đọc file:\n{str(e)}")

    def start_measurement(self):
        if not self.ser or not self.ser.is_open:
            self.messagebox.showerror("COM", "Chưa kết nối cổng COM!")
            return
        
        if self.is_measuring:
            self.messagebox.showwarning("Cảnh báo", "Đang đo, vui lòng chờ!")
            return

        try:
            s_vol = int(self.start_entry.get())
            e_vol = int(self.end_entry.get())
            step = int(self.step_entry.get())
            amp = int(self.amp_entry.get())
            width = int(self.width_entry.get())

            num_samples = ((e_vol - s_vol) // step) + 1

            if self.use_deposition.get():
                dep_v = self.dep_voltage.get()
                dep_t = self.dep_time.get()
                cmd = f"6#{s_vol}?{e_vol}/{step}|{amp}${width}|{dep_v}${dep_t}!"
                print(f"Gửi lệnh DPV+Deposition: {cmd}")
            else:
                cmd = f"6#{s_vol}?{e_vol}/{step}|{amp}${width}!"
                print(f"Gửi lệnh DPV standard: {cmd}")

            self.safe_clear_buffer()
            self.ser.write(cmd.encode('utf-8'))
            self.time.sleep(0.1)
            
            self.is_measuring = True
            self.threading.Thread(target=self.read_measurement_data, 
                                args=(num_samples,), daemon=True).start()

        except Exception as e:
            self.messagebox.showerror("Lỗi đo", str(e))
            self.is_measuring = False

    def read_measurement_data(self, expected_samples):
        """Đọc dữ liệu đo trong thread riêng"""
        serial_lines = []
        timeout_count = 0
        max_timeout = 100
        
        try:
            while self.is_measuring and len(serial_lines) < expected_samples:
                try:
                    self.ser.timeout = 0.1
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    if line == "END":
                        print("Nhận được tín hiệu END")
                        break
                        
                    if ";" in line:
                        try:
                            parts = line.split(";")
                            if len(parts) == 2:
                                float(parts[1])  # Test parse current
                                serial_lines.append(line)
                                timeout_count = 0
                                print(f"Nhận data: {len(serial_lines)}/{expected_samples}")
                        except ValueError:
                            print(f"Invalid data format: {line}")
                            continue
                    else:
                        timeout_count += 1
                        if timeout_count > max_timeout:
                            print("Timeout - dừng đọc data")
                            break
                            
                except self.serial.SerialTimeoutException:
                    timeout_count += 1
                    if timeout_count > max_timeout:
                        break
                except UnicodeDecodeError as e:
                    print(f"Encoding error: {e}")
                    continue
                except Exception as e:
                    print(f"Lỗi đọc serial: {e}")
                    break
            
            if serial_lines:
                self.root.after(0, lambda: self.process_and_draw(serial_lines))
            else:
                self.root.after(0, lambda: self.messagebox.showwarning("Cảnh báo", "Không nhận được dữ liệu hợp lệ!"))
                
        except Exception as e:
            print(f"Lỗi trong read_measurement_data: {e}")
        finally:
            self.is_measuring = False

    def process_and_draw(self, serial_lines):
        """Xử lý data và vẽ đồ thị trong main thread"""
        try:
            self.dpv_data_process(serial_lines)
            self.draw_graph()
            self.messagebox.showinfo("Thành công", f"Đo xong! Nhận được {len(serial_lines)} điểm dữ liệu")
        except Exception as e:
            self.messagebox.showerror("Lỗi xử lý", f"Lỗi xử lý dữ liệu: {e}")

    def smoothing_data_dpv(self, data):
        """Original DPV smoothing (kept for compatibility)"""
        if len(data) < 3:
            return data
        result = [data[0], (data[0] + data[1]) / 2]
        for i in range(2, len(data)):
            result.append((data[i] + data[i - 1] + data[i - 2]) / 3)
        return result

    # ✅ THÊM: Hàm smooth_data nâng cao cho DPV
    def smoothing_data_dpv_advanced(self, data):
        """✅ Bộ lọc đa dạng cho DPV - Chọn theo filter_type"""
        b = data
        if len(b) == 0: 
            return []
        
        try:
            filter_type = self.filter_type
            
            if filter_type == "1/3 (Moving Avg 3pt)":
                # 3-point moving average (original)
                result = [0] * len(b)
                result[0] = b[0]
                if len(b) > 1:
                    result[1] = round((b[0] + b[1]) / 2, 3)
                for i in range(2, len(b)):
                    result[i] = round((b[i] + b[i-1] + b[i-2]) / 3, 3)
                return result
                    
            elif filter_type == "1/7 (Moving Avg 7pt)":
                # 7-point moving average
                if len(b) < 7:
                    return self.apply_simple_moving_average_dpv(b, 3)
                else:
                    kernel = self.np.ones(7) / 7
                    padded = self.np.pad(b, (3, 3), mode='edge')
                    filtered = self.np.convolve(padded, kernel, mode='valid')
                    return [round(x, 3) for x in filtered]
                    
            elif filter_type == "1/9 (Moving Avg 9pt)":
                # 9-point moving average
                if len(b) < 9:
                    return self.apply_simple_moving_average_dpv(b, 3)
                else:
                    kernel = self.np.ones(9) / 9
                    padded = self.np.pad(b, (4, 4), mode='edge')
                    filtered = self.np.convolve(padded, kernel, mode='valid')
                    return [round(x, 3) for x in filtered]
                    
            elif filter_type == "1/11 (Moving Avg 11pt)":
                # 11-point moving average
                if len(b) < 11:
                    return self.apply_simple_moving_average_dpv(b, 3)
                else:
                    kernel = self.np.ones(11) / 11
                    padded = self.np.pad(b, (5, 5), mode='edge')
                    filtered = self.np.convolve(padded, kernel, mode='valid')
                    return [round(x, 3) for x in filtered]

            elif filter_type == "1/13 (Moving Avg 13pt)":
                # 13-point moving average
                if len(b) < 13:
                    return self.apply_simple_moving_average_dpv(b, 3)
                else:
                    kernel = self.np.ones(13) / 13
                    padded = self.np.pad(b, (6, 6), mode='edge')
                    filtered = self.np.convolve(padded, kernel, mode='valid')
                    return [round(x, 3) for x in filtered]

            elif filter_type == "1/15 (Moving Avg 15pt)":
                # 15-point moving average
                if len(b) < 15:
                    return self.apply_simple_moving_average_dpv(b, 3)
                else:
                    kernel = self.np.ones(15) / 15
                    padded = self.np.pad(b, (7, 7), mode='edge')
                    filtered = self.np.convolve(padded, kernel, mode='valid')
                    return [round(x, 3) for x in filtered]

            elif filter_type == "Savitzky-Golay":
                # Savitzky-Golay filter - tốt nhất cho giữ peak
                if len(b) < 5:
                    return self.apply_simple_moving_average_dpv(b, 3)
                else:
                    window = min(7, len(b) if len(b) % 2 == 1 else len(b) - 1)
                    filtered = self.savgol_filter(b, window_length=window, polyorder=2)
                    return [round(x, 3) for x in filtered]
                    
            elif filter_type == "Gaussian Filter":
                # Gaussian filter - smooth tự nhiên
                if len(b) < 5:
                    return self.apply_simple_moving_average_dpv(b, 3)
                else:
                    sigma = 1.0  # Có thể điều chỉnh
                    filtered = self.gaussian_filter1d(b, sigma=sigma)
                    return [round(x, 3) for x in filtered]
                    
            elif filter_type == "Median Filter":
                # Median filter - tốt cho loại bỏ spike
                if len(b) < 3:
                    return b.copy()
                else:
                    kernel_size = min(5, len(b) if len(b) % 2 == 1 else len(b) - 1)
                    filtered = self.medfilt(b, kernel_size=kernel_size)
                    return [round(x, 3) for x in filtered]
                    
            elif filter_type == "Combo Filter":
                # Combo: Median + Savitzky-Golay - tốt nhất
                if len(b) < 7:
                    return self.apply_simple_moving_average_dpv(b, 3)
                else:
                    # Bước 1: Median filter loại spike
                    b_despike = self.medfilt(b, kernel_size=3)
                    # Bước 2: Savitzky-Golay smooth
                    filtered = self.savgol_filter(b_despike, window_length=5, polyorder=2)
                    return [round(x, 3) for x in filtered]
                    
            elif filter_type == "Exponential Smoothing":
                # Exponential smoothing - ưu tiên data gần
                alpha = 0.3  # Smoothing factor
                result = [b[0]]
                for i in range(1, len(b)):
                    smoothed = alpha * b[i] + (1 - alpha) * result[i-1]
                    result.append(round(smoothed, 3))
                return result
                    
            else:
                # Default fallback
                return self.apply_simple_moving_average_dpv(b, 3)
                
        except Exception as e:
            print(f"DPV Filter error: {e}")
            # Fallback to simple moving average
            return self.apply_simple_moving_average_dpv(b, 3)

    # ✅ THÊM: Helper function cho DPV
    def apply_simple_moving_average_dpv(self, data, window=3):
        """Helper function for simple moving average cho DPV"""
        result = [0] * len(data)
        result[0] = data[0]
        if len(data) > 1:
            result[1] = round((data[0] + data[1]) / 2, 3)
        for i in range(2, len(data)):
            if window == 3:
                result[i] = round((data[i] + data[i-1] + data[i-2]) / 3, 3)
            elif window == 5 and i >= 4:
                result[i] = round((data[i] + data[i-1] + data[i-2] + data[i-3] + data[i-4]) / 5, 3)
            else:
                result[i] = round((data[i] + data[i-1] + data[i-2]) / 3, 3)
        return result

    def dpv_data_process(self, serial_lines):
        self.bufferV.clear()
        self.bufferI.clear()
        self.bufferIfilter.clear()
        try:
            s_vol = int(self.start_entry.get())
            step = int(self.step_entry.get())
        except:
            s_vol = -200
            step = 10
        for line in serial_lines:
            try:
                index, current = line.split(";")
                voltage = s_vol + int(index) * step
                self.bufferV.append(voltage)
                self.bufferI.append(float(current))
            except:
                continue
        # ✅ THAY ĐỔI: Sử dụng hàm filter nâng cao
        self.bufferIfilter = self.smoothing_data_dpv_advanced(self.bufferI)

    def draw_graph(self):
        self.ax.cla()
        self.fig.patch.set_facecolor("#E1F5FE")
        self.ax.set_facecolor('white')
        
        # ✅ THÊM: Hiển thị filter trong title
        filter_short = self.filter_type.split(" ")[0] if hasattr(self, 'filter_type') else "1/7"
        title = f"Differential Pulse Voltammetry - Filter: {filter_short}"
        
        self.ax.set_title(title, fontsize=18, fontweight="bold", color="#0288D1")
        self.ax.set_xlabel("Voltage (mV)", fontsize=18, fontweight="bold", color="#0288D1")
        self.ax.set_ylabel("Current (μA)", fontsize=18, fontweight="bold", color="#0288D1")
        self.ax.tick_params(axis='both', labelsize=18, width=2, direction='inout', length=8)
        self.ax.grid(True, linewidth=1.5)
        if len(self.bufferV) > 0:
            self.ax.plot(self.bufferV, self.bufferI, label="Raw", color="red")
            self.ax.plot(self.bufferV, self.bufferIfilter, label="Filtered", color="black")
            y_min = min(self.bufferI + self.bufferIfilter)
            y_max = max(self.bufferI + self.bufferIfilter)
            if y_min == y_max:
                delta = abs(y_min) * 0.1 if y_min != 0 else 1
                y_min -= delta
                y_max += delta
            else:
                y_range = y_max - y_min
                y_min -= y_range * 0.05
                y_max += y_range * 0.05
            self.ax.set_xlim(min(self.bufferV), max(self.bufferV))
            self.ax.set_ylim(y_min, y_max)
            self.ax.legend()
        # Làm to, đậm, đổi màu các số trên trục cho đồng bộ
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#760FCF")  # Màu tím đồng bộ
        # Làm đậm các đường trục
        for spine in ['top', 'bottom', 'left', 'right']:
            self.ax.spines[spine].set_linewidth(2)
            self.ax.spines[spine].set_color('black')
        self.canvas.draw()
# SWV
class SWVApp:
    def __init__(self, parent):
        import tkinter as tk
        from tkinter import ttk, messagebox, filedialog
        import matplotlib.pyplot as plt
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
        import serial
        import serial.tools.list_ports
        import threading
        import csv
        from PIL import Image, ImageTk

        self.serial = serial
        self.threading = threading
        self.messagebox = messagebox
        self.filedialog = filedialog
        self.ttk = ttk

        self.ser = None
        self.is_measuring = False
        self.bufferVSW = []
        self.bufferCnet = []
        self.bufferCf = []
        self.bufferCb = []
        self.bufffil = []


        self.parent = parent
        
        style = ttk.Style()
        style.configure("SWV.TButton", font=("Segoe UI", 14, "bold"), padding=8)
        style.configure("SWV.TLabel", font=("Segoe UI", 14, "bold"))
        style.configure("SWV.TSpinbox", font=("Segoe UI", 14, "bold"))
        
        if isinstance(self.parent, (tk.Tk, tk.Toplevel)):
            self.parent.protocol("WM_DELETE_WINDOW", self.on_closing)

        # ================= Top Bar with Logos ===================
        top_frame = tk.Frame(self.parent, bg="#94F1C6")
        top_frame.pack(side="top", fill="x", padx=5, pady=5)

        self.logo_frame = tk.Frame(top_frame, bg="#94F1C6")
        self.logo_frame.pack(side="right", padx=10)

        try:
            img1 = Image.open(resource_path("dai-hoc-khoa-hoc-tu-nhien-Photoroom.png")).resize((100, 80), Image.Resampling.LANCZOS)
            self.logo1 = ImageTk.PhotoImage(img1)
            logo1_label = tk.Label(self.logo_frame, image=self.logo1, bg="#94F1C6")
            logo1_label.grid(row=0, column=0, padx=5)

            title_label = tk.Label(
                self.logo_frame,
                text="Portable Electrochemical\nMeasuring System by HUS",
                font=("Segoe UI", 13, "bold"),
                bg="#D5F4E6",
                fg="#1976D2",
                justify="center"
            )
            title_label.grid(row=0, column=1, padx=10)

            img2 = Image.open("E:/oE/Download/images-Photoroom.png").resize((50, 50), Image.Resampling.LANCZOS)
            self.logo2 = ImageTk.PhotoImage(img2)
            logo2_label = tk.Label(self.logo_frame, image=self.logo2, bg="#94F1C6")
            logo2_label.grid(row=0, column=2, padx=5)
        except Exception as e:
            print("Lỗi ảnh logo:", e)

        # ================== Main Content =====================
        main_content = tk.Frame(self.parent, bg="#94F1C6")
        main_content.pack(fill="both", expand=True)

        self.frame_left = tk.Frame(main_content, bg="#94F1C6", width=260)
        self.frame_left.pack(side="left", padx=10, pady=10, fill="y")
        self.frame_left.pack_propagate(False)

        control_panel = tk.Frame(self.frame_left, bg="#94F1C6")
        control_panel.pack(anchor="n", fill="x", padx=0, pady=0)

        # Serial Port Control
        tk.Label(control_panel, text="Serial Port Control", font=("Segoe UI", 15, "bold"), fg="#1976D2", bg="#94F1C6").pack(anchor="w", pady=(5, 10))
        self.port_combo = ttk.Combobox(control_panel, font=("Segoe UI", 14, "bold"), width=14, postcommand=self.refresh_ports)
        self.port_combo.pack(fill="x", pady=5)
        ttk.Button(control_panel, text="Refresh COM", command=self.refresh_ports, style="SWV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="\U0001F50C Connect", command=self.connect_serial, style="SWV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="\u274C Disconnect", command=self.disconnect_serial, style="SWV.TButton").pack(fill="x", pady=3)

        # Program Control
        tk.Label(control_panel, text="Program Control", font=("Segoe UI", 15, "bold"), fg="#388E3C", bg="#94F1C6").pack(anchor="w", pady=(12, 5))
        ttk.Button(control_panel, text="\u25B6\ufe0f Measure", command=lambda: threading.Thread(target=self.start_measurement).start(), style="SWV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="🧹 Clear Data", command=self.clear_all, style="SWV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="\U0001F4C2 Import CSV", command=self.import_from_csv, style="SWV.TButton").pack(fill="x", pady=3)
        ttk.Button(control_panel, text="\U0001F4BE Export CSV", command=self.export_to_csv, style="SWV.TButton").pack(fill="x", pady=3)

        # Parameter config
        tk.Label(control_panel, text="Parameter Config", font=("Segoe UI", 15, "bold"), fg="#D84315", bg="#94F1C6").pack(anchor="w", pady=(10, 4))
        self.start_entry = self.add_labeled_entry("Start Voltage (mV)", -200, font=("Segoe UI", 12, "bold"), parent=control_panel)
        self.end_entry = self.add_labeled_entry("End Voltage (mV)", 600, font=("Segoe UI", 12, "bold"), parent=control_panel)
        self.step_entry = self.add_labeled_entry("E_Step (mV)", 10, font=("Segoe UI", 12, "bold"), parent=control_panel)
        self.amp_entry = self.add_labeled_entry("Amplitude (mV)", 25, font=("Segoe UI", 12, "bold"), parent=control_panel)
        self.freq_entry = self.add_labeled_entry("Frequency (Hz)", 10, font=("Segoe UI", 12, "bold"), parent=control_panel)

        # Sweep selection
        tk.Label(control_panel, text="Sweep Display", font=("Segoe UI", 15, "bold"), fg="#00838F", bg="#94F1C6").pack(anchor="w", pady=(10, 4))
        self.sweep_var = tk.StringVar(value="All")
        ttk.Radiobutton(control_panel, text="Show all sweeps", variable=self.sweep_var, value="All", style="Custom.TRadiobutton").pack(anchor="w", pady=1)
        ttk.Radiobutton(control_panel, text="1th", variable=self.sweep_var, value="1", style="Custom.TRadiobutton").pack(anchor="w", pady=1)
        ttk.Radiobutton(control_panel, text="2th", variable=self.sweep_var, value="2", style="Custom.TRadiobutton").pack(anchor="w", pady=1)

        # ========== Plot Area ==========
        self.frame_right = tk.Frame(main_content, bg="#94F1C6")  # Đặt màu nền frame chứa đồ thị giống CV
        self.frame_right.pack(side="right", expand=True, fill="both", padx=10, pady=10)

        self.fig, self.ax = plt.subplots(figsize=(6.5, 5))
        self.fig.patch.set_facecolor("#CFF6E4")  # Đặt màu nền toàn bộ figure giống CV
        self.ax.set_facecolor('white')  # Nền trục vẫn là trắng cho nổi bật đường vẽ

        self.ax.set_title("Square Wave Voltammetry", fontsize=20, fontweight="bold", color="#F14B09")
        self.ax.set_xlabel("Voltage (mV)", fontsize=15, color="#F14B09")
        self.ax.set_ylabel("Current (μA)", fontsize=15, color="#F14B09")
        self.ax.grid(True)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.frame_right)
        self.canvas_widget = self.canvas.get_tk_widget()
        self.canvas_widget.pack(expand=True, fill="both")
        plt.tight_layout()
        plt.close(self.fig)
        
        # ...sau khi tạo self.fig, self.ax, set_title, set_xlabel, set_ylabel...
        self.ax.plot([], [])  # Vẽ trống để sinh ra tick label
        self.canvas.draw()    # Vẽ canvas để cập nhật tick label

        # Làm to, đậm, đổi màu tick label ngay từ đầu
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#000000")
        self.canvas.draw()

        
    def on_closing(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.parent.destroy()

    def add_labeled_entry(self, label, default, font=("Segoe UI", 14, "bold"), parent=None):
        if parent is None:
            parent = self.frame_left
        frame = tk.Frame(parent, bg="#FFFFFF")
        frame.pack(fill="x", pady=4)
        tk.Label(
            frame,
            text=label,
            font=font,
            bg="#43A047",
            fg="white",
            anchor="w",
            width=18,
            height=2
        ).pack(side="left", padx=(0, 5))
        # Sử dụng tk.Entry để chắc chắn đổi được màu nền và màu chữ
        entry = tk.Entry(
            frame,
            width=12,
            font=font,
            justify="center",
            fg="black",
            bg="white"
        )
        entry.insert(0, str(default))
        entry.pack(side="right", fill="x", ipady=6)
        return entry


    def refresh_ports(self):
        ports = self.serial.tools.list_ports.comports()
        self.port_combo['values'] = [port.device for port in ports]
        self.port_combo.set(ports[0].device if ports else '')

    def connect_serial(self):
        try:
            port = self.port_combo.get()
            self.ser = self.serial.Serial(port, 115200, timeout=1)
            self.messagebox.showinfo("COM", f"Kết nối {port} thành công!")
        except Exception as e:
            self.messagebox.showerror("Lỗi COM", str(e))

    def disconnect_serial(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.messagebox.showinfo("COM", "Đã ngắt kết nối!")

    def clear_all(self):
        self.bufferVSW.clear()
        self.bufferCnet.clear()
        self.bufferCf.clear()
        self.bufferCb.clear()
        self.bufffil.clear()
        self.ax.cla()
        # Tăng kích thước font cho tiêu đề và nhãn trục
        self.ax.set_title("Square Wave Voltammetry", fontsize=18, fontweight="bold")  # Tăng kích thước font tiêu đề
        self.ax.set_xlabel("Voltage (mV)", fontsize=18)  # Tăng kích thước font nhãn trục X
        self.ax.set_ylabel("Current (μA)", fontsize=18)  # Tăng kích thước font nhãn trục Y

        self.ax.grid(True)
        self.canvas.draw()

    def export_to_csv(self):
        if not self.bufferVSW:
            self.messagebox.showwarning("Cảnh báo", "Không có dữ liệu để xuất!")
            return

        file_path = self.filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV Files", "*.csv")])
        if not file_path:
            return

        try:
            with open(file_path, mode='w', newline='', encoding='utf-8') as file:
                writer = csv.writer(file)
                writer.writerow(["Start Voltage", self.start_entry.get(), "[mV]"])
                writer.writerow(["End Voltage", self.end_entry.get(), "[mV]"])
                writer.writerow(["Step", self.step_entry.get(), "[mV]"])
                writer.writerow(["Amplitude", self.amp_entry.get(), "[mV]"])
                writer.writerow(["Frequency", self.freq_entry.get(), "[Hz]"])
                writer.writerow(["Voltage (mV)", "Net (μA)", "If (μA)", "Ib (μA)", "Filtered (μA)"])
                for i in range(len(self.bufferVSW)):
                    writer.writerow([
                        self.bufferVSW[i],
                        self.bufferCnet[i],
                        self.bufferCf[i],
                        self.bufferCb[i],
                        self.bufffil[i]
                    ])
            import os
            image_path = os.path.splitext(file_path)[0] + "_plot.png"
            self.fig.savefig(image_path, bbox_inches='tight', dpi=300)
            self.messagebox.showinfo("Thành công", f"Xuất dữ liệu và đồ thị thành công!\nCSV: {file_path}\nẢnh: {image_path}")
        except Exception as e:
            self.messagebox.showerror("Lỗi", f"Lỗi khi ghi file:\n{str(e)}")


    def import_from_csv(self):
        file_path = self.filedialog.askopenfilename(filetypes=[("CSV Files", "*.csv")])
        if not file_path:
            return

        try:
            with open(file_path, mode='r') as file:
                reader = csv.reader(file)
                rows = list(reader)

                self.spin_start.set(rows[0][1])
                self.spin_end.set(rows[1][1])
                self.spin_step.set(rows[2][1])
                self.spin_amp.set(rows[3][1])
                self.spin_freq.set(rows[4][1])

                self.clear_all()

                for row in rows[5:]:
                    if len(row) >= 5:
                        self.bufferVSW.append(float(row[0]))
                        self.bufferCnet.append(float(row[1]))
                        self.bufferCf.append(float(row[2]))
                        self.bufferCb.append(float(row[3]))
                        self.bufffil.append(float(row[4]))
            self.draw_graph()
            self.messagebox.showinfo("Thành công", f"Đã nhập dữ liệu từ file:\n{file_path}")
        except Exception as e:
            self.messagebox.showerror("Lỗi", f"Lỗi khi đọc file:\n{str(e)}")

    def start_measurement(self):
        if not self.ser or not self.ser.is_open:
            self.messagebox.showerror("COM", "Chưa kết nối cổng COM!")
            return

        try:
            s_vol = int(self.start_entry.get())
            e_vol = int(self.end_entry.get())
            step = int(self.step_entry.get())
            amp = int(self.amp_entry.get()) * 2
            freq = int(self.freq_entry.get())

            temp_s_vol = s_vol
            s_vol = -s_vol
            e_vol = (e_vol - temp_s_vol) + s_vol

            num_samples = ((e_vol - s_vol) // step) * 2
            cmd = f"4#{s_vol}?{e_vol}/{step}|{amp}${freq}!"
            print("Gửi lệnh:", cmd)
            self.ser.write(cmd.encode())

            self.is_measuring = True
            serial_lines = []

            while len(serial_lines) < num_samples:
                line = self.ser.readline().decode().strip()
                if ";" in line:
                    serial_lines.append(line)

            self.sw_data_process(serial_lines, s_vol, step)
            self.draw_graph()

        except Exception as e:
            self.messagebox.showerror("Lỗi đo", str(e))
        finally:
            self.is_measuring = False

    def smoothing_data_swv_filter(self, data):
        if len(data) < 3:
            return
        data[0] = data[0]
        data[1] = round((data[0] + data[1]) / 2, 3)
        for i in range(2, len(data)):
            data[i] = round((data[i] + data[i - 1] + data[i - 2]) / 3, 3)

    def sw_data_process(self, serial_lines, s_vol, step):
        self.bufferVSW.clear()
        self.bufferCf.clear()
        self.bufferCb.clear()
        self.bufferCnet.clear()
        self.bufffil.clear()

        for i in range(0, len(serial_lines) - 1, 2):
            try:
                index1, current1 = serial_lines[i].split(";")
                index2, current2 = serial_lines[i + 1].split(";")
                current1 = float(current1)
                current2 = float(current2)
                voltage = -s_vol + (i // 2) * step
                self.bufferVSW.append(voltage)
                self.bufferCf.append(current1)
                self.bufferCb.append(current2)
                self.bufferCnet.append(current1 - current2)
                self.bufffil.append(current1 - current2)
            except:
                continue
        self.smoothing_data_swv_filter(self.bufffil)

    def draw_graph(self):
        self.ax.cla()  # Xóa đồ thị hiện tại

        # Đặt màu nền của toàn bộ khung đồ thị (fig) thành màu xanh lá cây nhạt
        self.fig.patch.set_facecolor('#90EE90')

        # Đặt màu nền của trục đồ thị (axis) thành trắng để không ảnh hưởng đến vẽ đồ thị
        self.ax.set_facecolor('white')

        # Tiêu đề và nhãn trục với màu chữ xanh lá cây nhạt
        self.ax.set_title("Square Wave Voltammetry", fontsize=18, fontweight="bold", color="#F14B09")
        self.ax.set_xlabel("Voltage (mV)", fontsize=18, fontweight="bold", color="#F14B09")
        self.ax.set_ylabel("Current (μA)", fontsize=18, fontweight="bold", color="#F14B09")

        # Tăng cỡ chữ và làm đậm các số trên trục (các tick labels)
        self.ax.tick_params(axis='both', labelsize=18, width=2, direction='inout', length=8)

        # Làm đậm, to và đổi màu các số trên trục
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#760FCF")
            
        self.canvas.draw()  # Vẽ lại đồ thị
        # Làm cho các đường trục (trục điện thế và dòng điện) đậm lên
        self.ax.spines['top'].set_linewidth(2)  # Đường trục trên dày lên
        self.ax.spines['bottom'].set_linewidth(2)  # Đường trục dưới dày lên
        self.ax.spines['left'].set_linewidth(2)  # Đường trục trái dày lên
        self.ax.spines['right'].set_linewidth(2)  # Đường trục phải dày lên

        # Đặt màu sắc các trục thành màu đen để rõ ràng hơn
        self.ax.spines['top'].set_color('black')
        self.ax.spines['bottom'].set_color('black')
        self.ax.spines['left'].set_color('black')
        self.ax.spines['right'].set_color('black')

        # Lưới đồ thị (grid)
        self.ax.grid(True, linewidth=1.5)  # Tăng độ dày lưới

        # Kiểm tra nếu có dữ liệu
        # ...existing code...
        if len(self.bufferVSW) > 0:
            # Vẽ các đường
            self.ax.plot(self.bufferVSW, self.bufferCnet, label="Net (If - Ib)", color="red")
            self.ax.plot(self.bufferVSW, self.bufferCf, label="If", color="blue")
            self.ax.plot(self.bufferVSW, self.bufferCb, label="Ib", color="green")
            self.ax.plot(self.bufferVSW, self.bufffil, label="Filtered", color="black")

            # Tính min/max toàn bộ dữ liệu Y
            all_y = self.bufferCnet + self.bufferCf + self.bufferCb + self.bufffil
            y_min = min(all_y)
            y_max = max(all_y)
            if y_min == y_max:
                # Nếu chỉ có 1 giá trị, mở rộng ±10%
                delta = abs(y_min) * 0.1 if y_min != 0 else 1
                y_min -= delta
                y_max += delta
            else:
                # Mở rộng biên ±5% mỗi đầu
                y_range = y_max - y_min
                y_min -= y_range * 0.05
                y_max += y_range * 0.05

            self.ax.set_xlim(min(self.bufferVSW), max(self.bufferVSW))
            self.ax.set_ylim(y_min, y_max)
        # ...existing code...
        self.canvas.draw()  # Vẽ lại đồ thị


# CV
# CV - THÊM BỘ LỌC CHỌN LỰA
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from PIL import Image, ImageTk
import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
import threading
import pandas as pd
import tkinter.font as tkFont
import numpy as np
from scipy.signal import savgol_filter, medfilt
from scipy.ndimage import gaussian_filter1d

class CVApp:
    def __init__(self, parent):
        self.root = parent
        self.serial_port = None

        # Thông số mặc định
        self.repeat_times = 1
        self.start_voltage = -200  # mV
        self.end_voltage = 600    # mV
        self.step_voltage = 10    # mV
        self.scan_rate = 100.0    # mV/s
        
        # ✅ THÊM: Bộ lọc mặc định
        self.filter_type = "1/7"  # Mặc định là 7-point moving average
        
        # Dữ liệu
        self.buffer_serial = []
        self.buffer_voltage = []
        self.buffer_current_raw = []
        self.buffer_current_filtered = []
        self.x_data = []
        self.y_data = []
        self.receiver_count = 0
        self.expected_samples = 0
        self.is_receiving = False
        self.is_measuring = False  # ✅ THÊM: Cờ để tránh đo trùng

        self.setup_gui()
        self.setup_plot()

    def setup_gui(self):
        # ===== Top bar với logo và tiêu đề =====
        top_frame = tk.Frame(self.root, bg="#FFCCCB")
        top_frame.pack(side="top", fill="x", padx=5, pady=5)

        logo_frame = tk.Frame(top_frame, bg="#FFCCCB")
        logo_frame.pack(side="right", padx=10)

        try:
            img1 = Image.open(resource_path("dai-hoc-khoa-hoc-tu-nhien-Photoroom.png")).resize((120, 100), Image.Resampling.LANCZOS)
            self.logo1 = ImageTk.PhotoImage(img1)
            tk.Label(logo_frame, image=self.logo1, bg="#FFCCCB").grid(row=0, column=0, padx=5)

            title_label = tk.Label(
                logo_frame,
                text="Cyclic Voltammetry\nby HUS",
                font=("Segoe UI", 14, "bold"),
                bg="#FFCCCB",
                fg="#760FCF",  # Màu tím đặc sắc cho tiêu đề chính
                justify="center"
            )
            title_label.grid(row=0, column=1, padx=10)

            img2 = Image.open(resource_path("Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png")).resize((60, 60), Image.Resampling.LANCZOS)
            self.logo2 = ImageTk.PhotoImage(img2)
            tk.Label(logo_frame, image=self.logo2, bg="#FFCCCB").grid(row=0, column=2, padx=5)
        except Exception as e:
            print("Lỗi logo:", e)

        # ===== Main content chia 2 panel =====
        main_content = tk.Frame(self.root, bg="#FFCCCB")
        main_content.pack(fill="both", expand=True)

        self.frame_left = tk.Frame(main_content, bg="#FFCCCB")
        self.frame_left.pack(side="left", padx=15, pady=10, fill="y")

        self.frame_right = tk.Frame(main_content, bg="#FFCCCB")
        self.frame_right.pack(side="right", expand=True, fill="both", padx=10, pady=10)

        # ===== Left panel: Serial, Control, Parameter, Import/Export =====
        style = ttk.Style()
        style.configure("CV.TButton", font=("Segoe UI", 14, "bold"), padding=8)
        style.configure("CV.TLabel", font=("Segoe UI", 14, "bold"))

        # Serial Port Control
        ttk.Label(self.frame_left, text="Serial Port Control", font=("Segoe UI", 15, "bold"), foreground="#1976D2").pack(anchor="w", pady=(5, 10))
        self.port_combo = ttk.Combobox(self.frame_left, font=("Segoe UI", 14, "bold"), postcommand=self.refresh_ports)
        self.port_combo.pack(fill="x", pady=5)
        ttk.Button(self.frame_left, text="Refresh COM", command=self.refresh_ports, style="CV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\U0001F50C Connect", command=self.connect_serial, style="CV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\u274C Disconnect", command=self.disconnect_serial, style="CV.TButton").pack(fill="x", pady=3)

        # Program Control
        ttk.Label(self.frame_left, text="Program Control", font=("Segoe UI", 15, "bold"), foreground="#388E3C").pack(anchor="w", pady=(12, 5))
        ttk.Button(self.frame_left, text="\u25B6\ufe0f Measure", command=self.send_measure_command, style="CV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="🧹 Clear Data", command=self.clear_data, style="CV.TButton").pack(fill="x", pady=3)

        # Parameter config
        ttk.Label(self.frame_left, text="Parameter Config", font=("Segoe UI", 15, "bold"), foreground="#D84315").pack(anchor="w", pady=(12, 5))
        param_frame = tk.Frame(self.frame_left, bg="#FFCCCB")
        param_frame.pack(fill="x", pady=2)
        self.start_entry = self.add_labeled_entry(param_frame, "Start Voltage (mV):", self.start_voltage, row=0)
        self.end_entry = self.add_labeled_entry(param_frame, "End Voltage (mV):", self.end_voltage, row=1)
        self.step_entry = self.add_labeled_entry(param_frame, "Step (mV):", self.step_voltage, row=2)
        self.scan_rate_entry = self.add_labeled_entry(param_frame, "Scan Rate (mV/s):", self.scan_rate, row=3)
        self.repeat_entry = self.add_labeled_entry(param_frame, "Repeat:", 1, row=4)

        # ✅ THÊM: Filter Selection
        ttk.Label(self.frame_left, text="Filter Selection", font=("Segoe UI", 15, "bold"), foreground="#9C27B0").pack(anchor="w", pady=(12, 5))
        filter_frame = tk.Frame(self.frame_left, bg="#FFCCCB")
        filter_frame.pack(fill="x", pady=2)
        
        tk.Label(filter_frame, text="Filter Type:", font=("Segoe UI", 14, "bold"), bg="#FFCCCB", anchor="w", width=18).grid(row=0, column=0, sticky="w", padx=(0, 5), pady=2)
        
        self.filter_combo = ttk.Combobox(
            filter_frame, 
            values=[
                "1/3 (Moving Avg 3pt)",
                "1/7 (Moving Avg 7pt)", 
                "1/9 (Moving Avg 9pt)",
                "1/11 (Moving Avg 11pt)",
                "1/13 (Moving Avg 13pt)",
                "1/15 (Moving Avg 15pt)",  # ✅ THÊM MỚI 1/15
                "Savitzky-Golay",
                "Gaussian Filter",
                "Median Filter",
                "Combo Filter",
                "Exponential Smoothing"
            ],
            font=("Segoe UI", 12, "bold"),
            state="readonly",
            width=20
        )
        self.filter_combo.set("1/7 (Moving Avg 7pt)")  # Mặc định
        self.filter_combo.grid(row=0, column=1, sticky="ew", pady=2)
        self.filter_combo.bind("<<ComboboxSelected>>", self.on_filter_changed)
        filter_frame.grid_columnconfigure(1, weight=1)

        # ✅ THÊM: Filter Info Label
        self.filter_info_label = tk.Label(
            self.frame_left, 
            text="Current: 7-point moving average\nBest for: General smoothing", 
            font=("Segoe UI", 10), 
            bg="#FFCCCB", 
            fg="#666666",
            justify="left",
            anchor="w"
        )
        self.filter_info_label.pack(anchor="w", pady=(2, 8))

        # Import/Export
        ttk.Label(self.frame_left, text="Data Control", font=("Segoe UI", 15, "bold"), foreground="#00838F").pack(anchor="w", pady=(12, 5))
        ttk.Button(self.frame_left, text="\U0001F4C2 Import CSV", command=self.import_file, style="CV.TButton").pack(fill="x", pady=3)
        ttk.Button(self.frame_left, text="\U0001F4BE Export CSV", command=self.export_file, style="CV.TButton").pack(fill="x", pady=3)

        # Status label
        self.status_label = ttk.Label(self.frame_left, text="Ready", foreground="blue", font=("Segoe UI", 13, "bold"))
        self.status_label.pack(pady=8)

    def add_labeled_entry(self, parent, label, default, row):
        label_widget = tk.Label(parent, text=label, font=("Segoe UI", 14, "bold"), bg="#FFCCCB", anchor="w", width=18)
        label_widget.grid(row=row, column=0, sticky="w", padx=(0, 5), pady=2)
        entry = ttk.Entry(parent, width=10, font=("Segoe UI", 14, "bold"))
        entry.insert(0, str(default))
        entry.grid(row=row, column=1, sticky="ew", pady=2)
        parent.grid_columnconfigure(1, weight=1)
        return entry

    # ✅ THÊM: Xử lý khi thay đổi bộ lọc
    def on_filter_changed(self, event=None):
        selected = self.filter_combo.get()
        
        # Cập nhật thông tin bộ lọc
        filter_info = {
            "1/3 (Moving Avg 3pt)": "Current: 3-point moving average\nBest for: Fast processing, minimal smoothing",
            "1/7 (Moving Avg 7pt)": "Current: 7-point moving average\nBest for: General smoothing, good balance",
            "1/9 (Moving Avg 9pt)": "Current: 9-point moving average\nBest for: Heavy smoothing, noisy data",
            "1/11 (Moving Avg 11pt)": "Current: 11-point moving average\nBest for: Maximum smoothing, very noisy data",
            "1/13 (Moving Avg 13pt)": "Current: 13-point moving average\nBest for: Ultra smoothing, extremely noisy data",
            "1/15 (Moving Avg 15pt)": "Current: 15-point moving average\nBest for: Maximum smoothing, extreme noise reduction",  # ✅ THÊM MỚI 1/15
            "Savitzky-Golay": "Current: Savitzky-Golay filter\nBest for: Preserving peaks, scientific data",
            "Gaussian Filter": "Current: Gaussian smoothing\nBest for: Natural smoothing, edge preservation",
            "Median Filter": "Current: Median filter\nBest for: Removing spikes, outlier removal",
            "Combo Filter": "Current: Median + Savitzky-Golay\nBest for: Ultimate quality, spike + noise removal",
            "Exponential Smoothing": "Current: Exponential weighted average\nBest for: Real-time data, recent emphasis"
        }
        
        self.filter_info_label.config(text=filter_info.get(selected, "Unknown filter"))
        
        # Lưu lại filter type để sử dụng trong smooth_data
        self.filter_type = selected
        
        # Nếu đã có dữ liệu, áp dụng filter mới ngay lập tức
        if self.buffer_current_raw:
            self.smooth_data()
            self.y_data = self.buffer_current_filtered
            self.update_plot()

    def setup_plot(self):
        self.fig, self.ax = plt.subplots(figsize=(6, 5))
        self.fig.patch.set_facecolor("#FFCCCB")
        self.ax.set_facecolor("#FDFCFC")
        self.ax.set_title("CV Data", fontsize=18, fontweight='bold', color="#760FCF")
        self.ax.set_xlabel("Voltage (mV)", fontsize=16, color="#760FCF")
        self.ax.set_ylabel("Current (µA)", fontsize=16, color="#760FCF")
        self.ax.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.line, = self.ax.plot([], [], 'r-', linewidth=2)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.frame_right)
        self.canvas.draw()
        self.toolbar = NavigationToolbar2Tk(self.canvas, self.frame_right)
        self.toolbar.update()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Làm to và đậm các số trên trục
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#760FCF")

    # --- Các hàm chức năng giữ nguyên ---
    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])

    def connect_serial(self):
        port = self.port_combo.get().strip()
        if not port:
            messagebox.showwarning("Warning", "Please select a port")
            return
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            self.thread = threading.Thread(target=self.read_serial, daemon=True)
            self.thread.start()
            messagebox.showinfo("Connected", f"Connected to {port}")
        except serial.SerialException as e:
            messagebox.showerror("Serial Error", f"Could not open port {port}\n\n{str(e)}")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def disconnect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            messagebox.showinfo("Disconnected", "Serial port disconnected")

    def send_measure_command(self):
        # ✅ KIỂM TRA: Có đang đo không?
        if self.is_measuring:
            messagebox.showwarning("Warning", "Đang đo, vui lòng chờ hoàn tất!")
            return
            
        if self.serial_port and self.serial_port.is_open:
            try:
                self.start_voltage = int(self.start_entry.get())
                self.end_voltage = int(self.end_entry.get())
                self.step_voltage = int(self.step_entry.get())
                self.scan_rate = float(self.scan_rate_entry.get())
                self.repeat_times = int(self.repeat_entry.get())
                
                # Tính số steps đúng
                voltage_range = abs(self.end_voltage - self.start_voltage)
                steps_one_direction = int(voltage_range / self.step_voltage) + 1
                total_cv_steps = steps_one_direction * 2
                
                cv_command = f"1#{-self.end_voltage}?{-self.start_voltage}/{total_cv_steps}@{self.scan_rate}|{self.repeat_times}$0!"
                
                print(f"CV Command Debug:")
                print(f"  Command: '{cv_command}'")
                print(f"  Length: {len(cv_command)}")
                print(f"  Ends with '!': {cv_command.endswith('!')}")
                
                self.expected_samples = total_cv_steps * self.repeat_times
                
                # ✅ CLEAR BUFFER: Xóa dữ liệu cũ trước khi đo
                self.safe_clear_buffer()
                
                # ✅ GỬI LỆNH
                self.serial_port.write(cv_command.encode('utf-8'))
                print(f"✅ Sent: {cv_command}")
                
                # ✅ SLEEP: Chờ một chút để thiết bị nhận lệnh
                import time
                time.sleep(0.1)
                
                self.buffer_serial.clear()
                self.receiver_count = 0
                self.is_receiving = True
                self.is_measuring = True  # ✅ Đánh dấu đang đo
                self.status_label.config(text=f"Measuring: {self.scan_rate} mV/s...")
                self.thread = threading.Thread(target=self.read_serial, daemon=True)
                self.thread.start()
                
            except Exception as e:
                messagebox.showerror("Send Error", f"Could not send command.\n\n{str(e)}")
                self.is_measuring = False  # ✅ Reset cờ khi có lỗi
        else:
            messagebox.showwarning("Warning", "Serial port not connected")

    def safe_clear_buffer(self):
        """✅ Clear serial buffer an toàn trước khi đo"""
        if self.serial_port and self.serial_port.is_open:
            try:
                self.serial_port.reset_input_buffer()
                import time
                start_time = time.time()
                # Đọc và loại bỏ dữ liệu cũ trong 0.5 giây
                while time.time() - start_time < 0.5:
                    try:
                        self.serial_port.timeout = 0.1
                        remaining = self.serial_port.readline()
                        if not remaining:
                            break
                    except:
                        pass
                print("✅ Buffer cleared")
            except Exception as e:
                print(f"⚠️ Clear buffer warning: {e}")

    def read_serial(self):
        """✅ Đọc dữ liệu serial với timeout và error handling tốt hơn"""
        timeout_count = 0
        max_timeout = 100  # Số lần timeout tối đa trước khi dừng
        
        while self.is_receiving and self.receiver_count < self.expected_samples:
            try:
                self.serial_port.timeout = 0.2  # ✅ Timeout rõ ràng
                line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                
                # ✅ Kiểm tra tín hiệu END
                if line == "END":
                    print("✅ Nhận được tín hiệu END")
                    break
                
                # ✅ Xử lý dữ liệu hợp lệ
                if line and len(line) > 0:
                    if line[0].isdigit() or ";" in line:
                        self.buffer_serial.append(line)
                        self.receiver_count += 1
                        timeout_count = 0  # Reset timeout khi nhận được data
                        print(f"📊 CV Data: {self.receiver_count}/{self.expected_samples}")
                else:
                    timeout_count += 1
                    
            except Exception as e:
                print(f"⚠️ Read error: {e}")
                timeout_count += 1
                
            # ✅ Thoát nếu timeout quá nhiều
            if timeout_count >= max_timeout:
                print(f"❌ Timeout: Không nhận được dữ liệu sau {max_timeout} lần thử")
                break
                
        self.is_receiving = False
        self.is_measuring = False  # ✅ Reset cờ đo
        self.process_cv_data()

    def process_cv_data(self):
        S_Vol = self.start_voltage
        E_Vol = self.end_voltage
        Step = self.step_voltage
        RepeatTimes = self.repeat_times
        self.num_step = ((E_Vol - S_Vol) // Step + 1) * 2
        numStep = self.num_step
        numSample = numStep * RepeatTimes
        buff = self.buffer_serial.copy()
        if len(buff) >= 5:
            for i in range(4):
                buff[i] = buff[4]
        if len(buff) < self.num_step * self.repeat_times:
            print("❌ Dữ liệu chưa đủ:", len(buff), "đã nhận,", self.num_step * self.repeat_times, "cần thiết")
            self.status_label.config(text="Error: Not enough data")
            return
        for i in range(self.repeat_times * 2):
            start = i * (self.num_step // 2)
            for j in range(self.num_step // 4):
                idx1 = start + j
                idx2 = start + (self.num_step // 2 - j - 1)
                if idx1 < len(buff) and idx2 < len(buff):
                    buff[idx1], buff[idx2] = buff[idx2], buff[idx1]
        self.buffer_voltage = []
        self.buffer_current_raw = []
        for i in range(len(buff)):
            try:
                parts = buff[i].split(";")
                if parts[1].lower() == "inf":
                    continue
                current = float(parts[1]) * -25000
                self.buffer_current_raw.append(current)
                if i == 0:
                    voltage = S_Vol
                elif i % numStep == 0:
                    voltage = S_Vol
                elif ((i // (numStep // 2)) % 2 == 0):
                    voltage = self.buffer_voltage[i - 1] + Step
                elif (i % (numStep // 2) == 0):
                    voltage = self.buffer_voltage[i - 1]
                else:
                    voltage = self.buffer_voltage[i - 1] - Step
                self.buffer_voltage.append(voltage)
            except:
                continue
        self.smooth_data()
        self.x_data = self.buffer_voltage
        self.y_data = self.buffer_current_filtered
        self.status_label.config(text=f"Received: {self.receiver_count} points")
        self.update_plot()

    # ✅ THAY ĐỔI: Hàm smooth_data với nhiều bộ lọc
    def smooth_data(self):
        """✅ Bộ lọc đa dạng cho CV - Chọn theo filter_type"""
        b = self.buffer_current_raw
        if len(b) == 0: 
            self.buffer_current_filtered = []
            return
        
        try:
            filter_type = self.filter_type
            
            if filter_type == "1/3 (Moving Avg 3pt)":
                # 3-point moving average (original)
                self.buffer_current_filtered = [0] * len(b)
                f = self.buffer_current_filtered
                f[0] = b[0]
                if len(b) > 1:
                    f[1] = round((b[0] + b[1]) / 2, 3)
                for i in range(2, len(b)):
                    f[i] = round((b[i] + b[i-1] + b[i-2]) / 3, 3)
                    
            elif filter_type == "1/7 (Moving Avg 7pt)":
                # 7-point moving average
                if len(b) < 7:
                    self.apply_simple_moving_average(b, 3)
                else:
                    kernel = np.ones(7) / 7
                    padded = np.pad(b, (3, 3), mode='edge')
                    filtered = np.convolve(padded, kernel, mode='valid')
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]
                    
            elif filter_type == "1/9 (Moving Avg 9pt)":
                # 9-point moving average
                if len(b) < 9:
                    self.apply_simple_moving_average(b, 3)
                else:
                    kernel = np.ones(9) / 9
                    padded = np.pad(b, (4, 4), mode='edge')
                    filtered = np.convolve(padded, kernel, mode='valid')
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]
                    
            elif filter_type == "1/11 (Moving Avg 11pt)":
                # 11-point moving average
                if len(b) < 11:
                    self.apply_simple_moving_average(b, 3)
                else:
                    kernel = np.ones(11) / 11
                    padded = np.pad(b, (5, 5), mode='edge')
                    filtered = np.convolve(padded, kernel, mode='valid')
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]

            elif filter_type == "1/13 (Moving Avg 13pt)":
                # 13-point moving average
                if len(b) < 13:
                    self.apply_simple_moving_average(b, 3)
                else:
                    kernel = np.ones(13) / 13
                    padded = np.pad(b, (6, 6), mode='edge')
                    filtered = np.convolve(padded, kernel, mode='valid')
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]

            elif filter_type == "1/15 (Moving Avg 15pt)":  # ✅ THÊM MỚI 1/15
                # 15-point moving average
                if len(b) < 15:
                    self.apply_simple_moving_average(b, 3)
                else:
                    kernel = np.ones(15) / 15
                    padded = np.pad(b, (7, 7), mode='edge')
                    filtered = np.convolve(padded, kernel, mode='valid')
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]

            elif filter_type == "Savitzky-Golay":
                # Savitzky-Golay filter - tốt nhất cho giữ peak
                if len(b) < 5:
                    self.apply_simple_moving_average(b, 3)
                else:
                    window = min(7, len(b) if len(b) % 2 == 1 else len(b) - 1)
                    filtered = savgol_filter(b, window_length=window, polyorder=2)
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]
                    
            elif filter_type == "Gaussian Filter":
                # Gaussian filter - smooth tự nhiên
                if len(b) < 5:
                    self.apply_simple_moving_average(b, 3)
                else:
                    sigma = 1.0  # Có thể điều chỉnh
                    filtered = gaussian_filter1d(b, sigma=sigma)
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]
                    
            elif filter_type == "Median Filter":
                # Median filter - tốt cho loại bỏ spike
                if len(b) < 3:
                    self.buffer_current_filtered = b.copy()
                else:
                    kernel_size = min(5, len(b) if len(b) % 2 == 1 else len(b) - 1)
                    filtered = medfilt(b, kernel_size=kernel_size)
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]
                    
            elif filter_type == "Combo Filter":
                # Combo: Median + Savitzky-Golay - tốt nhất
                if len(b) < 7:
                    self.apply_simple_moving_average(b, 3)
                else:
                    # Bước 1: Median filter loại spike
                    b_despike = medfilt(b, kernel_size=3)
                    # Bước 2: Savitzky-Golay smooth
                    filtered = savgol_filter(b_despike, window_length=5, polyorder=2)
                    self.buffer_current_filtered = [round(x, 3) for x in filtered]
                    
            elif filter_type == "Exponential Smoothing":
                # Exponential smoothing - ưu tiên data gần
                alpha = 0.3  # Smoothing factor
                self.buffer_current_filtered = [b[0]]
                for i in range(1, len(b)):
                    smoothed = alpha * b[i] + (1 - alpha) * self.buffer_current_filtered[i-1]
                    self.buffer_current_filtered.append(round(smoothed, 3))
                    
            else:
                # Default fallback
                self.apply_simple_moving_average(b, 3)
                
        except Exception as e:
            print(f"Filter error: {e}")
            # Fallback to simple moving average
            self.apply_simple_moving_average(b, 3)

    def apply_simple_moving_average(self, data, window=3):
        """Helper function for simple moving average"""
        self.buffer_current_filtered = [0] * len(data)
        f = self.buffer_current_filtered
        f[0] = data[0]
        if len(data) > 1:
            f[1] = round((data[0] + data[1]) / 2, 3)
        for i in range(2, len(data)):
            if window == 3:
                f[i] = round((data[i] + data[i-1] + data[i-2]) / 3, 3)
            elif window == 5 and i >= 4:
                f[i] = round((data[i] + data[i-1] + data[i-2] + data[i-3] + data[i-4]) / 5, 3)
            else:
                f[i] = round((data[i] + data[i-1] + data[i-2]) / 3, 3)

    def update_plot(self):
        scaled_y = [y / 1000 for y in self.y_data]
        self.line.set_data(self.x_data, scaled_y)
        self.ax.relim()
        self.ax.autoscale_view()
        self.ax.set_xlim(self.start_voltage - 100, self.end_voltage + 100)
        self.ax.set_ylabel("Current (µA) (10³)", fontsize=14)
        
        # Update title với filter info
        scan_rate = getattr(self, 'scan_rate', 100)
        filter_short = self.filter_type.split(" ")[0] if self.filter_type else "1/7"
        self.ax.set_title(f"CV Data - {scan_rate} mV/s - Filter: {filter_short}", 
                         fontsize=16, fontweight='bold', color="#760FCF")
        
        # Làm to và đậm các số trên trục
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(18)
            label.set_fontweight('bold')
            label.set_color("#760FCF")
        self.canvas.draw()

    def clear_data(self):
        self.x_data.clear()
        self.y_data.clear()
        self.buffer_voltage.clear()
        self.buffer_current_raw.clear()
        self.buffer_current_filtered.clear()
        self.receiver_count = 0
        self.buffer_serial.clear()
        self.status_label.config(text="Ready")
        self.update_plot()

    def import_file(self):
        filepath = filedialog.askopenfilename(filetypes=[("CSV Files", "*.csv")])
        if filepath:
            with open(filepath, 'r') as f:
                lines = f.readlines()
                if len(lines) < 7:
                    messagebox.showerror("Error", "File format is incorrect!")
                    return
                try:
                    self.start_voltage = int(lines[0].split(',')[1])
                    self.end_voltage = int(lines[1].split(',')[1])
                    self.step_voltage = int(lines[2].split(',')[1])
                    self.scan_rate = float(lines[3].split(',')[1])
                    self.repeat_times = int(lines[4].split(',')[1])
                    
                    # Update GUI
                    self.start_entry.delete(0, tk.END)
                    self.start_entry.insert(0, str(self.start_voltage))
                    self.end_entry.delete(0, tk.END)
                    self.end_entry.insert(0, str(self.end_voltage))
                    self.step_entry.delete(0, tk.END)
                    self.step_entry.insert(0, str(self.step_voltage))
                    self.scan_rate_entry.delete(0, tk.END)
                    self.scan_rate_entry.insert(0, str(self.scan_rate))
                    self.repeat_entry.delete(0, tk.END)
                    self.repeat_entry.insert(0, str(self.repeat_times))
                    
                    # Import data
                    self.buffer_voltage.clear()
                    self.buffer_current_raw.clear()
                    self.buffer_current_filtered.clear()
                    for line in lines[6:]:
                        parts = line.strip().split(",")
                        if len(parts) == 3:
                            self.buffer_voltage.append(float(parts[0]))
                            self.buffer_current_raw.append(float(parts[1]))
                            self.buffer_current_filtered.append(float(parts[2]))
                            
                    self.x_data = self.buffer_voltage
                    self.y_data = self.buffer_current_filtered
                    self.status_label.config(text=f"Imported: {self.scan_rate} mV/s")
                    self.update_plot()
                except Exception as e:
                    messagebox.showerror("Import Error", str(e))

    def export_file(self):
        filepath = filedialog.asksaveasfilename(defaultextension=".csv")
        if filepath:
            try:
                with open(filepath, 'w') as f:
                    f.write(f"Start Voltage,{self.start_voltage},[mV]\n")
                    f.write(f"End Voltage,{self.end_voltage},[mV]\n")
                    f.write(f"Step,{self.step_voltage},[mV]\n")
                    f.write(f"Scan Rate,{getattr(self, 'scan_rate', 100)},[mV/s]\n")
                    f.write(f"Repeat Times,{self.repeat_times},[times]\n")
                    # ✅ THÊM: Lưu thông tin filter
                    f.write(f"Filter Type,{self.filter_type},[type]\n")
                    f.write("Voltage (mV),Current Raw (uA),Current Filtered (uA)\n")
                    for v, raw, filt in zip(self.buffer_voltage, self.buffer_current_raw, self.buffer_current_filtered):
                        f.write(f"{v},{raw},{filt}\n")
                import os
                image_path = os.path.splitext(filepath)[0] + "_plot.png"
                self.fig.savefig(image_path, bbox_inches='tight', dpi=300)
                messagebox.showinfo("Exported", f"Data and plot exported successfully!\nCSV: {filepath}\nImage: {image_path}")
            except Exception as e:
                messagebox.showerror("Export Error", str(e))

# eis_3e

class EISApp:
    def __init__(self, parent):
        import tkinter as tk
        from tkinter import ttk, messagebox
        import serial
        import threading
        import math
        import matplotlib.pyplot as plt
        import serial.tools.list_ports
        import numpy as np
        from scipy.ndimage import uniform_filter1d 
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
        from PIL import Image, ImageTk
        import pandas as pd
        from tkinter import filedialog
        from scipy.signal import savgol_filter
        
        self.root = parent
        self.serial_port = None
        self.running = False

        self.freqs = []
        self.magnitudes = []
        self.phases = []
        self.reals = []
        self.imags = []

        self.setup_ui()
        self.setup_plot()

    def setup_ui(self):
        tk = self.tk = __import__('tkinter')
        ttk = self.ttk = __import__('tkinter.ttk').ttk

        # Màu nền đồng bộ
        bg_color = "#E3F2FD"

        # Style cho nút
        style = ttk.Style()
        style.configure("EIS3E.TButton", font=("Segoe UI", 15, "bold"), padding=10, background="#E3F2FD")
        style.map("EIS3E.TButton", background=[('active', '#BBDEFB')])

        # Top frame
        top_frame = tk.Frame(self.root, bg=bg_color)
        top_frame.pack(padx=10, pady=5, fill=tk.X)

        # Khung chứa các nút
        control_frame = tk.Frame(top_frame, bg=bg_color)
        control_frame.pack(side=tk.LEFT, padx=10)

        self.port_combo = ttk.Combobox(control_frame, width=12, postcommand=self.refresh_ports, font=("Segoe UI", 14, "bold"))
        self.port_combo.grid(row=0, column=0, padx=5)

        self.connect_btn = ttk.Button(control_frame, text="\U0001F50C Connect", width=14, command=self.connect_serial, style="EIS3E.TButton")
        self.connect_btn.grid(row=0, column=1, padx=5)
        self.start_btn = ttk.Button(control_frame, text="\u25B6\ufe0f Start", width=14, command=self.start_reading, style="EIS3E.TButton")
        self.start_btn.grid(row=0, column=2, padx=5)
        self.stop_btn = ttk.Button(control_frame, text="\u23F9 Stop", width=14, command=self.stop_reading, style="EIS3E.TButton")
        self.stop_btn.grid(row=0, column=3, padx=5)
        self.disconnect_btn = ttk.Button(control_frame, text="\u274C Disconnect", width=14, command=self.disconnect_serial, style="EIS3E.TButton")
        self.disconnect_btn.grid(row=0, column=4, padx=5)
        self.clear_btn = ttk.Button(control_frame, text="🧹 Clear Data", width=14, command=self.clear_data, style="EIS3E.TButton")
        self.clear_btn.grid(row=0, column=5, padx=5)
        self.export_btn = ttk.Button(control_frame, text="\U0001F4BE Export CSV", width=14, command=self.export_to_excel, style="EIS3E.TButton")
        self.export_btn.grid(row=0, column=6, padx=5)
        self.import_btn = ttk.Button(control_frame, text="\U0001F4C2 Import CSV", width=14, command=self.import_from_excel, style="EIS3E.TButton")
        self.import_btn.grid(row=0, column=7, padx=5)

        # Logo bên phải
        from PIL import Image, ImageTk
        try:
            img1 = Image.open(resource_path("dai-hoc-khoa-hoc-tu-nhien-Photoroom.png")).resize((120, 100), Image.Resampling.LANCZOS)
            self.logo1 = ImageTk.PhotoImage(img1)
            tk.Label(top_frame, image=self.logo1, bg=bg_color).pack(side=tk.RIGHT, padx=5)
            img2 = Image.open(resource_path("Logo-DH-Quoc-Gia-Ha-Noi-VNU-Photoroom.png")).resize((65, 65), Image.Resampling.LANCZOS)
            self.logo2 = ImageTk.PhotoImage(img2)
            tk.Label(top_frame, image=self.logo2, bg=bg_color).pack(side=tk.RIGHT, padx=5)
        except Exception as e:
            tk.Label(top_frame, text="Logo error", bg=bg_color).pack(side=tk.RIGHT)

        # ==== Parameter frame ====
        param_frame = tk.Frame(self.root, bg=bg_color)
        param_frame.pack(padx=15, pady=10, fill=tk.X)

        font_conf = ("Segoe UI", 14, "bold")
        label_opts = dict(font=font_conf, bg=bg_color, anchor="w")
        spin_opts = dict(font=font_conf, width=12, justify="center")

        tk.Label(param_frame, text="Start Freq (Hz)", **label_opts).grid(row=0, column=0, padx=5, pady=5)
        self.start_freq_spin = tk.Spinbox(param_frame, from_=1, to=1_000_000, increment=1, **spin_opts)
        self.start_freq_spin.delete(0, "end")
        self.start_freq_spin.insert(0, "110")
        self.start_freq_spin.grid(row=0, column=1, padx=5)

        tk.Label(param_frame, text="Stop Freq (Hz)", **label_opts).grid(row=0, column=2, padx=5, pady=5)
        self.stop_freq_spin = tk.Spinbox(param_frame, from_=10, to=1_000_000, increment=10, **spin_opts)
        self.stop_freq_spin.delete(0, "end")
        self.stop_freq_spin.insert(0, "10000")
        self.stop_freq_spin.grid(row=0, column=3, padx=5)

        tk.Label(param_frame, text="Sweep Points", **label_opts).grid(row=0, column=4, padx=5, pady=5)
        self.sweep_points_spin = tk.Spinbox(param_frame, from_=1, to=1000, increment=1, **spin_opts)
        self.sweep_points_spin.delete(0, "end")
        self.sweep_points_spin.insert(0, "200")
        self.sweep_points_spin.grid(row=0, column=5, padx=5)

        tk.Label(param_frame, text="Repeat Times", **label_opts).grid(row=0, column=6, padx=5, pady=5)
        self.repeat_times_spin = tk.Spinbox(param_frame, from_=1, to=10, increment=1, **spin_opts)
        self.repeat_times_spin.delete(0, "end")
        self.repeat_times_spin.insert(0, "1")
        self.repeat_times_spin.grid(row=0, column=7, padx=5)
    
    
    def clear_data(self):
        self.freqs.clear()
        self.reals.clear()
        self.imags.clear()
        self.magnitudes.clear()
        self.phases.clear()

        # Xoá đồ thị Bode
        self.ax_bode.cla()
        self.ax_phase.cla()
        self.ax_bode.set_title("Bode Plot")
        self.ax_bode.set_xlabel("Frequency (Hz)")
        self.ax_bode.set_ylabel("Magnitude (Ohm)", color='r')
        self.ax_phase.set_ylabel("Phase (°)", color='b')
        self.canvas_bode.draw()

        # Xoá đồ thị Nyquist
        self.ax_nyquist.cla()
        self.ax_nyquist.set_title("Nyquist Plot")
        self.ax_nyquist.set_xlabel("Re(Z) (Ohm)")
        self.ax_nyquist.set_ylabel("Im(Z) (Ohm)")
        self.canvas_nyquist.draw()
    
    def export_to_excel(self):
        if not self.freqs:
            messagebox.showwarning("No Data", "No data to export.")
            return

        df = pd.DataFrame({
            'Frequency (Hz)': self.freqs,
            'Magnitude (Ohm)': self.magnitudes,
            'Phase (Degree)': self.phases,
            'Re(Z) (Ohm)': self.reals,
            'Im(Z) (Ohm)': self.imags
        })

        file_path = filedialog.asksaveasfilename(defaultextension=".csv",
                                                filetypes=[("CSV Files", "*.csv")])
        if file_path:
            df.to_csv(file_path, index=False)
            import os
            image_path_bode = os.path.splitext(file_path)[0] + "_bode_plot.png"
            image_path_nyquist = os.path.splitext(file_path)[0] + "_nyquist_plot.png"
            self.fig_bode.savefig(image_path_bode, dpi=300, bbox_inches='tight')
            self.fig_nyquist.savefig(image_path_nyquist, dpi=300, bbox_inches='tight')
            
            messagebox.showinfo("Exported", f"Dữ liệu và đồ thị đã xuất thành công!\nCSV: {file_path}\nBode: {image_path_bode}\nNyquist: {image_path_nyquist}")


    def import_from_excel(self):
        file_path = filedialog.askopenfilename(filetypes=[("CSV Files", "*.csv")])
        if not file_path:
            return

        try:
            df = pd.read_csv(file_path)

            self.freqs = df['Frequency (Hz)'].tolist()
            self.magnitudes = df['Magnitude (Ohm)'].tolist()
            self.phases = df['Phase (Degree)'].tolist()
            self.reals = df['Re(Z) (Ohm)'].tolist()
            self.imags = df['Im(Z) (Ohm)'].tolist()

            self.update_plots()
            messagebox.showinfo("Imported", f"Data imported from:\n{file_path}")
        except Exception as e:
            messagebox.showerror("Import Error", str(e))

            
    def disconnect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.running = False  # Dừng luồng đọc dữ liệu nếu đang chạy
            self.serial_port.close()
            messagebox.showinfo("Disconnected", "Serial port has been disconnected.")
        else:
            messagebox.showwarning("Warning", "No serial connection to disconnect.")
            
    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])  # chọn COM đầu tiên mặc định


    def setup_plot(self):
        import matplotlib.pyplot as plt
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

        bg_color = "#E3F2FD"
        plot_frame = self.tk.Frame(self.root, bg=bg_color)
        plot_frame.pack(fill=self.tk.BOTH, expand=True, padx=10, pady=10)

        # Bode plot (trái)
        self.fig_bode, self.ax_bode = plt.subplots(figsize=(6, 5))
        self.fig_bode.patch.set_facecolor(bg_color)
        self.ax_bode.set_facecolor("#FFFFFF")
        self.ax_bode.set_title("Bode Plot", fontsize=18, fontweight="bold", color="#1976D2")
        self.ax_bode.set_xlabel("Frequency (Hz)", fontsize=15, color="#1976D2")
        self.ax_bode.set_ylabel("Magnitude (Ohm)", fontsize=15, color="#1976D2")
        self.ax_bode.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.ax_phase = self.ax_bode.twinx()
        self.ax_phase.set_ylabel("Phase (°)", fontsize=15, color="#D84315")
        self.ax_phase.tick_params(axis='y', labelcolor="#D84315")
        self.canvas_bode = FigureCanvasTkAgg(self.fig_bode, master=plot_frame)
        self.canvas_bode.get_tk_widget().pack(side=self.tk.LEFT, fill=self.tk.BOTH, expand=True, padx=10)

        self.fig_bode.tight_layout()
        self.canvas_bode.draw()
        for label in (self.ax_bode.get_xticklabels() + self.ax_bode.get_yticklabels() + self.ax_phase.get_yticklabels()):
            label.set_fontsize(16)
            label.set_fontweight('bold')
            label.set_color("#1976D2")

        # Nyquist plot (phải)
        self.fig_nyquist, self.ax_nyquist = plt.subplots(figsize=(6, 5))
        self.fig_nyquist.patch.set_facecolor(bg_color)
        self.ax_nyquist.set_facecolor("#FFFFFF")
        self.ax_nyquist.set_title("Nyquist Plot", fontsize=18, fontweight="bold", color="#388E3C")
        self.ax_nyquist.set_xlabel("Re(Z) (Ohm)", fontsize=15, color="#388E3C")
        self.ax_nyquist.set_ylabel("Im(Z) (Ohm)", fontsize=15, color="#388E3C")
        self.ax_nyquist.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.canvas_nyquist = FigureCanvasTkAgg(self.fig_nyquist, master=plot_frame)
        self.canvas_nyquist.get_tk_widget().pack(side=self.tk.LEFT, fill=self.tk.BOTH, expand=True, padx=10)

        self.fig_nyquist.tight_layout()
        self.canvas_nyquist.draw()
        for label in (self.ax_nyquist.get_xticklabels() + self.ax_nyquist.get_yticklabels()):
            label.set_fontsize(16)
            label.set_fontweight('bold')
            label.set_color("#388E3C")

    def connect_serial(self):
        port = self.port_combo.get().strip()
        if not port:
            messagebox.showwarning("Warning", "Please select a port.")
            return
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            messagebox.showinfo("Connected", f"Connected to {port}")
        except Exception as e:
            messagebox.showerror("Connection Error", f"Could not open {port}\n{e}")


    def start_reading(self):
        if not self.serial_port or not self.serial_port.is_open:
            messagebox.showwarning("Warning", "Serial port not connected.")
            return

        try:
            # Đọc và kiểm tra giá trị đầu vào
            start_freq = int(self.start_freq_spin.get())
            stop_freq = int(self.stop_freq_spin.get())
            self.sweep_points = int(self.sweep_points_spin.get())
            self.repeat_times = int(self.repeat_times_spin.get())

            if start_freq >= stop_freq:
                messagebox.showerror("Invalid Frequency Range", "Start frequency must be less than stop frequency.")
                return

            if self.sweep_points < 2:
                messagebox.showerror("Invalid Sweep Points", "Sweep points must be at least 2.")
                
                return

            # Tạo và gửi lệnh
            command = f"3#{start_freq}?{stop_freq}/{self.sweep_points}|{self.repeat_times}$1!"
            self.serial_port.write(command.encode())
            print(f"Sent: {command}")

            # Xoá dữ liệu cũ trước khi đọc mới
            self.clear_data()

            # Gán expected_points để theo dõi số lượng dữ liệu mong đợi
            self.expected_points = self.sweep_points * self.repeat_times

            # Bắt đầu luồng đọc
            self.running = True
            threading.Thread(target=self.read_serial, daemon=True).start()

        except ValueError:
            messagebox.showerror("Input Error", "Please enter valid numbers for all parameters.")
        except Exception as e:
            messagebox.showerror("Send Error", f"Could not send command.\n\n{str(e)}")


    def stop_reading(self):
        self.running = False

    def read_serial(self):
        while self.running:
            try:
                line = self.serial_port.readline().decode().strip()
                if not line or ";" not in line:
                    continue

                print(f"Received: {line}")

                parts = line.split(";")
                if len(parts) != 3:
                    continue

                try:
                    freq = float(parts[0])

                    if parts[1].lower() == "inf" or parts[2].lower() == "inf":
                        magnitude = 600_000_000
                        phase = 0
                        rz_real = 600_000_000
                        rz_imag = 0
                    else:
                        rz_real = float(parts[1])
                        rz_imag = float(parts[2])
                        magnitude = math.sqrt(rz_real**2 + rz_imag**2)
                        phase = math.degrees(math.atan2(-rz_imag, rz_real))

                    self.freqs.append(freq)
                    self.reals.append(rz_real)
                    self.imags.append(rz_imag)
                    self.magnitudes.append(magnitude)
                    self.phases.append(phase)

                    self.update_plots()

                    # ✅ Nếu đo đủ điểm, dừng tự động
                    if len(self.freqs) >= self.expected_points:
                        print("✅ Đã đo xong, dừng đọc.")
                        self.running = False

                except ValueError as e:
                    print("⚠️ Lỗi định dạng:", e)

            except Exception as e:
                print("❌ Lỗi khi đọc dữ liệu:", e)


    def update_plots(self):
        # Bỏ qua nếu chưa có đủ dữ liệu
        if len(self.freqs) < 5:
            return

        # ==== HÀM LÀM MƯỢT ====
        def smooth_data(data):
            result = []
            for i in range(len(data)):
                if i == 0:
                    result.append(data[0])
                elif i == 1:
                    result.append(round((data[0] + data[1]) / 2, 3))
                else:
                    result.append(round((data[i] + data[i - 1] + data[i - 2]) / 3, 3))
            return result

        def smooth_nyquist(real_list, imag_list, window=11, poly=3):
            if len(real_list) < window or len(imag_list) < window:
                return real_list, imag_list  # không đủ điểm để lọc

            # Áp dụng Savitzky-Golay filter
            real_smooth = savgol_filter(real_list, window_length=window, polyorder=poly).tolist()
            imag_smooth = savgol_filter(imag_list, window_length=window, polyorder=poly).tolist()
            return real_smooth, imag_smooth


        # ==== LÀM MƯỢT DỮ LIỆU ====
        mag_smooth = smooth_data(self.magnitudes)
        pha_smooth = smooth_data(self.phases)
        re_smooth, im_smooth = smooth_nyquist(self.reals, self.imags, window=11, poly=3)

        
        # ==== BODE PLOT ====
        self.ax_bode.cla()
        self.ax_phase.cla()
        self.ax_bode.set_title("Bode Plot")
        self.ax_bode.set_xlabel("Frequency (Hz)")
        self.ax_bode.set_ylabel("Magnitude (Ohm)", color='r')
        self.ax_phase.set_ylabel("Phase (°)", color='b')

        self.ax_bode.set_xscale('linear')  # bạn có thể đổi thành 'log' nếu cần
        self.ax_bode.set_xlim(min(self.freqs), max(self.freqs))
        self.ax_bode.set_ylim(0, max(mag_smooth) * 1.2)
        self.ax_phase.set_ylim(-100, 10)

        self.ax_bode.plot(self.freqs, mag_smooth, 'k-', linewidth=2.5, label="Magnitude (Ohm)")
        self.ax_phase.plot(self.freqs, pha_smooth, 'r-', linewidth=2.5, label="Phase (Degree)")

        self.ax_bode.grid(True)
        self.canvas_bode.draw()

        # ==== NYQUIST PLOT ====
        self.ax_nyquist.cla()
        self.ax_nyquist.set_title("Nyquist Plot")
        self.ax_nyquist.set_xlabel("Re(Z) (Ohm)")
        self.ax_nyquist.set_ylabel("Im(Z) (Ohm)")

        self.ax_nyquist.plot(re_smooth, im_smooth, 'k-', linewidth=2.5, label="Smoothed")
        self.ax_nyquist.plot(self.reals, self.imags, 'r.', markersize=3, label="Raw Data")

        self.ax_nyquist.set_xlim(0, max(self.reals) * 1.1)
        self.ax_nyquist.set_ylim(min(self.imags) * 1.1, max(self.imags) * 1.1)

        self.ax_nyquist.grid(True)
        self.ax_nyquist.legend()
        self.canvas_nyquist.draw()

# CA

class ChronoAmperometryApp:
    def __init__(self, parent):
        import tkinter as tk
        from tkinter import ttk, messagebox, filedialog
        import serial
        import serial.tools.list_ports
        import matplotlib.pyplot as plt
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
        import threading
        import pandas as pd
        from PIL import Image, ImageTk
        import os

        self.tk = tk
        self.ttk = ttk
        self.messagebox = messagebox
        self.filedialog = filedialog
        self.serial = serial
        self.pd = pd
        self.Image = Image
        self.ImageTk = ImageTk
        self.plt = plt
        self.os = os
        self.threading = threading

        self.root = parent
        self.serial_port = None
        self.running = False

        self.e_voltage = tk.IntVar(value=200)
        self.time_run = tk.IntVar(value=10)
        self.time_interval = tk.IntVar(value=100)

        self.time_data = []
        self.current_data = []

        # ===== Top bar với logo và tiêu đề =====
        top_frame = tk.Frame(self.root, bg="#FFF9C4")
        top_frame.pack(side="top", fill="x", padx=5, pady=5)

        logo_frame = tk.Frame(top_frame, bg="#FFF9C4")
        logo_frame.pack(side="right", padx=10)

        try:
            img1 = self.Image.open(resource_path("dai-hoc-khoa-hoc-tu-nhien-Photoroom.png")).resize((100, 80), self.Image.Resampling.LANCZOS)
            self.logo1 = self.ImageTk.PhotoImage(img1)
            tk.Label(logo_frame, image=self.logo1, bg="#FFF9C4").grid(row=0, column=0, padx=5)

            title_label = tk.Label(
                logo_frame,
                text="Chronoamperometry\nby HUS",
                font=("Segoe UI", 14, "bold"),
                bg="#FFF9C4",
                fg="#F9A825",  # Vàng cam nổi bật
                justify="center"
            )
            title_label.grid(row=0, column=1, padx=10)

            img2 = self.Image.open("E:/oE/Download/images-Photoroom.png").resize((50, 50), self.Image.Resampling.LANCZOS)
            self.logo2 = self.ImageTk.PhotoImage(img2)
            tk.Label(logo_frame, image=self.logo2, bg="#FFF9C4").grid(row=0, column=2, padx=5)
        except Exception as e:
            print("Lỗi ảnh logo:", e)

        # ===== Main content chia 2 panel =====
        main_content = tk.Frame(self.root, bg="#FFF9C4")
        main_content.pack(fill="both", expand=True)

        self.frame_left = tk.Frame(main_content, bg="#FFF9C4", width=270)
        self.frame_left.pack(side="left", padx=15, pady=10, fill="y")
        self.frame_left.pack_propagate(False)

        # ... trong __init__ của ChronoAmperometryApp ...

        # Tạo style cho nút CA
        style = self.ttk.Style()
        style.configure("CA.TButton", font=("Segoe UI", 15, "bold"), padding=8)

        # Serial Port Control
        tk.Label(self.frame_left, text="Serial Port Control", font=("Segoe UI", 15, "bold"), fg="#1976D2", bg="#FFF9C4").pack(anchor="w", pady=(5, 10))
        port_frame = tk.Frame(self.frame_left, bg="#FFF9C4")
        port_frame.pack(fill="x", pady=2)
        tk.Label(port_frame, text="Port:", font=("Segoe UI", 14, "bold"), bg="#FFF9C4").pack(side=tk.LEFT)
        self.port_combo = ttk.Combobox(port_frame, values=self.get_serial_ports(), width=16, font=("Segoe UI", 14, "bold"))
        self.port_combo.pack(side=tk.LEFT, padx=5)

        self.ttk.Button(self.frame_left, text="Refresh COM", command=self.refresh_ports, style="CA.TButton", width=16).pack(fill="x", pady=3)
        self.ttk.Button(self.frame_left, text="\U0001F50C Connect", command=self.connect_serial, style="CA.TButton", width=16).pack(fill="x", pady=3)
        self.ttk.Button(self.frame_left, text="\u274C Disconnect", command=self.disconnect_serial, style="CA.TButton", width=16).pack(fill="x", pady=3)

        # Program Control
        tk.Label(self.frame_left, text="Program Control", font=("Segoe UI", 15, "bold"), fg="#388E3C", bg="#FFF9C4").pack(anchor="w", pady=(12, 5))
        self.ttk.Button(self.frame_left, text="\u25B6\ufe0f Measure", command=self.start_measurement, style="CA.TButton", width=16).pack(fill="x", pady=3)
        self.ttk.Button(self.frame_left, text="🧹 Clear Data", command=self.clear_data, style="CA.TButton", width=16).pack(fill="x", pady=3)

        # Parameter config
        tk.Label(self.frame_left, text="Parameter Config", font=("Segoe UI", 15, "bold"), fg="#F9A825", bg="#FFF9C4").pack(anchor="w", pady=(12, 5))
        self.add_labeled_entry("E Voltage (mV):", self.e_voltage, row=0, font=("Segoe UI", 14, "bold"))
        self.add_labeled_entry("Time Run (s):", self.time_run, row=1, font=("Segoe UI", 14, "bold"))
        self.add_labeled_entry("Time Interval (ms):", self.time_interval, row=2, font=("Segoe UI", 14, "bold"))

        # Import/Export
        tk.Label(self.frame_left, text="Data Control", font=("Segoe UI", 15, "bold"), fg="#00838F", bg="#FFF9C4").pack(anchor="w", pady=(12, 5))
        self.ttk.Button(self.frame_left, text="\U0001F4C2 Import CSV", command=self.import_csv, style="CA.TButton", width=16).pack(fill="x", pady=3)
        self.ttk.Button(self.frame_left, text="\U0001F4BE Export CSV", command=self.export_csv, style="CA.TButton", width=16).pack(fill="x", pady=3)

        # Status label
        self.status_label = tk.Label(self.frame_left, text="Ready", foreground="#F9A825", font=("Segoe UI", 14, "bold"), bg="#FFF9C4")
        self.status_label.pack(pady=8)

        # ===== Plot Area =====
        self.frame_right = tk.Frame(main_content, bg="#FFF9C4")
        self.frame_right.pack(side="right", expand=True, fill="both", padx=10, pady=10)

        self.fig, self.ax = self.plt.subplots(figsize=(7, 5))
        self.fig.patch.set_facecolor("#FFF9C4")
        self.ax.set_facecolor("#FFFFFF")
        self.ax.set_title("Chronoamperometry", fontsize=18, fontweight="bold", color="#F9A825")
        self.ax.set_xlabel("Time (s)", fontsize=15, color="#F9A825")
        self.ax.set_ylabel("Current (uA)", fontsize=15, color="#F9A825")
        self.ax.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.line_plot, = self.ax.plot([], [], color='red', linewidth=2)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.frame_right)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # Làm to và đậm các số trên trục
        for label in (self.ax.get_xticklabels() + self.ax.get_yticklabels()):
            label.set_fontsize(16)
            label.set_fontweight('bold')
            label.set_color("#F9A825")

    def add_labeled_entry(self, label, var, row, font=("Segoe UI", 14, "bold")):
        frame = self.tk.Frame(self.frame_left, bg="#FFF9C4")
        frame.pack(fill="x", pady=3)
        self.tk.Label(frame, text=label, font=font, bg="#FFF9C4", anchor="w", width=18).pack(side="left")
        entry = self.tk.Entry(frame, textvariable=var, font=font, width=12, justify="center")
        entry.pack(side="right", fill="x")

    # --- Các hàm chức năng giữ nguyên ---
    def get_serial_ports(self):
        return [port.device for port in self.serial.tools.list_ports.comports()]

    def refresh_ports(self):
        ports = self.get_serial_ports()
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])

    def connect_serial(self):
        port_name = self.port_combo.get()
        if not port_name:
            self.messagebox.showwarning("Warning", "Please select a COM port.")
            return
        try:
            self.serial_port = self.serial.Serial(port_name, 115200, timeout=1)
            self.status_label.config(text=f"Connected: {port_name}")
            self.messagebox.showinfo("Connected", f"Connected to {port_name}")
        except Exception as e:
            self.messagebox.showerror("Error", str(e))

    def disconnect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.running = False
            self.serial_port.close()
            self.status_label.config(text="Disconnected")
            self.messagebox.showinfo("Disconnected", "Disconnected from serial port.")

    def start_measurement(self):
        if not self.serial_port or not self.serial_port.is_open:
            self.messagebox.showwarning("Warning", "Serial port not connected!")
            return

        e_vol = self.e_voltage.get()
        t_run = self.time_run.get()
        t_int = self.time_interval.get()

        # Reset dữ liệu
        self.time_data.clear()
        self.current_data.clear()
        self.line_plot.set_data([], [])

        # Cập nhật lại nhãn
        self.ax.set_title("Chronoamperometry", fontsize=18, fontweight="bold", color="#F9A825")
        self.ax.set_xlabel("Time (s)", fontsize=15, color="#F9A825")
        self.ax.set_ylabel("Current (uA)", fontsize=15, color="#F9A825")
        self.ax.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.canvas.draw_idle()

        # Gửi lệnh xuống vi điều khiển
        command = f"5#{t_run}?{e_vol}/{t_int}|{t_int}$0!"
        self.serial_port.write(command.encode())
        print("Gửi lệnh:", command)

        # Bắt đầu luồng đọc dữ liệu
        self.running = True
        self.threading.Thread(target=self.read_serial_data, daemon=True).start()

    def read_serial_data(self):
        max_points = int(self.time_run.get() * 1000 / self.time_interval.get())
        while self.running and len(self.time_data) < max_points:
            try:
                line = self.serial_port.readline().decode(errors='ignore').strip()
                if ";" in line:
                    try:
                        t_str, i_str = line.split(";")
                        t = float(t_str)
                        i = 0 if i_str == "inf" else float(i_str)
                        if i >= 5234:
                            i = 5234
                        self.time_data.append(t)
                        self.current_data.append(i)
                        if len(self.time_data) % 5 == 0:
                            self.update_plot()
                    except ValueError:
                        continue
            except Exception as e:
                print("❌ Lỗi khi đọc dữ liệu từ serial:", e)
        self.running = False
        self.status_label.config(text=f"Done: {len(self.time_data)} points")
        self.messagebox.showinfo("Thông báo", f"Đã hoàn thành đo {len(self.time_data)} điểm.")

    def import_csv(self):
        file_path = self.filedialog.askopenfilename(filetypes=[("CSV/Excel files", "*.csv;*.xls;*.xlsx")])
        if not file_path:
            return
        try:
            df = self.pd.read_csv(file_path)
            self.time_run.set(int(df.iloc[0, 1]))
            self.e_voltage.set(int(df.iloc[1, 1]))
            self.time_interval.set(int(df.iloc[2, 1]))
            data = self.pd.read_csv(file_path, skiprows=5)
            self.time_data = list(data.iloc[:, 0])
            self.current_data = list(data.iloc[:, 1])
            self.ax.clear()
            self.ax.plot(self.time_data, self.current_data, color='red')
            self.ax.set_title("Chronoamperometry", fontsize=18, fontweight="bold", color="#F9A825")
            self.ax.set_xlabel("Time (s)", fontsize=15, color="#F9A825")
            self.ax.set_ylabel("Current (uA)", fontsize=15, color="#F9A825")
            self.ax.grid(True, linestyle='--', color='gray', alpha=0.5)
            self.canvas.draw()
            self.status_label.config(text="Imported")
            self.messagebox.showinfo("Import", "Dữ liệu đã được nhập thành công!")
        except Exception as e:
            self.messagebox.showerror("Import Error", str(e))

    def export_csv(self):
        if not self.time_data or not self.current_data:
            self.messagebox.showwarning("Export", "Không có dữ liệu để xuất.")
            return
        file_path = self.filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV Files", "*.csv")])
        if not file_path:
            return
        try:
            with open(file_path, 'w') as f:
                f.write(f"Time Run,{self.time_run.get()},[s]\n")
                f.write(f"E Voltage,{self.e_voltage.get()},[mV]\n")
                f.write(f"Time Interval,{self.time_interval.get()},[ms]\n")
                f.write("\n\n")
                f.write("Time (s),Current (uA)\n")
                for t, i in zip(self.time_data, self.current_data):
                    f.write(f"{t},{i}\n")
            image_path = self.os.path.splitext(file_path)[0] + "_plot.png"
            self.fig.savefig(image_path)
            self.status_label.config(text="Exported")
            self.messagebox.showinfo("Export", f"Đã xuất dữ liệu và đồ thị:\n{file_path}\n{image_path}")
        except Exception as e:
            self.messagebox.showerror("Export Error", str(e))

    def clear_data(self):
        self.running = False
        self.time_data.clear()
        self.current_data.clear()
        self.line_plot.set_data([], [])
        self.ax.set_title("Chronoamperometry", fontsize=18, fontweight="bold", color="#F9A825")
        self.ax.set_xlabel("Time (s)", fontsize=15, color="#F9A825")
        self.ax.set_ylabel("Current (uA)", fontsize=15, color="#F9A825")
        self.ax.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas.draw_idle()
        self.status_label.config(text="Ready")

    def update_plot(self):
        self.line_plot.set_data(self.time_data, self.current_data)
        self.ax.relim()
        self.ax.autoscale_view()
        self.canvas.draw_idle()

# eis_2e
class EIS2EApp:
    def __init__(self, parent):
        import tkinter as tk
        from tkinter import ttk, messagebox, filedialog
        import serial
        import serial.tools.list_ports
        import matplotlib.pyplot as plt
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
        import threading
        import pandas as pd
        import numpy as np
        import math
        from PIL import Image, ImageTk
        from scipy.ndimage import uniform_filter1d

        self.tk = tk
        self.ttk = ttk
        self.messagebox = messagebox
        self.filedialog = filedialog
        self.serial = serial
        self.pd = pd
        self.plt = plt
        self.Image = Image
        self.ImageTk = ImageTk
        self.threading = threading
        self.uniform_filter1d = uniform_filter1d
        self.math = math
        self.np = np

        self.root = parent
        self.serial_port = None
        self.running = False

        self.freqs = []
        self.magnitudes = []
        self.phases = []
        self.reals = []
        self.imags = []
        self.receiver_count = 0
        self.build_gui()
        self.setup_plot()

    def build_gui(self):
        tk = self.tk
        ttk = self.ttk

        bg_color = "#FFF9C4"

        # Style cho nút
        style = ttk.Style()
        style.configure("EIS2E.TButton", font=("Segoe UI", 15, "bold"), padding=10, background=bg_color)
        style.map("EIS2E.TButton", background=[('active', '#FFF176')])

        top_frame = tk.Frame(self.root, bg=bg_color)
        top_frame.pack(side=tk.TOP, fill=tk.X, padx=10, pady=5)

        try:
            img1 = self.Image.open("E:/eis_phuc/dai-hoc-khoa-hoc-tu-nhien.jpg").resize((120, 90), self.Image.Resampling.LANCZOS)
            self.logo1 = self.ImageTk.PhotoImage(img1)
            tk.Label(top_frame, image=self.logo1, bg=bg_color).pack(side=tk.RIGHT, padx=5)

            img2 = self.Image.open("E:/oE/Download/images-Photoroom.png").resize((65, 65), self.Image.Resampling.LANCZOS)
            self.logo2 = self.ImageTk.PhotoImage(img2)
            tk.Label(top_frame, image=self.logo2, bg=bg_color).pack(side=tk.RIGHT, padx=5)
        except:
            tk.Label(top_frame, text="Logo error", bg=bg_color).pack(side=tk.RIGHT)

        button_frame = tk.Frame(top_frame, bg=bg_color)
        button_frame.pack(side=tk.LEFT, padx=5)

        self.port_combo = ttk.Combobox(button_frame, width=12, postcommand=self.refresh_ports, font=("Segoe UI", 14, "bold"))
        self.port_combo.grid(row=0, column=0, padx=2)
        for i, (label, cmd) in enumerate([
            ("\U0001F50C Connect", self.connect_serial),
            ("\u25B6\ufe0f Start", self.start_reading),
            ("\u23F9 Stop", self.stop_reading),
            ("\u274C Disconnect", self.disconnect_serial),
            ("🧹 Clear Data", self.clear_data),
            ("\U0001F4BE Export CSV", self.export_excel),
            ("\U0001F4C2 Import CSV", self.import_excel)
        ], 1):
            ttk.Button(button_frame, text=label, command=cmd, style="EIS2E.TButton", width=14).grid(row=0, column=i, padx=2)

        param = tk.Frame(self.root, bg=bg_color)
        param.pack(fill=tk.X, padx=10, pady=8)
        font_conf = ("Segoe UI", 14, "bold")
        label_opts = dict(font=font_conf, bg=bg_color, anchor="w")
        entry_opts = dict(font=font_conf, width=12, justify="center")

        tk.Label(param, text="Start Freq (Hz)", **label_opts).grid(row=0, column=0, padx=5, pady=5)
        self.start_freq = tk.Entry(param, **entry_opts)
        self.start_freq.insert(0, "100")
        self.start_freq.grid(row=0, column=1, padx=5)

        tk.Label(param, text="Stop Freq (Hz)", **label_opts).grid(row=0, column=2, padx=5, pady=5)
        self.stop_freq = tk.Entry(param, **entry_opts)
        self.stop_freq.insert(0, "10000")
        self.stop_freq.grid(row=0, column=3, padx=5)

        tk.Label(param, text="Points", **label_opts).grid(row=0, column=4, padx=5, pady=5)
        self.points = tk.Entry(param, **entry_opts)
        self.points.insert(0, "100")
        self.points.grid(row=0, column=5, padx=5)

        tk.Label(param, text="Repeats", **label_opts).grid(row=0, column=6, padx=5, pady=5)
        self.repeats = tk.Entry(param, **entry_opts)
        self.repeats.insert(0, "1")
        self.repeats.grid(row=0, column=7, padx=5)

    def setup_plot(self):
        from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

        bg_color = "#FFF9C4"
        plot_frame = self.tk.Frame(self.root, bg=bg_color)
        plot_frame.pack(fill=self.tk.BOTH, expand=True, padx=10, pady=10)

        # Bode plot (trái)
        self.fig_bode, self.ax_bode = self.plt.subplots(figsize=(6, 5))
        self.fig_bode.patch.set_facecolor(bg_color)
        self.ax_bode.set_facecolor("#FFFFFF")
        self.ax_bode.set_title("Bode Plot", fontsize=18, fontweight="bold", color="#F9A825")
        self.ax_bode.set_xlabel("Frequency (Hz)", fontsize=15, color="#F9A825")
        self.ax_bode.set_ylabel("Magnitude (Ohm)", fontsize=15, color="#F9A825")
        self.ax_bode.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.ax_phase = self.ax_bode.twinx()
        self.ax_phase.set_ylabel("Phase (°)", fontsize=15, color="#D84315")
        self.ax_phase.tick_params(axis='y', labelcolor="#D84315")
        self.canvas_bode = FigureCanvasTkAgg(self.fig_bode, master=plot_frame)
        self.canvas_bode.get_tk_widget().pack(side=self.tk.LEFT, fill=self.tk.BOTH, expand=True, padx=10)

        self.fig_bode.tight_layout()
        self.canvas_bode.draw()
        for label in (self.ax_bode.get_xticklabels() + self.ax_bode.get_yticklabels() + self.ax_phase.get_yticklabels()):
            label.set_fontsize(16)
            label.set_fontweight('bold')
            label.set_color("#F9A825")

        # Nyquist plot (phải)
        self.fig_nyquist, self.ax_nyquist = self.plt.subplots(figsize=(6, 5))
        self.fig_nyquist.patch.set_facecolor(bg_color)
        self.ax_nyquist.set_facecolor("#FFFFFF")
        self.ax_nyquist.set_title("Nyquist Plot", fontsize=18, fontweight="bold", color="#388E3C")
        self.ax_nyquist.set_xlabel("Re(Z) (Ohm)", fontsize=15, color="#388E3C")
        self.ax_nyquist.set_ylabel("Im(Z) (Ohm)", fontsize=15, color="#388E3C")
        self.ax_nyquist.grid(True, linestyle='--', color='gray', alpha=0.5)
        self.canvas_nyquist = FigureCanvasTkAgg(self.fig_nyquist, master=plot_frame)
        self.canvas_nyquist.get_tk_widget().pack(side=self.tk.LEFT, fill=self.tk.BOTH, expand=True, padx=10)

        self.fig_nyquist.tight_layout()
        self.canvas_nyquist.draw()
        for label in (self.ax_nyquist.get_xticklabels() + self.ax_nyquist.get_yticklabels()):
            label.set_fontsize(16)
            label.set_fontweight('bold')
            label.set_color("#388E3C")

    def clear_all(self):
        self.freqs.clear()
        self.magnitudes.clear()
        self.phases.clear()
        self.reals.clear()
        self.imags.clear()
        self.receiver_count = 0

    def handle_serial_data(self, line):
        try:
            parts = line.strip().split(";")
            if len(parts) < 2:
                return False

            freq = float(parts[0])
            if parts[1].lower() == "inf":
                mag = 60000000.0
                phase = 0.0
            else:
                mag = float(parts[1])
                phase = float(parts[2]) if len(parts) > 2 else 0.0

            phase_rad = math.radians(phase)
            real = mag * math.cos(phase_rad)
            imag = -mag * math.sin(phase_rad)

            self.freqs.append(round(freq, 3))
            self.magnitudes.append(round(mag, 3))
            self.phases.append(round(phase, 3))
            self.reals.append(round(real, 3))
            self.imags.append(round(imag, 3))

            self.receiver_count += 1
            return True
        except Exception as e:
            print("Parse error:", e)
            return False

    def smooth_nyquist(self):
        if self.receiver_count < 1:
            return self.reals, self.imags

        real_smooth = []
        imag_smooth = []
        for i in range(self.receiver_count):
            if i == 0:
                r, im = self.reals[0], self.imags[0]
            elif i == 1:
                r = round((self.reals[0] + self.reals[1]) / 2, 3)
                im = round((self.imags[0] + self.imags[1]) / 2, 3)
            else:
                r = round((self.reals[i] + self.reals[i-1] + self.reals[i-2]) / 3, 3)
                im = round((self.imags[i] + self.imags[i-1] + self.imags[i-2]) / 3, 3)
            real_smooth.append(r)
            imag_smooth.append(im)
        return real_smooth, imag_smooth

    def smooth_bode(self):
        if self.receiver_count < 3:
            return self.magnitudes, self.phases
        mag = self.np.array(self.magnitudes)
        pha = self.np.array(self.phases)
        return self.uniform_filter1d(mag, size=3), self.uniform_filter1d(pha, size=3)

    def export_to_csv(self, filepath, sweep=True, log=False):
        df = pd.DataFrame({
            'Freq (Hz)': self.freqs,
            '|Z| (Ohm)': self.magnitudes,
            'Phase (°)': self.phases,
            'Re(Z) (Ohm)': self.reals,
            'Im(Z) (Ohm)': self.imags
        })
        df.to_csv(filepath, index=False)
        meta_path = filepath.replace(".csv", "_meta.csv")
        meta = pd.DataFrame({
            'Thông tin': [
                'Sweep Enable', 'Logarithmic', 'Start Frequency',
                'Stop Frequency', 'Sweep Points', 'Repeat Times'
            ],
            'Giá trị': [
                str(sweep), str(log), self.freqs[0] if self.freqs else '',
                self.freqs[-1] if self.freqs else '', len(self.freqs), 1
            ]
        })
        meta.to_csv(meta_path, index=False, header=False)
        
    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports:
            self.port_combo.set(ports[0])

    def connect_serial(self):
        port = self.port_combo.get()
        if port:
            try:
                self.serial_port = serial.Serial(port, 115200, timeout=1)
                messagebox.showinfo("Connected", f"Connected to {port}")
            except Exception as e:
                messagebox.showerror("Error", str(e))

    def start_reading(self):
        if not self.serial_port or not self.serial_port.is_open:
            messagebox.showwarning("Warning", "Please connect to the port.")
            return
        try:
            start = int(self.start_freq.get())
            stop = int(self.stop_freq.get())
            points = int(self.points.get())
            repeats = int(self.repeats.get())
            if start >= stop or points < 2:
                messagebox.showerror("Input Error", "Invalid frequency range or sweep points.")
                return
            self.clear_data()
            sweep_enabled, log_enabled = True, False
            if sweep_enabled:
                log_flag = '1' if log_enabled else '0'
                command = f"2#{start}?{stop}/{points}|{repeats}${log_flag}!"
            else:
                command = f"2#{start}?{start}/{points}|{repeats}$0!"
            self.serial_port.write(command.encode())
            print("Command sent:", command)
            for child in self.root.winfo_children():
                for btn in child.winfo_children():
                    if isinstance(btn, ttk.Button) and btn['text'] not in ['Stop']:
                        btn.config(state='disabled')
            self.running = True
            threading.Thread(target=self.read_serial, daemon=True).start()
        except Exception as e:
            messagebox.showerror("Error", f"Measurement error: {e}")

    def stop_reading(self):
        if self.serial_port and self.serial_port.is_open:
            try:
                self.serial_port.write(b's')
                print("Sent stop command")
            except:
                pass
        self.running = False
        for child in self.root.winfo_children():
            for btn in child.winfo_children():
                if isinstance(btn, ttk.Button):
                    btn.config(state='normal')

    def disconnect_serial(self):
        if self.serial_port and self.serial_port.is_open:
            self.running = False
            self.serial_port.close()
            messagebox.showinfo("Disconnected", "Serial port disconnected")

    def clear_data(self):
        self.clear_all()
        self.ax_bode.cla()
        self.ax_phase.cla()
        self.canvas_bode.draw()
        self.ax_nyquist.cla()
        self.canvas_nyquist.draw()

    def read_serial(self):
        while self.running:
            try:
                line = self.serial_port.readline().decode().strip()
                if self.handle_serial_data(line):
                    self.update_plots()
            except Exception as e:
                print("Serial error:", e)

    def update_plots(self):
        if self.receiver_count < 5:
            return
        mag_smooth, pha_smooth = self.smooth_bode()
        re_smooth, im_smooth = self.smooth_nyquist()
        freqs = self.freqs

        self.ax_bode.clear()
        self.ax_phase.clear()
        self.ax_bode.plot(freqs, mag_smooth, 'b-', label='Magnitude')
        self.ax_phase.plot(freqs, pha_smooth, 'r-', label='Phase')
        self.ax_bode.set_xlabel("Frequency (Hz)", fontsize=11)
        self.ax_bode.set_ylabel("Magnitude (Ω)", fontsize=11, color='b')
        self.ax_phase.set_ylabel("Phase (°)", fontsize=11, color='r')
        self.ax_bode.tick_params(axis='y', labelcolor='b')
        self.ax_phase.tick_params(axis='y', labelcolor='r')
        self.ax_bode.grid(True, linestyle='--', alpha=0.6)
        lines1, labels1 = self.ax_bode.get_legend_handles_labels()
        lines2, labels2 = self.ax_phase.get_legend_handles_labels()
        self.ax_bode.legend(lines1 + lines2, labels1 + labels2, loc='upper right')
        self.fig_bode.tight_layout()
        self.canvas_bode.draw()

        self.ax_nyquist.clear()
        self.ax_nyquist.plot(re_smooth, im_smooth, 'k-', linewidth=1)
        self.ax_nyquist.plot(re_smooth, im_smooth, 'ks', markersize=4)
        self.ax_nyquist.set_xlabel("Z' (Ω)", fontsize=11)
        self.ax_nyquist.set_ylabel("Z'' (Ω)", fontsize=11)
        self.ax_nyquist.grid(True, linestyle='--', alpha=0.6)
        self.fig_nyquist.tight_layout()
        self.canvas_nyquist.draw()

    def export_excel(self):
        file = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV files", "*.csv")])
        if file:
            self.export_to_csv(file)
            import os
            image_path_bode = os.path.splitext(file)[0] + "_bode_plot.png"
            image_path_nyquist = os.path.splitext(file)[0] + "_nyquist_plot.png"
            self.fig_bode.savefig(image_path_bode, dpi=300, bbox_inches='tight')
            self.fig_nyquist.savefig(image_path_nyquist, dpi=300, bbox_inches='tight')
            
            self.messagebox.showinfo("Exported", f"Dữ liệu và đồ thị đã xuất thành công!\nCSV: {file}\nBode: {image_path_bode}\nNyquist: {image_path_nyquist}")

    def import_excel(self):
        file = filedialog.askopenfilename(filetypes=[("Excel files", "*.xlsx")])
        if file:
            try:
                df_meta = pd.read_excel(file, sheet_name='EIS Data', nrows=6, header=None)
                df_data = pd.read_excel(file, sheet_name='EIS Data', skiprows=7)
                self.freqs = df_data['Freq'].tolist()
                self.magnitudes = df_data['|Z|'].tolist()
                self.phases = df_data['Phase'].tolist()
                self.reals = df_data['Re(Z)'].tolist()
                self.imags = df_data['Im(Z)'].tolist()
                self.receiver_count = len(self.freqs)
                self.start_freq.delete(0, tk.END)
                self.stop_freq.delete(0, tk.END)
                self.points.delete(0, tk.END)
                self.repeats.delete(0, tk.END)
                self.start_freq.insert(0, str(df_meta.iloc[2, 1]))
                self.stop_freq.insert(0, str(df_meta.iloc[3, 1]))
                self.points.insert(0, str(df_meta.iloc[4, 1]))
                self.repeats.insert(0, str(df_meta.iloc[5, 1]))
                self.update_plots()
            except Exception as e:
                messagebox.showerror("Import Error", f"Failed to import Excel file:\\n{e}")
      
root = tk.Tk()
root.title("Hệ thống đo điện hóa")

# Tự động điều chỉnh cửa sổ theo màn hình
screen_width = root.winfo_screenwidth()
screen_height = root.winfo_screenheight()

# Tính toán kích thước cửa sổ (90% màn hình để không bị che taskbar)
window_width = int(screen_width * 0.9)
window_height = int(screen_height * 0.9)

# Đặt vị trí cửa sổ ở giữa màn hình
x_position = (screen_width - window_width) // 2
y_position = (screen_height - window_height) // 2

root.geometry(f"{window_width}x{window_height}+{x_position}+{y_position}")

# Cho phép maximize và resize cửa sổ
root.state('zoomed')  # Tự động maximize trên Windows
root.resizable(True, True)  # Cho phép thay đổi kích thước

# ======= Tạo thanh menu chọn SWV hoặc EIS =======
nav_frame = ttk.Frame(root)
nav_frame.pack(fill="x", pady=5)

# Style chung cho tất cả các nút menu
button_style = ttk.Style()
button_style.configure(
    "Nav.TButton",
    font=("Segoe UI", 14, "bold"),
    padding=(10, 5),
    anchor="center"
)
button_style.map("Nav.TButton", background=[('active', '#B1F0C8')])

main_frame = ttk.Frame(root)
main_frame.pack(fill="both", expand=True)

frame_swv = ttk.Frame(main_frame)
frame_cv = ttk.Frame(main_frame)
frame_eis_3e = ttk.Frame(main_frame)
frame_ca = ttk.Frame(main_frame)
frame_eis_2e = ttk.Frame(main_frame)
frame_dpv = ttk.Frame(main_frame)
frame_lsv = ttk.Frame(main_frame)
frame_asv = ttk.Frame(main_frame)
for frame in (frame_swv, frame_cv, frame_eis_3e, frame_ca, frame_eis_2e, frame_dpv, frame_lsv, frame_asv):
    frame.place(in_=main_frame, x=0, y=0, relwidth=1, relheight=1)

nav_buttons = [
    ("SWV", frame_swv),
    ("CV", frame_cv),
    ("EIS_3E", frame_eis_3e),
    ("CA", frame_ca),
    ("EIS_2E", frame_eis_2e),
    ("DPV", frame_dpv),
    ("LSV", frame_lsv),
    ("ASV", frame_asv)
]

def show_frame(f):
    f.tkraise()

for i, (label, frame) in enumerate(nav_buttons):
    btn = ttk.Button(
        nav_frame,
        text=label,
        command=lambda f=frame: show_frame(f),
        style="Nav.TButton"
    )
    btn.grid(row=0, column=i, sticky="nsew", padx=8, pady=2, ipady=10)

for i in range(len(nav_buttons)):
    nav_frame.columnconfigure(i, weight=1)

# ==== TẠO GIAO DIỆN SWV  ====
swv_app = SWVApp(frame_swv)

# ==== TẠO GIAO DIỆN CV  ====
cv_app = CVApp(frame_cv)

# ==== TẠO GIAO DIỆN EIS_3E  ====
eis_3e_app = EISApp(frame_eis_3e)

# ==== TẠO GIAO DIỆN CA  ====
ca_app = ChronoAmperometryApp(frame_ca)

# ==== TẠO GIAO DIỆN EIS_2E  ====
eis_2e_app = EIS2EApp(frame_eis_2e)

# ==== TẠO GIAO DIỆN DPV  ====
dpv_app = DPVApp(frame_dpv)

# ==== TẠO GIAO DIỆN LSV  ====
lsv_app = LSVApp(frame_lsv)

# ==== TẠO GIAO DIỆN ASV  ====
asv_app = ASVApp(frame_asv)

# Khởi đầu với giao diện SWV
show_frame(frame_swv)
root.mainloop()