
from __future__ import annotations

import logging
import queue
import threading
import time

from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import cv2
import face_recognition
import numpy as np
import serial
import serial.tools.list_ports

import tkinter as tk
from tkinter import ttk, messagebox


# ============================================================
# APPLICATION CONFIGURATION
# ============================================================

@dataclass(frozen=True)
class Config:
    APP_NAME: str = "SALIM DEV | Face Recognition System"
    VERSION: str = "2.0.0"

    DATASET_DIR: Path = Path("dataset")
    DEFAULT_PERSON: str = "know"

    CAMERA_INDEX: int = 0
    CAMERA_WIDTH: int = 640
    CAMERA_HEIGHT: int = 480

    SERIAL_BAUDRATE: int = 9600
    SERIAL_TIMEOUT: float = 1.0

    FACE_TOLERANCE: float = 0.6
    FACE_MODEL: str = "hog"

    HAAR_CASCADE: str = "haarcascade_frontalface_default.xml"

    CAMERA_WINDOW: str = "SALIM DEV | Face Recognition"

    @property
    def cascade_path(self) -> Path:
        return Path(self.HAAR_CASCADE)


CONFIG = Config()


# ============================================================
# LOGGING CONFIGURATION
# ============================================================

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s | %(levelname)-8s | %(name)s | %(message)s",
    datefmt="%H:%M:%S"
)

logger = logging.getLogger("SalimDev.FaceRecognition")


# ============================================================
# FACE DATASET MANAGER
# ============================================================

class FaceDataset:

    def __init__(self, root: Path):
        self.root = root
        self.root.mkdir(parents=True, exist_ok=True)

    def person_directory(self, name: str) -> Path:

        name = name.strip()

        if not name or name in {".", ".."}:
            raise ValueError("Invalid person name.")

        if Path(name).name != name or "/" in name or "\\" in name:
            raise ValueError("Person name cannot contain path separators.")

        directory = self.root / name
        directory.mkdir(parents=True, exist_ok=True)

        return directory

    def save_face(self, name: str, image: np.ndarray) -> Path:

        directory = self.person_directory(name)

        index = len(list(directory.glob("*.jpg"))) + 1

        while True:
            image_path = directory / f"face_{index:05d}.jpg"

            if not image_path.exists():
                break

            index += 1

        if not cv2.imwrite(str(image_path), image):
            raise OSError(f"Could not save image: {image_path}")

        return image_path

    def load_faces(self):

        encodings = []
        names = []

        image_paths = sorted(
            self.root.glob("*/*.jpg")
        )

        if not image_paths:
            raise FileNotFoundError(
                f"No registered images found in {self.root.resolve()}"
            )

        for image_path in image_paths:

            try:
                image = face_recognition.load_image_file(
                    str(image_path)
                )

                face_encodings = face_recognition.face_encodings(
                    image,
                    num_jitters=1,
                    model="small"
                )

                if not face_encodings:
                    logger.warning(
                        "No face detected in %s",
                        image_path
                    )
                    continue

                encodings.append(face_encodings[0])
                names.append(image_path.parent.name)

            except Exception:
                logger.exception(
                    "Failed to process image: %s",
                    image_path
                )

        if not encodings:
            raise ValueError(
                "No valid face encodings could be generated."
            )

        logger.info(
            "Loaded %d face samples belonging to %d people.",
            len(encodings),
            len(set(names))
        )

        return encodings, names


# ============================================================
# FACE RECOGNITION ENGINE
# ============================================================

class FaceRecognitionEngine:

    def __init__(self, dataset: FaceDataset):

        self.dataset = dataset
        self.known_encodings = []
        self.known_names = []

    def train(self):

        logger.info("Loading registered faces...")

        self.known_encodings, self.known_names = (
            self.dataset.load_faces()
        )

        logger.info("Face database successfully loaded.")

        return len(self.known_encodings)

    def recognize(self, frame: np.ndarray):

        rgb_frame = cv2.cvtColor(
            frame,
            cv2.COLOR_BGR2RGB
        )

        locations = face_recognition.face_locations(
            rgb_frame,
            model=CONFIG.FACE_MODEL
        )

        if not locations:
            return []

        encodings = face_recognition.face_encodings(
            rgb_frame,
            locations
        )

        results = []

        for location, encoding in zip(locations, encodings):

            distances = face_recognition.face_distance(
                self.known_encodings,
                encoding
            )

            matches = face_recognition.compare_faces(
                self.known_encodings,
                encoding,
                tolerance=CONFIG.FACE_TOLERANCE
            )

            matched_indices = np.flatnonzero(matches)

            name = "UNKNOWN"

            if len(matched_indices):

                best_index = min(
                    matched_indices,
                    key=lambda index: distances[index]
                )

                name = self.known_names[best_index]

            results.append((location, name))

        return results


# ============================================================
# SERIAL COMMUNICATION MANAGER
# ============================================================

class SerialController:

    def __init__(self):

        self.connection: Optional[serial.Serial] = None
        self.last_command = None

    @staticmethod
    def available_ports():

        return list(
            serial.tools.list_ports.comports()
        )

    def connect(self, port: Optional[str]):

        if not port:
            logger.warning(
                "No serial port selected. Recognition will run without hardware control."
            )
            return False

        self.disconnect()

        try:

            self.connection = serial.Serial(
                port=port,
                baudrate=CONFIG.SERIAL_BAUDRATE,
                timeout=CONFIG.SERIAL_TIMEOUT,
                write_timeout=CONFIG.SERIAL_TIMEOUT
            )

            time.sleep(2)

            self.connection.reset_input_buffer()

            logger.info(
                "Connected to serial port %s",
                port
            )

            self.send("OFF", force=True)

            return True

        except serial.SerialException:

            logger.exception(
                "Serial connection failed."
            )

            self.connection = None
            return False

    def send(self, command: str, force: bool = False):

        if not self.connection:
            return

        if not self.connection.is_open:
            return

        if command == self.last_command and not force:
            return

        try:

            self.connection.write(
                f"{command}\r".encode("ascii")
            )

            self.connection.flush()

            self.last_command = command

            logger.info(
                "Serial command transmitted: %s",
                command
            )

        except serial.SerialException:

            logger.exception(
                "Serial transmission failed."
            )

            self.disconnect()

    def disconnect(self):

        if self.connection:

            try:
                if self.connection.is_open:
                    self.send("OFF", force=True)
                    self.connection.close()

            except (serial.SerialException, OSError):
                logger.exception(
                    "Error while closing serial connection."
                )

            finally:
                self.connection = None
                self.last_command = None


# ============================================================
# CAMERA MANAGER
# ============================================================

class CameraManager:

    def __init__(self, camera_index: int):

        self.camera_index = camera_index
        self.capture = None

    def open(self):

        self.capture = cv2.VideoCapture(
            self.camera_index
        )

        if not self.capture.isOpened():

            self.release()

            raise RuntimeError(
                "Unable to access the camera."
            )

        self.capture.set(
            cv2.CAP_PROP_FRAME_WIDTH,
            CONFIG.CAMERA_WIDTH
        )

        self.capture.set(
            cv2.CAP_PROP_FRAME_HEIGHT,
            CONFIG.CAMERA_HEIGHT
        )

        logger.info("Camera initialized.")

    def read(self):

        if self.capture is None:
            return False, None

        return self.capture.read()

    def release(self):

        if self.capture is not None:

            self.capture.release()
            self.capture = None

            logger.info("Camera released.")


# ============================================================
# FACE REGISTRATION SERVICE
# ============================================================

class FaceRegistration:

    def __init__(self, dataset: FaceDataset):

        self.dataset = dataset

        cascade_path = str(CONFIG.cascade_path)

        self.detector = cv2.CascadeClassifier(
            cascade_path
        )

        if self.detector.empty():

            raise FileNotFoundError(
                f"Cannot load Haar Cascade: {cascade_path}"
            )

    def capture_faces(
        self,
        person_name: str,
        number_of_images: int,
        stop_event: threading.Event,
        notify
    ):

        camera = CameraManager(
            CONFIG.CAMERA_INDEX
        )

        saved_count = 0

        try:

            camera.open()

            while (
                saved_count < number_of_images
                and not stop_event.is_set()
            ):

                ret, frame = camera.read()

                if not ret:
                    raise RuntimeError(
                        "Failed to read camera frame."
                    )

                gray = cv2.cvtColor(
                    frame,
                    cv2.COLOR_BGR2GRAY
                )

                faces = self.detector.detectMultiScale(
                    gray,
                    scaleFactor=1.3,
                    minNeighbors=5,
                    minSize=(40, 40)
                )

                for (x, y, w, h) in faces:

                    if saved_count >= number_of_images:
                        break

                    face_image = frame[
                        y:y + h,
                        x:x + w
                    ]

                    if face_image.size == 0:
                        continue

                    path = self.dataset.save_face(
                        person_name,
                        face_image
                    )

                    saved_count += 1

                    notify(
                        "progress",
                        saved_count,
                        number_of_images
                    )

                    logger.info(
                        "Face image saved: %s",
                        path
                    )

                    cv2.rectangle(
                        frame,
                        (x, y),
                        (x + w, y + h),
                        (0, 255, 0),
                        2
                    )

                cv2.putText(
                    frame,
                    f"Captured: {saved_count}/{number_of_images}",
                    (15, 35),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.8,
                    (0, 255, 0),
                    2
                )

                cv2.imshow(
                    "SALIM DEV | Face Registration",
                    frame
                )

                if cv2.waitKey(1) & 0xFF == ord("q"):
                    stop_event.set()
                    break

            notify(
                "registration_finished",
                saved_count
            )

        except Exception as error:

            logger.exception(
                "Face registration failed."
            )

            notify("error", str(error))

        finally:

            camera.release()

            cv2.destroyAllWindows()


# ============================================================
# MAIN APPLICATION
# ============================================================

class FaceRecognitionApp:

    def __init__(self, root: tk.Tk):

        self.root = root

        self.root.title(CONFIG.APP_NAME)

        self.root.geometry("900x600")
        self.root.minsize(800, 550)

        self.root.configure(
            bg="#101820"
        )

        self.dataset = FaceDataset(
            CONFIG.DATASET_DIR
        )

        self.engine = FaceRecognitionEngine(
            self.dataset
        )

        self.serial = SerialController()

        self.registration = FaceRegistration(
            self.dataset
        )

        self.events = queue.Queue()

        self.worker = None

        self.stop_event = threading.Event()

        self.running = False

        self._configure_style()
        self._build_interface()
        self.refresh_ports()

        self.root.protocol(
            "WM_DELETE_WINDOW",
            self.close_application
        )

        self.root.after(
            100,
            self._process_events
        )

        logger.info(
            "%s v%s initialized.",
            CONFIG.APP_NAME,
            CONFIG.VERSION
        )

    # --------------------------------------------------------
    # GUI STYLING
    # --------------------------------------------------------

    def _configure_style(self):

        style = ttk.Style()

        style.theme_use("clam")

        style.configure(
            "TFrame",
            background="#101820"
        )

        style.configure(
            "TLabel",
            background="#101820",
            foreground="#F0F4F8",
            font=("Segoe UI", 11)
        )

        style.configure(
            "Title.TLabel",
            font=("Segoe UI", 23, "bold"),
            foreground="#00D6A3"
        )

        style.configure(
            "Subtitle.TLabel",
            font=("Segoe UI", 10),
            foreground="#A7B4C2"
        )

        style.configure(
            "TButton",
            font=("Segoe UI", 11, "bold"),
            padding=10
        )

        style.map(
            "TButton",
            background=[
                ("active", "#00A982"),
                ("!disabled", "#007F65")
            ],
            foreground=[
                ("!disabled", "white")
            ]
        )

        style.configure(
            "TCombobox",
            padding=7
        )

        style.configure(
            "TProgressbar",
            thickness=10,
            background="#00D6A3"
        )

    # --------------------------------------------------------
    # GUI CONSTRUCTION
    # --------------------------------------------------------

    def _build_interface(self):

        main = ttk.Frame(
            self.root,
            padding=30
        )

        main.pack(
            fill="both",
            expand=True
        )

        ttk.Label(
            main,
            text="SALIM DEV",
            style="Title.TLabel"
        ).pack(anchor="w")

        ttk.Label(
            main,
            text="INTELLIGENT FACE RECOGNITION & ACCESS CONTROL SYSTEM",
            style="Subtitle.TLabel"
        ).pack(
            anchor="w",
            pady=(5, 25)
        )

        ttk.Separator(
            main
        ).pack(fill="x", pady=10)

        # Registration section

        registration_frame = ttk.LabelFrame(
            main,
            text="  FACE REGISTRATION  ",
            padding=20
        )

        registration_frame.pack(
            fill="x",
            pady=12
        )

        ttk.Label(
            registration_frame,
            text="Person name:"
        ).grid(row=0, column=0, sticky="w")

        self.person_name = tk.StringVar(
            value=CONFIG.DEFAULT_PERSON
        )

        ttk.Entry(
            registration_frame,
            textvariable=self.person_name,
            width=25
        ).grid(
            row=0,
            column=1,
            padx=12,
            sticky="w"
        )

        ttk.Label(
            registration_frame,
            text="Number of images:"
        ).grid(
            row=1,
            column=0,
            sticky="w",
            pady=12
        )

        self.image_count = tk.StringVar(
            value="100"
        )

        ttk.Combobox(
            registration_frame,
            textvariable=self.image_count,
            values=[
                "100", "200", "300", "400",
                "500", "1000", "2000", "3000",
                "5000", "7000", "10000"
            ],
            state="readonly",
            width=22
        ).grid(
            row=1,
            column=1,
            padx=12,
            sticky="w"
        )

        self.register_button = ttk.Button(
            registration_frame,
            text="REGISTER FACES",
            command=self.start_registration
        )

        self.register_button.grid(
            row=0,
            column=2,
            rowspan=2,
            padx=25
        )

        registration_frame.columnconfigure(
            3,
            weight=1
        )

        # Recognition section

        recognition_frame = ttk.LabelFrame(
            main,
            text="  FACE RECOGNITION  ",
            padding=20
        )

        recognition_frame.pack(
            fill="x",
            pady=12
        )

        ttk.Label(
            recognition_frame,
            text="Serial port:"
        ).grid(
            row=0,
            column=0,
            sticky="w"
        )

        self.port_var = tk.StringVar()

        self.port_combo = ttk.Combobox(
            recognition_frame,
            textvariable=self.port_var,
            state="readonly",
            width=25
        )

        self.port_combo.grid(
            row=0,
            column=1,
            padx=12
        )

        ttk.Button(
            recognition_frame,
            text="REFRESH PORTS",
            command=self.refresh_ports
        ).grid(
            row=0,
            column=2,
            padx=10
        )

        self.start_button = ttk.Button(
            recognition_frame,
            text="START RECOGNITION",
            command=self.start_recognition
        )

        self.start_button.grid(
            row=1,
            column=1,
            pady=20,
            sticky="ew"
        )

        self.stop_button = ttk.Button(
            recognition_frame,
            text="STOP",
            command=self.stop_operation,
            state="disabled"
        )

        self.stop_button.grid(
            row=1,
            column=2,
            pady=20,
            padx=10,
            sticky="ew"
        )

        # Status section

        status_frame = ttk.LabelFrame(
            main,
            text="  SYSTEM STATUS  ",
            padding=20
        )

        status_frame.pack(
            fill="both",
            expand=True,
            pady=12
        )

        self.status_var = tk.StringVar(
            value="System ready."
        )

        ttk.Label(
            status_frame,
            textvariable=self.status_var,
            font=("Segoe UI", 12, "bold")
        ).pack(anchor="w")

        self.progress = ttk.Progressbar(
            status_frame,
            mode="determinate",
            maximum=100
        )

        self.progress.pack(
            fill="x",
            pady=15
        )

        self.log_box = tk.Text(
            status_frame,
            height=5,
            bg="#17232E",
            fg="#00D6A3",
            insertbackground="white",
            font=("Consolas", 9),
            relief="flat",
            state="disabled"
        )

        self.log_box.pack(
            fill="both",
            expand=True
        )

        footer = ttk.Label(
            main,
            text=f"Version {CONFIG.VERSION} | Developed by SALIM DEV",
            style="Subtitle.TLabel"
        )

        footer.pack(
            anchor="e",
            pady=10
        )

    # --------------------------------------------------------
    # EVENT HANDLING
    # --------------------------------------------------------

    def notify(self, *event):

        self.events.put(event)

    def _process_events(self):

        try:

            while True:

                event = self.events.get_nowait()

                event_type = event[0]

                if event_type == "status":

                    self.status_var.set(event[1])

                elif event_type == "log":

                    self._write_log(event[1])

                elif event_type == "progress":

                    current, total = event[1], event[2]

                    self.progress["value"] = (
                        current / total * 100
                    )

                    self.status_var.set(
                        f"Registering faces: {current}/{total}"
                    )

                elif event_type == "registration_finished":

                    self._finish_operation()

                    self.status_var.set(
                        f"Registration completed: {event[1]} images saved."
                    )

                    messagebox.showinfo(
                        "Registration",
                        f"{event[1]} images saved successfully."
                    )

                elif event_type == "finished":

                    self._finish_operation()

                    self.status_var.set(
                        "Recognition stopped."
                    )

                elif event_type == "error":

                    self._finish_operation()

                    self.status_var.set(
                        "An error occurred."
                    )

                    messagebox.showerror(
                        "System Error",
                        event[1]
                    )

        except queue.Empty:
            pass

        self.root.after(
            100,
            self._process_events
        )

    def _write_log(self, message):

        self.log_box.configure(
            state="normal"
        )

        self.log_box.insert(
            "end",
            f"[{time.strftime('%H:%M:%S')}] {message}\n"
        )

        self.log_box.see("end")

        self.log_box.configure(
            state="disabled"
        )

    # --------------------------------------------------------
    # PORT MANAGEMENT
    # --------------------------------------------------------

    def refresh_ports(self):

        ports = SerialController.available_ports()

        port_names = [
            port.device for port in ports
        ]

        self.port_combo["values"] = port_names

        if port_names:

            if self.port_var.get() not in port_names:
                self.port_var.set(port_names[0])

        else:

            self.port_var.set("")

        self._write_log(
            f"{len(port_names)} serial port(s) detected."
        )

    # --------------------------------------------------------
    # THREAD MANAGEMENT
    # --------------------------------------------------------

    def _start_worker(self, target):

        if self.running:
            return

        self.running = True

        self.stop_event.clear()

        self.register_button.configure(
            state="disabled"
        )

        self.start_button.configure(
            state="disabled"
        )

        self.stop_button.configure(
            state="normal"
        )

        self.progress["value"] = 0

        self.worker = threading.Thread(
            target=target,
            daemon=True
        )

        self.worker.start()

    def _finish_operation(self):

        self.running = False

        self.register_button.configure(
            state="normal"
        )

        self.start_button.configure(
            state="normal"
        )

        self.stop_button.configure(
            state="disabled"
        )

    # --------------------------------------------------------
    # FACE REGISTRATION
    # --------------------------------------------------------

    def start_registration(self):

        if self.running:
            return

        name = self.person_name.get().strip()

        if not name:

            messagebox.showwarning(
                "Validation",
                "Please enter a person name."
            )

            return

        try:

            count = int(
                self.image_count.get()
            )

            if count <= 0:
                raise ValueError

            self.dataset.person_directory(name)

        except ValueError:

            messagebox.showerror(
                "Validation",
                "Invalid image count or person name."
            )

            return

        self.status_var.set(
            "Starting face registration..."
        )

        self._start_worker(
            lambda: self.registration.capture_faces(
                name,
                count,
                self.stop_event,
                self.notify
            )
        )

    # --------------------------------------------------------
    # FACE RECOGNITION
    # --------------------------------------------------------

    def start_recognition(self):

        if self.running:
            return

        selected_port = self.port_var.get().strip()

        self.status_var.set(
            "Initializing recognition engine..."
        )

        self._start_worker(
            lambda: self._recognition_worker(
                selected_port
            )
        )

    def _recognition_worker(self, port):

        camera = CameraManager(
            CONFIG.CAMERA_INDEX
        )

        try:

            # Load registered face encodings.

            number_of_faces = self.engine.train()

            self.notify(
                "log",
                f"{number_of_faces} face samples loaded."
            )

            # Establish serial communication.

            connected = self.serial.connect(
                port or None
            )

            if connected:

                self.notify(
                    "log",
                    f"Serial connected: {port}"
                )

            else:

                self.notify(
                    "log",
                    "Running without serial hardware."
                )

            # Initialize camera.

            camera.open()

            self.notify(
                "status",
                "Face recognition is running..."
            )

            self.notify(
                "log",
                "Camera started successfully."
            )

            previous_command = None

            while not self.stop_event.is_set():

                ret, frame = camera.read()

                if not ret:
                    raise RuntimeError(
                        "Camera frame acquisition failed."
                    )

                faces = self.engine.recognize(
                    frame
                )

                command = "OFF"

                if len(faces) == 1:

                    location, name = faces[0]

                    top, right, bottom, left = location

                    if name != "UNKNOWN":
                        command = "ON"

                    color = (
                        (0, 255, 0)
                        if command == "ON"
                        else (0, 0, 255)
                    )

                    cv2.rectangle(
                        frame,
                        (left, top),
                        (right, bottom),
                        color,
                        2
                    )

                    cv2.putText(
                        frame,
                        name,
                        (left, bottom + 25),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.7,
                        color,
                        2
                    )

                elif len(faces) > 1:

                    cv2.rectangle(
                        frame,
                        (15, 70),
                        (543, 120),
                        (0, 200, 0),
                        3
                    )

                    cv2.putText(
                        frame,
                        "Please one person",
                        (20, 110),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.8,
                        (0, 0, 255),
                        2
                    )

                else:

                    cv2.rectangle(
                        frame,
                        (85, 175),
                        (550, 300),
                        (0, 200, 0),
                        3
                    )

                    cv2.putText(
                        frame,
                        "There is no one",
                        (124, 245),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.8,
                        (0, 0, 255),
                        2
                    )

                # Send only when the state changes.

                if command != previous_command:

                    self.serial.send(command)

                    previous_command = command

                    self.notify(
                        "log",
                        f"Access state: {command}"
                    )

                cv2.imshow(
                    CONFIG.CAMERA_WINDOW,
                    frame
                )

                key = cv2.waitKey(1) & 0xFF

                if key == ord("q"):
                    break

            self.notify(
                "log",
                "Recognition session terminated."
            )

        except Exception as error:

            logger.exception(
                "Recognition process failed."
            )

            self.notify(
                "error",
                str(error)
            )

        finally:

            self.serial.disconnect()

            camera.release()

            cv2.destroyAllWindows()

            self.notify("finished")

    # --------------------------------------------------------
    # STOP AND SHUTDOWN
    # --------------------------------------------------------

    def stop_operation(self):

        if not self.running:
            return

        self.stop_event.set()

        self.status_var.set(
            "Stopping operation..."
        )

    def close_application(self):

        self.stop_event.set()

        self.serial.disconnect()

        cv2.destroyAllWindows()

        self.root.destroy()


# ============================================================
# APPLICATION ENTRY POINT
# ============================================================

def main():

    root = tk.Tk()

    FaceRecognitionApp(root)

    root.mainloop()


if __name__ == "__main__":

    main()
