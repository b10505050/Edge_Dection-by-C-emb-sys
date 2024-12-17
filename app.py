from flask import Flask, render_template_string, Response, jsonify
from picamera2 import Picamera2
from libcamera import controls, Transform
import os
import time
import fcntl
import struct
import cv2
import numpy as np

app = Flask(__name__)

# 全域變數用於追蹤是否啟用邊緣偵測
edge_detection_enabled = False

# Constants for IOCTL control
DEVICE_FILE = "/dev/edge_detection"
IOCTL_ENABLE_EDGE_DETECTION = 0x40046501  # 請根據實際驅動定義確認

# HTML Template
template = '''
<!DOCTYPE html>
<html lang="en">
    <body>
        <img src="{{ url_for('video_stream') }}" width="100%">
        <button onclick="fetch('/api/enable_edge_detection').then(response => response.json()).then(data => alert(data.message))">Enable Edge Detection</button>
        <button onclick="fetch('/api/disable_edge_detection').then(response => response.json()).then(data => alert(data.message))">Disable Edge Detection</button>
        <button onclick="fetch('/api/snapshot').then(response => response.json()).then(data => alert(data.message))">Take Snapshot</button>
    </body>
</html>
'''

# 初始化相機
cam = Picamera2()
config = cam.create_still_configuration(
    main={"size": (1920, 1080), "format": "XBGR8888"},
    transform=Transform(vflip=1),
    controls={'NoiseReductionMode': controls.draft.NoiseReductionModeEnum.HighQuality, 'Sharpness': 1.5}
)
cam.configure(config)
cam.start()  # start camera streaming

def process_frame():
    # 從相機取得原始影像陣列 (XBGR8888格式)
    frame = cam.capture_array("main")  # 取得一幀影像 (height, width, 4通道 BGRX)
    bgr_frame = frame[:, :, :3]  # 取前3通道 BGR

    if edge_detection_enabled:
        # 如果啟用邊緣偵測，將影像灰階化後送入 /dev/edge_detection 取得處理結果
        gray = cv2.cvtColor(bgr_frame, cv2.COLOR_BGR2GRAY)
        # 縮放成 640x480
        gray_resized = cv2.resize(gray, (640, 480))

        # 將灰階影像寫入 /dev/edge_detection
        with open(DEVICE_FILE, 'wb') as f:
            f.write(gray_resized.tobytes())

        # 從 /dev/edge_detection 讀出處理後的影像資料
        processed_data = bytearray()
        with open(DEVICE_FILE, 'rb') as f:
            processed_data = f.read(640*480)

        # 將讀出的資料轉為 numpy array (灰階)
        processed_frame = np.frombuffer(processed_data, dtype=np.uint8).reshape((480, 640))

        # 將處理後的灰階影像編碼為 JPEG
        ret, jpeg = cv2.imencode('.jpg', processed_frame)
    else:
        # 如果未啟用邊緣偵測，直接顯示原始影像 (bgr_frame)
        # 在此不需要傳入 /dev/edge_detection
        # 可適度縮放或直接使用原尺寸
        resized_frame = cv2.resize(bgr_frame, (640, 480))
        ret, jpeg = cv2.imencode('.jpg', resized_frame)

    if not ret:
        return None
    return jpeg.tobytes()

def gen_frames():
    while True:
        frame = process_frame()
        if frame is not None:
            yield (b'--frame\r\nContent-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
        else:
            # 若無法取得有效影像，暫停一下避免CPU過載
            time.sleep(0.1)

@app.route("/", methods=['GET'])
def get_stream_html():
    return render_template_string(template)

@app.route('/api/stream')
def video_stream():
    return Response(gen_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/api/snapshot', methods=['GET'])
def take_snapshot():
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    snapshot_dir = "./snapshots"
    os.makedirs(snapshot_dir, exist_ok=True)
    snapshot_path = f"{snapshot_dir}/snapshot_{timestamp}.jpg"

    frame = process_frame()
    if frame is not None:
        with open(snapshot_path, "wb") as snapshot_file:
            snapshot_file.write(frame)
        return jsonify({"message": f"Snapshot saved as {snapshot_path}"})
    else:
        return jsonify({"message": "Failed to capture snapshot"})

@app.route('/api/enable_edge_detection', methods=['GET'])
def enable_edge_detection():
    global edge_detection_enabled
    try:
        with open(DEVICE_FILE, 'wb') as f:
            fcntl.ioctl(f, IOCTL_ENABLE_EDGE_DETECTION, struct.pack('i', 1))
        edge_detection_enabled = True
        return jsonify({"message": "Edge detection enabled"})
    except Exception as e:
        return jsonify({"message": f"Failed to enable edge detection: {str(e)}"})

@app.route('/api/disable_edge_detection', methods=['GET'])
def disable_edge_detection():
    global edge_detection_enabled
    try:
        with open(DEVICE_FILE, 'wb') as f:
            fcntl.ioctl(f, IOCTL_ENABLE_EDGE_DETECTION, struct.pack('i', 0))
        edge_detection_enabled = False
        return jsonify({"message": "Edge detection disabled"})
    except Exception as e:
        return jsonify({"message": f"Failed to disable edge detection: {str(e)}"})

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000)
