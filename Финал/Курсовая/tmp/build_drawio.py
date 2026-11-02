from pathlib import Path
from xml.sax.saxutils import escape


OUT = Path(r"S:\1Electrolizer\Финал\Курсовая\Структурная схема H2 PRO.drawio")


def cell(cid, value="", style="", parent="1", vertex=False, edge=False, source=None, target=None, x=0, y=0, w=0, h=0, relative=False):
    attrs = [f'id="{cid}"', f'value="{escape(value)}"', f'style="{style}"', f'parent="{parent}"']
    if vertex:
        attrs.append('vertex="1"')
    if edge:
        attrs.append('edge="1"')
    if source:
        attrs.append(f'source="{source}"')
    if target:
        attrs.append(f'target="{target}"')
    geom = f'<mxGeometry x="{x}" y="{y}" width="{w}" height="{h}"'
    if relative:
        geom += ' relative="1"'
    geom += ' as="geometry"/>'
    return f'<mxCell {" ".join(attrs)}>{geom}</mxCell>'


cells = ['<mxCell id="0"/>', '<mxCell id="1" parent="0"/>']

# A4 portrait, dimensions in draw.io points/pixels with an ESKD-like frame and title block.
frame = 'rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#000000;strokeWidth=2;'
box = 'rounded=0;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#000000;strokeWidth=1;fontFamily=Arial;fontSize=12;align=center;verticalAlign=middle;'
group = 'rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#000000;strokeWidth=1;dashed=1;dashPattern=8 4;fontFamily=Arial;fontSize=11;fontStyle=1;align=left;verticalAlign=top;spacingTop=4;spacingLeft=8;'
title = 'rounded=0;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#000000;strokeWidth=1;fontFamily=Arial;fontSize=11;align=center;verticalAlign=middle;'
tiny = 'rounded=0;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#000000;strokeWidth=1;fontFamily=Arial;fontSize=8;align=left;verticalAlign=middle;spacingLeft=3;'
edge = 'edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;strokeColor=#000000;strokeWidth=1;endArrow=classic;endFill=0;'
power = 'edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;strokeColor=#000000;strokeWidth=1;dashed=1;endArrow=classic;endFill=0;'
radio = 'edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;strokeColor=#000000;strokeWidth=1;dashed=1;dashPattern=4 4;endArrow=classic;endFill=0;startArrow=classic;startFill=0;'

cells.append(cell('frame','',frame,vertex=True,x=38,y=18,w=742,h=1087))
cells.append(cell('brain_group','МОЗГ',group,vertex=True,x=70,y=90,w=420,h=665))
cells.append(cell('remote_group','ПУЛЬТ',group,vertex=True,x=515,y=90,w=235,h=665))

blocks = [
    ('psu','Блок питания<br>5 В, не менее 2 А',85,145,145,65),
    ('temp','Датчик температуры<br>DS18B20',85,255,145,65),
    ('press','Датчик давления<br>0…10 бар',85,355,145,65),
    ('mpu','Инерциальный модуль<br>MPU-6050',85,455,145,65),
    ('brain','Микроконтроллерный блок<br>ESP32 DevKit',285,320,165,90),
    ('lcd','Блок отображения<br>LCD2004',285,155,165,65),
    ('act','Исполнительный блок<br>реле K1–K2, вентилятор',285,510,165,75),
    ('web','Веб-интерфейс<br>Wi-Fi',285,630,165,65),
    ('rfb','Радиомодуль<br>nRF24L01+',285,240,165,55),
    ('rfr','Радиомодуль<br>nRF24L01+',550,240,165,55),
    ('remote','Микроконтроллерный блок<br>ESP32 DevKit',550,335,165,90),
    ('controls','Органы управления<br>кнопки, энкодер',550,470,165,65),
    ('oled','Блок отображения<br>OLED 128×64',550,570,165,65),
    ('led','Блок индикации<br>светодиоды',550,665,165,55),
]
for cid,val,x,y,w,h in blocks:
    cells.append(cell(cid,val,box,vertex=True,x=x,y=y,w=w,h=h))

links = [
    ('e1','temp','brain',edge),('e2','press','brain',edge),('e3','mpu','brain',edge),
    ('e4','brain','lcd',edge),('e5','brain','act',edge),('e6','brain','web',edge),
    ('e7','brain','rfb',edge),('e8','rfr','remote',edge),('e9','controls','remote',edge),
    ('e10','remote','oled',edge),('e11','remote','led',edge),('e12','rfb','rfr',radio),
    ('p1','psu','brain',power),('p2','psu','lcd',power),('p3','psu','act',power),
]
for cid,src,dst,sty in links:
    cells.append(cell(cid,'',sty,edge=True,source=src,target=dst,relative=True))

# Vertical designation field and title block copied in structure from the supplied A4 example.
cells.append(cell('vert','ГУИР.400201.NNN Э1','rounded=0;whiteSpace=wrap;html=1;rotation=270;fillColor=#ffffff;strokeColor=#000000;strokeWidth=1;fontFamily=Arial;fontSize=16;fontStyle=2;align=center;verticalAlign=middle;',vertex=True,x=610,y=142,w=275,h=45))
cells.append(cell('stamp','',frame,vertex=True,x=38,y=930,w=742,h=175))
cells.append(cell('desig','ГУИР.400201.NNN Э1',title,vertex=True,x=270,y=930,w=510,h=55))
cells.append(cell('lefthead','Изм.   Лист      № докум.       Подп.      Дата',tiny,vertex=True,x=38,y=930,w=232,h=25))
cells.append(cell('dev','Разраб.   [Фамилия]',tiny,vertex=True,x=38,y=955,w=232,h=28))
cells.append(cell('check','Пров.      Селезнёв',tiny,vertex=True,x=38,y=983,w=232,h=28))
cells.append(cell('norm','Н.контр.',tiny,vertex=True,x=38,y=1011,w=232,h=28))
cells.append(cell('approve','Утв.',tiny,vertex=True,x=38,y=1039,w=232,h=66))
cells.append(cell('docname','Микропроцессорное устройство<br>дистанционного управления объектом<br><br>Схема электрическая структурная',title,vertex=True,x=270,y=985,w=360,h=120))
cells.append(cell('sheet','Лит. У        Лист 1        Листов 1',tiny,vertex=True,x=630,y=985,w=150,h=45))
cells.append(cell('org','БГУИР, каф. ЭВМ<br>гр. [номер группы]',title,vertex=True,x=630,y=1030,w=150,h=75))

xml = f'''<mxfile host="app.diagrams.net" modified="2026-09-21T00:00:00.000Z" agent="Codex" version="26.0.4" type="device">
  <diagram id="h2-pro-e1" name="Э1 A4">
    <mxGraphModel dx="1200" dy="900" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
      <root>{''.join(cells)}</root>
    </mxGraphModel>
  </diagram>
</mxfile>'''

OUT.write_text(xml, encoding='utf-8')
print(OUT)
