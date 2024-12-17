from PIL import Image
import numpy as np
def save_image(input_file, output_file):
    with open(input_file, 'rb') as f:
        data = f.read()
    img = np.frombuffer(data, dtype=np.uint8).reshape((480, 640))
  
    output_img = Image.fromarray(img)
    output_img.save(output_file)

save_image("output.raw", "output.jpg")
