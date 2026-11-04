# Face Recognition System

## Installation Guide

Follow these steps to install all the required dependencies for the Face Recognition System.

### Prerequisites

- Windows 64-bit
- Python 3.7, 3.8, 3.9, or 3.11
- `pip` package manager

Check your Python version:

```bash
python --version
```

---

### Step 1: Install Dlib

First, install the Dlib wheel file that matches your Python version.

**For Python 3.11:**

```bash
pip install dlib-19.24.1-cp311-cp311-win_amd64.whl
```

**For Python 3.9:**

```bash
pip install dlib-19.22.99-cp39-cp39m-win_amd64.whl
```

**For Python 3.8:**

```bash
pip install dlib-19.22.99-cp38-cp38m-win_amd64.whl
```

**For Python 3.7:**

```bash
pip install dlib-19.22.99-cp37-cp37m-win_amd64.whl
```

> Make sure the wheel file is compatible with your Python version and Windows architecture.

---

### Step 2: Install Face Recognition

```bash
pip3 install face_recognition
```

---

### Step 3: Install OpenCV and NumPy

Install OpenCV:

```bash
pip install opencv-python
```

Install NumPy:

```bash
pip install numpy
```

---

### Step 4: Install PySerial

PySerial is required for serial communication with Esp32.

```bash
pip install pyserial
```

---

### Step 5: Verify Installation

Run the following command to check whether all dependencies are installed correctly:

```bash
python -c "import dlib, face_recognition, cv2, numpy, serial; print('All dependencies installed successfully!')"
```

---

## Required Libraries

| Library | Purpose |
|---|---|
| Dlib | Facial detection and recognition algorithms |
| Face Recognition | Face recognition and face encoding |
| OpenCV | Image processing and camera access |
| NumPy | Numerical operations and array processing |
| PySerial | Serial communication with Arduino |

---

**Note:** Install the Dlib wheel corresponding to your Python version before installing `face_recognition`.
