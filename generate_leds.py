import os
from PIL import Image, ImageDraw, ImageFont

# Configuración
fuente_path = r"C:\Users\ajaba\Downloads\fonts-DSEG_v046\fonts-DSEG_v046\DSEG14-Modern\DSEG14Modern-Italic.ttf"
tamano = 22 # Adjusted for 23px height
carpeta_salida = r"d:\desarrollos\ABDJUNiO601\Source\UI\WebUI\assets\led"
os.makedirs(carpeta_salida, exist_ok=True)

# Digits and some letters for the LED display
CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ- "
# Small Green Diagnostic (Build 86)
WIDTH, HEIGHT = 17, 23
BACKGROUND = (0, 255, 0, 255) 
FOREGROUND = (255, 0, 0, 255) # Pure Red

try:
    font = ImageFont.truetype(fuente_path, tamano)
except Exception as e:
    print(f"Error loading font: {e}")
    exit(1)

for char in CHARS:
    filename = char if char != " " else "SPACE"
    if char == "-": filename = "DASH"
    
    img = Image.new('RGBA', (WIDTH, HEIGHT), color=BACKGROUND)
    draw = ImageDraw.Draw(img)
    
    # Center text
    bbox = draw.textbbox((0, 0), char, font=font)
    w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text(((WIDTH - w) / 2, (HEIGHT - h) / 2 - 4), char, font=font, fill=FOREGROUND)
    
    img.save(os.path.join(carpeta_salida, f"{filename}.png"))
    print(f"Generated {filename}.png")
