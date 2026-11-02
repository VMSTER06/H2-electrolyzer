from pathlib import Path
from xml.sax.saxutils import escape

BASE = Path(r"S:\1Electrolizer\Финал\Курсовая")
FRAME_OUT = BASE / "Рамка А4 как в оригинале.drawio"
SCHEME_OUT = BASE / "Структурная схема H2 PRO.drawio"
PW, PH = 827.0, 1169.0
SX, SY = PW / 595.2, PH / 841.8
def X(v): return round(v * SX, 2)
def Y(v): return round(v * SY, 2)

def rect(cid, value, x, y, w, h, style):
    return (f'<mxCell id="{cid}" value="{escape(value)}" style="{style}" parent="1" vertex="1">'
            f'<mxGeometry x="{X(x)}" y="{Y(y)}" width="{X(w)}" height="{Y(h)}" as="geometry"/></mxCell>')

def line(cid, x1, y1, x2, y2, width=0.72):
    return (f'<mxCell id="{cid}" value="" style="html=1;rounded=0;strokeColor=#000000;strokeWidth={width};endArrow=none;" parent="1" edge="1">'
            f'<mxGeometry relative="1" as="geometry"><mxPoint x="{X(x1)}" y="{Y(y1)}" as="sourcePoint"/>'
            f'<mxPoint x="{X(x2)}" y="{Y(y2)}" as="targetPoint"/></mxGeometry></mxCell>')

def poly(cid, pts, width=0.5, dashed=False):
    dash = "dashed=1;dashPattern=7 4;" if dashed else ""
    middle = ''.join(f'<mxPoint x="{X(x)}" y="{Y(y)}"/>' for x, y in pts[1:-1])
    return (f'<mxCell id="{cid}" value="" style="edgeStyle=orthogonalEdgeStyle;html=1;rounded=0;orthogonalLoop=1;strokeColor=#000000;strokeWidth={width};{dash}endArrow=open;endFill=0;endSize=8;" parent="1" edge="1">'
            f'<mxGeometry relative="1" as="geometry"><mxPoint x="{X(pts[0][0])}" y="{Y(pts[0][1])}" as="sourcePoint"/>'
            f'<mxPoint x="{X(pts[-1][0])}" y="{Y(pts[-1][1])}" as="targetPoint"/><Array as="points">{middle}</Array></mxGeometry></mxCell>')

OUTLINE = "rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#000000;strokeWidth=3.12;"
BOX = "rounded=0;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#000000;strokeWidth=0.35;fontFamily=Arial;fontSize=14;align=center;verticalAlign=middle;"
TXT = "rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=none;fontFamily=Arial;fontSize=8;align=left;verticalAlign=middle;"
TXTC = "rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=none;fontFamily=Arial;fontSize=10;align=center;verticalAlign=middle;"

def frame_cells(blank=False):
    c = [
        rect("outer","",56.69,15.24,524.41,811.81,OUTLINE),
        # diagrams.net rotates around the centre, therefore pre-rotation width/height
        # are swapped to reproduce the original visible box x=541.42..581.11,
        # y=14.93..260.13 pt.
        rect("vertical","ГУИР.400201.NNN Э1",438.66,117.69,245.20,39.69,"rounded=0;whiteSpace=wrap;html=1;rotation=270;fillColor=#ffffff;strokeColor=#000000;strokeWidth=2.84;fontFamily=Arial;fontSize=16;fontStyle=2;align=center;verticalAlign=middle;"),
        line("s_top",56.69,697.71,581.10,697.71,2.16), line("s_x239",239.68,697.71,239.68,827.05,2.16),
        line("s_x436",436.75,754.40,436.75,827.05,2.16), line("s_x76",76.40,697.71,76.40,754.40,2.16),
        line("s_x104",104.31,697.71,104.31,827.05,2.16), line("s_x179",179.44,697.71,179.44,827.05,2.16),
        line("s_x211",211.53,697.71,211.53,827.05,2.16),
        line("s_y712",56.69,711.87,239.68,711.87), line("s_y726",56.69,726.05,239.68,726.05),
        line("s_y740",56.69,740.22,239.68,740.22,2.16), line("s_y754",56.69,754.40,581.10,754.40,2.16),
        line("s_y769l",56.69,768.57,239.68,768.57), line("s_y783l",56.69,782.74,239.68,782.74),
        line("s_y797l",56.69,796.91,239.68,796.91), line("s_y811l",56.69,811.09,239.68,811.09),
        line("s_y769r",436.75,768.57,581.10,768.57,2.16), line("s_y783r",436.75,782.74,581.10,782.74,2.16),
        line("s_x479",478.98,754.40,478.98,782.74,2.16), line("s_x521",521.20,754.40,521.20,782.74,2.16),
        line("s_x451",450.82,768.57,450.82,782.74), line("s_x465",464.90,768.57,464.90,782.74),
        rect("t_head","Изм.  Лист      № докум.       Подп.   Дата",56.8,697.8,182.7,14.0,TXT),
        rect("t_dev","Разраб.   [Фамилия]",56.8,712.0,182.7,14.0,TXT), rect("t_check","Пров.      Селезнёв",56.8,726.2,182.7,14.0,TXT),
        rect("t_norm","Н.контр.",56.8,783.0,182.7,14.0,TXT), rect("t_approve","Утв.",56.8,811.2,182.7,14.0,TXT),
        rect("t_desig","ГУИР.400201.NNN Э1",239.8,697.8,341.0,56.3,"rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=none;fontFamily=Arial;fontSize=18;fontStyle=2;align=center;verticalAlign=middle;"),
        rect("t_lit","Лит.",436.8,754.5,42.0,14.0,TXT), rect("t_sheet","Лист",479.0,754.5,42.0,14.0,TXT),
        rect("t_sheets","Листов",521.2,754.5,59.7,14.0,TXT), rect("t_sheet_no","1",479.0,768.6,42.0,14.0,TXTC),
        rect("t_sheets_no","1",521.2,768.6,59.7,14.0,TXTC),
        rect("t_org","БГУИР, гр. [номер группы]",436.8,782.9,144.0,43.8,"rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=none;fontFamily=Arial;fontSize=11;fontStyle=2;align=center;verticalAlign=middle;")
    ]
    if not blank:
        c.append(rect("t_name","Микропроцессорное устройство<br>дистанционного управления объектом.<br>Схема электрическая структурная",239.8,754.5,196.7,72.2,"rounded=0;whiteSpace=wrap;html=1;fillColor=none;strokeColor=none;fontFamily=Arial;fontSize=10;fontStyle=2;align=center;verticalAlign=middle;"))
    return c

def write(path, cells, name):
    xml = f'<mxfile host="app.diagrams.net" modified="2026-09-21T00:00:00.000Z" agent="Codex" version="26.0.4" type="device"><diagram id="original-layout" name="{escape(name)}"><mxGraphModel dx="1200" dy="900" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0"><root><mxCell id="0"/><mxCell id="1" parent="0"/>{"".join(cells)}</root></mxGraphModel></diagram></mxfile>'
    path.write_text(xml, encoding="utf-8")

write(FRAME_OUT, frame_cells(True), "Рамка из оригинала")
c = frame_cells(False) + [
    rect("b1","Датчики температуры<br>и давления",99.21,139.64,127.56,85.04,BOX),
    rect("b2","Датчик положения<br>MPU-6050",382.68,139.64,127.56,85.04,BOX),
    rect("b3","Блок питания",70.86,267.21,127.56,85.04,BOX),
    rect("b4","Микропроцессорный блок<br>мозга ESP32",240.94,267.21,127.56,85.04,BOX),
    rect("b5","Блок отображения<br>информации LCD2004",99.21,394.77,127.56,85.04,BOX),
    rect("b6","Исполнительный блок<br>реле и вентилятор",382.68,394.77,127.56,85.04,BOX),
    rect("b7","Пульт управления<br>ESP32, OLED, nRF24L01+",240.94,493.99,127.56,85.04,BOX),
    poly("a1",((162.99,224.68),(162.99,245.94),(283.46,245.94),(283.46,267.21)),0.5),
    poly("a2",((446.46,224.68),(446.46,245.94),(325.98,245.94),(325.98,267.21)),0.5),
    poly("a3",((198.43,309.73),(240.94,309.73)),0.25,True),
    poly("a4",((333.07,352.25),(333.07,437.29),(382.68,437.29)),0.25),
    poly("a5",((276.97,352.25),(276.97,437.29),(226.77,437.29)),0.25),
    poly("a6",((304.72,493.99),(304.72,352.25)),0.25),
]
write(SCHEME_OUT, c, "Э1 A4")
print(FRAME_OUT); print(SCHEME_OUT)
