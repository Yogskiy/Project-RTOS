import sys
import time
import random
from datetime import datetime
from PyQt6.QtWidgets import *
from PyQt6.QtCore import *
from queue import Queue
import threading

# ================= KEY =================
def generate_key():
    return f"{random.randint(0, 255):02X}"

def next_key(current_key):
    value = int(current_key, 16)
    value = (value * 7 + 3) % 256
    return f"{value:02X}"

# ================= DATABASE =================
# identifier -> current expected hex key
backend_state = {}

# ================= RTOS SIM =================
rfid_event = threading.Event()
auth_queue = Queue()
log_queue = Queue()

pending_action = {"type": None, "uid": None}

# ================= UTIL =================
def clear_queue(q):
    while not q.empty():
        try:
            q.get_nowait()
        except:
            break

# ================= THREAD =================
class TaskThread(QThread):
    update_signal = pyqtSignal(str, dict)

    def __init__(self, func):
        super().__init__()
        self.func = func

    def run(self):
        self.func(self.update_signal)

# ================= TASKS =================

def input_task(signal):
    while True:
        rfid_event.wait()
        rfid_event.clear()

        action = pending_action.copy()
        pending_action["type"] = None
        pending_action["uid"] = None

        if not action["uid"]:
            continue

        signal.emit("FLOW", {"stage": "INPUT"})
        signal.emit("INPUT", {"uid": action["uid"], "action": action["type"]})

        auth_queue.put(action)
        time.sleep(0.2)


def auth_task(signal):
    while True:
        action = auth_queue.get()
        uid = action["uid"]
        action_type = action["type"]

        signal.emit("FLOW", {"stage": "AUTH"})

        # ===== REGISTER =====
        # uid is now the full string: all-but-last-2 = identifier, last 2 = key
        if action_type == "register":
            if len(uid) < 3:
                data = {
                    "uid": uid, "user": "?",
                    "status": "INVALID ❌", "extra": "UID TOO SHORT",
                    "time": datetime.now().strftime("%H:%M:%S"),
                    "action": action_type
                }
                log_queue.put(data)
                signal.emit("AUTH", data)
                time.sleep(0.2)
                continue

            identifier = uid[:-2]
            key = uid[-2:].upper()

            if identifier in backend_state:
                status = "ALREADY REGISTERED ⚠"
                extra = f"CURRENT KEY → {backend_state[identifier]}"
                full_uid = identifier + backend_state[identifier]
            else:
                # Use the provided key instead of generating a random one
                backend_state[identifier] = key
                status = "REGISTERED ✅"
                full_uid = identifier + key
                extra = f"CARD UID → {full_uid}  (use this to scan)"

            data = {
                "uid": full_uid,
                "user": identifier,
                "status": status,
                "extra": extra,
                "time": datetime.now().strftime("%H:%M:%S"),
                "action": action_type
            }

        # ===== SCAN =====
        # uid is full UID: all-but-last-2 = identifier, last 2 = key
        elif action_type == "scan":
            if len(uid) < 3:
                data = {
                    "uid": uid, "user": "?",
                    "status": "INVALID ❌", "extra": "UID TOO SHORT",
                    "time": datetime.now().strftime("%H:%M:%S"),
                    "action": action_type
                }
                log_queue.put(data)
                signal.emit("AUTH", data)
                time.sleep(0.2)
                continue

            identifier = uid[:-2]
            key = uid[-2:].upper()

            if identifier not in backend_state:
                status = "UNKNOWN ❌"
                extra = "NOT REGISTERED — register this ID first"
            else:
                expected = backend_state[identifier]
                if key == expected:
                    new_key = next_key(expected)
                    backend_state[identifier] = new_key
                    status = "GRANTED ✔"
                    extra = f"NEXT UID → {identifier}{new_key}"
                else:
                    status = "CLONE / INVALID ❌"
                    extra = f"EXPECTED KEY → {expected}  |  GOT → {key}"

            data = {
                "uid": uid,
                "user": identifier,
                "status": status,
                "extra": extra,
                "time": datetime.now().strftime("%H:%M:%S"),
                "action": action_type
            }

        else:
            data = {
                "uid": uid, "user": "?",
                "status": "UNKNOWN ACTION ❌", "extra": "",
                "time": datetime.now().strftime("%H:%M:%S"),
                "action": action_type
            }

        log_queue.put(data)
        signal.emit("AUTH", data)
        time.sleep(0.2)


def comm_task(signal):
    while True:
        data = log_queue.get()
        signal.emit("FLOW", {"stage": "COMM"})
        signal.emit("COMM", data)
        signal.emit("DONE", {})
        time.sleep(0.2)


def security_task(signal):
    while True:
        signal.emit("SECURITY", {"cpu": random.randint(10, 90)})
        time.sleep(2)

# ================= VALIDATORS =================

class HexLineEdit(QLineEdit):
    """QLineEdit that only accepts hex characters (0-9, A-F), auto-uppercases."""
    def __init__(self, max_chars, placeholder=""):
        super().__init__()
        self.max_chars = max_chars
        self.setPlaceholderText(placeholder)
        self.setMaxLength(max_chars)
        self.textChanged.connect(self._enforce_hex)

    def _enforce_hex(self, text):
        clean = ''.join(c for c in text.upper() if c in "0123456789ABCDEF")
        clean = clean[:self.max_chars]
        if clean != text:
            self.blockSignals(True)
            self.setText(clean)
            self.blockSignals(False)

# ================= UI =================

class RTOSWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Secure RTOS RFID Simulator")
        self.setGeometry(100, 100, 1000, 720)
        self.setStyleSheet("""
            QWidget        { background:#121212; color:white; font-family:monospace; }
            QLineEdit      { background:#1e1e1e; border:1px solid #444; padding:6px;
                             color:white; font-size:14px; border-radius:3px; }
            QPushButton    { padding:8px 18px; font-size:13px; border:none; border-radius:4px; color:white; }
            QTextEdit      { background:#1a1a1a; border:1px solid #333; color:#ccc; font-size:12px; }
            QProgressBar   { background:#1e1e1e; border:1px solid #444; height:14px; border-radius:3px; }
            QProgressBar::chunk { background:#00c853; border-radius:3px; }
            QGroupBox      { border:1px solid #333; margin-top:10px; padding:8px; color:#888; }
            QGroupBox::title { color:#888; subcontrol-origin:margin; padding:0 4px; }
        """)

        root = QVBoxLayout()
        root.setSpacing(8)

        # ── REGISTER ──────────────────────────────────────
        reg_group = QGroupBox("Register New Card  (enter full UID = ID + initial 2-char key)")
        reg_row = QHBoxLayout()
        # Max chars raised from 8 to 10 to fit the full UID string
        self.reg_input = HexLineEdit(8, "Full UID, e.g.  C8F0DD67  (ID + 2-char key)")
        reg_row.addWidget(self.reg_input)
        self.reg_btn = QPushButton("📋 Register")
        self.reg_btn.setStyleSheet("background:#1565c0;")
        self.reg_btn.clicked.connect(self.do_register)
        reg_row.addWidget(self.reg_btn)
        reg_group.setLayout(reg_row)
        root.addWidget(reg_group)

        # ── SCAN ──────────────────────────────────────────
        scan_group = QGroupBox("Scan Card  (enter full UID = ID + rolling key)")
        scan_row = QHBoxLayout()
        self.scan_input = HexLineEdit(8, "Full UID, e.g.  E8A4C2F1  (ID + 2-char key)")
        scan_row.addWidget(self.scan_input)
        self.scan_btn = QPushButton("🔍 Scan")
        self.scan_btn.setStyleSheet("background:#00695c;")
        self.scan_btn.clicked.connect(self.do_scan)
        scan_row.addWidget(self.scan_btn)
        scan_group.setLayout(scan_row)
        root.addWidget(scan_group)

        # ── FLOW ──────────────────────────────────────────
        self.flow_lbl = QLabel("FLOW: IDLE")
        self.flow_lbl.setStyleSheet("color:#90caf9; font-size:11px;")
        root.addWidget(self.flow_lbl)

        # ── LAST RESULT ───────────────────────────────────
        res_group = QGroupBox("Last Result")
        res_grid = QGridLayout()
        self.uid_lbl   = QLabel("UID: -")
        self.user_lbl  = QLabel("ID: -")
        self.stat_lbl  = QLabel("Status: READY")
        self.extra_lbl = QLabel("Info: -")
        self.extra_lbl.setStyleSheet("color:#80cbc4;")
        self.led = QLabel("●")
        self.led.setStyleSheet("font-size:40px; color:gray;")
        res_grid.addWidget(self.uid_lbl,   0, 0)
        res_grid.addWidget(self.user_lbl,  1, 0)
        res_grid.addWidget(self.stat_lbl,  2, 0)
        res_grid.addWidget(self.extra_lbl, 3, 0)
        res_grid.addWidget(self.led,       0, 1, 4, 1, Qt.AlignmentFlag.AlignCenter)
        res_group.setLayout(res_grid)
        root.addWidget(res_group)

        # ── LIVE DB ───────────────────────────────────────
        db_group = QGroupBox("Live Backend DB  (ID → current expected key)")
        db_lay = QVBoxLayout()
        self.db_view = QTextEdit()
        self.db_view.setReadOnly(True)
        self.db_view.setMaximumHeight(75)
        self.db_view.setStyleSheet("color:#80cbc4; background:#0d1f1d;")
        db_lay.addWidget(self.db_view)
        db_group.setLayout(db_lay)
        root.addWidget(db_group)

        # ── CPU ───────────────────────────────────────────
        cpu_row = QHBoxLayout()
        cpu_row.addWidget(QLabel("CPU:"))
        self.cpu = QProgressBar()
        cpu_row.addWidget(self.cpu)
        root.addLayout(cpu_row)

        # ── LOG ───────────────────────────────────────────
        log_group = QGroupBox("Event Log")
        log_lay = QVBoxLayout()
        self.log = QTextEdit()
        self.log.setReadOnly(True)
        log_lay.addWidget(self.log)
        log_group.setLayout(log_lay)
        root.addWidget(log_group)

        self.setLayout(root)
        self.refresh_db()
        self.start_tasks()

    # ── HELPERS ───────────────────────────────────────────
    def refresh_db(self):
        if backend_state:
            lines = [f"  {i}  →  {k}" for i, k in backend_state.items()]
            self.db_view.setText("\n".join(lines))
        else:
            self.db_view.setText("  (empty)")

    def reset_display(self):
        self.flow_lbl.setText("FLOW: IDLE")
        self.led.setStyleSheet("font-size:40px; color:gray;")

    # ── ACTIONS ───────────────────────────────────────────
    def do_register(self):
        uid = self.reg_input.text().strip()
        if len(uid) < 8:
            QMessageBox.warning(self, "Too short", "Full UID must be at least 8 characters (RFID Full UID).")
            return
        clear_queue(auth_queue)
        clear_queue(log_queue)
        pending_action["type"] = "register"
        pending_action["uid"] = uid
        self.log.append(f'<span style="color:#90caf9">>>> REGISTER UID: {uid} '
                        f'(ID={uid[:-2]}, KEY={uid[-2:]})</span>')
        rfid_event.set()

    def do_scan(self):
        uid = self.scan_input.text().strip()
        if len(uid) < 8:
            QMessageBox.warning(self, "Too short", "Full UID must be at least 8 characters (RFID Full UID).")
            return
        clear_queue(auth_queue)
        clear_queue(log_queue)
        pending_action["type"] = "scan"
        pending_action["uid"] = uid
        self.log.append(f'<span style="color:#90caf9">>>> SCAN UID: {uid}  '
                        f'(ID={uid[:-2]}, KEY={uid[-2:]})</span>')
        rfid_event.set()

    # ── UPDATE UI ─────────────────────────────────────────
    def update_ui(self, source, data):
        if source == "FLOW":
            self.flow_lbl.setText(f"FLOW: {data['stage']}")

        elif source == "INPUT":
            self.uid_lbl.setText(f"UID: {data['uid']}  [{data['action'].upper()}]")

        elif source == "AUTH":
            self.user_lbl.setText(f"ID: {data['user']}")
            self.stat_lbl.setText(f"Status: {data['status']}")
            self.extra_lbl.setText(f"Info: {data['extra']}")
            if "GRANTED" in data["status"] or "REGISTERED ✅" in data["status"]:
                self.led.setStyleSheet("font-size:40px; color:#00e676;")
            elif "⚠" in data["status"]:
                self.led.setStyleSheet("font-size:40px; color:#ffd740;")
            else:
                self.led.setStyleSheet("font-size:40px; color:#ff1744;")
            self.refresh_db()

        elif source == "COMM":
            colors = {
                "GRANTED ✔":            "#00e676",
                "REGISTERED ✅":        "#40c4ff",
                "CLONE / INVALID ❌":   "#ff5252",
                "UNKNOWN ❌":           "#ff5252",
                "ALREADY REGISTERED ⚠":"#ffd740",
                "INVALID ❌":           "#ff5252",
            }
            c = colors.get(data["status"], "#ccc")
            tag = data.get("action", "scan").upper()
            self.log.append(
                f'<span style="color:{c}">'
                f'{data["time"]} | [{tag}] {data["uid"]} | {data["status"]} | {data["extra"]}'
                f'</span>'
            )

        elif source == "SECURITY":
            self.cpu.setValue(data["cpu"])

        elif source == "DONE":
            QTimer.singleShot(1200, self.reset_display)

    def start_tasks(self):
        self.threads = []
        for func in [input_task, auth_task, comm_task, security_task]:
            t = TaskThread(func)
            t.update_signal.connect(self.update_ui)
            t.start()
            self.threads.append(t)

# ================= MAIN =================
if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = RTOSWindow()
    window.show()
    sys.exit(app.exec())