from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).parent
shots = root / 'android_redesign_2026-10-15'
canvas = Image.new('RGB', (860, 970), '#e8e7e1')
draw = ImageDraw.Draw(canvas)
font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 16)
title = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 23)
draw.text((25, 17), 'H2 PRO / ANDROID 1.1', font=title, fill='#252b26')
for i, (name, label) in enumerate([('light_home', 'СВЕТЛАЯ'), ('dark_home', 'ТЁМНАЯ')]):
    x = 25 + i * 420
    draw.text((x, 56), label, font=font, fill='#5e6859')
    shot = Image.open(shots / (name + '.png')).convert('RGB').resize((390, 844), Image.Resampling.LANCZOS)
    canvas.paste(shot, (x, 85))
draw.text((25, 941), 'Предпросмотр интерфейса · тестовые данные', font=font, fill='#5e6859')
canvas.save(shots / 'H2_PRO_дизайн.png')
