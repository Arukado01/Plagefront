"""Build Plaguefront's approved environment pack from local original raster sources.

Source art: built-in image_gen (approved studies + missing material/prop studies).
This deterministic authoring pass performs measured extraction, palette adaptation,
pixel cleanup, restrained four-phase detail animation, and atlas packing. It never
reads rules, edits a map, or changes gameplay. Pillow and numpy are required.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SOURCES = Path(__file__).resolve().parent / "Sources/Environment"
ACTIVE = ROOT / "Content/Textures/Plaguefront/Pixel"
PREVIEWS = ROOT / "build/art-final/environment"
NN, AREA = Image.Resampling.NEAREST, Image.Resampling.BOX
TERRAINS = ["Plain", "Forest", "Hill", "Swamp", "Ruins", "Road", "River", "Bridge", "DeepWater", "Blocked"]
PROPS = ["forest", "ruins", "blocked_rock", "house", "fortress", "outpost", "corpse", "rot"]
RAMPS = {
    "ink": ["101b25", "192c32", "263b3b"],
    "earth": ["493d36", "6b5642", "a88d60", "d7ceaa"],
    "stone": ["394542", "5a6556", "899078", "b1b397"],
    "wood": ["332e2b", "67513b", "94734b", "c39b60"],
    "foliage": ["1d3734", "315444", "4f7250", "79935a", "566a45", "768454"],
    "water": ["1d354b", "2a5363", "417981", "83a9a3"],
    "roof": ["5e3340", "ac4b4c", "dd7956"],
    "iron": ["303e4c", "526674", "849aa0", "c2d0c1"],
    "rot": ["334535", "647d3c", "9fb34f", "d5d47a"],
    "window": ["883b37", "c5643f", "e7a451", "f1d794"],
}


def rgb(value):
    return tuple(bytes.fromhex(value.lstrip("#")))


def color(value):
    return (*rgb(value), 255)


def palette(*names):
    return np.array([rgb(v) for v in dict.fromkeys(v for name in names for v in RAMPS[name])], dtype=np.int32)


def open_source(name):
    return Image.open(SOURCES / name).convert("RGBA")


def adapt(im, names, clean=False):
    """Quantize materials, eliminate isolated noise, retain genuine source alpha."""
    data = np.array(im.convert("RGBA"))
    choices = palette(*names)
    delta = data[:, :, None, :3].astype(np.int32) - choices[None, None, :, :]
    index = np.argmin((delta * delta * np.array([.30, .59, .11])).sum(axis=3), axis=2)
    if clean:
        # A lone material pixel cannot define a readable cluster at native scale.
        before = index.copy()
        for y in range(1, im.height - 1):
            for x in range(1, im.width - 1):
                neighbours = before[y-1:y+2, x-1:x+2].ravel()
                if np.count_nonzero(neighbours == before[y, x]) < 2:
                    index[y, x] = np.bincount(neighbours).argmax()
    data[:, :, :3] = choices[index].astype(np.uint8)
    data[:, :, 3] = np.where(data[:, :, 3] >= 128, 255, 0)
    data[data[:, :, 3] == 0] = 0
    return Image.fromarray(data)


def terrain_variant(q, r):
    """Bit-exact counterpart of PixelRenderer.cpp's unsigned coordinate hash."""
    value = (((q & 0xffffffff) * 0x8da6b343) ^ ((r & 0xffffffff) * 0xd8163841)) & 0xffffffff
    value ^= value >> 16
    value = (value * 0x7feb352d) & 0xffffffff
    value ^= value >> 15
    return value % 4


def detail_seed(x, y, kind, variant):
    return (x * 71 + y * 97 + kind * 149 + variant * 211) % 997


def clipped_overlay(base, overlay):
    alpha = base.getchannel("A")
    base.alpha_composite(overlay)
    base.putalpha(alpha)
    data = np.array(base)
    data[data[:, :, 3] == 0] = 0
    return Image.fromarray(data)


def grass_tuft(draw, x, y, phase, dark="315444", light="79935a"):
    sway = [0, 1, 0, -1][phase]
    draw.line([(x, y), (x + sway, y - 3)], fill=color(dark))
    draw.line([(x, y), (x - 2 + sway, y - 2)], fill=color(light))
    draw.point((x + 1 + (phase == 2), y - 1), fill=color(light))


def animate_water(base, frame, variant):
    """Only reflection pixels move; dark water pools remain anchored to the cell."""
    data = np.array(base)
    original = data.copy()
    light = np.array(rgb("83a9a3"))
    middle = np.array(rgb("417981"))
    highlights = (original[:, :, :3] == light).all(axis=2) & (original[:, :, 3] > 0)
    data[highlights, :3] = middle
    offset = [0, 1, 0, -1][frame]
    for y, x in np.argwhere(highlights):
        xx = int(x) + offset
        if 0 <= xx < base.width and original[y, xx, 3]:
            data[y, xx, :3] = light if (x + y + frame + variant) % 5 else middle
    result = Image.fromarray(data)
    overlay = Image.new("RGBA", base.size)
    draw = ImageDraw.Draw(overlay)
    # Small hand-timed surface glints; no temporal random noise or moving texture.
    for i in range(3):
        x = 11 + (i * 9 + variant * 5) % 24
        y = 10 + (i * 5 + variant * 3) % 13
        x += [0, 1, 0, -1][(frame + i) % 4]
        draw.line((x, y, x + 2 + ((frame+i) % 3 == 1), y), fill=color("417981" if (frame+i) % 4 == 0 else "83a9a3"))
    return clipped_overlay(result, overlay)


def ground_frames():
    approved = open_source("materials-approved.png")
    supplement = open_source("materials-supplement.png")
    mask = open_source("surface-mask-source.png").getchannel("A")
    deck_source = np.array(open_source("bridge-deck-source.png"))
    wood_palette = palette("wood", "earth")
    # Keep the existing deck footprint so the renderer's road joins still agree.
    deck_mask = np.zeros((32, 48), dtype=np.uint8)
    for shade in wood_palette:
        deck_mask[(deck_source[:, :, :3] == shade).all(axis=2)] = 255
    deck_mask = Image.fromarray(deck_mask)
    panel_specs = {
        0: (approved, 2, 3, 0, ["ink", "foliage", "stone"]),
        1: (approved, 2, 3, 1, ["ink", "earth", "wood", "foliage"]),
        2: (supplement, 2, 2, 0, ["ink", "earth", "stone", "foliage"]),
        3: (supplement, 2, 2, 1, ["ink", "earth", "foliage", "water"]),
        4: (approved, 2, 3, 5, ["ink", "stone", "foliage"]),
        5: (approved, 2, 3, 2, ["ink", "earth", "stone"]),
        6: (approved, 2, 3, 3, ["water"]),
        7: (approved, 2, 3, 4, ["wood", "earth"]),
        8: (supplement, 2, 2, 2, ["water"]),
        9: (supplement, 2, 2, 3, ["ink", "stone", "foliage"]),
    }
    bases = {}
    crops = []
    for kind, (source, columns, rows, panel, names) in panel_specs.items():
        pw, ph = source.width // columns, source.height // rows
        panel_box = ((panel % columns)*pw, (panel // columns)*ph, (panel % columns+1)*pw, (panel // columns+1)*ph)
        region = source.crop(panel_box)
        # Each variant uses a different broad region of the original material study.
        # Crop differences change stone/grass/mud composition, not merely its tint.
        cw, ch = round(pw*.70), round(ph*.70)
        for variant, (cx, cy) in enumerate([(0,0), (pw-cw,0), (0,ph-ch), (pw-cw,ph-ch)]):
            box = (cx, cy, cx+cw, cy+ch)
            tile = adapt(region.crop(box).resize((48,32), AREA), names, clean=True)
            tile.putalpha(mask)
            bases[kind, variant] = tile
            crops.append({"terrain": TERRAINS[kind], "variant": variant, "panel": panel_box, "region_crop": box})
    frames = {}
    for variant in range(4):
        for kind in range(10):
            for frame in range(4):
                tile = bases[kind,variant].copy()
                if kind in (3,6,8):
                    # Swamp reflections are confined to the water colors, reeds below.
                    tile = animate_water(tile, frame, variant)
                elif kind == 7:
                    tile = animate_water(bases[6,variant].copy(), frame, variant)
                    deck = bases[kind,variant].copy()
                    deck.putalpha(deck_mask)
                    tile.alpha_composite(deck)
                    tile.putalpha(mask)
                if kind not in (6,7,8):
                    overlay = Image.new("RGBA", (48,32)); draw = ImageDraw.Draw(overlay)
                    positions = [(12+(variant*5)%11,22), (29+(variant*3)%6,13)]
                    for i, (x,y) in enumerate(positions):
                        if kind == 3:
                            # Reed stems sway while their mud roots remain stationary.
                            grass_tuft(draw,x,y,(frame+i)%4,"67513b","a88d60")
                            draw.point((x+[0,1,0,-1][(frame+i)%4],y-4),fill=color("c39b60"))
                        else:
                            grass_tuft(draw,x,y,(frame+i)%4,"315444","768454" if kind in (2,4,5,9) else "79935a")
                    tile = clipped_overlay(tile,overlay)
                frames[kind,variant,frame] = tile
    return frames, crops


def prop_bases():
    approved = open_source("props-approved.png")
    supplement = open_source("props-supplement.png")
    # Measured component boxes, not an assumed equal grid: generated bases differ.
    specs = [
        # Keep an opaque canopy in the upper twelve rows, as expected by the
        # existing hover regression, with the original (32,68) ground anchor.
        (0,approved,(16,36,560,692),(46,59),["ink","wood","foliage"]),
        (1,approved,(604,828,1092,1328),(42,43),["ink","stone","wood","foliage"]),
        (2,supplement,(12,72,552,688),(47,53),["ink","stone","foliage"]),
        (3,approved,(556,32,1108,688),(42,48),["ink","earth","stone","wood","foliage","roof","window"]),
        (4,approved,(28,720,580,1324),(48,58),["ink","earth","stone","wood","foliage","roof","window"]),
        (5,supplement,(568,56,1116,736),(46,54),["ink","earth","stone","wood","foliage","roof","window"]),
        (6,supplement,(16,952,564,1280),(43,25),["ink","earth","wood","iron"]),
        (7,supplement,(568,800,1116,1336),(44,42),["ink","rot"]),
    ]
    bases, windows, metadata = {}, {}, []
    for kind, source, box, size, names in specs:
        crop = source.crop(box).resize(size,AREA)
        canvas = Image.new("RGBA",(64,80))
        position = (32-size[0]//2,68-size[1])
        canvas.alpha_composite(adapt(crop,names),position)
        bases[kind] = canvas
        # Isolate only amber luminous pixels, keeping wood/roof materials unchanged.
        data = np.array(crop)
        r,g,b = (data[:,:,i].astype(np.int32) for i in range(3))
        warm = (r>g*1.17) & (g>b*1.6) & (r>155) & (g>90) & (data[:,:,3]>=128)
        window_mask = Image.new("L",(64,80)); window_mask.paste(Image.fromarray(warm.astype(np.uint8)*255),position)
        windows[kind] = np.array(window_mask)>0
        metadata.append({"prop": PROPS[kind],"source":"props-approved.png" if source is approved else "props-supplement.png", "crop":box,"native_size":size,"position":position})
    return bases, windows, metadata


def prop_frames():
    bases, windows, metadata = prop_bases()
    frames = {}
    for kind in range(8):
        for frame in range(4):
            im = bases[kind].copy()
            if kind == 0:
                # Articulated canopy: upper leaves sway; trunk and roots never move.
                source = np.array(im); data = np.zeros_like(source)
                for y in range(80):
                    shift = [0,1,0,-1][frame] if y<39 else 0
                    for x in range(64):
                        xx = x+shift
                        if 0 <= xx < 64:
                            data[y,xx] = source[y,x]
                im=Image.fromarray(data)
            elif kind in (3,4,5):
                data=np.array(im)
                shades=["e7a451","f1d794","c5643f","e7a451"]
                for y,x in np.argwhere(windows[kind]):
                    data[y,x,:3]=rgb(shades[(frame+(int(x)//2)%2)%4])
                im=Image.fromarray(data)
            elif kind == 7:
                # Only exact rot source shades are used: runtime palette remapping remains valid.
                data=np.array(im); original=data.copy()
                for level,shade in enumerate(RAMPS["rot"]):
                    selected=(original[:,:,:3]==rgb(shade)).all(axis=2)&(original[:,:,3]>0)
                    if level>=1:
                        yy,xx=np.indices(selected.shape)
                        pulse=selected&(((xx+yy+frame*2)%7)<2)
                        data[pulse,:3]=rgb(RAMPS["rot"][min(3,level+1)])
                im=Image.fromarray(data)
            overlay=Image.new("RGBA",(64,80)); draw=ImageDraw.Draw(overlay)
            if kind in (0,1,2):
                x,y = [(26,29),(24,64),(18,65)][kind]
                grass_tuft(draw,x,y,frame)
                # Two independent leaf tips ensure four distinct canopy stages.
                grass_tuft(draw,x+10,y-4,(frame+1)%4)
            elif kind in (3,4,5):
                # Explicit tiny slit glows retain a readable flicker after downsampling.
                x,y={3:(22,35),4:(17,33),5:(28,37)}[kind]
                draw.point((x,y),fill=color(["c5643f","e7a451","f1d794","e7a451"][frame]))
                draw.point((x+1,y+1),fill=color(["e7a451","f1d794","c5643f","883b37"][frame]))
            elif kind == 6:
                # A loose hem settles by one pixel; armour, weapon and body stay anchored.
                x=35+[0,1,0,-1][frame]
                draw.line((x,63,x+3,63),fill=color("67513b"))
                draw.point((x+1,62+(frame//2)),fill=color("94734b"))
            elif kind == 7:
                for i in range(4):
                    x=19+i*8+[0,1,0,-1][(frame+i)%4]
                    y=49-(frame+i)%4*3
                    draw.point((x,y),fill=color(RAMPS["rot"][2+(i+frame)%2]))
            im.alpha_composite(overlay)
            # Every final frame has real binary transparency and clear outer padding.
            data=np.array(im); data[data[:,:,3]==0]=0
            frames[kind,frame]=Image.fromarray(data)
    return frames, metadata


def pack(ground, props):
    terrain=Image.new("RGBA",(1920,128)); prop=Image.new("RGBA",(512,320))
    for (kind,variant,frame),im in ground.items():
        terrain.alpha_composite(im,((kind+10*variant)*48,frame*32))
    for (kind,frame),im in props.items():
        prop.alpha_composite(im,(kind*64,frame*80))
    return terrain,prop


def validate(terrain, props, ground, prop_images):
    assert terrain.size==(1920,128) and props.size==(512,320)
    mask=np.array(open_source("surface-mask-source.png").getchannel("A"))
    for im in (terrain,props):
        data=np.array(im)
        assert set(np.unique(data[:,:,3]))=={0,255}
        assert np.all(data[data[:,:,3]==0]==0)
        assert len(np.unique(data.reshape(-1,4),axis=0))<=128
    ground_sequences=[]
    for kind in range(10):
        variant_hashes=[]
        for variant in range(4):
            frames=[ground[kind,variant,f] for f in range(4)]
            assert all(np.array_equal(np.array(im.getchannel("A")),mask) for im in frames)
            hashes=[hashlib.sha256(im.tobytes()).hexdigest() for im in frames]
            assert len(set(hashes))==4,(kind,variant,"repeated phases")
            variant_hashes.append(hashes[0])
            ground_sequences.append({"terrain":TERRAINS[kind],"variant":variant,"distinct_phases":4,"hashes":hashes})
        assert len(set(variant_hashes))==4,(kind,"repeated variants")
    prop_checks=[]
    for kind in range(8):
        frames=[prop_images[kind,f] for f in range(4)]
        hashes=[hashlib.sha256(im.tobytes()).hexdigest() for im in frames]
        assert len(set(hashes))==4,(kind,"repeated prop phases")
        if kind == 0:
            assert all(np.any(np.asarray(im)[:12, :, 3]) for im in frames), "upper canopy hover contract"
        bboxes=[im.getbbox() for im in frames]
        assert all(b and b[0]>=4 and b[1]>=4 and b[2]<=60 and b[3]<=71 for b in bboxes),(kind,bboxes)
        prop_checks.append({"prop":PROPS[kind],"distinct_phases":4,"bounds":bboxes,"hashes":hashes})
    # Test negative axial coordinates too, and establish all four choices on real maps.
    distributions={}
    for path in sorted((ROOT/"Content/Plaguefront/scenarios").glob("*.json")):
        cells=json.loads(path.read_text(encoding="utf-8"))["cells"]
        counts=[0]*4
        for cell in cells:
            counts[terrain_variant(*cell["pos"])]+=1
        assert all(counts),counts
        distributions[path.name]=counts
    return {"pass":True,"terrain_size":[1920,128],"props_size":[512,320],"terrain_frames":160,"prop_frames":32,
            "variants_per_terrain":4,"phases_per_sequence":4,"environment_fps":5,"alpha":[0,255],
            "terrain_mask_preserved":True,"props_outer_padding_pass":True,"coordinate_variant_distribution":distributions,
            "terrain_sequences":ground_sequences,"prop_sequences":prop_checks,
            "visual_review":"Contact sheets and animation previews must additionally be inspected; hashes do not establish artistic quality."}


def preview(terrain, props, directory):
    directory.mkdir(parents=True,exist_ok=True)
    font_path=ROOT/"Content/Fonts/DejaVuSans.ttf"
    font=lambda size:ImageFont.truetype(str(font_path),size)
    sheet=Image.new("RGBA",(1220,470),"#10191f"); draw=ImageDraw.Draw(sheet)
    draw.text((20,12),"PLAGUEFRONT / 4 variantes por terreno",font=font(22),fill="#cfb779")
    for kind,name in enumerate(TERRAINS):
        x=20+kind*120
        draw.text((x,53),name,font=font(13),fill="#e0e2d5")
        for variant in range(4):
            im=terrain.crop(((kind+variant*10)*48,0,(kind+variant*10+1)*48,32))
            sheet.alpha_composite(im.resize((96,64),NN),(x,80+variant*94))
    sheet.save(directory/"terrain-variants-2x.png")
    prop_sheet=Image.new("RGBA",(1080,242),"#10191f"); draw=ImageDraw.Draw(prop_sheet)
    draw.text((20,10),"Construcciones y entorno / escala 2x",font=font(20),fill="#cfb779")
    for kind,name in enumerate(PROPS):
        x=10+kind*134
        draw.text((x,46),name,font=font(12),fill="#e0e2d5")
        prop_sheet.alpha_composite(props.crop((kind*64,0,(kind+1)*64,80)).resize((128,160),NN),(x,69))
    prop_sheet.save(directory/"props-2x.png")
    terrain_frames=[]; props_frames=[]
    for frame in range(4):
        ground=Image.new("RGBA",(480,128),"#10191f")
        for variant in range(4):
            ground.alpha_composite(terrain.crop((variant*480,frame*32,(variant+1)*480,(frame+1)*32)),(0,variant*32))
        terrain_frames.append(ground.resize((960,256),NN).convert("RGB"))
        props_frames.append(props.crop((0,frame*80,512,(frame+1)*80)).resize((1024,160),NN).convert("RGB"))
    terrain_frames[0].save(directory/"terrain-animation-5fps.gif",save_all=True,append_images=terrain_frames[1:],duration=200,loop=0)
    props_frames[0].save(directory/"props-animation-5fps.gif",save_all=True,append_images=props_frames[1:],duration=200,loop=0)
    board_preview(terrain,props,directory)


def board_preview(terrain,props,directory):
    scenario=json.loads((ROOT/"Content/Plaguefront/scenarios/highlands.json").read_text(encoding="utf-8"))
    canvas=Image.new("RGBA",(520,520),"#11171d"); draw=ImageDraw.Draw(canvas)
    def point(cell):
        q,r=cell["pos"]
        return (48+q*30,86+q*14+r*28-cell["height"]*6)
    queue=[]
    for cell in scenario["cells"]:
        depth=14*cell["pos"][0]+28*cell["pos"][1]
        queue.append((depth,0,cell))
        queue.append((depth+.5,1,cell))
    for _,kind,cell in sorted(queue,key=lambda entry:(entry[0],entry[1])):
        x,y=point(cell)
        if kind==0:
            top=[(x+10,y-14),(x+20,y),(x+10,y+14),(x-10,y+14),(x-20,y),(x-10,y-14)]
            extrusion=max(4,6*(cell["height"]+1))
            shades=["#223140","#1e2c38","#182431"] if cell["terrain"] in ("River","DeepWater") else ["#564838","#453930","#302f2c"]
            for side in range(3):
                a,b=top[side+1],top[side+2]
                draw.polygon([a,b,(b[0],b[1]+extrusion),(a[0],a[1]+extrusion)],fill=shades[side])
            column=TERRAINS.index(cell["terrain"])+10*terrain_variant(*cell["pos"])
            canvas.alpha_composite(terrain.crop((column*48,0,(column+1)*48,32)),(x-24,y-16))
        else:
            prop={"Forest":0,"Ruins":1,"Blocked":2}.get(cell["terrain"])
            if cell.get("building"):prop={"House":3,"Fortress":4,"Outpost":5}[cell["building"]]
            if prop is not None:
                canvas.alpha_composite(props.crop((prop*64,0,(prop+1)*64,80)),(x-32,y-68))
    canvas.save(directory/"board-environment-1x.png")
    canvas.resize((1040,1040),NN).save(directory/"board-environment-2x.png")


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir",type=Path,default=ACTIVE)
    parser.add_argument("--preview-dir",type=Path,default=PREVIEWS)
    parser.add_argument("--verify-only",action="store_true",help="Regenerate in memory and compare existing atlas bytes; do not write files.")
    args=parser.parse_args()
    ground,crops=ground_frames(); prop_images,prop_metadata=prop_frames()
    terrain,props=pack(ground,prop_images)
    report=validate(terrain,props,ground,prop_images)
    report["sources"]={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(SOURCES.glob("*.png"))}
    report["source_material_crops"]=crops
    report["source_prop_crops"]=prop_metadata
    if args.verify_only:
        for name,im in [("terrain.png",terrain),("props.png",props)]:
            actual=Image.open(args.output_dir/name).convert("RGBA")
            assert actual.size==im.size and actual.tobytes()==im.tobytes(),name
    else:
        args.output_dir.mkdir(parents=True,exist_ok=True)
        terrain.save(args.output_dir/"terrain.png"); props.save(args.output_dir/"props.png")
        preview(terrain,props,args.preview_dir)
        (args.preview_dir/"validation.json").write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    print(json.dumps({"pass":True,"verify_only":args.verify_only,"terrain_frames":160,"prop_frames":32,"terrain_size":terrain.size,"props_size":props.size,"output_dir":str(args.output_dir)},ensure_ascii=False))


if __name__=="__main__":
    main()
