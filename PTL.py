from PIL import Image
import numpy as np

def prepare_image(input_file, output_file):
    img = Image.open(input_file).convert('L')
    img = img.resize((640, 480))
    data = np.array(img, dtype=np.uint8)
    with open(output_file, 'wb') as f:
        f.write(data.tobytes())

# 
prepare_image("input.jpg", "input.raw")
