"""Reconstruye las unidades y conserva exactamente el reposo del atlas original aprobado."""
from __future__ import annotations

import argparse
import colorsys
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[2]
SRC = Path(__file__).resolve().parent / 'Sources' / 'Units'
NN = Image.Resampling.NEAREST
AREA = Image.Resampling.BOX
NAMES = ['Saber', 'Archer', 'Lancer', 'Rider', 'Caster', 'Assassin', 'Berserker']
STATES = ['idle', 'walk', 'attack', 'hurt', 'death', 'cast']
BLUE = ['1e3e76', '2866b7', '67a9e3', 'aad5f0']
RED = ['571c32', 'a92e4c', 'de5b6c', 'f4b09c']
INK = '#101b25'
CREAM = '#d7ceaa'
RENDER_STATS={'padded_transforms':0,'padded_layer_compositions':0,'pre_encoding_clipped_pixels':0,'native_fit_events':0}
APPROVED_ATLAS=None
APPROVED_SHA256='fa1b4944a505b7c22da6fb3574d4148f086556b73b4602006b03202b2f2ed603'


def authored_idle(cls,team,rear,phase):
    row=cls+7*team+28*int(rear)
    return approved_atlas().crop((phase*48,row*64,(phase+1)*48,(row+1)*64))


def approved_atlas():
    global APPROVED_ATLAS
    if APPROVED_ATLAS is None:
        path=SRC/'Approved/approved-atlas.png'
        if hashlib.sha256(path.read_bytes()).hexdigest()!=APPROVED_SHA256:
            raise AssertionError('La copia inmutable del atlas aprobado ha cambiado')
        APPROVED_ATLAS=Image.open(path).convert('RGBA')
        if APPROVED_ATLAS.size!=(2304,3584):
            raise AssertionError('El atlas aprobado no conserva el contrato 2304x3584')
    return APPROVED_ATLAS


def restore_approved_idle(root=None,output_atlas=None,preview_only=False):
    root=Path(root) if root is not None else ROOT
    path=root/'Content/Textures/Plaguefront/Pixel/units.png'
    before=Image.open(path).convert('RGBA')
    if before.size!=(2304,3584):
        raise AssertionError('El atlas de destino no conserva el contrato 2304x3584')
    restored=before.copy()
    restored.paste(approved_atlas().crop((0,0,384,3584)),(0,0))
    previous=np.asarray(before)
    result=np.asarray(restored)
    if not np.array_equal(previous[:,384:],result[:,384:]):
        raise AssertionError('La restauración alteró un estado ajeno a Idle')
    output=Path(output_atlas) if output_atlas is not None else path
    if not preview_only:
        output.parent.mkdir(parents=True,exist_ok=True)
        restored.save(output)
    report={'operation':'restore-approved-idle','source':str(SRC/'Approved/approved-atlas.png'),
            'source_sha256':APPROVED_SHA256,'cell':[48,64],'anchor':[24,56],
            'idle_cells_copied_exactly':448,'non_idle_cells_preserved_exactly':2240,
            'changed_pixels':int(np.any(previous!=result,axis=2).sum()),'preview_only':preview_only,
            'output':str(output),'no_new_drawings_or_emblems':True}
    if not preview_only:
        report['output_sha256']=hashlib.sha256(output.read_bytes()).hexdigest()
    evidence=root/'build/unit-integration/units';evidence.mkdir(parents=True,exist_ok=True)
    (evidence/'approved-idle-restore.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps(report,indent=2,ensure_ascii=False))
    return restored


def rgba(h):
    return tuple(bytes.fromhex(h.removeprefix('#'))) + (255,)


def transparent(size=(48, 64)):
    return Image.new('RGBA', size)


def trim(im):
    a = np.asarray(im).copy()
    a[a[:, :, 3] < 128] = 0
    a[a[:, :, 3] >= 128, 3] = 255
    im = Image.fromarray(a)
    box = im.getbbox()
    if not box:
        raise ValueError('Empty source crop')
    return im.crop(box)


def crop(file, box):
    return trim(Image.open(SRC / file).convert('RGBA').crop(box))


def material_palette(im, source_team=0, cls=0):
    """Retain separate steel, skin, leather, violet, and faction cloth materials."""
    palette_data = json.loads((ROOT / 'Content/Plaguefront/art.json').read_text())['pixel_art']['palette']
    # Restrict materials: no grass/rot/water/fire/sky colors on characters.
    shared=['#101b25',*palette_data['iron'],*palette_data['wood'],*palette_data['skin'],'#d7ceaa']
    if cls==4: shared.extend(palette_data['violet'])
    palette = np.array([rgba(v)[:3] for v in dict.fromkeys(shared)], dtype=np.int32)
    blue, red = np.array([rgba(v)[:3] for v in BLUE]), np.array([rgba(v)[:3] for v in RED])
    a = np.asarray(im).copy()
    for y, x in np.argwhere(a[:, :, 3] > 0):
        color = a[y, x, :3].astype(np.int32)
        hue, sat, value = colorsys.rgb_to_hsv(*(color / 255))
        choices = palette
        if value<.15:
            choices=np.array([rgba(INK)[:3]])
        elif source_team == 0 and .53 < hue < .74 and sat > .36:
            choices = blue
        elif source_team == 1 and (hue > .90 or hue < .055) and sat > .38:
            choices = red
        a[y, x, :3] = choices[np.argmin(np.sum((choices - color) ** 2 * [.3, .59, .11], axis=1))]
        a[y, x, 3] = 255 if a[y,x,3]>=128 else 0
        if a[y,x,3]==0:a[y,x]=0
    return Image.fromarray(a)


def faction(im, source_team, target_team):
    a = np.asarray(im).copy()
    if source_team != target_team:
        old, new = (BLUE, RED) if source_team == 0 else (RED, BLUE)
        for old_c, new_c in zip(old, new):
            sel = np.all(a[:, :, :3] == rgba(old_c)[:3], axis=2) & (a[:, :, 3] > 0)
            a[sel, :3] = rgba(new_c)[:3]
    return Image.fromarray(a)


def finish_emblem(im, cls, rear, team, kind='idle'):
    """Replace the source cross/bar locally; armor and cream hems stay intact.

    The source designs have varied emblems. At 48x64, every faction gets one
    consistent, deliberately authored 3x5 cloth insignia in a material patch.
    """
    out = clean_source_emblems(im,team,cls)
    a = np.asarray(out)
    cloth = np.array([rgba(v)[:3] for v in (BLUE if team == 0 else RED)])
    mask = np.any(np.all(a[:, :, None, :3] == cloth[None, None, :, :], axis=3), axis=2)
    if kind == 'fallen':
        search = (12, 42, 36, 54)
    elif kind == 'contact':
        search = (14, 25 if cls != 3 else 27, 32, 50)
    else:
        y = {0: 35, 1: 33, 2: 34, 3: 39, 4: 35, 5: 40, 6: 46}[cls]
        if rear and cls not in (3, 6):
            y += 2
        search = (14, y - 6, 33, min(53, y + 8))
    best = None
    # Choose an actual cloth patch with room for a recognizable badge.
    for y in range(search[1] + 2, search[3] - 2):
        for x in range(search[0] + 2, search[2] - 2):
            patch = mask[y-2:y+3, x-2:x+3]
            if patch.shape != (5, 5):
                continue
            score = patch.sum() - abs(x - 24) * .16 - abs(y - (search[1]+search[3]) / 2) * .10
            if best is None or score > best[0]:
                best = (score, x, y)
    if best and best[0] >= 9:
        _, x, y = best
        draw = ImageDraw.Draw(out)
        # This is an authored cloth patch, not a global replacement of pale pixels.
        draw.rectangle((x-2, y-2, x+2, y+2), fill=rgba((BLUE if team == 0 else RED)[1]))
        if team == 0:
            draw.line((x, y-2, x, y+2), fill=CREAM)
            draw.line((x-1, y-1, x+1, y-1), fill=CREAM)
        else:
            draw.line((x-1, y-1, x+1, y-1), fill=CREAM)
            draw.line((x-1, y+1, x+1, y+1), fill=CREAM)
    return out


def clean_source_emblems(im,team,cls):
    """Remove small pale source insignias enclosed by cloth before redrawing.

    The hand-painted sources include crosses on shields and saddle blankets as
    well as tabards. Cleaning connected pale clusters avoids leaving a blue
    cross shape on a crimson team's second item of equipment.
    """
    a=np.asarray(im).copy()
    cloth=np.array([rgba(v)[:3] for v in (BLUE if team==0 else RED)])
    iscloth=np.any(np.all(a[:,:,None,:3]==cloth[None,None,:,:],axis=3),axis=2)
    pale=np.all(a[:,:,:3]==rgba(CREAM)[:3],axis=2)&(a[:,:,3]>0)
    seen=set();components=[]
    for yy,xx in np.argwhere(pale):
        if (int(xx),int(yy)) in seen:continue
        todo=[(int(xx),int(yy))];pixels=[]
        while todo:
            x,y=todo.pop()
            if (x,y) in seen or not(0<=x<48 and 0<=y<64) or not pale[y,x]:continue
            seen.add((x,y));pixels.append((x,y))
            todo.extend([(x-1,y),(x+1,y),(x,y-1),(x,y+1)])
        if pixels:components.append(pixels)
    for pixels in components:
        xs,ys=zip(*pixels); x0,x1,y0,y1=min(xs),max(xs),min(ys),max(ys)
        if y0<(15 if cls==3 else 25) or len(pixels)>32 or x1-x0>8 or y1-y0>10:continue
        region=iscloth[max(0,y0-1):min(64,y1+2),max(0,x0-1):min(48,x1+2)]
        if region.sum()<len(pixels)*.8:continue
        for x,y in pixels:a[y,x]=rgba((BLUE if team==0 else RED)[1])
    return Image.fromarray(a)


def idle_sources():
    out = {}
    # Preserve the approved Saber and Lancer scale and silhouettes.
    specs = {
        (0, False): ('approved-saber-lancer.png', (48, 276, 516, 784), (35, 38), 0),
        (0, True): ('approved-saber-lancer.png', (80, 900, 572, 1384), (38, 38), 0),
        (2, False): ('approved-saber-lancer.png', (568, 116, 1048, 784), (36, 50), 1),
        (2, True): ('approved-saber-lancer.png', (592, 784, 1036, 1384), (37, 50), 1),
    }
    row_specs = [(1, 0, 285, (30, 38)), (3, 285, 640, (44, 53)),
                 (4, 640, 943, (31, 42)), (5, 943, 1143, (34, 30)),
                 (6, 1143, 1447, (36, 38))]
    for cls, y0, y1, size in row_specs:
        for rear in (False, True):
            specs[(cls, rear)] = ('classes-keyposes.png', (543 if rear else 0, y0, 1087 if rear else 543, y1), size, 0)
    for (cls, rear), (file, box, size, source_team) in specs.items():
        source = crop(file, box).resize(size, AREA)
        source = material_palette(source, source_team,cls)
        canvas = transparent()
        canvas.alpha_composite(source, (24 - size[0] // 2, 56 - size[1]))
        for team in (0, 1):
            out[(cls, team, rear)] = finish_emblem(faction(canvas, source_team, team), cls, rear, team)
    return out


CONTACT_BOXES = {
    False: [(0, 50, 525, 368), (0, 368, 490, 730), (540, 50, 1086, 375),
            (490, 375, 1086, 755), (0, 730, 540, 1050), (540, 760, 1086, 1050), (0, 1050, 650, 1448)],
    True: [(0, 50, 540, 376), (0, 376, 490, 740), (540, 50, 1086, 375),
           (490, 375, 1086, 780), (0, 740, 540, 1060), (540, 780, 1086, 1060), (0, 1060, 650, 1448)],
}


def contact_sources():
    out = {}
    # Width remains inside the existing cell. Attack reach is visual only: the
    # gameplay and effect trajectories remain entirely controlled by the engine.
    heights = [36, 38, 36, 44, 40, 29, 37]
    widths = [43, 39, 44, 44, 41, 44, 43]
    for rear in (False, True):
        for cls in range(7):
            im = crop('attack-contact-back.png' if rear else 'attack-contact-front.png', CONTACT_BOXES[rear][cls])
            # A low thrust needs a shorter shaft inside 48 px. Compressing the
            # entire source at this stage retains a single coherent contact pose.
            im = material_palette(im.resize((widths[cls], heights[cls]), AREA), 1 if cls == 2 else 0,cls)
            canvas = transparent()
            canvas.alpha_composite(im, (24-widths[cls]//2, 56-heights[cls]))
            for team in (0, 1):
                out[(cls, team, rear)] = finish_emblem(faction(canvas, 1 if cls == 2 else 0, team), cls, rear, team, 'contact')
    return out


def fallen_sources():
    boxes = [(0, 170, 548, 430), (0, 480, 530, 760), (550, 180, 1086, 430),
             (530, 470, 1086, 795), (0, 820, 540, 1080), (550, 840, 1086, 1070), (0, 1120, 550, 1410)]
    out = {}
    for cls, box in enumerate(boxes):
        size = (43, 20 if cls == 3 else 17 if cls == 6 else 16)
        im = material_palette(crop('fallen-keyposes.png', box).resize(size, AREA), 1 if cls == 2 else 0,cls)
        canvas = transparent()
        canvas.alpha_composite(im, (24-size[0]//2, 56-size[1]))
        for team in (0, 1):
            out[(cls, team)] = finish_emblem(faction(canvas, 1 if cls == 2 else 0, team), cls, False, team, 'fallen')
    return out


def shift(image, dx=0, dy=0):
    # Never translate on a tightly bounded runtime frame: doing so can lose a
    # rear rider's lance BEFORE the final bounds check. Keep every source pixel
    # in this padded intermediate, then fit the complete pose in safe_frame.
    padded=image.size==(48,64)
    out = transparent((80,96) if padded else image.size)
    ox=oy=16 if padded else 0
    out.alpha_composite(image, (ox+int(dx),oy+int(dy)))
    before=np.count_nonzero(np.asarray(image)[:,:,3])
    after=np.count_nonzero(np.asarray(out)[:,:,3])
    lost=max(0,int(before-after))
    RENDER_STATS['pre_encoding_clipped_pixels']+=lost
    RENDER_STATS['padded_transforms']+=1
    if lost:raise AssertionError(f'Translation clipped {lost} opaque pixels')
    return out


def layers(image, cls, rear):
    """Partition generated native art into rig regions, not old sprite parts."""
    a = np.asarray(image)
    y, x = np.mgrid[:64, :48]
    alpha = a[:, :, 3] > 0
    top = image.getbbox()[1]
    if cls == 3:
        legs = alpha & (y >= 47) & (x>=12)
        head = alpha & (y < 21) & (x>=17)&(x<=29)
        if top<10:
            weapon=(np.abs(x-(43-.45*(y-3)))<=2)&(y<43)
            grip=(x>=24)&(x<=32)&(y>=20)&(y<33)
        else:
            weapon=(x>=25)&(y>=23)&(y<=28)
            grip=(x>=24)&(x<=32)&(y>=22)&(y<33)
        arms = alpha & (weapon|grip)&~legs&~head
    else:
        legline = 46 if cls != 5 else 48
        legs = alpha & (y >= legline)
        head = alpha & (y < top + (14 if cls == 4 else 11)) & (x < 34)
        arms = alpha & ~legs & ~head & ((x >= 31) | ((x <= 14) & (y >= top+12)))
    masks = [legs & (x < 24), legs & (x >= 24), head, arms]
    occupied = np.logical_or.reduce(masks)
    masks.append(alpha & ~occupied)
    result = []
    for mask in masks:
        part = np.zeros_like(a)
        part[mask] = a[mask]
        result.append(Image.fromarray(part))
    return result


def flutter(image, team, phase, strength=1):
    """Sway only faction cloth, preserving steel/skin/leather and all opacity."""
    a = np.asarray(image)
    cloth = np.array([rgba(v)[:3] for v in (BLUE if team == 0 else RED)])
    mask = np.any(np.all(a[:, :, None, :3] == cloth[None, None, :, :], axis=3), axis=2) & (a[:, :, 3] > 0)
    # Cloth at silhouette edges only, not the chest emblem or internal seams.
    result = a.copy()
    moves={}
    for y in range(25, 54):
        xs = np.where(mask[y])[0]
        if len(xs) < 2:
            continue
        delta = round(math.sin((phase / 8 * math.tau) + y / 7) * strength)
        for x in xs:
            if x < 16 or x > 33:
                xx = x + delta
                if xx!=x and 2 <= xx < 46 and (a[y,xx,3]==0 or mask[y,xx]):
                    moves[(int(x),y)]=(int(xx),y)
    # Se resuelven ocupaciones antes de escribir para conservar cada píxel de tela y sus uniones.
    while True:
        blocked=[source for source,target in moves.items() if a[target[1],target[0],3]>0 and target not in moves]
        if not blocked:break
        for source in blocked:del moves[source]
    for x,y in moves:result[y,x]=0
    for (x,y),(xx,yy) in moves.items():result[yy,xx]=a[y,x]
    if np.count_nonzero(result[:,:,3])!=np.count_nonzero(a[:,:,3]):
        raise AssertionError('La ondulación perdió píxeles opacos')
    return Image.fromarray(result)


def rig(image, cls, rear, team, phase, state):
    left, right, head, arms, body = layers(image, cls, rear)
    if cls==6 and state=='walk':
        # El faldellín original pertenece a la cintura y se compone delante de ambas piernas.
        spans=({46:(17,24),47:(16,24),48:(15,25),49:(15,24),50:(17,24),51:(18,23),52:(20,22)},
               {46:(13,23),47:(13,23),48:(13,23),49:(13,23),50:(15,23),51:(15,24),52:(16,19)})[int(rear)]
        source=np.asarray(image)
        waist=np.zeros((64,48),dtype=bool)
        for y,(x0,x1) in spans.items():waist[y,x0:x1]=source[y,x0:x1,3]>0
        parts=[np.asarray(part).copy() for part in (left,right,head,arms,body)]
        for part in parts:part[waist]=0
        parts[4][waist]=source[waist]
        left,right,head,arms,body=[Image.fromarray(part) for part in parts]
    wave = math.sin(phase * math.tau / 8)
    cos = math.cos(phase * math.tau / 8)
    # Rotate and move on a padded canvas. Finishing measures the complete pose
    # before placing it in the native cell, so no weapon or hoof is clipped.
    out = transparent((80,96))
    def add(part, dx=0, dy=0, angle=0, pivot=(24,35)):
        padded=transparent((80,96));padded.alpha_composite(part,(16,16))
        if angle:
            padded=padded.rotate(angle,NN,center=(pivot[0]+16,pivot[1]+16))
        # Source art has a transparent border on the padded intermediate, even
        # while a blade or hoof passes beyond its eventual native cell.
        alpha=np.asarray(padded)[:,:,3]
        if np.any(alpha[0]) or np.any(alpha[-1]) or np.any(alpha[:,0]) or np.any(alpha[:,-1]):
            raise AssertionError('Rig transform reached padded canvas edge')
        bounds=padded.getbbox()
        if bounds and (bounds[0]+dx<0 or bounds[1]+dy<0 or bounds[2]+dx>80 or bounds[3]+dy>96):
            raise AssertionError('Rig composition would clip transformed pixels')
        RENDER_STATS['padded_layer_compositions']+=1
        out.alpha_composite(padded,(int(dx),int(dy)))
    if state == 'walk':
        stride = round(wave * (3 if cls != 3 else 2))
        # Alternating independent legs, joints retained beneath the body layer.
        angle=8 if cls==3 else 13
        add(left,-stride//2,-max(0,round(cos)),-wave*angle,(19,47))
        add(right,stride//2,-max(0,round(-cos)),wave*angle,(29,47))
        bob = -int(phase in (1, 2, 5, 6))
        add(flutter(body,team,phase,1),0,bob)
        add(arms,0,bob,wave*5,(30,33))
        add(head,0 if cls==3 else round(wave*.55),bob)
    elif state == 'cast':
        rise = [0, 1, 3, 5, 6, 5, 3, 1][phase]
        add(left);add(right)
        add(flutter(body,team,phase,1))
        add(arms,0 if cls==3 else -int(rise>3)+int(phase>=6)+int(cls==0 and phase==5),-rise//(3 if cls==3 else 2),-rise*(.5 if cls==3 else 2.4),(29,35))
        add(head,0,-int(rise>3))
    elif state == 'hurt':
        recoil = [-1, -2, -3, -3, -2, -1, 0, 0][phase]
        support=recoil//2 if cls==3 else 0
        add(left,support);add(right,support)
        settle=0 if phase==7 else phase//3
        add(body,recoil,settle,0 if cls==3 else -recoil*2.4,(24,48))
        add(arms,recoil,settle,0 if cls==3 else recoil*5,(30,35))
        add(head,recoil if cls==3 else recoil-1 if phase<3 else recoil,settle+int(cls==3 and phase in(2,4)))
    elif state == 'attack':
        # Contact source has genuinely different arms/weapon/legs for each class.
        swing = [-5, -3, 0, 2, 4, 3, 1, 0][phase]
        add(left,0,0,swing,(19,47))
        add(right,int(phase in(3,4)),0,-swing,(29,47))
        add(flutter(body,team,phase,1))
        add(arms,int(phase in(3,4)),0,swing*(.5 if cls==3 else 1.4),(29,35))
        add(head,-int(phase<2),-int(phase==2))
    else:
        bob = [0, 0, -1, -1, -1, 0, 0, 0][phase]
        add(left);add(right)
        add(flutter(body,team,phase,1),0,bob)
        add(arms,0,bob,[-6,-4,-2,0,2,4,2,-2][phase],(30,35))
        add(head,int(phase==6),bob)
    return safe_frame(out)


def safe_frame(im):
    """Make runtime margins explicit, without silently accepting cropped pixels."""
    box = im.getbbox()
    if not box:
        raise ValueError('Empty animated frame')
    offset=16 if im.size==(80,96) else 0
    part=im.crop(box)
    if part.width>46 or part.height>55:
        raise AssertionError(('La pose excede el marco sin cambiar su escala',part.size))
    x=max(1,min(47-part.width,box[0]-offset))
    y=max(1,min(56-part.height,box[1]-offset))
    out=transparent();out.alpha_composite(part,(x,y))
    return out


def frame(idle, contact, fallen, cls, team, rear, state, phase):
    if state==0:
        return authored_idle(cls,team,rear,phase)
    if state == 2:
        if phase in (0,1,6,7):
            pose = rig(idle, cls, rear, team, phase, 'attack')
            # Anticipation and recovery stay visibly in a fighting stance.
            pose = shift(pose, [-1,-2,0,0,0,0,1,0][phase], [0,1,0,0,0,0,1,0][phase])
        else:
            pose = rig(contact, cls, rear, team, phase, 'attack')
    elif state == 4:
        if phase < 4:
            pose = rig(idle, cls, rear, team, phase, 'hurt')
            box = pose.getbbox()
            small = pose.crop(box)
            angle = [0, 13, 32, 59][phase]
            small = small.rotate(angle, NN, expand=True)
            maxw = 43
            if small.width > maxw:
                small = small.resize((maxw, round(small.height*maxw/small.width)), NN)
            pose = transparent(); pose.alpha_composite(small, (24-small.width//2, 56-small.height+phase))
        else:
            pose = fallen.copy()
            if rear:
                pose = ImageOps.mirror(pose)
            # Recoil settles into a real authored horizontal body, ending at 7.
            pose = shift(pose, [0,0,0,0,-1,1,0,0][phase], [0,0,0,0,-3,-1,-1,0][phase])
    elif state == 5 and cls == 4:
        # The caster has a separate generated spell contact pose with open hand.
        pose = rig(contact if 2 <= phase <= 5 else idle, cls, rear, team, phase, 'cast')
    else:
        pose = rig(idle, cls, rear, team, phase, STATES[state])
    return safe_frame(pose)


def preview_grid(images, cols, cell=(48,64), scale=3, pad=6):
    rows = math.ceil(len(images)/cols)
    w,h=cell
    out = Image.new('RGBA', (cols*(w+pad)+pad, rows*(h+pad)+pad), '#17262c')
    for i, im in enumerate(images):
        out.alpha_composite(im, (pad+(i%cols)*(w+pad), pad+(i//cols)*(h+pad)))
    return out.resize((out.width*scale,out.height*scale), NN)


def portraits(idles):
    out = transparent((168,48))
    for team in range(2):
        for cls in range(7):
            im = idles[(cls,team,False)]
            top = im.getbbox()[1]
            y = (5 if cls == 3 else top)
            # For a tall lance, identify the actual head instead of the tip.
            if cls == 2: y=17
            if cls == 4: y=14
            head = im.crop((14,y,34,y+20))
            badge=transparent((24,24)); d=ImageDraw.Draw(badge)
            d.polygon([(2,1),(21,1),(23,3),(23,21),(21,23),(2,23),(0,21),(0,3)], fill=INK)
            d.rectangle((2,2,21,21), fill=rgba((BLUE if team==0 else RED)[0]))
            d.line((2,2,21,2), fill=rgba((BLUE if team==0 else RED)[2]))
            badge.alpha_composite(head,(2,2))
            d=ImageDraw.Draw(badge); d.rectangle((2,16,8,22),fill=rgba((BLUE if team==0 else RED)[0]))
            if team==0:
                d.line((5,17,5,21),fill=CREAM);d.line((4,18,6,18),fill=CREAM)
            else:
                d.line((3,18,7,18),fill=CREAM);d.line((3,20,7,20),fill=CREAM)
            out.alpha_composite(badge,(cls*24,team*24))
    return out


def main():
    global ROOT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root',type=Path,default=ROOT)
    parser.add_argument('--idle-only',action='store_true')
    parser.add_argument('--restore-approved-idle',action='store_true')
    parser.add_argument('--output-atlas',type=Path)
    args=parser.parse_args(); ROOT=args.project_root.resolve()
    if args.idle_only or args.restore_approved_idle:
        restore_approved_idle(ROOT,args.output_atlas)
        return
    evidence=ROOT/'build/art-final/units'; evidence.mkdir(parents=True,exist_ok=True)
    keys=evidence/'keyposes';keys.mkdir(exist_ok=True)
    idles,contacts,fallen=idle_sources(),contact_sources(),fallen_sources()
    for (cls,team,rear),im in idles.items():
        im.save(keys/f'{NAMES[cls].lower()}-{team}-{"back" if rear else "front"}-idle.png')
    for (cls,team,rear),im in contacts.items():
        im.save(keys/f'{NAMES[cls].lower()}-{team}-{"back" if rear else "front"}-contact.png')
    atlas=transparent((2304,3584)); allframes={}; sequences=[]; edge=[]
    for direction in range(4):
        rear=direction>=2
        for team in range(2):
            for cls in range(7):
                row=cls+7*team+14*direction
                for state in range(6):
                    frames=[]
                    for phase in range(8):
                        im=frame(idles[(cls,team,rear)],contacts[(cls,team,rear)],fallen[(cls,team)],cls,team,rear,state,phase)
                        if direction%2: im=ImageOps.mirror(im)
                        frames.append(im)
                        box=im.getbbox()
                        if box[0]<1 or box[1]<1 or box[2]>47 or box[3]>60:edge.append([cls,team,direction,state,phase,box])
                        atlas.alpha_composite(im,((state*8+phase)*48,row*64))
                        allframes[(cls,team,direction,state,phase)]=im
                    unique=len({hashlib.sha256(im.tobytes()).hexdigest() for im in frames})
                    sequences.append({'class':NAMES[cls],'owner':team,'direction':direction,'state':STATES[state],'unique_frames':unique})
    bad=[s for s in sequences if s['unique_frames']!=8]
    if edge: raise AssertionError(f'Frame touches unsafe edge: {edge[:10]}')
    # Repeated poses are explicitly reported; no random pixel flicker is used to
    # manufacture uniqueness. Motion is revised in the rig when a check fails.
    if bad:
        (evidence/'duplicate-sequences.json').write_text(json.dumps(bad,indent=2))
        raise AssertionError(f'Duplicate-pose sequences: {len(bad)} {bad[:10]}')
    (evidence/'duplicate-sequences.json').unlink(missing_ok=True)
    icon=Image.open(ROOT/'Content/Textures/Plaguefront/Pixel/unit-icons.png').convert('RGBA')
    output=args.output_atlas if args.output_atlas is not None else ROOT/'Content/Textures/Plaguefront/Pixel/units.png'
    output.parent.mkdir(parents=True,exist_ok=True)
    atlas.save(output)
    preview_grid([idles[(c,t,False)] for t in range(2) for c in range(7)],7,scale=4).save(evidence/'class-lineup-teams-4x.png')
    preview_grid([idles[(c,t,False)] for t in range(2) for c in range(7)],7,scale=1).save(evidence/'class-lineup-native.png')
    preview_grid([contacts[(c,t,False)] for t in range(2) for c in range(7)],7,scale=4).save(evidence/'attack-contact-4x.png')
    preview_grid([idles[(c,t,True)] for t in range(2) for c in range(7)],7,scale=4).save(evidence/'class-lineup-backs-4x.png')
    preview_grid([contacts[(c,t,True)] for t in range(2) for c in range(7)],7,scale=4).save(evidence/'attack-contact-backs-4x.png')
    preview_grid([fallen[(c,t)] for t in range(2) for c in range(7)],7,scale=4).save(evidence/'fallen-teams-4x.png')
    icon.resize((672,192),NN).save(evidence/'unit-icons-4x.png')
    for state,name in enumerate(STATES):
        images=[allframes[(c,t,0,state,f)] for t in range(2) for c in range(7) for f in range(8)]
        preview_grid(images,8,scale=2).save(evidence/f'{name}-sequences-2x.png')
        frames=[preview_grid([allframes[(c,t,0,state,f)] for t in range(2) for c in range(7)],7,scale=3).convert('RGB') for f in range(8)]
        duration=[110,60,56,38,81,75][state]
        frames[0].save(evidence/f'{name}-lineup.gif',save_all=True,append_images=frames[1:],duration=duration,loop=0,disposal=2)
        if state in (1,2,4):
            images=[allframes[(c,t,2,state,f)] for t in range(2) for c in range(7) for f in range(8)]
            preview_grid(images,8,scale=2).save(evidence/f'{name}-back-sequences-2x.png')
    min_shared=9999;min_cloth=9999
    for cls in range(7):
        for direction in range(4):
            for state in range(6):
                for phase in range(8):
                    a=np.asarray(allframes[(cls,0,direction,state,phase)])
                    b=np.asarray(allframes[(cls,1,direction,state,phase)])
                    min_shared=min(min_shared,int(np.sum(np.all(a==b,axis=2)&(a[:,:,3]>0))))
                    for data,swatches in ((a,BLUE),(b,RED)):
                        cloth=np.array([rgba(v)[:3] for v in swatches])
                        count=int(np.any(np.all(data[:,:,None,:3]==cloth[None,None,:,:],axis=3),axis=2).sum())
                        min_cloth=min(min_cloth,count)
    alpha=np.unique(np.asarray(atlas)[:,:,3]).tolist()
    assert alpha==[0,255]
    assert min_shared>=40 and min_cloth>=5,(min_shared,min_cloth)
    report={'atlas':[2304,3584],'frame':[48,64],'anchor':[24,56],'column':'state*8+frame','row':'class+7*owner+14*direction',
            'unit_frames':2688,'nonempty_frames':len(allframes),'sequences':len(sequences),
            'unique_eight_frame_sequences':len(sequences)-len(bad),'duplicate_sequences':bad,
            'alpha_values':alpha,'unsafe_edge_frames':edge,'portrait_atlas':[168,48],
            'pre_encoding_transforms':RENDER_STATS,
            'faction_pairs_checked':1344,'minimum_identical_shared_material_pixels':min_shared,'minimum_faction_cloth_pixels':min_cloth,
            'source_pngs':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(SRC.glob('*.png'))},
            'runtime_sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
            'approved_idle_source_sha256':APPROVED_SHA256,'approved_idle_cells_preserved_exactly':448,
            'berserker_walk_waist_repair':{'cells':64,'states':['walk'],'classes':['Berserker'],
                                         'repair_cause':'La máscara genérica de piernas incluía el faldellín; la pierna derecha ocultaba cuatro de los seis píxeles de las barras de facción.',
                                         'repair':'Se reasignan los mismos píxeles originales de tela y contorno a la capa de cintura delante de ambas piernas, sin redibujar la insignia.'},
            'artwork':'Reposo copiado exactamente del atlas original aprobado; estados restantes conservan el acabado nativo reparado.',
            'states':STATES,'classes':NAMES,'front_back_authored':True,'left_views':'mirrored right views, as existing runtime contract',
            'engine_integration':'Existing paths, dimensions, atlas ordering and pivots unchanged; no game code modified by this script.'}
    (evidence/'validation.json').write_text(json.dumps(report,indent=2))
    (evidence/'sequences.json').write_text(json.dumps(sequences,indent=2))
    print(json.dumps({k:report[k] for k in ['unit_frames','unique_eight_frame_sequences','alpha_values','unsafe_edge_frames','runtime_sha256']},indent=2))


if __name__=='__main__':main()
