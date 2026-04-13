import numpy as np
import cv2

# -------- CONFIG --------
WIDTH = 80
HEIGHT = 60
FILE = "get"   # change filename

# -------- LOAD RAW --------
with open(FILE, "rb") as f:
    data = np.frombuffer(f.read(), dtype=np.uint8)

# -------- VALIDATE --------
expected_size = WIDTH * HEIGHT
if len(data) != expected_size:
    print(f"Size mismatch! Expected {expected_size}, got {len(data)}")
    exit()

# -------- RESHAPE --------
image = data.reshape((HEIGHT, WIDTH))

# -------- DISPLAY --------
cv2.imshow("RAW Image", image)
cv2.waitKey(0)
cv2.destroyAllWindows()
cv2.imwrite("output.png", image)