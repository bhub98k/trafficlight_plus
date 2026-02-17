import cv2
from ultralytics import YOLO
from multiprocessing import Process, Pipe
import numpy as np

model_path = "yolo11n.pt"

cam_urls = [
    "http://192.168.109.34/stream",  
    "http://192.168.109.199/stream",  
]

def detect_process(cam_id, url, conn):
    try:
        model = YOLO(model_path)
    except Exception as e:
        print(f"Error loading model: {e}")
        return  # 若載入模型失敗，退出函數

    cap = cv2.VideoCapture(url)

    while True:
        ret, frame = cap.read()
        if not ret:
            print(f"Failed to capture frame from camera {cam_id}")
            continue

        # 設置 conf 和 iou 來調整靈敏度
        try:
            results = model(frame, conf=0.4, iou=0.5)[0]
        except Exception as e:
            print(f"Error during inference: {e}")
            continue

        car_count = 0
        moto_count = 0

        for box in results.boxes:
            cls = int(box.cls)
            label = model.names[cls]
            x1, y1, x2, y2 = map(int, box.xyxy[0])

            if label == "car":
                car_count += 1
                color = (0, 255, 0)
            elif label == "motorbike":
                moto_count += 1
                color = (255, 0, 0)
            else:
                continue

            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            cv2.putText(frame, label, (x1, y1 - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

        cv2.putText(frame, f"Car: {car_count}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,0), 2)
        cv2.putText(frame, f"Moto: {moto_count}", (10, 70), cv2.FONT_HERSHEY_SIMPLEX, 1, (255,0,0), 2)

        frame_resized = cv2.resize(frame, (640, 480))

        try:
            conn.send((frame_resized, car_count, moto_count))
        except Exception as e:
            print(f"Error sending data to pipe: {e}")

if __name__ == "__main__":
    from multiprocessing import set_start_method
    try:
        set_start_method('spawn', force=True)
    except RuntimeError:
        pass  # 如果已經設定過了，則不會再次設定

    pipes = []
    processes = []

    for i, url in enumerate(cam_urls):
        parent_conn, child_conn = Pipe()
        p = Process(target=detect_process, args=(i + 1, url, child_conn))
        p.start()
        pipes.append(parent_conn)
        processes.append(p)

    while True:
        frames = []
        counts = []

        for conn in pipes:
            if conn.poll():
                frame, car_count, moto_count = conn.recv()
                frames.append(frame)
                counts.append((car_count, moto_count))

        if len(frames) == len(cam_urls):
            combined = cv2.hconcat(frames)

            txt = f"Cam1 Cars:{counts[0][0]} Motos:{counts[0][1]} | Cam2 Cars:{counts[1][0]} Motos:{counts[1][1]}"
            winner = "Cam1 > Cam2" if sum(counts[0]) > sum(counts[1]) else "Cam2 > Cam1"

            cv2.putText(combined, txt, (10, 460), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 0), 2)
            cv2.putText(combined, winner, (10, 500), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 2)

            cv2.imshow("Multi-Camera Comparison", combined)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cv2.destroyAllWindows()
