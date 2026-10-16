"""Reconstruye únicamente los cinco impactos mágicos desde sus fotogramas originales."""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Tools/Art/Sources/Effects'
OUTPUT = ROOT / 'Content/Textures/Plaguefront/Pixel/effects.png'
EVIDENCE = ROOT / 'build/visual-corrections/effects'
RAMPS = [
    ['101b25','883b37','c5643f','dd7956','e7a451','f1d794','d7ceaa'],
    ['101b25','1d354b','2a5363','417981','83a9a3','849aa0','c2d0c1','e4ede2'],
    ['101b25','34314e','594a76','8a74ac','c3b3d6','d7ceaa','e4ede2'],
    ['1d3734','315444','4f7250','79935a','9fb34f','d5d47a','d7ceaa','f1d794'],
    ['34314e','594a76','8a74ac','c3b3d6','a88d60','dfbd79','d7ceaa','c2d0c1','e4ede2'],
]
NAMES = ['Fire', 'Ice', 'Lightning', 'Heal', 'Purify']
COLUMNS = [2,3,4,5,7]

def main():
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    base = Image.open(SOURCE / 'base-effects.png').convert('RGBA')
    atlas = base.copy()
    original = Image.open(SOURCE / 'caster-impacts.png').convert('RGBA')
    preview = Image.new('RGBA', (4*56,5*66), '#101b25')
    draw = ImageDraw.Draw(preview)
    font = ImageFont.truetype(str(ROOT/'Content/Fonts/DejaVuSans.ttf'),9)
    records = []
    for row,(name,column,colors) in enumerate(zip(NAMES,COLUMNS,RAMPS)):
        palette = np.array([tuple(bytes.fromhex(c)) for c in colors],dtype=np.int32)
        phases=[]
        for phase in range(4):
            box=(round(phase*original.width/4),round(row*original.height/5),round((phase+1)*original.width/4),round((row+1)*original.height/5))
            small=original.crop(box).resize((40,40),Image.Resampling.BOX)
            data=np.array(small)
            delta=data[:,:,None,:3].astype(np.int32)-palette[None,None,:,:]
            selected=np.argmin((delta*delta*np.array([.3,.59,.11])).sum(axis=3),axis=2)
            data[:,:,:3]=palette[selected]
            data[:,:,3]=np.where(data[:,:,3]>=128,255,0)
            data[data[:,:,3]==0]=0
            cell=Image.new('RGBA',(48,48))
            cell.alpha_composite(Image.fromarray(data),(4,4))
            assert cell.getbbox() and cell.getbbox()[0]>=4 and cell.getbbox()[2]<=44
            atlas.paste(cell,(column*48,phase*48))
            preview.alpha_composite(cell,(phase*56+4,row*66+14))
            phases.append(hashlib.sha256(cell.tobytes()).hexdigest())
        assert len(set(phases))==4
        draw.text((3,row*66+2),name,font=font,fill='#d7ceaa')
        records.append({'spell':name,'column':column,'frames':4,'phase_hashes':phases})
    for column in [0,1,6]:
        assert atlas.crop((column*48,0,(column+1)*48,192)).tobytes()==base.crop((column*48,0,(column+1)*48,192)).tobytes()
    assert set(np.unique(np.array(atlas)[:,:,3]))=={0,255}
    atlas.save(OUTPUT)
    preview.resize((preview.width*3,preview.height*3),Image.Resampling.NEAREST).save(EVIDENCE/'caster-impacts-3x.png')
    report={'atlas':[384,192],'cell':[48,48],'changed_columns':COLUMNS,'preserved_columns':[0,1,6],
            'sequences':records,'alpha':[0,255],'source':'image_gen, fases originales distintas con adaptación nativa de escala fija',
            'sha256':hashlib.sha256(OUTPUT.read_bytes()).hexdigest()}
    (EVIDENCE/'validation.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps({'magic_frames':20,'preserved_columns':[0,1,6],'sha256':report['sha256']}))

if __name__=='__main__':
    main()
