#!/usr/bin/env python3
"""Render approximate previews of every Game & Pebble screen at the Pebble
Time 2 resolution (200x228, emery). These mirror the C layout math in src/c
so we can eyeball composition without flashing a watch. Output: a contact
sheet at tools/preview.png.

NOTE: previews are illustrative - colours use the Pebble palette and the
geometry follows the code, but they are not a pixel-exact emulator capture.
"""
import os, math
from PIL import Image, ImageDraw, ImageFont

W, H = 200, 228
HUD = 22

# --- Pebble palette (2 bits/channel) approximations ---
BLACK=(0,0,0); WHITE=(255,255,255); YELLOW=(255,255,0); GREEN=(0,255,0)
CYAN=(0,255,255); MAGENTA=(255,0,255); RED=(255,0,0); BLUE=(0,0,255)
ORANGE=(255,170,0); CHROME=(255,170,0); OXFORD=(0,0,85); SPRINGBUD=(170,255,0)
PINK=(255,85,170); PURPLE=(170,0,170); DKGRAY=(85,85,85); LTGRAY=(170,170,170)
SCREAMIN=(85,255,85); ISLAMIC=(0,170,0); LCD=(170,170,85); MELON=(255,170,170)
BRIGHTGREEN=(85,255,0); JAEGER=(0,85,0); WINDSORTAN=(170,85,0); VIVIDCERU=(0,170,255)
KELLY=(0,170,0); PICTON=(85,170,255); DARKGREEN=(0,85,0)

def font(sz, bold=True):
    paths = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ]
    for p in paths:
        if os.path.exists(p):
            return ImageFont.truetype(p, sz)
    return ImageFont.load_default()

def text(d, s, x, y, w, sz, col, align="left", bold=True):
    f = font(sz, bold)
    bb = d.textbbox((0,0), s, font=f); tw = bb[2]-bb[0]
    if align=="center": x = x + (w-tw)//2
    elif align=="right": x = x + (w-tw)
    d.text((x,y), s, font=f, fill=col)

def hud(d, left, right, accent):
    d.rectangle([0,0,W,HUD], fill=accent)
    if left:  text(d, left, 6, 3, W//2, 13, BLACK, "left")
    if right: text(d, right, W//2-6, 3, W//2, 13, BLACK, "right")

def newimg():
    im = Image.new("RGB", (W,H), BLACK)
    return im, ImageDraw.Draw(im)

def pac(d, cx, cy, r, heading_deg, open_deg, col=YELLOW):
    # Pebble angle 0=top, clockwise; PIL pieslice 0=3o'clock -> subtract 90
    a = heading_deg + open_deg/2 - 90
    b = heading_deg + 360 - open_deg/2 - 90
    d.pieslice([cx-r, cy-r, cx+r, cy+r], a, b, fill=col)

def ghost(d, cx, cy, r, col, dirx=0, diry=0, scared=False):
    body = BLUE if scared else col
    d.ellipse([cx-r, cy-r, cx+r, cy+r], fill=body)
    d.rectangle([cx-r, cy, cx+r, cy+r], fill=body)
    for k in range(3):                              # skirt notches
        nx = cx - r + r//2 + k*r
        d.ellipse([nx-(r//3+1), cy+r-(r//3+1), nx+(r//3+1), cy+r+(r//3+1)], fill=BLACK)
    if not scared:
        for ex in (-r//2, r//2):
            d.ellipse([cx+ex-(r//3+1), cy-1-(r//3+1), cx+ex+(r//3+1), cy-1+(r//3+1)], fill=WHITE)
            d.ellipse([cx+ex+dirx*2-2, cy-1+diry*2-2, cx+ex+dirx*2+2, cy-1+diry*2+2], fill=BLUE)
    else:
        for ex in (-r//2, r//2):
            d.ellipse([cx+ex-2, cy-3, cx+ex+2, cy+1], fill=WHITE)

# ---------------- MENU ----------------
def screen_menu():
    im, d = newimg()
    GAMES = [("PEBBLE-MAN","chomp the dots",YELLOW),("SNAKE","eat & grow",GREEN),
             ("ASTEROIDS","blast the rocks",CYAN),("BLOCKS","stack & clear",MAGENTA),
             ("BREAKOUT","smash bricks",ORANGE),("CATCH!","an LCD classic",SPRINGBUD)]
    ROW=38; top=HUD+34+4-30  # banner 34, list_top
    list_top=38
    sel=0
    for i,(name,tag,acc) in enumerate(GAMES):
        y=list_top+i*ROW
        if y>H: break
        tile=(8+4, y+(ROW-26)//2, 26)
        if i==sel:
            d.rounded_rectangle([4,y+2,W-4,y+ROW-2], radius=6, fill=acc)
            glyph(d, name, 4+4+13, y+ROW//2, BLACK)
            text(d, name, 4+34, y+2, W-40, 21, BLACK, "left")
            text(d, "%s  ·  HI 0"%tag, 4+34, y+22, W-40, 12, BLACK, "left", bold=False)
        else:
            glyph(d, name, 4+4+13, y+ROW//2, acc)
            text(d, name, 4+34, y+(ROW-20)//2, W-40, 16, WHITE, "left", bold=False)
    # banner on top
    d.rectangle([0,0,W,34], fill=OXFORD)
    laney=34-9
    px=40
    d.ellipse([px-2,laney-2,px+2,laney+2], fill=YELLOW)  # placeholder eaten area
    for x in range(8,W,12):
        if x>px+6: d.ellipse([x-2,laney-2,x+2,laney+2], fill=CHROME)
    ghost(d, px-22, laney, 6, MELON)
    pac(d, px, laney, 7, 90, 70)
    text(d, "GAME & PEBBLE", 0,1,W,16, WHITE, "center")
    return im

def glyph(d, name, cx, cy, fg):
    if name=="PEBBLE-MAN": pac(d, cx, cy, 9, 90, 80, fg)
    elif name=="SNAKE":
        for i in range(4): d.rounded_rectangle([cx-10+i*5, cy+6-i*5, cx-10+i*5+5, cy+6-i*5+5], radius=1, fill=fg)
    elif name=="ASTEROIDS":
        d.line([cx,cy-9,cx-7,cy+8], fill=fg, width=2); d.line([cx,cy-9,cx+7,cy+8], fill=fg, width=2)
        d.line([cx-4,cy+3,cx+4,cy+3], fill=fg, width=2)
    elif name=="BLOCKS":
        for (a,b) in [(-2,-9),(4,-9),(-9,-2),(-2,-2)]: d.rounded_rectangle([cx+a,cy+b,cx+a+7,cy+b+7], radius=1, fill=fg)
    elif name=="BREAKOUT":
        for i in range(3): d.rounded_rectangle([cx-10+i*7,cy-8,cx-10+i*7+6,cy-4], radius=1, fill=fg)
        d.ellipse([cx-3,cy+1,cx+3,cy+7], fill=fg)
    elif name=="CATCH!":
        d.ellipse([cx-3,cy-9,cx+3,cy-3], fill=fg)
        d.line([cx-9,cy+2,cx-9,cy+8], fill=fg, width=2); d.line([cx+9,cy+2,cx+9,cy+8], fill=fg, width=2)
        d.line([cx-9,cy+8,cx+9,cy+8], fill=fg, width=2)

# ---------------- PEBBLE-MAN ----------------
def screen_pebbleman():
    MAZE=["###############","#o....#.#....o#","#.###.#.#.###.#","#.............#",
          "#.###.###.###.#","#...#...#...#.#","###.###.###.#.#","#.....# #.....#",
          "#.###.#-#.###.#","#.#...G G...#.#","#.#.#.###.#.#.#","#...#..P..#...#",
          "#.#########.#.#","#.#.......#.#.#","#o..#.###.#..o#","#####.....#####","###############"]
    MR,MC=17,15
    im,d=newimg()
    tile=W//MC
    if tile*MR>H-HUD: tile=(H-HUD)//MR
    ox=(W-tile*MC)//2; oy=HUD+(H-HUD-tile*MR)//2
    for r in range(MR):
        for c in range(MC):
            x=ox+c*tile; y=oy+r*tile; ch=MAZE[r][c]
            if ch=='#': d.rectangle([x,y,x+tile,y+tile], fill=BLUE)
            elif ch=='.': d.ellipse([x+tile//2-2,y+tile//2-2,x+tile//2+2,y+tile//2+2], fill=CHROME)
            elif ch=='o': d.ellipse([x+tile//2-(tile//4+1),y+tile//2-(tile//4+1),x+tile//2+(tile//4+1),y+tile//2+(tile//4+1)], fill=CHROME)
    er=tile//2
    # ghosts at starts
    for (gc,gr,col,dx,dy) in [(6,9,RED,-1,0),(8,9,PINK,1,0),(7,7,CYAN,0,-1),(7,9,ORANGE,0,1)]:
        ghost(d, ox+gc*tile+tile//2, oy+gr*tile+tile//2, er, col, dx, dy)
    # pac at start
    pac(d, ox+7*tile+tile//2, oy+11*tile+tile//2, er, 270, 50)
    hud(d, "SCORE 120", None, YELLOW)
    for i in range(3): pac(d, W-11-i*14, 11, 5, 270, 45)
    return im

# ---------------- SNAKE ----------------
def screen_snake():
    im,d=newimg(); CELL=10
    pw,ph=W,H-HUD; cols=pw//CELL; rows=ph//CELL
    ox=(W-cols*CELL)//2; oy=HUD+(ph-rows*CELL)//2
    d.rectangle([ox-2,oy-2,ox+cols*CELL+2,oy+rows*CELL+2], outline=DKGRAY)
    body=[(6,9),(7,9),(8,9),(9,9),(10,9),(10,10),(10,11),(11,11),(12,11)]
    food=(15,6)
    fc=(ox+food[0]*CELL+CELL//2, oy+food[1]*CELL+CELL//2)
    d.ellipse([fc[0]-CELL//2+1,fc[1]-CELL//2+1,fc[0]+CELL//2-1,fc[1]+CELL//2-1], fill=RED)
    for i,(cx,cy) in enumerate(body):
        col=SCREAMIN if i==len(body)-1 else GREEN
        d.rounded_rectangle([ox+cx*CELL+1,oy+cy*CELL+1,ox+cx*CELL+CELL-2,oy+cy*CELL+CELL-2], radius=2, fill=col)
    hud(d, "SCORE 5", "BEST 21", GREEN)
    return im

# ---------------- ASTEROIDS ----------------
def screen_asteroids():
    im,d=newimg()
    import random; random.seed(3)
    rocks=[(40,80,18),(150,60,11),(110,150,18),(70,180,6),(170,170,6)]
    for (x,y,r) in rocks:
        pts=[]
        for k in range(8):
            ang=k*2*math.pi/8; rr=r+random.randint(-r//4,r//2)
            pts.append((x+rr*math.sin(ang), y-rr*math.cos(ang)))
        d.line(pts+[pts[0]], fill=LTGRAY, width=2)
    # ship center
    cx,cy=W//2,H//2; ang=20*math.pi/180
    def P(a,rr): return (cx+rr*math.sin(a), cy-rr*math.cos(a))
    nose=P(ang,9); l=P(ang+140*math.pi/180,8); rr=P(ang-140*math.pi/180,8)
    d.line([nose,l],fill=WHITE,width=2); d.line([nose,rr],fill=WHITE,width=2); d.line([l,rr],fill=WHITE,width=2)
    d.ellipse([cx+20-2,cy-30-2,cx+20+2,cy-30+2], fill=YELLOW)  # a bullet
    hud(d, "SCORE 740", None, CYAN)
    for i in range(3):
        x=W-12-i*12; y=11
        d.line([x,y-5,x-4,y+4],fill=BLACK,width=2); d.line([x,y-5,x+4,y+4],fill=BLACK,width=2); d.line([x-4,y+4,x+4,y+4],fill=BLACK,width=2)
    return im

# ---------------- BLOCKS ----------------
def screen_blocks():
    im,d=newimg(); COLS,ROWS=10,16
    ph=H-HUD; cell=ph//ROWS
    if cell*COLS>W-6: cell=(W-6)//COLS
    bx=(W-cell*COLS)//2; by=HUD+(ph-cell*ROWS)//2
    d.rounded_rectangle([bx-2,by-2,bx+COLS*cell+2,by+ROWS*cell+2], radius=2, fill=OXFORD, outline=DKGRAY)
    # some settled blocks
    field={(0,15):CYAN,(1,15):CYAN,(2,15):YELLOW,(3,15):YELLOW,(2,14):YELLOW,(3,14):YELLOW,
           (7,15):RED,(8,15):RED,(9,15):RED,(9,14):RED,(0,14):CYAN,(5,15):GREEN,(6,15):GREEN}
    for (x,y),c in field.items():
        d.rounded_rectangle([bx+x*cell, by+y*cell, bx+x*cell+cell-1, by+y*cell+cell-1], radius=1, fill=c)
    # active T piece near top + ghost
    for (x,y) in [(4,1),(3,2),(4,2),(5,2)]:
        d.rounded_rectangle([bx+x*cell, by+y*cell, bx+x*cell+cell-1, by+y*cell+cell-1], radius=1, fill=PURPLE)
    for (x,y) in [(4,12),(3,13),(4,13),(5,13)]:
        d.rectangle([bx+x*cell+1, by+y*cell+1, bx+x*cell+cell-2, by+y*cell+cell-2], outline=LTGRAY)
    hud(d, "SCORE 2400", "LV 3", MAGENTA)
    px=bx+COLS*cell+6
    if W-px>=30:
        text(d,"NEXT",px,by,40,12,WHITE,"left",bold=False)
        for (x,y) in [(0,1),(1,1),(2,1),(3,1)]:  # I piece
            d.rounded_rectangle([px+x*7,by+18+y*7,px+x*7+6,by+18+y*7+6], radius=1, fill=CYAN)
    return im

# ---------------- BREAKOUT ----------------
def screen_breakout():
    im,d=newimg(); COLS,ROWS=7,5
    fx,fy,fw,fh=0,HUD,W,H-HUD
    bw=fw//COLS; bh=11; btop=fy+8
    rowcol=[RED,ORANGE,YELLOW,GREEN,CYAN]
    for r in range(ROWS):
        for c in range(COLS):
            if r==0 and c in (2,4): continue
            if r==1 and c==3: continue
            d.rounded_rectangle([fx+c*bw+1, btop+r*bh+1, fx+c*bw+bw-2, btop+r*bh+bh-2], radius=1, fill=rowcol[r])
    pw=fw//5; py=fy+fh-12; pxc=W//2+10
    d.rounded_rectangle([pxc-pw//2, py, pxc+pw//2, py+5], radius=2, fill=WHITE)
    d.ellipse([W//2+30-3, py-40-3, W//2+30+3, py-40+3], fill=WHITE)
    hud(d, "SCORE 310", "BALLS 2", ORANGE)
    return im

# ---------------- CATCH ----------------
def screen_catch():
    im,d=newimg(); LANES,STEPS=4,7
    d.rounded_rectangle([5,5,W-5,H-5], radius=4, fill=LCD)
    lcd=(5,5,W-10,H-10)
    text(d,"CATCH",11,7,80,16,BLACK,"left")
    text(d,"7",W-69,7,58,20,BLACK,"right")
    for m in range(3):
        pip=[11+m*9, 28, 11+m*9+6, 34]
        if m<1: d.rounded_rectangle(pip, radius=1, fill=BLACK)
        else: d.rectangle(pip, outline=BLACK)
    grid_top=5+34; catch_y=5+(H-10)-22; grid_h=catch_y-grid_top
    lw=(W-10)//LANES; sh=grid_h//STEPS; gx=5+((W-10)-lw*LANES)//2
    for (lane,step) in [(0,4),(2,2),(3,5),(1,0)]:
        cx=gx+lane*lw+lw//2; cy=grid_top+step*sh+sh//2
        d.ellipse([cx-5,cy-5,cx+5,cy+5], fill=BLACK)
    catcher=2; bx=gx+catcher*lw+lw//2
    d.rounded_rectangle([bx-13,catch_y+8,bx+13,catch_y+12], radius=1, fill=BLACK)
    d.rounded_rectangle([bx-13,catch_y,bx-9,catch_y+10], radius=1, fill=BLACK)
    d.rounded_rectangle([bx+9,catch_y,bx+13,catch_y+10], radius=1, fill=BLACK)
    return im

# ---------------- INVADERS ----------------
def screen_invaders():
    im,d=newimg(); AROWS,ACOLS=4,6
    ax=(W-16)//ACOLS; ay=15; aw=ax-6; fx=8; fy=34
    for r in range(AROWS):
        for c in range(ACOLS):
            if r==0 and c in (1,4): continue
            x=fx+c*ax; y=fy+r*ay
            d.rounded_rectangle([x,y,x+aw,y+8], radius=2, fill=BRIGHTGREEN)
            d.rectangle([x+2,y+2,x+3,y+3], fill=BLACK); d.rectangle([x+aw-4,y+2,x+aw-3,y+3], fill=BLACK)
    sy=H-14-30; scell=6
    for i in range(4):
        sx=(W*(i+1))//5-(5*scell)//2
        for r in range(3):
            for c in range(5):
                if i==1 and r==0 and c in (2,3): continue
                d.rectangle([sx+c*scell,sy+r*scell,sx+c*scell+scell,sy+r*scell+scell], fill=JAEGER)
    cx=W//2+18; cyy=H-14
    d.rounded_rectangle([cx-11,cyy,cx+11,cyy+6], radius=1, fill=WHITE)
    d.rectangle([cx-2,cyy-5,cx+2,cyy+1], fill=WHITE)
    d.rectangle([cx-1,cyy-30,cx+1,cyy-12], fill=YELLOW)  # bullet
    for (bx,by) in [(60,90),(120,70)]:
        d.line([bx,by,bx,by+5], fill=RED, width=2)
    hud(d,"SCORE 480",None,BRIGHTGREEN)
    for i in range(3): d.rounded_rectangle([W-14-i*13,8,W-14-i*13+10,13], radius=1, fill=WHITE)
    return im

# ---------------- FLAPPY ----------------
def screen_flappy():
    im=Image.new("RGB",(W,H),VIVIDCERU); d=ImageDraw.Draw(im)
    top=0; ground=H-12; PW=24; GAP=62
    for (x,gy) in [(40,150),(140,90)]:
        d.rectangle([x,top,x+PW,gy-GAP//2], fill=ISLAMIC)
        d.rectangle([x,gy+GAP//2,x+PW,ground], fill=ISLAMIC)
        d.rounded_rectangle([x-2,gy-GAP//2-6,x+PW+2,gy-GAP//2], radius=1, fill=GREEN)
        d.rounded_rectangle([x-2,gy+GAP//2,x+PW+2,gy+GAP//2+6], radius=1, fill=GREEN)
    d.rectangle([0,ground,W,H], fill=WINDSORTAN); d.rectangle([0,ground,W,ground+3], fill=GREEN)
    bx,by=W//3,120
    d.ellipse([bx-7,by-7,bx+7,by+7], fill=YELLOW)
    d.rectangle([bx+5,by-1,bx+10,by+2], fill=ORANGE)
    d.ellipse([bx+1,by-5,bx+5,by-1], fill=WHITE); d.ellipse([bx+3,by-4,bx+5,by-2], fill=BLACK)
    text(d,"3",0,6,W,30,WHITE,"center")
    return im

# ---------------- PONG ----------------
def screen_pong():
    im,d=newimg(); pw=W//4; py=H-16; aiy=30
    for x in range(4,W,12): d.rectangle([x,H//2-1,x+6,H//2+1], fill=DKGRAY)
    d.rounded_rectangle([W//2-pw//2,py,W//2+pw//2,py+5], radius=2, fill=WHITE)
    d.rounded_rectangle([W//2+10-pw//2,aiy,W//2+10+pw//2,aiy+5], radius=2, fill=PICTON)
    d.rounded_rectangle([W//2-3,H//2-30-3,W//2+3,H//2-30+3], radius=1, fill=WHITE)
    hud(d,"SCORE 4","x3",PICTON)
    return im

# ---------------- FROG ----------------
def screen_frog():
    im,d=newimg(); COLS=10; cell=W//COLS; rows=(H-HUD)//cell
    ox=(W-cell*COLS)//2; oy=HUD+(H-HUD-cell*rows)//2; gw=COLS*cell
    import random; random.seed(7)
    def traffic(r): return 0<r<rows-1 and r%3!=0
    carcol=[RED,YELLOW,ORANGE,PICTON]
    for r in range(rows):
        y=oy+r*cell
        col = DKGRAY if traffic(r) else (DARKGREEN if r==0 else JAEGER)
        d.rectangle([ox,y,ox+gw,y+cell], fill=col)
        if traffic(r):
            off=random.randint(0,cell*3)
            x=off-cell*3
            while x<gw:
                d.rounded_rectangle([ox+x,y+2,ox+x+(cell*3)//2,y+cell-2], radius=2, fill=carcol[r%4]); x+=cell*3
    for c in range(1,COLS,2): d.ellipse([ox+c*cell+cell//2-2,oy+cell//2-2,ox+c*cell+cell//2+2,oy+cell//2+2], fill=BRIGHTGREEN)
    fcol,frow=COLS//2,rows-1; fx=ox+fcol*cell+cell//2; fy=oy+frow*cell+cell//2; rr=cell//2-2
    d.ellipse([fx-rr,fy-rr,fx+rr,fy+rr], fill=KELLY)
    for ex in (-rr//2,rr//2):
        d.ellipse([fx+ex-2,fy-rr//2-2,fx+ex+2,fy-rr//2+2], fill=BRIGHTGREEN)
        d.ellipse([fx+ex-1,fy-rr//2-1,fx+ex+1,fy-rr//2+1], fill=BLACK)
    hud(d,"SCORE 30","x3",KELLY)
    return im

screens=[("Menu",screen_menu()),("Pebble-Man",screen_pebbleman()),("Snake",screen_snake()),
         ("Asteroids",screen_asteroids()),("Blocks",screen_blocks()),("Breakout",screen_breakout()),
         ("Catch!",screen_catch()),("Invaders",screen_invaders()),("Flappy",screen_flappy()),
         ("Pong",screen_pong()),("Pebble-Frog",screen_frog())]

# contact sheet
pad=14; lbl=18; cols=4
rows=(len(screens)+cols-1)//cols
sheet=Image.new("RGB",(cols*(W+pad)+pad, rows*(H+lbl+pad)+pad),(30,30,34))
sd=ImageDraw.Draw(sheet)
for i,(name,im) in enumerate(screens):
    cx=pad+(i%cols)*(W+pad); cy=pad+(i//cols)*(H+lbl+pad)
    sheet.paste(im,(cx,cy))
    sd.rectangle([cx-1,cy-1,cx+W,cy+H], outline=(80,80,90))
    sd.text((cx+2,cy+H+3),name,fill=WHITE,font=font(13))
out=os.path.join(os.path.dirname(__file__),"preview.png")
sheet.save(out); print("wrote",out,sheet.size)
