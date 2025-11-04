from PIL import Image

img = Image.open("mo.png")
img.save("mo.ico",format="ico",sizes=[(64,64)])