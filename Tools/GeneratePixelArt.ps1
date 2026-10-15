param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))

# Genera arte original con píxeles y agrupaciones enteras a resolución nativa, sin filtros, interpolación, suavizado ni conexión de red.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$outputDir = Join-Path $ProjectRoot 'Content/Textures/Plaguefront/Pixel'
$previewDir = Join-Path $ProjectRoot 'build/phase4-art'
New-Item -ItemType Directory -Force -Path $outputDir,$previewDir | Out-Null
$source = @'
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;

public static class PlaguefrontPixelArt {
    static int C(string h) { return unchecked((int)(0xff000000u|Convert.ToUInt32(h,16))); }
    static readonly int Ink=C("101b25"), Deep=C("192c32"), Shadow=C("263b3b"), Soil=C("493d36"), Dirt=C("6b5642"), Sand=C("a88d60"), Pale=C("d7ceaa");
    static readonly int Iron0=C("303e4c"), Iron1=C("526674"), Iron2=C("849aa0"), Iron3=C("c2d0c1");
    static readonly int Stone0=C("394542"), Stone1=C("5a6556"), Stone2=C("899078"), Stone3=C("b1b397");
    static readonly int Wood0=C("332e2b"), Wood1=C("67513b"), Wood2=C("94734b"), Wood3=C("c39b60");
    static readonly int Leaf0=C("1d3734"), Leaf1=C("315444"), Leaf2=C("4f7250"), Leaf3=C("79935a"), Grass=C("566a45"), GrassLight=C("768454");
    static readonly int Water0=C("1d354b"), Water1=C("2a5363"), Water2=C("417981"), Water3=C("83a9a3");
    static readonly int Blue0=C("1e3e76"), Blue1=C("2866b7"), Blue2=C("67a9e3"), Blue3=C("aad5f0");
    static readonly int Crimson0=C("571c32"), Crimson1=C("a92e4c"), Crimson2=C("de5b6c"), Crimson3=C("f4b09c");
    static readonly int Ochre0=C("543f37"), Ochre1=C("8b6242"), Ochre2=C("b98a50"), Ochre3=C("dfbd79");
    static readonly int Violet0=C("34314e"), Violet1=C("594a76"), Violet2=C("8a74ac"), Violet3=C("c3b3d6");
    static readonly int Skin0=C("775344"), Skin1=C("b88c67"), Skin2=C("e0b98a"), Red0=C("5e3340"), Red1=C("ac4b4c"), Red2=C("dd7956");
    static readonly int Rot0=C("334535"), Rot1=C("647d3c"), Rot2=C("9fb34f"), Rot3=C("d5d47a"), Fire0=C("883b37"), Fire1=C("c5643f"), Fire2=C("e7a451"), Fire3=C("f1d794");
    static readonly int[] Hex={34,2,44,16,34,30,14,30,4,16,14,2};
    class Px {
        public readonly int W,H; public readonly int[] Data; public int ClippedMarks;
        public Px(int w,int h) {W=w;H=h;Data=new int[w*h];}
        public int Get(int x,int y) {return x>=0&&x<W&&y>=0&&y<H?Data[y*W+x]:0;}
        public void Dot(int x,int y,int col) {if(x>=0&&x<W&&y>=0&&y<H)Data[y*W+x]=col;else if(col!=0)ClippedMarks++;}
        public void Box(int x,int y,int w,int h,int col) {for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)Dot(xx,yy,col);}
        public void Line(int x0,int y0,int x1,int y1,int col,int width=1) {int dx=Math.Abs(x1-x0),sx=x0<x1?1:-1,dy=-Math.Abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;while(true){Box(x0-width/2,y0-width/2,width,width,col);if(x0==x1&&y0==y1)break;int e2=e*2;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}}
        public void Poly(int col,params int[] a) {int minx=W,maxx=0,miny=H,maxy=0,n=a.Length/2;for(int i=0;i<n;i++){minx=Math.Min(minx,a[i*2]);maxx=Math.Max(maxx,a[i*2]);miny=Math.Min(miny,a[i*2+1]);maxy=Math.Max(maxy,a[i*2+1]);}for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++){bool inside=false;double xx=x+.5,yy=y+.5;for(int i=0,j=n-1;i<n;j=i++){int xi=a[i*2],yi=a[i*2+1],xj=a[j*2],yj=a[j*2+1];if(((yi>yy)!=(yj>yy))&&(xx<(double)(xj-xi)*(yy-yi)/(yj-yi)+xi))inside=!inside;}if(inside)Dot(x,y,col);}}
        public void Stamp(Px src,int x,int y,bool mirror=false) {for(int yy=0;yy<src.H;yy++)for(int xx=0;xx<src.W;xx++){int c=src.Get(mirror?src.W-1-xx:xx,yy);if(c!=0)Dot(x+xx,y+yy,c);}}
        public void Outline(int color) {int[] old=(int[])Data.Clone();for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(old[y*W+x]==0){bool touch=false;for(int d=0;d<4;d++){int xx=x+(d==0?-1:d==1?1:0),yy=y+(d==2?-1:d==3?1:0);if(xx>=0&&xx<W&&yy>=0&&yy<H&&old[yy*W+xx]!=0)touch=true;}if(touch)Data[y*W+x]=color;}}
        public void Save(string path) {using(var b=new Bitmap(W,H,PixelFormat.Format32bppArgb)){var data=b.LockBits(new Rectangle(0,0,W,H),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);System.Runtime.InteropServices.Marshal.Copy(Data,0,data.Scan0,Data.Length);b.UnlockBits(data);b.Save(path,ImageFormat.Png);}}
        public Px Zoom(int n){var r=new Px(W*n,H*n);for(int y=0;y<H;y++)for(int x=0;x<W;x++)r.Box(x*n,y*n,n,n,Get(x,y));return r;}
    }
    static int Hash(int x,int y,int seed) {unchecked {uint n=(uint)(x*374761393+y*668265263+seed*1274126177);n=(n^(n>>13))*1274126177;return (int)((n^(n>>16))&0x7fffffff);}}
    static void Speckle(Px c,int baseColor,int light,int dark,int seed,int chance) {for(int y=0;y<c.H;y++)for(int x=0;x<c.W;x++)if(c.Get(x,y)==baseColor){int h=Hash(x/2,y,seed)%100;if(h<chance){c.Dot(x,y,light);if(Hash(x,y,seed)%3==0&&c.Get(x+1,y)==baseColor)c.Dot(x+1,y,light);}else if(h>97)c.Dot(x,y,dark);}}
    static void ShadowAt(Px c,int x,int y,int w) {c.Poly(Deep,x-w/2+3,y-3,x+w/2-3,y-3,x+w/2,y,x+w/2-4,y+3,x-w/2+4,y+3,x-w/2,y);for(int a=x-w/2;a<x+w/2;a+=3)c.Dot(a,y+2,Shadow);}
    static void Blade(Px c,int x,int y,int dx,int dy,int length,bool dagger=false) {int ex=x+dx*length,ey=y+dy*length;c.Line(x,y,ex,ey,Ink,3);c.Line(x,y,ex,ey,Iron2,2);c.Line(x,y,ex,ey,Iron3);c.Dot(ex+dx,ey+dy,Iron3);int px=-dy,py=dx;c.Line(x+px*3,y+py*3,x-px*3,y-py*3,Wood3);c.Line(x-dx,y-dy,x-dx*4,y-dy*4,Wood1,2);c.Dot(x-dx*4,y-dy*4,Ochre3);}

    static Px Terrain(int kind,int frame) {
        var c=new Px(48,32);
        int surface=kind==0?Grass:kind==1?Leaf1:kind==2?Stone2:kind==3?Leaf1:kind==4?Stone1:kind==5?Dirt:kind==6?Water1:kind==7?Water1:kind==8?Water0:Stone0;
        c.Poly(surface,Hex);
        // Redibuja las agrupaciones sobre la superficie más alta a resolución nativa, sin ampliar texturas anteriores.
        for(int y=2;y<=30;y++)for(int x=4;x<=44;x++)if(c.Get(x,y)!=0){
            int n=Hash(x,y,kind+34)%100;if(n<6)c.Dot(x,y,kind>=6&&kind<=8?Water2:kind==5?Wood1:kind==2?Stone3:kind==9?Stone1:GrassLight);
            if(n>96)c.Dot(x,y,kind>=6&&kind<=8?Water0:Soil);
        }
        if(kind==0||kind==1)for(int k=0;k<9;k++){int x=10+(k*7)%29,y=7+(k*11)%19;if(c.Get(x,y)!=0){c.Dot(x,y,Leaf3);if(c.Get(x-1,y-1)!=0)c.Dot(x-1,y-1,GrassLight);if(c.Get(x+1,y-2)!=0)c.Dot(x+1,y-2+(k+frame)%2,Leaf2);}}
        if(kind==2){c.Poly(Stone1,8,15,16,7,24,10,31,5,40,16,33,27,20,28,11,23);c.Poly(Stone2,10,14,17,9,24,13,31,8,37,16,30,24,20,25,13,20);c.Line(14,13,21,15,Stone3);c.Line(29,12,34,15,Stone3);c.Line(12,22,19,24,Stone0);c.Line(26,26,32,23,Stone0);}
        if(kind==3){c.Poly(Water0,7,15,15,8,25,9,32,17,23,26,11,23);c.Poly(Water1,10,15,17,11,24,12,28,18,20,24,12,21);c.Line(12+frame%2,15,18+frame%2,15,Water2);c.Line(24-frame%3,24,28-frame%3,24,Leaf2);for(int k=0;k<5;k++){int x=12+k*6,y=9+(k%3)*8;c.Line(x,y,x+((frame+k)%3-1),y-4,Wood2);c.Dot(x,y-5,Wood3);c.Line(x+2,y,x+3,y-3,Leaf3);}}
        if(kind==4){for(int k=0;k<10;k++){int x=12+(k*5)%25,y=8+(k*7)%18;c.Box(x,y,4,2,Stone0);c.Box(x,y,3,1,Stone2);}c.Poly(Grass,10,21,17,19,22,24,31,23,36,28,15,28);}
        if(kind==5){c.Poly(Soil,4,12,14,2,44,21,34,30);c.Poly(Sand,5,14,14,5,43,19,34,27);for(int k=0;k<13;k++){int x=9+(k*7)%28,y=9+(k*5)%15;if(c.Get(x,y)==Sand){c.Line(x,y,x+2,y+1,Dirt);c.Dot(x+2,y+1,Wood3);}}c.Line(7,16,32,28,Wood1);c.Line(16,5,42,18,Wood3);}
        if(kind==6||kind==7||kind==8){for(int k=0;k<13;k++){int y=5+(k*7)%24,x=8+(k*11+frame*2)%29;for(int a=0;a<3+k%5;a++)if(c.Get(x+a,y)!=0)c.Dot(x+a,y,k%3==0?Water3:Water2);}
            if(kind==7){c.Poly(Wood0,5,11,13,3,44,21,35,30);for(int k=0;k<9;k++){int x=6+k*4,y=10+k;c.Poly(k%2==0?Wood2:Wood1,x,y,x+3,y-3,x+13,y+4,x+10,y+7);c.Line(x+1,y,x+9,y+5,Wood3);}c.Line(7,11,36,29,Wood3);c.Line(13,3,44,21,Wood2);c.Box(7,8,2,6,Wood0);c.Box(39,22,2,5,Wood0);}}
        if(kind==9){c.Poly(Stone1,7,16,14,7,22,12,30,5,42,16,33,28,14,27);c.Line(14,12,22,18,Stone0);c.Line(23,18,30,12,Stone0);c.Line(22,18,20,26,Stone0);c.Line(29,14,35,17,Stone2);}
        // Recorta las agrupaciones decorativas a los seis bordes de la superficie antes de dibujar el contorno.
        var mask=new Px(48,32);mask.Poly(Pale,Hex);for(int y=0;y<32;y++)for(int x=0;x<48;x++)if(mask.Get(x,y)==0)c.Dot(x,y,0);
        int edgeLight=kind==6||kind==8?Water2:kind==3?Leaf2:Stone2,edgeDark=kind>=6&&kind<=8?Water0:Soil;
        c.Line(14,2,34,2,edgeLight);c.Line(4,16,14,2,kind==6||kind==8?Water2:Leaf2);c.Line(4,16,14,30,edgeDark);c.Line(14,30,34,30,edgeDark);c.Line(34,30,44,16,edgeDark);
        return c;
    }

    static void Canopy(Px c,int x,int y,int w,int h,int frame,int seed) {
        int sway=frame==1?1:frame==3?-1:0;x+=sway;
        c.Poly(Leaf0,x-w/2,y+h-3,x-w/2-2,y+h/2,x-w/3,y+h/2-4,x-w/3+1,y+5,x-4,y+5,x-1,y,x+7,y+2,x+9,y+6,x+w/3,y+7,x+w/3+2,y+h/2,x+w/2,y+h/2+4,x+w/2-1,y+h-4,x+4,y+h+1);
        c.Poly(Leaf1,x-w/2+2,y+h/2,x-w/3+3,y+8,x-1,y+2,x+6,y+4,x+w/3-2,y+10,x+w/2-2,y+h/2+4,x+w/3,y+h-4,x-4,y+h-1,x-w/3,y+h-5);
        c.Poly(Leaf2,x-w/3+2,y+8,x-1,y+3,x+5,y+5,x+8,y+10,x+2,y+13,x-9,y+13,x-w/3+1,y+17);
        c.Poly(Leaf2,x-12,y+h/2+2,x-4,y+h/2-1,x+3,y+h/2+4,x-4,y+h/2+8,x-13,y+h/2+8);
        for(int k=0;k<14;k++){int xx=x-w/3+Hash(k,5,seed)%(w*2/3),yy=y+5+Hash(k,1,seed)%(h-7);int v=c.Get(xx,yy);if(v==Leaf1||v==Leaf2){c.Box(xx,yy,2,1,Leaf3);if(k%3==0)c.Dot(xx+1,yy-1,Leaf2);}}
        c.Line(x-5,y+h-1,x+3,y+h,Leaf0);c.Box(x+3,y+h-2,3,2,Leaf0);
    }
    static void PixelTree(Px c,int x,int baseY,int height,int frame,int seed) {
        ShadowAt(c,x,baseY,29);c.Poly(Wood0,x-4,baseY,x-3,baseY-22,x+1,baseY-31,x+4,baseY-1);c.Box(x-2,baseY-24,3,23,Wood1);c.Line(x-1,baseY-22,x-1,baseY-4,Wood2);c.Line(x-1,baseY-13,x-9,baseY-22,Wood1,2);c.Line(x+1,baseY-17,x+8,baseY-29,Wood1,2);c.Box(x-7,baseY-2,13,2,Wood0);
        Canopy(c,x,baseY-height,31,height-21,frame,seed);Canopy(c,x-7,baseY-height+17,22,21,(frame+1)%4,seed+1);Canopy(c,x+7,baseY-height+14,23,23,frame,seed+2);
    }
    static void Wall(Px c,int x,int y,int w,int h) {
        c.Box(x,y,w,h,Stone0);c.Box(x,y,w-2,h-1,Stone1);c.Line(x,y,x+w-2,y,Stone3);
        for(int yy=y+1;yy<y+h;yy+=5){c.Line(x,yy+4,x+w-2,yy+4,Stone0);for(int xx=x+((yy-y)/5%2)*5;xx<x+w-2;xx+=9){c.Line(xx,yy,xx,yy+3,Stone0);if(xx+2<x+w-2)c.Line(xx+2,yy+1,Math.Min(xx+6,x+w-3),yy+1,Stone2);}}
    }
    static void Roof(Px c,int x,int y,int w,int h) {
        c.Poly(Wood0,x-w/2-1,y+h,x-2,y-2,x+w/2+2,y+h,x+5,y+h+4);c.Poly(Red0,x-w/2,y+h,x-1,y,x+w/2,y+h,x+4,y+h+2);c.Poly(Wood2,x-w/2+2,y+h-1,x-1,y+1,x+2,y+h+1);
        for(int yy=3;yy<h;yy+=3){int inset=(h-yy)*w/(h*2);c.Line(x-w/2+inset,y+yy,x+w/2-inset,y+yy,yy%2==0?Wood1:Red1);for(int xx=x-w/2+inset+((yy/3)%2)*4;xx<x+w/2-inset;xx+=7)c.Dot(xx,y+yy+1,Wood0);}c.Line(x-1,y,x+w/2,y+h,Wood3);
    }
    static void Flag(Px c,int x,int y,int frame,int team=0) {c.Line(x,y,x,y-17,Iron2);c.Dot(x,y-18,Ochre3);int f=frame==1?1:frame==3?-1:0;int fold=frame==2?-2:0;int cloth=team==0?Blue1:Crimson2;c.Poly(cloth,x+1,y-16,x+12+fold,y-14+f,x+9+fold,y-9+f,x+1,y-10);c.Line(x+2,y-15,x+9+fold,y-13+f,team==0?Blue3:Crimson3);if(team==0){c.Box(x+4,y-14,1,4,Pale);c.Box(x+3,y-13,3,1,Pale);}else{c.Dot(x+3,y-13,Ink);c.Dot(x+4,y-12,Ink);c.Dot(x+5,y-13,Ink);}}
    static void Tower(Px c,int x,int y,int w,int h) {Wall(c,x,y,w,h);c.Poly(Stone0,x+w-2,y,x+w+4,y-3,x+w+4,y+h-3,x+w-2,y+h);for(int a=0;a<w;a+=5)c.Box(x+a,y-4,3,6,Stone2);c.Line(x,y+3,x+w,y+3,Stone0);c.Box(x+w/2-2,y+10,3,7,Ink);c.Box(x+w/2-1,y+11,1,4,Wood3);if(h>27){c.Box(x+w/2-2,y+25,3,6,Ink);c.Dot(x+w/2-1,y+26,Wood3);}}
    static Px Prop(int kind,int frame) {
        var c=new Px(64,80);
        if(kind==0){PixelTree(c,21,63,49,(frame+1)%4,55);PixelTree(c,41,66,59,frame,41);c.Box(12,66,4,1,Leaf3);c.Box(48,65,4,1,GrassLight);}
        if(kind==1){ShadowAt(c,32,68,48);Wall(c,13,42,10,23);Wall(c,21,37,12,29);Wall(c,34,49,16,17);c.Poly(Stone0,31,38,35,34,37,48,33,66,31,65);c.Box(17,40,4,5,Stone2);c.Box(22,34,6,4,Stone2);c.Box(27,38,4,10,Ink);c.Box(43,46,6,5,Stone2);for(int k=0;k<8;k++){int x=11+k*5,y=65+k%3;c.Box(x,y,4,2,Stone0);c.Box(x,y,3,1,Stone2);}c.Line(14,57,17,61,Leaf2);c.Line(17,61,19,55,Leaf2);c.Dot(15+frame%2,52,Leaf3);}
        if(kind==2){ShadowAt(c,32,68,47);c.Poly(Stone0,9,63,15,39,25,31,28,41,40,23,48,43,56,65,39,71,23,69);c.Poly(Stone1,10,62,17,40,25,33,26,55,20,65);c.Poly(Stone2,17,40,25,33,23,47,20,45,14,57);c.Poly(Stone1,27,52,40,26,48,47,47,61,37,66);c.Poly(Stone3,35,38,40,26,42,41,39,38,32,51);c.Line(27,51,26,62,Ink);c.Line(42,51,48,58,Stone0);c.Box(22,66,3,1,Stone2);c.Box(47,66,4,1,Stone2);}
        if(kind==3){ShadowAt(c,32,68,48);Wall(c,15,43,29,22);c.Poly(Stone0,44,43,53,38,53,61,44,65);c.Line(47,44,47,61,Stone1);Roof(c,29,25,39,19);c.Poly(Wood0,40,35,48,30,57,40,50,44);c.Poly(Red0,41,34,48,31,55,40,49,42);Wall(c,45,23,5,13);c.Box(44,22,7,2,Stone3);c.Box(26,52,9,13,Wood0);c.Box(27,54,6,10,Wood1);c.Line(30,55,30,63,Wood0);c.Dot(32,59,Ochre3);c.Box(18,48,5,6,Ink);c.Box(19,49,3,4,Wood3);c.Dot(19+frame%3,49+frame%2,Fire3);c.Line(20,49,20,52,Wood1);c.Line(18,51,22,51,Wood1);c.Box(42,53,3,6,Ink);c.Line(15,64,43,64,Stone2);c.Line(12,67,40,67,Dirt);}
        if(kind==4){ShadowAt(c,32,68,59);Wall(c,8,46,45,23);c.Poly(Stone0,52,47,59,41,59,65,52,69);Tower(c,5,31,13,35);Tower(c,45,27,12,39);Tower(c,24,28,15,40);Roof(c,31,17,21,13);c.Box(26,34,3,5,Ink);c.Box(34,35,2,5,Ink);c.Dot(27,35,Wood3);c.Dot(34,36,Wood3);c.Dot(27,35+frame%3,Fire3);c.Dot(34,36+frame%2,Wood3);for(int x=18;x<46;x+=6)c.Box(x,42,4,6,Stone2);c.Poly(Ink,25,68,25,56,29,51,34,51,38,56,38,68);c.Box(27,57,9,11,Wood1);for(int x=28;x<37;x+=3)c.Line(x,57,x,67,Wood0);c.Line(26,67,37,67,Wood3);c.Box(28,57,2,2,Wood2);c.Box(7,66,8,2,Stone3);c.Box(46,66,9,2,Stone3);}
        if(kind==5){ShadowAt(c,32,68,51);Wall(c,10,49,41,18);for(int x=10;x<51;x+=7)c.Box(x,45,4,7,Stone2);Tower(c,23,30,18,36);Roof(c,32,17,28,15);c.Box(28,51,8,16,Wood0);c.Box(29,53,5,13,Wood1);c.Dot(33,58,Wood3);c.Dot(31,41+frame%3,frame%2==0?Fire3:Wood3);c.Line(11,62,21,62,Stone2);c.Line(42,61,49,61,Stone2);}
        if(kind==6){ShadowAt(c,31,68,31);c.Poly(Ochre0,17,65,28,60,43,64,42,69,26,70);c.Box(21,62,10,4,Iron1);c.Box(17,61,5,5,Skin1);c.Box(15,60,5,4,Pale);c.Dot(16,61,Ink);c.Dot(18,61,Ink);c.Box(30,66,11,2,Wood1);c.Line(33,63,47,59,Iron2);c.Line(37,64,39,60,Wood3);c.Dot(25+frame%2,67,Red0);}
        if(kind==7){ShadowAt(c,32,68,42);for(int k=0;k<7;k++){int x=11+k*7,y=65+(k%2)*3;c.Line(32,65,x,y,Rot0,2);c.Line(32,64,x+3,y-6,Rot1);c.Dot(x,y,Rot2);}c.Poly(Rot0,20,65,25,54,31,50,39,57,45,67,37,71,26,70);c.Poly(Rot1,23,64,28,55,32,53,35,59,39,65,34,68,27,67);c.Box(27,58,6,4,Rot2);c.Line(27,58,31,57,Rot3);for(int k=0;k<8;k++){int x=19+(k*5)%26,y=56+(k*7)%12;c.Box(x,y,2,2,k%2==frame%2?Rot2:Rot0);c.Dot(x,y,Rot3);}for(int k=0;k<5;k++){int yy=43+(k*4+frame*3)%19,xx=18+(k*7+frame)%28;c.Dot(xx,yy,Rot2);c.Dot(xx+1,yy-1,Rot3);}}
        return c;
    }

    // Combina máscaras de rostro, armadura, botas, ropa y arreos con esqueletos articulados de ocho poses a resolución nativa.
    static void Mask(Px c,int x,int y,string[] rows,params int[] colors) {
        for(int yy=0;yy<rows.Length;yy++)for(int xx=0;xx<rows[yy].Length;xx++){
            char key=rows[yy][xx];if(key=='.')continue;int k=key<='9'?key-'0':key-'A'+10;
            if(k>=0&&k<colors.Length)c.Dot(x+xx,y+yy,colors[k]);
        }
    }
    static void Crest(Px c,int x,int y,int team) {
        if(team==0){c.Box(x+1,y,1,4,Pale);c.Box(x,y+1,3,1,Pale);}
        else{c.Box(x,y,3,1,Pale);c.Box(x,y+2,3,1,Pale);}
    }
    static void Shield(Px c,int x,int y,int team,int phase) {
        Mask(c,x,y,new[]{"..00000..",".0123320.","012222210","012222210","012222210","012222210","012222210",".0122210.","..01210..","...010...","....0...."},
            Ink,Iron1,team==0?Blue1:Crimson1,Iron3);
        Crest(c,x+3,y+2,team);c.Line(x+1,y+2,x+2,y+7,team==0?Blue2:Crimson2);c.Dot(x+6,y+7,Iron0);
    }
    static void CharacterHead(Px c,int x,int y,int cls,int team,bool rear,int phase) {
        int cloth0=team==0?Blue0:Crimson0,cloth1=team==0?Blue1:Crimson1,cloth2=team==0?Blue2:Crimson2;
        // Define el perfil con frente, ceja y ojo, nariz saliente, mejilla y mandíbula.
        Mask(c,x-4,y,new[]{"..00000...",".0111110..","012223110.","012233310.","0112033310",".01223310.",".01222310.","..011210..","...0110...","....00...."},
            Ink,Skin0,Skin1,Skin2);
        if(cls==1||cls==5){
            Mask(c,x-5,y-2,rear?new[]{"...00000...","..0112110..",".012222210.","01222222210","01222222210","01222222210","01222222210",".012222210.","..0122210..","..0122210..","...01110..."}:
                new[]{"...00000...","..0112110..",".012222210.","01222111210","012210...10","01210....0.","01210......",".0110......","..010......","..010......","...0......."},Ink,cloth0,cls==5?Iron1:cloth1);
            c.Line(x-3,y-1,x,y-1,cloth2);if(cls==5&&!rear){c.Line(x,y+6,x+3,y+6,cloth1);c.Dot(x+2,y+4,Iron3);}
        }else if(cls==4){
            // Distingue el ala plegada y la copa doblada del sombrero.
            Mask(c,x-6,y-9,new[]{".......00.....","......0120....",".....012210...","....0122210...","....0122210...","...012222110..","..0122222210..",".011111111110.","01222222222210",".033333333310.","...00000000..."},Ink,Violet0,Violet1,Wood3);
            c.Line(x,y-7,x-2,y-3,Violet2);c.Dot(x-1,y-6,Violet3);if(rear){c.Box(x-3,y+1,7,7,Wood0);c.Line(x-2,y+2,x,y+6,Wood2);}
        }else{
            // Separa la visera biselada y las carrilleras; la parte trasera del casco no muestra rostro.
            Mask(c,x-5,y-2,rear?new[]{"...00000...","..0122210..",".012333210.","01222222210","01222122210","01222122210","01222122210",".012212210.","..0122210..","...01110..."}:
                new[]{"...00000...","..0122210..",".012333210.","01222222210","01221000010","012210...0.","012210.....",".01210.....","..0110.....","...00......"},Ink,Iron0,Iron1,Iron3);
            c.Line(x-4,y+1,x-4,y+4,Iron2);if(!rear){c.Dot(x+2,y+3,Ink);c.Dot(x+3,y+5,Skin2);c.Box(x-1,y+7,3,1,Skin0);}
            if(cls==6){c.Line(x-4,y-1,x-7,y-4,Wood2,2);c.Dot(x-8,y-5,Wood3);c.Line(x+3,y-2,x+5,y-6,Wood2,2);c.Dot(x+6,y-7,Wood3);if(!rear){c.Line(x,y+7,x+3,y+8,Red0,2);c.Dot(x+1,y+9,Wood2);}}
            else {int plume=phase<4?-1:0;c.Line(x-1,y-3,x-2,y-6+plume,cloth1,2);c.Line(x-2,y-6+plume,x+2,y-5,cloth2);c.Dot(x+2,y-5,team==0?Blue3:Crimson3);}
        }
        if(rear&&(cls==1||cls==5))c.Line(x-1,y+3,x,y+7,cloth0);
    }
    static void Mantle(Px c,int x,int y,int cls,int team,int phase,int state,bool rear) {
        int dark=team==0?Blue0:Crimson0,mid=team==0?Blue1:Crimson1,light=team==0?Blue2:Crimson2;
        int[] waves={0,1,2,1,0,-1,-2,-1};int wave=waves[phase];if(state==1)wave=waves[(phase+2)%8]*2;
        int width=cls==5?10:cls==4?13:9,length=cls==5?14:cls==4?20:16;
        c.Poly(Ink,x-5,y-2,x+4,y,x+5+wave,y+length-3,x-2+wave,y+length,x-width+wave,y+length-2,x-7,y+5);
        c.Poly(dark,x-4,y-1,x+3,y+1,x+3+wave,y+length-4,x-2+wave,y+length-2,x-width+wave+2,y+length-3,x-6,y+6);
        c.Poly(mid,x-4,y,x-1,y+1,x-1+wave,y+length-4,x-5+wave,y+length-3,x-5,y+5);
        c.Line(x-4,y+2,x-4+wave,y+length-4,light);c.Line(x-2+wave,y+length-2,x+2+wave,y+length-4,Wood2);
        if(rear){c.Line(x+1,y+3,x+2+wave,y+length-5,dark);c.Line(x-5,y+1,x+3,y+2,light);}
        if(rear)Crest(c,x-4,y+5,team);
    }
    static void Boot(Px c,int x,int y,bool plate) {
        Mask(c,x-3,y-5,new[]{".0000...","012210..","012210..","0122210.","01222210",".000000."},Ink,plate?Iron0:Wood0,plate?Iron1:Wood1);
        c.Line(x-1,y-4,x-1,y-1,plate?Iron2:Wood2);c.Dot(x+2,y-1,plate?Iron3:Wood2);
    }
    static void Limb(Px c,int sx,int sy,int ex,int ey,int hx,int hy,int main,int light,int thickness) {
        c.Line(sx,sy,ex,ey,Ink,thickness+2);c.Line(ex,ey,hx,hy,Ink,thickness+2);
        c.Line(sx-1,sy,ex-1,ey,main,thickness);c.Line(ex,ey,hx,hy,main,thickness);
        c.Line(sx-1,sy,ex-1,ey,light);c.Dot(ex,ey,light);
    }
    static void Hand(Px c,int x,int y,bool glove,bool rear) {
        Mask(c,x-2,y-2,new[]{".000.","01210","01220",".0110","..00."},Ink,glove?Wood1:Skin0,glove?Wood3:Skin1);
        c.Dot(x,y-1,glove?Wood2:Skin2);if(!rear)c.Dot(x+1,y,glove?Wood0:Skin0);
    }
    static void Torso(Px c,int x,int y,int cls,int team,bool rear) {
        int cloth0=team==0?Blue0:Crimson0,cloth1=team==0?Blue1:Crimson1,cloth2=team==0?Blue2:Crimson2;
        if(cls==4){
            Mask(c,x-8,y-5,new[]{".....000000.....","...0012222100...","..012122221210..",".01221222212210.",".01221222212210.",".01221222212210.","..012122212210..","..012122212210..","..012122212210..","...0111111110...","...0333443330...","..012122212210..","..012122212210..","..012122212210.."},Ink,Violet0,Violet1,Wood1,Wood3);
            c.Line(x-3,y-3,x-2,y+6,Violet2);c.Line(x+3,y-2,x+2,y+6,Violet0);c.Dot(x-4,y-2,Violet3);
            // La estola y el dobladillo identifican al ejército; la túnica y el sombrero conservan el violeta compartido del Caster.
            c.Box(x-1,y-4,4,8,cloth1);c.Line(x-1,y-4,x-1,y+3,cloth2);Crest(c,x,y-3,team);
            c.Box(x-4,y+6,3,3,cloth0);c.Box(x+2,y+6,3,3,cloth1);
        }else if(cls==6){
            Mask(c,x-10,y-6,new[]{"...00000..00000...",".0012321001232100.","012333321123333210","012222211112222210",".0122212221222210.","..0121222122210..","..0121222122210..","..0121222122210..","..0121222122210..","..0111111111110..","..0333344333330..","..0122222222210..","...01222222210...","...00000000000..."},Ink,Iron0,Iron1,Iron2,Wood3);
            c.Line(x-6,y-4,x-3,y-4,Iron3);c.Line(x+3,y-4,x+6,y-4,Iron3);c.Box(x-5,y+6,4,3,cloth1);c.Box(x+1,y+6,4,3,cloth0);
        }else if(cls==5){
            Mask(c,x-6,y-4,new[]{"...00000....","..0122210...",".012222210..","01222222210.","012222222210",".01222222210","..012222210.","..012222210.","..011111110.",".0333343330.",".0122222210.","..01222210..","..00000000.."},Ink,Iron0,cloth0,Wood1,Wood3);
            if(!rear)c.Line(x-4,y-2,x+3,y+6,Wood2);c.Line(x-3,y-2,x-3,y+3,cloth2);
        }else if(cls==1){
            Mask(c,x-6,y-5,new[]{"...00000....","..0122210...",".012222210..","012122221210","012122221210","012122221210",".0122222210.",".0122222210.",".0122222210.",".0333343330.",".0121221210.",".0121221210.","..00000000.."},Ink,cloth0,cloth1,Wood1,Wood3);
            c.Line(x-3,y-3,x+2,y+4,Wood3);c.Line(x-2,y-3,x+3,y+4,Wood0);c.Line(x-4,y-2,x-4,y+3,cloth2);c.Box(x+2,y+6,3,3,Wood0);c.Dot(x+3,y+7,Wood2);
        }else{
            Mask(c,x-7,y-5,new[]{"...0000000....",".001223322100.","01233333333210","01222212222210","01222212222210",".012221222210.",".012221222210.",".012222222210.",".012222222210.",".011111111110.",".033334433330.",".012212212210.","..0121221210..","..0000000000.."},Ink,Iron0,Iron1,Wood1,Wood3);
            c.Line(x-5,y-3,x-2,y-4,Iron3);c.Line(x-4,y-2,x-4,y+1,Iron2);c.Line(x+1,y-3,x+4,y-2,Iron2);c.Line(x-4,y+7,x-3,y+9,cloth2);c.Box(x+2,y+7,3,2,cloth1);
            if(!rear)Crest(c,x-1,y-1,team);else{c.Line(x,y-2,x,y+3,Iron0);c.Line(x-3,y+4,x+3,y+4,Iron0);}
            if(cls==2){c.Box(x+4,y-5,2,8,cloth1);c.Line(x+4,y-5,x+4,y+2,cloth2);}
        }
        if(cls!=1&&cls!=4&&cls!=5){
            c.Box(x-2,y-2,5,7,cloth1);c.Line(x-2,y-2,x-2,y+4,cloth2);Crest(c,x-1,y-1,team);
        }else if(cls==1||cls==5){Crest(c,x,y-1,team);}
        if(rear&&(cls==1||cls==5))c.Line(x-2,y-3,x+3,y+5,Wood0,2);
    }
    static void Quiver(Px c,int x,int y,bool rear) {
        c.Poly(Ink,x-3,y+14,x-4,y+3,x+1,y-2,x+5,y,x+2,y+16);c.Poly(Wood1,x-2,y+13,x-3,y+4,x+1,y,x+3,y+1,x+1,y+14);
        c.Line(x-2,y+4,x+1,y+2,Wood3);c.Line(x-1,y+4,x,y+12,Wood2);c.Line(x-1,y+1,x+1,y-6,Wood2);c.Line(x+1,y+1,x+4,y-5,Wood2);c.Line(x-2,y+1,x-1,y-7,Wood2);
        c.Line(x-2,y-7,x,y-5,Iron3);c.Line(x+1,y-6,x+3,y-4,Iron2);c.Line(x+4,y-5,x+6,y-3,Iron3);
    }
    static void Horse(Px c,int phase,int state,int team,bool rear) {
        int[] stride={-3,-2,0,2,3,2,0,-2};int step=state==1?stride[phase]:state==2?stride[(phase+2)%8]/2:0;
        int bob=state==1?(phase==1||phase==2||phase==5||phase==6?-1:0):0,y=43+bob;
        // Articula las cuatro patas mediante cadera, rodilla y menudillo, con cascos oscuros.
        Limb(c,13,y,12-step/2,50,10-step,55,Wood0,Wood1,2);c.Box(8-step,55,5,2,Ink);
        Limb(c,31,y,33+step/2,49,35+step,55,Wood0,Wood1,2);c.Box(33+step,55,5,2,Ink);
        c.Poly(Ink,6,y-7,11,y-12,28,y-13,35,y-8,36,y-2,31,y+4,13,y+4,7,y);
        c.Poly(Wood1,8,y-7,13,y-10,28,y-11,33,y-7,34,y-2,29,y+1,14,y+2,9,y-1);
        c.Poly(Wood2,10,y-7,15,y-9,26,y-9,30,y-6,30,y-3,16,y-3,10,y-1);c.Line(12,y-8,24,y-9,Wood3);
        c.Poly(Wood0,7,y-8,5,y-9,2,y-4,3,y+4,5,y+6,4,y+1,5,y-3,8,y-5);
        Limb(c,16,y+1,16+step/2,49,16+step,55,Wood1,Wood2,3);c.Box(14+step,55,6,2,Ink);c.Dot(17+step,54,Wood3);
        Limb(c,29,y,28-step/2,49,28-step,55,Wood1,Wood2,3);c.Box(26-step,55,6,2,Ink);c.Dot(29-step,54,Wood3);
        int nod=state==2?(phase>=2&&phase<=4?1:0):state==1?(phase/2)%2:0;
        c.Poly(Ink,28,y-8,29,y-15,33,y-23,38,y-25,42+nod,y-21,46+nod,y-16,45+nod,y-11,41+nod,y-10,35,y-14,34,y-3);
        c.Poly(Wood2,30,y-8,31,y-15,34,y-21,38,y-23,41+nod,y-20,44+nod,y-16,43+nod,y-12,40+nod,y-12,35,y-16,33,y-7);
        c.Poly(Wood3,33,y-16,35,y-21,38,y-22,40+nod,y-20,38,y-18,36,y-18);c.Dot(40+nod,y-19,Ink);c.Dot(43+nod,y-14,Wood0);
        c.Line(34,y-23,33,y-27,Wood0,2);c.Line(38,y-24,39,y-27,Wood0);c.Line(31,y-18,30,y-9,Wood0,2);c.Line(35,y-14,44+nod,y-13,Iron1);c.Line(42+nod,y-14,39+nod,y-20,Wood0);c.Line(43+nod,y-13,26,y-6,Wood0);
        int cloth0=team==0?Blue0:Crimson0,cloth1=team==0?Blue1:Crimson1,cloth2=team==0?Blue2:Crimson2;
        c.Poly(Ink,15,y-12,28,y-12,31,y-1,27,y+4,15,y+2,13,y-5);c.Poly(cloth1,16,y-11,27,y-11,29,y-2,26,y+2,16,y+1,15,y-4);
        c.Line(16,y-10,26,y-10,cloth2);c.Line(16,y,27,y+1,Wood3);Crest(c,21,y-7,team);c.Line(17,y-12,26,y-12,Wood0,2);c.Line(18,y-13,25,y-13,Wood2);
        if(rear)c.Line(28,y-12,31,y-5,Wood0);
    }
    static Px Fallen(int cls,int team,int phase,bool rear) {
        var c=new Px(48,64);int f=phase-4,y=(cls==3?44:51)+f/2,x=17-f;
        int cloth=team==0?Blue1:Crimson1;ShadowAt(c,24,57,32);
        if(cls==3){c.Poly(Ink,5,51,10,45,30,46,39,49,43,55,38,59,11,59);c.Poly(Wood1,7,52,12,47,29,48,38,51,38,56,12,57);c.Line(12,54,7+f,60,Wood2,2);c.Line(31,55,38+f,60,Wood2,2);c.Poly(Wood2,34,48,40,45,46,49,46,53,40,54);c.Dot(43,49,Ink);c.Line(12,49,31,50,Wood3);c.Poly(cloth,13,49,29,50,30,56,14,56);Crest(c,21,51,team);}
        // Prolonga el desplome con cuatro poses que giran hombros y caderas y dejan caer el equipo.
        c.Poly(Ink,x-6,y+3,x-3,y-4,x+8,y-6,x+18,y-1,x+19,y+4,x+11,y+6,x-3,y+6);
        c.Poly(cls==4?Violet0:cloth,x-4,y+2,x-2,y-2,x+7,y-4,x+13,y-1,x+11,y+3,x+2,y+4);
        c.Line(x-2,y+2,x+7,y-2,team==0?Blue2:Crimson2,2);Crest(c,x+2,y-1,team);
        c.Line(x-1,y+3,x-9-f,y+4,Ink,4);c.Line(x-1,y+3,x-8-f,y+3,Wood1,2);c.Line(x+3,y+1,x-3-f,y-4,Ink,4);c.Line(x+3,y+1,x-3-f,y-4,Iron1,2);
        c.Line(x+11,y-1,x+15,y+3,cls==4?Violet1:cls==1?cloth:cls==5?Iron0:Iron2,3);c.Box(x+15,y-1,7,5,Ink);c.Box(x+16,y,5,3,Skin1);c.Dot(x+20,y+1,Skin2);c.Dot(x+18,y+1,Ink);c.Line(x+16,y-2,x+20,y-2,cls==4?Violet1:cls==1?cloth:cls==5?Iron0:Iron2);
        if(cls==4){c.Poly(Violet0,x+14,y-2,x+20,y-3,x+23,y+1,x+21,y+2);c.Dot(x+22,y,Violet2);}
        if(cls==1||cls==5)c.Line(x+15,y-1,x+15,y+3,cls==5?Iron0:cloth,2);
        Hand(c,x+8,y+4,true,rear);if(cls==0)Shield(c,21-f,y-8,team,phase);else if(cls==2)c.Line(5,y-8+f,44,y-9+f,Wood2);else if(cls==4){c.Line(6,y-8+f,37,y-9,Wood3);c.Box(37,y-11,3,4,Violet2);}else if(cls==6){c.Line(8,y-7,29,y-8+f,Wood2);c.Poly(Iron2,27,y-13+f,35,y-12+f,36,y-6+f,29,y-4+f);}else if(cls==1){c.Line(7,y-7+f,26,y-9,Wood2);c.Line(7,y-7+f,15,y-11,Iron2);}else Blade(c,9,y-8+f,1,0,7,true);
        
        c.Dot(9+phase,58,Stone1);return c;
    }
    static Px Unit(int cls,int team,int state,int phase,bool rear) {
        if(state==4&&phase>=4)return Fallen(cls,team,phase,rear);
        var c=new Px(48,64);bool mounted=cls==3;int[] breathe={0,-1,-1,-1,0,0,0,0},gait={-3,-2,0,2,3,2,0,-2};
        int[] leanAttack={-1,-2,-3,0,3,2,1,0},leanHurt={-3,-4,-3,-2,-1,-1,0,0};
        int bob=state==0?breathe[phase]:state==1?(phase%4==1||phase%4==2?-1:0):state==4?phase*2:state==5?(phase>1&&phase<5?-1:0):0;
        int lean=state==2?leanAttack[phase]:state==3?leanHurt[phase]:state==1?1:state==4?-phase:0;
        int x=23+lean,torso=mounted?25+bob:35+bob,feet=55,step=state==1?gait[phase]:0;
        if(cls==5)torso+=2;
        if(mounted)Horse(c,phase,state,team,rear);else ShadowAt(c,24,57,cls==6?30:24);
        int cloth0=team==0?Blue0:Crimson0,cloth1=team==0?Blue1:Crimson1,cloth2=team==0?Blue2:Crimson2;
        bool cape=cls==0||cls==3||cls==4||cls==5;
        if(cape&&!rear)Mantle(c,x-1,torso-3,cls,team,phase,state,false);
        if(cls==1&&!rear)Quiver(c,x-8,torso-7,false);
        // Dibuja los miembros traseros antes del cuerpo y articula rodillas y tobillos por separado en las fases de apoyo y balanceo.
        if(!mounted&&cls!=4){
            int width=cls==6?5:3,hipY=torso+7;
            Limb(c,x-3,hipY,x-4-step/2,49,x-5-step,53,Wood0,Wood1,width);Boot(c,x-5-step,feet,cls==0||cls==2||cls==6);
            Limb(c,x+3,hipY,x+3+step/2,48,x+3+step,53,cls==5?Iron0:cloth0,cls==5?Iron1:cloth1,width);Boot(c,x+3+step,feet,cls!=1&&cls!=5);
        }
        if(cls==4){int sw=gait[phase]/2;c.Poly(Ink,x-6,torso+4,x+5,torso+4,x+9+sw,55,x-10+sw,55);c.Poly(Violet0,x-5,torso+5,x+4,torso+5,x+7+sw,54,x-8+sw,54);c.Poly(Violet1,x-4,torso+6,x-1,torso+6,x-1+sw,53,x-6+sw,53);c.Line(x-4,torso+7,x-4+sw,52,Violet2);c.Line(x+3,torso+7,x+5+sw,53,cloth1);c.Line(x-8+sw,54,x+6+sw,54,Wood3);Boot(c,x+4+sw,56,false);}
        int leftX=x-8+(state==1?-step/2:0),leftY=torso+4+(phase%3==0?1:0);
        if(state==5){leftX=x-8-phase/3;leftY=torso+2-new int[]{0,2,5,8,9,7,4,1}[phase];}
        if(cls!=1&&cls!=2&&cls!=6){Limb(c,x-5,torso-3,x-8,torso+1,leftX,leftY,cloth0,cloth1,2);Hand(c,leftX,leftY,true,rear);}
        Torso(c,x,torso,cls,team,rear);
        if(mounted){Limb(c,x+3,torso+6,x+5,torso+13,x+4,torso+19,Iron0,Iron1,3);Boot(c,x+5,torso+21,true);c.Line(x+4,torso+18,x+8,torso+18,Iron3);}
        c.Box(x-1,torso-7,3,3,rear?Wood0:Skin0);c.Dot(x,torso-7,rear?Wood1:Skin1);
        CharacterHead(c,x,torso-16,cls,team,rear,phase);
        if(cape&&rear)Mantle(c,x,torso-3,cls,team,phase,state,true);
        if(cls==1&&rear)Quiver(c,x+1,torso-7,true);
        int[] idleH={0,-1,-1,0,0,1,1,0};int hx=x+9,hy=torso+3+idleH[phase];
        if(state==1){hx+=gait[(phase+2)%8]/2;hy+=new int[]{0,-1,-1,0,1,2,1,1}[phase];}
        if(state==2){hx+=new int[]{-4,-7,-5,2,6,5,2,0}[phase];hy+=new int[]{-3,-8,-13,-10,-2,4,6,2}[phase];}
        if(state==3){hx+=new int[]{-4,-5,-3,-2,-2,-1,0,0}[phase];hy+=new int[]{-2,-1,1,2,2,1,1,0}[phase];}
        if(state==4){hx-=phase*2;hy+=phase*2;}
        if(state==5){hx+=new int[]{0,0,-1,-2,-2,-1,0,0}[phase];hy-=new int[]{0,3,7,11,13,10,6,1}[phase];}
        hx=Math.Max(5,Math.Min(36,hx));hy=Math.Max(15,hy);
        if(cls==2&&state==5)hy=Math.Max(31,hy);
        if(cls==3&&state!=2)hy=Math.Max(29,hy);
        if(cls==0&&state==2&&phase>=3&&phase<=5)hx=Math.Min(31,hx);
        if(cls==2&&state==2&&phase>=3&&phase<=6)hx=Math.Min(27,hx);
        int elbowX=(x+5+hx)/2+(state==2&&phase<3?-2:1),elbowY=(torso-3+hy)/2+3;
        if(cls!=1)Limb(c,x+5,torso-3,elbowX,elbowY,hx,hy,cls==6?Skin1:Iron1,cls==6?Skin2:Iron2,cls==6?4:3);
        if(cls==0){
            Shield(c,leftX-5,leftY-6,team,phase);int[] dx={-1,-1,0,1,1,1,0,0},dy={-1,-1,-1,-1,0,1,1,-1};int i=state==2?phase:7;
            Blade(c,hx,hy,dx[i],dy[i],14);Hand(c,hx,hy,true,rear);
        }else if(cls==1){
            int bx=Math.Min(37,x+11+(state==2&&phase>=3&&phase<=5?2:0)),by=torso-4;
            int draw=state==2?new int[]{-1,-3,-6,-8,0,0,-1,-1}[phase]:-1;
            c.Line(bx-2,by-9,bx+2,by-5,Wood0,3);c.Line(bx+2,by-5,bx+3,by+4,Wood0,3);c.Line(bx+3,by+4,bx-2,by+10,Wood0,3);
            c.Line(bx-2,by-9,bx+1,by-5,Wood3);c.Line(bx+1,by-5,bx+2,by+4,Wood2);c.Line(bx+2,by+4,bx-2,by+10,Wood3);
            c.Line(bx-2,by-9,bx+draw,by,Iron1);c.Line(bx+draw,by,bx-2,by+10,Iron2);
            Limb(c,x+5,torso-3,bx-5,by+2,bx-1,by,cloth1,cloth2,2);Hand(c,bx-1,by,true,rear);
            Limb(c,x-4,torso-3,x-2,by+2,bx+draw,by,cloth0,cloth1,2);Hand(c,bx+draw,by,true,rear);
            if(state!=2||phase<4){c.Line(bx+draw,by,bx+10,by,Wood2);c.Dot(bx+11,by,Iron3);c.Dot(bx+9,by-1,Iron3);}
        }else if(cls==2){
            int[] dx={-1,-1,0,1,1,1,1,0},dy={-1,-1,-1,0,0,0,-1,-1};int i=state==2?phase:7;int len=state==2?(phase==4?13:11):24;
            c.Line(hx-dx[i]*7,hy-dy[i]*7,hx+dx[i]*len,hy+dy[i]*len,Ink,3);c.Line(hx-dx[i]*7,hy-dy[i]*7,hx+dx[i]*len,hy+dy[i]*len,Wood2);
            int ex=hx+dx[i]*len,ey=hy+dy[i]*len;c.Line(ex-dx[i]*2,ey-dy[i]*2,ex+dx[i]*3,ey+dy[i]*3,Iron2,2);c.Dot(ex+dx[i]*4,ey+dy[i]*4,Iron3);
            if(dx[i]==0)c.Poly(Iron3,ex-2,ey,ex,ey-6,ex+2,ey);else c.Poly(Iron3,ex,ey-2,ex+5,ey,ex,ey+2);
            Limb(c,x-4,torso-3,(x+hx)/2,torso+4,hx-dx[i]*7,hy-dy[i]*7,Iron0,Iron1,2);Hand(c,hx-dx[i]*7,hy-dy[i]*7,true,rear);Hand(c,hx,hy,true,rear);
        }else if(cls==3){
            if(state==2){int[] dy={-1,-1,-1,0,0,1,0,-1};Blade(c,hx,hy,phase<2?-1:phase==7?0:1,dy[phase],9);}
            else{c.Line(hx,hy+7,hx,hy-22,Wood0,3);c.Line(hx,hy+6,hx,hy-22,Wood2);c.Poly(Iron3,hx-2,hy-21,hx,hy-27,hx+2,hy-21);c.Poly(cloth1,hx+1,hy-20,hx+7,hy-18,hx+5,hy-15,hx+1,hy-16);c.Dot(hx+2,hy-19,cloth2);}
            Hand(c,hx,hy,true,rear);c.Line(leftX,leftY,38,32,Wood0);Hand(c,leftX,leftY,true,rear);
        }else if(cls==4){
            int sx=hx,top=Math.Max(6,hy-18);c.Line(sx,hy+12,sx,top,Wood0,3);c.Line(sx,hy+11,sx,top,Wood3);
            Mask(c,sx-3,top-5,new[]{"...0...","..010..",".01210.","0123210",".01210.","..010..","..040..","..040.."},Ink,Violet0,Violet2,Violet3,Wood3);
            Hand(c,hx,hy,false,rear);if(state==5||state==2){int rad=2+phase/2;c.Dot(sx-rad-2,top,Water3);c.Dot(sx+rad+2,top-2,Violet3);c.Dot(sx+1,Math.Max(1,top-rad-3),Violet2);c.Dot(leftX-2,leftY-3,Water3);}
        }else if(cls==5){
            int[] dx={-1,-1,0,1,1,1,0,0},dy={-1,-1,-1,-1,0,1,1,-1};int i=state==2?phase:7;Blade(c,hx,hy,dx[i],dy[i],6,true);Hand(c,hx,hy,true,rear);
            Blade(c,leftX,leftY,state==2&&phase>2?1:-1,state==2?-1:0,5,true);Hand(c,leftX,leftY,true,rear);c.Line(x-4,torso-5,x-9+phase%2,torso-2,cloth2);
        }else if(cls==6){
            int[] dx={-1,-1,0,1,1,1,0,0},dy={-1,-1,-1,-1,0,1,1,-1};int i=state==2?phase:7;int ex=hx+dx[i]*9,ey=hy+dy[i]*9;
            ex=Math.Max(8,Math.Min(37,ex));ey=Math.Max(7,ey);c.Line(hx-dx[i]*7,hy-dy[i]*7,ex,ey,Ink,4);c.Line(hx-dx[i]*7,hy-dy[i]*7,ex,ey,Wood2,2);
            c.Poly(Ink,ex-8,ey-5,ex-4,ey-7,ex-2,ey-4,ex+3,ey-4,ex+5,ey-7,ex+9,ey-4,ex+9,ey+3,ex+5,ey+6,ex+2,ey+3,ex-3,ey+3,ex-6,ey+5,ex-9,ey+1);
            c.Poly(Iron1,ex-7,ey-4,ex-4,ey-5,ex-2,ey-2,ex+3,ey-2,ex+5,ey-5,ex+7,ey-3,ex+7,ey+2,ex+5,ey+4,ex+2,ey+1,ex-3,ey+1,ex-5,ey+3,ex-7,ey);
            c.Line(ex-7,ey-3,ex-7,ey,Iron3);c.Line(ex+7,ey-3,ex+7,ey+1,Iron2);c.Dot(ex,ey-2,Iron3);
            Limb(c,x-5,torso-3,x-3,torso+3,hx-dx[i]*6,hy-dy[i]*6,Skin0,Skin1,3);Hand(c,hx-dx[i]*6,hy-dy[i]*6,false,rear);Hand(c,hx,hy,false,rear);
        }
        if(state==3){c.Line(x-10,torso-14,x-13-phase%2,torso-17,Red2);c.Dot(x+8,torso-12-phase%3,Pale);}
        if(state==5&&cls!=4){c.Box(leftX-1,leftY-5,2,2,team==0?Water3:Crimson3);c.Dot(leftX-3-phase%2,leftY-6,Violet2);}
        // Mantiene las puntadas unidas a los pliegues de la tela durante las ocho poses del ciclo de reposo.
        c.Dot(x-3+(phase%4),torso+5+(phase/4),cloth2);
        return c;
    }

    static Px Effect(int kind,int frame) {
        var c=new Px(48,48);int f=frame;
        if(kind==0){int[] arc={9,6,16,4,29,8,37,17,40,28,35,36,27,40,35,30,36,22,30,14,18,9};for(int i=0;i<arc.Length;i+=2)arc[i]+=frame-1;c.Poly(frame==3?Iron1:Iron3,arc);c.Line(13+f,8,28+f,11,Ochre3);c.Line(29+f,13,36+f,23,Pale);if(f==1||f==2){c.Line(20,22,30,32,Fire3);c.Line(30,22,20,32,Iron3);}}
        if(kind==1){int x=7+frame*7;c.Line(x-7,25,x+13,19,Wood2);c.Line(x-7,24,x+13,18,Iron2);c.Poly(Iron3,x+12,16,x+19,17,x+14,22);c.Line(x-6,24,x-9,20,Iron3);c.Line(x-3,23,x-6,19,Iron3);c.Line(x-7,25,x-10,29,Wood3);}
        if(kind==2){int h=new int[]{14,24,31,21}[f];c.Poly(Fire0,10,40,8,29,14,20,17,27,23,40-h,28,21,32,29,37,24,39,36,34,43,18,44);c.Poly(Fire1,13,38,13,28,18,32,23,14+f*2,29,33,34,29,35,39,27,43,18,42);c.Poly(Fire2,18,39,20,29,24,34,27,24+f,31,38,27,42,21,42);c.Poly(Fire3,22,40,24,34,27,40);for(int k=0;k<4;k++)c.Box(10+k*8,7+(k*5+f*3)%15,1+k%2,2,Fire2);}
        if(kind==3){int y=36-f*2;c.Poly(Water0,11,y,17,y-19,23,y-12,31,y-25,36,y-12,40,y,27,y+7);c.Poly(Water2,13,y,17,y-17,21,y-9,22,y+3);c.Poly(Water3,18,y-16,19,y-4,21,y+2,16,y-2);c.Poly(Iron3,27,y,31,y-23,34,y-11,32,y+4);c.Poly(Water2,35,y,36,y-11,39,y,35,y+2);if(f==3){c.Line(8,13,13,8,Water3);c.Line(38,9,42,12,Iron3);}}
        if(kind==4){int shift=f%2*2;c.Line(26+shift,2,20,13,Violet1,7);c.Line(20,13,29,15,Violet1,7);c.Line(29,15,16,30,Violet1,7);c.Line(16,30,25,29,Violet1,7);c.Line(25,29,19,43,Violet1,7);c.Line(26+shift,2,20,13,Violet3,3);c.Line(20,13,29,15,Violet3,3);c.Line(29,15,16,30,Pale,3);c.Line(16,30,25,29,Pale,2);c.Line(25,29,19,43,Pale,2);c.Line(20,17,11-f,21,Violet2);c.Line(11-f,21,8-f,29-f*2,Violet3);c.Dot(31+f,32-f*3,Iron3);}
        if(kind==5||kind==7){int col=kind==5?Water3:Rot3,shade=kind==5?Leaf3:Violet2;int r=8+f*2;c.Poly(shade,24-r,37,24,32-r/2,24+r,37,24,42+r/3);c.Poly(Deep,26-r,37,24,34-r/2,22+r,37,24,40+r/3);for(int k=0;k<4;k++){int x=12+k*8,y=31-(k*6+f*5)%25;c.Box(x,y-2,1,5,col);c.Box(x-2,y,5,1,col);c.Dot(x+1,y+1,shade);}if(kind==7){c.Line(23,9,23,31,Pale,2);c.Line(16,17,30,17,Pale,2);}}
        if(kind==6){for(int k=0;k<12;k++){int x=10+(k*7+f*2)%29,y=39-(k*5+f*7)%32;c.Box(x,y,2+k%2,2,Rot1);c.Dot(x,y,Rot3);if(k%2==0)c.Dot(x-1,y+1,Rot2);}c.Poly(Rot0,12,40,20,33,28,34,37,41,29,44,18,44);c.Line(15,40,29,38,Rot2);}
        return c;
    }
    static Px Icon(int i) {
        var c=new Px(16,16);
        if(i==0||i==24){Blade(c,5,11,1,-1,6);if(i==24){c.Line(12,10,12,14,Ochre3);c.Line(10,12,14,12,Ochre3);}}
        else if(i==1){c.Line(4,2,10,4,Wood2);c.Line(10,4,12,8,Wood3);c.Line(12,8,10,12,Wood3);c.Line(10,12,4,14,Wood2);c.Line(4,2,4,14,Iron2);c.Line(2,8,14,8,Iron3);c.Dot(13,7,Iron3);c.Dot(13,9,Iron3);}
        else if(i==2||i==26){c.Line(3,14,11,3,Wood2);c.Poly(Iron3,9,4,13,0,13,5,11,7);if(i==26){c.Line(1,11,8,1,Iron2);c.Line(8,15,15,5,Iron2);}}
        else if(i==3||i==27){c.Poly(Wood0,3,14,3,7,8,2,12,3,15,8,12,10,8,8,8,14);c.Poly(Wood2,4,13,4,8,8,4,11,4,13,8,11,8,8,6,6,12);c.Dot(11,5,Ink);c.Line(5,3,3,7,Wood3);if(i==27)c.Line(1,10,5,9,Iron3);}
        else if(i==4){c.Poly(Fire0,3,13,2,9,6,3,6,8,9,0,13,8,14,12,10,15,6,15);c.Poly(Fire1,4,12,5,8,7,10,9,4,11,10,12,13,8,15);c.Poly(Fire3,6,13,8,9,10,13,8,15);}
        else if(i==5){Blade(c,5,9,1,-1,5,true);Blade(c,11,9,-1,-1,5,true);}
        else if(i==6){c.Line(3,14,10,3,Wood2,2);c.Poly(Iron0,5,4,8,0,12,2,15,1,15,7,11,10,8,7,4,8);c.Poly(Iron2,6,4,8,2,11,4,14,3,14,6,11,8,9,6,6,6);c.Line(5,4,5,7,Iron3);}
        else if(i==7){c.Poly(Ink,2,3,8,0,14,3,13,11,8,16,3,11);c.Poly(Blue1,3,4,8,2,13,4,12,10,8,14,4,10);c.Line(3,4,8,2,Iron3);Crest(c,6,5,0);}
        else if(i>=8&&i<=12){int liquid=i==8?Red1:i==9?Red2:i==10?Violet2:i==11?Leaf3:Rot2;c.Box(6,1,4,2,Wood2);c.Poly(Iron1,5,3,11,3,11,7,14,10,13,15,3,15,2,10,5,7);c.Poly(Iron3,6,4,10,4,10,8,12,11,11,14,5,14,4,11,6,8);c.Box(5,10,6,3,liquid);c.Box(4,11,1,2,liquid);c.Dot(5,8,Pale);if(i==9||i==12){c.Line(8,9,8,13,Pale);c.Line(6,11,10,11,Pale);}}
        else if(i==13){c.Poly(Wood1,3,3,6,1,11,1,14,4,14,11,11,14,5,14,2,11,2,5);c.Poly(Ochre2,4,3,6,2,10,2,12,4,12,10,10,12,5,12,3,10,3,5);c.Line(5,4,10,4,Ochre3);c.Line(8,4,6,8,Wood1);c.Line(6,8,9,8,Wood1);c.Line(9,8,7,11,Wood1);}
        else if(i==14){c.Line(4,15,4,1,Iron2);c.Poly(Ochre2,5,2,14,4,11,8,5,7);c.Line(6,3,11,4,Ochre3);c.Line(2,15,7,15,Wood3);}
        else if(i==15){c.Line(3,14,12,3,Wood2,2);c.Poly(Iron1,6,3,10,0,15,5,12,9);c.Line(9,1,14,5,Iron3);}
        else if(i==16){c.Poly(Ochre2,6,4,10,4,12,6,12,10,10,12,6,12,4,10,4,6);c.Box(6,5,3,3,Ochre3);for(int k=0;k<4;k++){c.Dot(7+k%2,1+k/2*13,Ochre3);c.Dot(1+k/2*13,7+k%2,Ochre3);}c.Dot(2,2,Ochre2);c.Dot(13,2,Ochre2);c.Dot(2,13,Ochre2);c.Dot(13,13,Ochre2);}
        else if(i==17||i==18||i==19){c.Poly(Iron1,2,6,5,6,5,3,10,2,13,5,15,7,14,10,2,10,0,8);c.Box(6,4,4,2,Iron2);c.Box(2,7,10,2,Iron2);if(i==17){c.Line(4,12,3,15,Water3);c.Line(9,12,8,15,Water2);c.Line(14,12,13,15,Water3);}else if(i==18){c.Line(1,12,14,12,Iron2);c.Line(4,15,15,15,Iron3);}else{c.Poly(Ochre3,9,8,5,13,9,12,7,16,13,10,9,11);}}
        else if(i==20){c.Poly(Rot1,7,0,10,5,15,7,11,10,9,15,5,11,0,9,5,5);c.Box(6,5,5,5,Rot2);c.Box(7,6,3,3,Ink);c.Dot(7,5,Rot3);}
        else if(i==21){c.Poly(Iron2,4,2,11,2,14,5,13,10,11,12,11,15,5,15,5,12,2,9,2,5);c.Box(4,5,3,3,Ink);c.Box(9,5,3,3,Ink);c.Dot(8,9,Ink);c.Line(6,13,10,13,Ink);c.Dot(7,14,Ink);c.Dot(9,14,Ink);}
        else if(i==22){c.Line(2,12,7,12,Iron3,2);c.Line(7,12,7,5,Iron3,2);c.Line(7,5,13,5,Iron3,2);c.Poly(Ochre3,11,1,16,5,11,9);c.Box(1,11,2,2,Ochre3);}
        else if(i==23){c.Line(3,4,6,1,Iron2);c.Line(6,1,11,1,Iron2);c.Line(11,1,14,4,Iron2);c.Line(3,4,1,9,Iron2);c.Line(1,9,5,14,Iron2);c.Line(5,14,11,14,Iron2);c.Line(11,14,14,10,Iron2);c.Poly(Ochre3,9,0,15,3,11,7);c.Line(8,5,8,9,Wood3);c.Line(8,9,11,10,Wood3);}
        else if(i==25){c.Box(3,3,10,1,Wood3);c.Box(3,12,10,1,Wood3);c.Box(3,3,1,10,Wood3);c.Box(12,3,1,10,Wood3);c.Box(6,6,4,4,Red1);c.Line(8,0,8,5,Iron3);c.Line(8,11,8,15,Iron3);c.Line(0,8,5,8,Iron3);c.Line(11,8,15,8,Iron3);}
        else if(i==28){c.Line(8,0,8,15,Water3);c.Line(1,4,14,12,Water3);c.Line(1,12,14,4,Water3);c.Line(5,1,8,4,Iron3);c.Line(8,4,11,1,Iron3);c.Line(5,14,8,11,Iron3);c.Line(8,11,11,14,Iron3);}
        else if(i==29){c.Poly(Violet2,9,0,2,9,7,8,5,16,15,5,9,6);c.Line(9,1,5,7,Violet3);c.Line(8,9,6,13,Iron3);}
        else if(i==30){c.Box(6,2,4,12,Leaf3);c.Box(2,6,12,4,Leaf3);c.Box(7,3,2,10,Iron3);c.Box(3,7,10,2,Iron3);}
        else if(i==31){c.Poly(Iron0,3,12,1,9,4,5,8,5,9,1,13,2,15,6,12,9,14,13,10,15,4,15);c.Poly(Iron1,3,10,5,7,9,7,10,3,12,3,13,6,10,9,12,12,9,13,5,13);c.Line(4,9,8,9,Iron2);c.Box(10,4,2,2,Iron2);}
        return c;
    }
    static Px UI() {var c=new Px(96,32);for(int k=0;k<3;k++){int x=k*32,body=k==0?Deep:k==1?Wood0:Wood1;c.Box(x,0,32,32,Ink);c.Box(x+1,1,30,30,Stone0);c.Box(x+2,2,28,28,k==2?Wood3:Wood2);c.Box(x+3,3,26,26,Ink);c.Box(x+4,4,24,24,body);c.Line(x+4,4,x+27,4,k==2?Ochre3:Stone1);c.Line(x+4,4,x+4,27,k==2?Ochre3:Stone1);c.Line(x+4,28,x+28,28,Ink);c.Line(x+28,4,x+28,28,Ink);for(int a=0;a<2;a++)for(int b=0;b<2;b++){int px=x+2+a*27,py=2+b*27;c.Box(px,py,2,2,Iron0);c.Dot(px,py,Iron3);c.Dot(px+1,py+1,Stone0);}c.Dot(x+6,2,Wood3);c.Dot(x+25,2,Wood3);c.Dot(x+2,6,Wood3);c.Dot(x+29,6,Wood3);}return c;}

    static void MenuTree(Px c,int x,int y,int scale) {var tree=new Px(64,80);PixelTree(tree,32,68,59,0,x+y);for(int yy=0;yy<80;yy++)for(int xx=0;xx<64;xx++){int col=tree.Get(xx,yy);if(col!=0)c.Box(x+(xx-32)*scale,y+(yy-68)*scale,scale,scale,col);}}
    static void MenuTower(Px c,int x,int y,int w,int h,bool roof) {Wall(c,x,y,w,h);c.Poly(Stone0,x+w,y,x+w+8,y-4,x+w+8,y+h-3,x+w,y+h);c.Line(x+w+2,y+1,x+w+2,y+h-4,Stone1);for(int yy=y+9;yy<y+h-8;yy+=19){c.Box(x+w/2-2,yy,4,9,Ink);c.Box(x+w/2-1,yy+1,2,5,Wood3);c.Dot(x+w/2-1,yy+2,Fire3);}if(roof)Roof(c,x+w/2,y-20,w+14,23);else{for(int xx=x-2;xx<x+w+3;xx+=7)c.Box(xx,y-7,4,9,Stone2);c.Line(x-2,y+3,x+w+3,y+3,Stone0);}}
    static Px Menu() {
        var c=new Px(720,450);int[] sky={C("111d2b"),C("1a2936"),C("263943"),C("34494a"),C("44554c")};
        // Separa el cielo en cinco bandas con tramado ordenado de un píxel en sus transiciones.
        for(int y=0;y<450;y++)for(int x=0;x<720;x++){int band=Math.Min(4,y/62);int col=sky[band];if(band>0&&y%62<16&&((x+y)&1)==0)col=sky[band-1];c.Dot(x,y,col);}
        for(int k=0;k<130;k++){int x=280+Hash(k,1,122)%437,y=Hash(k,2,122)%144;c.Dot(x,y,k%4==0?Stone2:Stone1);}
        // Dibuja el halo lunar escalonado y tramado, con cráteres formados por agrupaciones de píxeles.
        int mx=559,my=88;for(int y=31;y<145;y++)for(int x=502;x<617;x++){int d=(x-mx)*(x-mx)+(y-my)*(y-my);if(d<52*52&&d>37*37&&((x+y)%3==0))c.Dot(x,y,Stone1);if(d<37*37)c.Dot(x,y,Stone2);if(d<35*35)c.Dot(x,y,Stone3);}
        for(int k=0;k<14;k++){int x=535+Hash(k,3,77)%47,y=62+Hash(k,4,77)%48;if((x-mx)*(x-mx)+(y-my)*(y-my)<28*28){int w=3+k%6;c.Poly(Stone2,x,y,x+w,y-1,x+w+2,y+3,x+w-1,y+5,x,y+4);c.Line(x+1,y+1,x+w-2,y+1,Stone1);}}
        c.Poly(Shadow,0,233,38,199,69,207,117,166,156,203,215,187,263,215,308,164,350,192,389,155,431,211,471,190,512,218,563,180,608,215,658,184,720,213,720,450,0,450);
        c.Poly(Deep,0,271,47,234,94,244,142,224,185,254,232,223,278,251,325,209,363,241,409,225,455,252,499,219,553,247,602,222,648,250,720,227,720,450,0,450);
        for(int k=0;k<80;k++){int x=244+Hash(k,3,541)%490,y=240+Hash(k,8,541)%90;var t=new Px(24,36);t.Line(12,34,12,9,Wood0,2);t.Poly(Leaf0,12,2,3,24,7,23,1,31,22,31,17,23,21,24);t.Poly(Leaf1,12,4,7,18,11,17,5,27,12,24,12,31,3,31);c.Stamp(t,x,y-35);}
        // Eleva la cresta de la fortaleza con estratos expuestos y un camino roto hacia el valle.
        c.Poly(Soil,351,309,384,273,451,260,555,275,614,308,604,341,554,358,451,352,390,337);c.Poly(Stone0,352,308,393,299,449,316,510,300,608,309,604,341,554,358,451,352,390,337);c.Poly(Stone1,356,308,392,304,450,322,511,307,607,309,599,320,543,326,497,324,449,336,394,319);for(int k=0;k<34;k++){int x=367+Hash(k,1,67)%222,y=314+Hash(k,2,67)%29;if(c.Get(x,y)==Stone0||c.Get(x,y)==Stone1)c.Line(x,y,x+5+k%5,y,Stone2);}
        c.Poly(Water0,454,341,480,342,447,369,467,386,541,403,595,450,360,450,412,409,393,383);c.Poly(Water1,460,344,472,345,431,371,439,389,506,411,553,450,385,450,432,415,408,385);
        for(int k=0;k<65;k++){int x=378+Hash(k,6,71)%176,y=357+Hash(k,7,71)%92;int old=c.Get(x,y);if(old==Water0||old==Water1)c.Line(x,y,x+5+k%12,y,k%4==0?Water2:Deep);}
        // Delinea por separado mampostería, ventanas, tejas y muralla exterior inclinada.
        Wall(c,397,255,163,53);for(int x=396;x<563;x+=10)c.Box(x,249,6,10,Stone2);c.Poly(Stone0,558,255,579,244,579,298,558,308);for(int y=257;y<299;y+=7)c.Line(560,y,578,y-9,Stone1);
        MenuTower(c,387,211,27,99,false);MenuTower(c,550,204,30,103,false);MenuTower(c,452,167,35,147,true);MenuTower(c,425,218,22,88,true);MenuTower(c,502,225,23,81,true);
        c.Poly(Ink,461,309,461,280,465,273,473,270,481,276,485,284,485,309);c.Poly(Wood0,463,307,463,281,469,274,474,274,482,283,482,307);for(int x=465;x<482;x+=4)c.Line(x,282,x,307,Iron0);c.Line(463,307,484,307,Stone3);Flag(c,470,150,2,0);Flag(c,566,202,0,1);
        c.Poly(Dirt,461,310,481,310,477,324,457,336,445,347,436,346,447,330);c.Line(460,317,474,319,Wood2);c.Line(451,331,464,333,Wood1);
        // Extiende la putrefacción por la mampostería y el río mediante raíces irregulares y quebradas.
        for(int k=0;k<12;k++){int x=501+k*7,y=321+(k%4)*7;c.Line(546,294,x,y,Rot0,3);c.Line(545,295,x-4,y+5,Rot1);c.Line(x-4,y+5,x+18,369+k%3*6,Rot1);c.Dot(x,y,Rot2);}
        for(int k=0;k<50;k++){int x=485+Hash(k,4,84)%142,y=273+Hash(k,6,84)%109;if(k%3==0)c.Box(x,y,2,2,Rot2);else c.Dot(x,y,Rot1);if(k%7==0)c.Dot(x+1,y-1,Rot3);}
        // Enmarca la silueta con salientes en primer plano.
        c.Poly(Ink,0,373,48,362,107,381,166,370,219,388,290,372,352,397,380,450,0,450);c.Poly(Deep,0,374,51,366,105,385,166,375,219,394,290,377,342,401,333,412,254,398,174,414,92,399,0,406);
        c.Poly(Ink,550,450,589,397,628,380,665,358,720,353,720,450);MenuTree(c,696,404,2);MenuTree(c,655,389,1);
        // Compone ramas desnudas y asimétricas con píxeles en primer plano.
        c.Line(21,420,35,283,Ink,8);c.Line(35,328,8,281,Ink,4);c.Line(33,343,63,302,Ink,4);c.Line(30,376,79,342,Ink,4);c.Line(61,354,83,354,Ink,2);c.Line(15,300,3,296,Ink,2);c.Line(51,319,51,289,Ink,2);c.Line(35,284,24,260,Ink,3);
        for(int k=0;k<95;k++){int x=Hash(k,4,577)%720,y=401+Hash(k,7,577)%49;if(x<350||x>568){c.Line(x,y,x+(k%5-2),y-3-k%9,Deep);if(k%8==0)c.Dot(x,y-7,Leaf1);}}
        // Despeja el área izquierda de texto mediante un velo tramado, sin degradado suave.
        for(int x=0;x<343;x++)for(int y=0;y<450;y++){if(x<205)c.Dot(x,y,(y<250)?sky[0]:Ink);else if(x<275&&((x+y)&1)==0)c.Dot(x,y,sky[0]);else if(x>=275&&x<320&&((x+y)%4==0))c.Dot(x,y,sky[0]);}
        c.Line(14,36,14,14,Wood2);c.Line(14,14,62,14,Wood2);c.Line(658,14,705,14,Wood2);c.Line(705,14,705,37,Wood2);c.Line(14,414,14,436,Wood1);c.Line(14,436,62,436,Wood1);c.Line(658,436,705,436,Wood1);c.Line(705,414,705,436,Wood1);
        for(int k=0;k<5;k++){c.Dot(22+k*6,13,Wood3);c.Dot(23+k*6,14,Wood3);c.Dot(22+k*6,15,Wood3);}
        return c;
    }
    static Px Atlas(int fw,int fh,int cols,int rows,Func<int,int,Px> get) {var a=new Px(fw*cols,fh*rows);for(int r=0;r<rows;r++)for(int col=0;col<cols;col++)a.Stamp(get(col,r),col*fw,r*fh);return a;}
    static void PreviewUnits(Px atlas,string root) {
        var lineup=new Px(7*48,4*64);lineup.Box(0,0,lineup.W,lineup.H,Deep);
        for(int dir=0;dir<4;dir++)for(int cls=0;cls<7;cls++)for(int y=0;y<64;y++)for(int x=0;x<48;x++){int col=atlas.Get(x,(cls+dir*14)*64+y);if(col!=0)lineup.Dot(cls*48+x,dir*64+y,col);}
        lineup.Save(Path.Combine(root,"class-lineup-native.png"));lineup.Zoom(3).Save(Path.Combine(root,"class-lineup-3x.png"));
        var teams=new Px(7*48,2*64);teams.Box(0,0,teams.W,teams.H,Deep);for(int team=0;team<2;team++)for(int cls=0;cls<7;cls++)for(int y=0;y<64;y++)for(int x=0;x<48;x++){int col=atlas.Get(x,(cls+team*7)*64+y);if(col!=0)teams.Dot(cls*48+x,team*64+y,col);}teams.Zoom(4).Save(Path.Combine(root,"class-lineup-teams-4x.png"));
        string[] states={"idle","walk","attack","hurt","death","cast"};
        for(int state=0;state<6;state++){var strip=new Px(8*48,7*64);strip.Box(0,0,strip.W,strip.H,Deep);for(int cls=0;cls<7;cls++)for(int f=0;f<8;f++)for(int y=0;y<64;y++)for(int x=0;x<48;x++){int color=atlas.Get((state*8+f)*48+x,cls*64+y);if(color!=0)strip.Dot(f*48+x,cls*64+y,color);}strip.Zoom(2).Save(Path.Combine(root,"units-"+states[state]+"-2x.png"));}
    }
    static string Fingerprint(Px a,int x,int y,int w,int h) {
        var bytes=new byte[w*h*4];for(int row=0;row<h;row++)Buffer.BlockCopy(a.Data,((y+row)*a.W+x)*4,bytes,row*w*4,w*4);
        using(var sha=System.Security.Cryptography.SHA256.Create())return Convert.ToBase64String(sha.ComputeHash(bytes));
    }
    static Px UnitBadge(int cls,int team) {
        var c=new Px(24,24);int cloth=team==0?Blue0:Crimson0,light=team==0?Blue2:Crimson2;
        c.Poly(Ink,2,1,21,1,23,3,23,21,21,23,2,23,0,21,0,3);
        c.Box(2,2,20,20,cloth);c.Line(2,2,21,2,light);c.Line(2,2,2,21,Iron2);
        var pose=Unit(cls,team,0,0,false);int sourceY=cls==3?0:8;
        for(int y=0;y<20;y++)for(int x=0;x<20;x++){int color=pose.Get(x+14,y+sourceY);if(color!=0)c.Dot(x+2,y+2,color);}
        c.Box(3,16,5,6,cloth);Crest(c,4,17,team);return c;
    }
    static Px Cursor(int state) {
        var c=new Px(24,24);
        // Conserva la punta de acero como primer píxel opaco y punto activo (1,1) en todas las variantes del cursor.
        c.Poly(Ink,1,1,2,18,6,13,10,22,15,20,11,12,18,12);
        c.Poly(Iron1,2,3,3,15,6,11,11,20,13,19,9,10,14,10);
        c.Poly(Iron3,2,3,3,12,6,9,12,10);c.Line(3,4,7,9,Iron2);
        c.Dot(1,1,Iron3);c.Line(7,12,11,20,Wood2,2);c.Dot(9,16,Ochre3);c.Dot(10,18,Iron3);
        if(state==1){c.Poly(Ink,14,15,18,11,23,16,20,16,20,22,16,22,16,16);c.Poly(Water3,16,15,18,13,21,15,19,15,19,21,17,21,17,15);}
        else if(state==2){c.Line(14,22,22,14,Ink,4);c.Line(14,22,22,14,Iron3,2);c.Line(16,18,20,22,Crimson2,2);c.Dot(22,13,Iron3);c.Dot(14,22,Wood3);}
        else if(state==3){c.Line(16,13,16,23,Ink,3);c.Line(16,13,16,22,Iron2);c.Poly(Ink,17,13,23,14,21,18,17,17);c.Poly(Rot2,17,14,22,15,20,17,17,16);}
        else if(state==4){c.Poly(Ink,16,12,21,12,24,16,24,20,21,23,16,23,13,20,13,16);c.Poly(Ochre1,17,13,21,13,23,16,23,19,20,22,17,22,14,19,14,16);c.Line(17,14,21,14,Ochre3);c.Line(17,15,17,19,Wood3);c.Line(19,15,19,20,Ochre3);c.Dot(21,19,Ochre0);}
        else if(state==5){c.Poly(Ink,16,12,21,12,24,16,24,20,21,23,16,23,13,20,13,16);c.Poly(Crimson1,17,13,21,13,23,16,23,19,20,22,17,22,14,19,14,16);c.Line(16,15,21,20,Pale,2);c.Dot(15,18,Crimson3);}
        return c;
    }
    static void VerifyPack(string target,string previews,Px[] atlases) {
        string[] names={"terrain.png","props.png","units.png","effects.png","icons.png","ui.png","menu.png","cursors.png","unit-icons.png"};
        int[] widths={48,64,48,48,16,32,720,24,24},heights={32,80,64,48,16,32,450,24,24};
        string report="[\n";int framesChecked=0;
        for(int n=0;n<atlases.Length;n++) {
            var a=atlases[n];int[] colors=new int[128];int colorCount=0,visible=0;
            foreach(int color in a.Data){bool known=false;for(int v=0;v<colorCount;v++)if(colors[v]==color){known=true;break;}if(!known){if(colorCount>=colors.Length)throw new Exception("Palette exceeds 128 colors");colors[colorCount++]=color;}if(color!=0){visible++;if(((uint)color>>24)!=255)throw new Exception("Intermediate alpha in "+names[n]);}}
            using(var saved=new Bitmap(Path.Combine(target,names[n])))if(saved.Width!=a.W||saved.Height!=a.H)throw new Exception("PNG dimensions differ: "+names[n]);
            for(int y=0;y<a.H;y+=heights[n])for(int x=0;x<a.W;x+=widths[n]){bool content=false;for(int yy=0;yy<heights[n]&&!content;yy++)for(int xx=0;xx<widths[n];xx++)if(a.Get(x+xx,y+yy)!=0){content=true;break;}if(!content)throw new Exception("Empty frame in "+names[n]);framesChecked++;}
            report+="  {\"file\":\""+names[n]+"\",\"width\":"+a.W+",\"height\":"+a.H+",\"colors\":"+colorCount+",\"visible\":"+visible+",\"alpha\":\"0/255\"}"+(n+1<atlases.Length?",\n":"\n");
        }
        var units=atlases[2];int sequences=0;
        for(int row=0;row<56;row++)for(int state=0;state<6;state++){var hashes=new string[8];for(int f=0;f<8;f++){hashes[f]=Fingerprint(units,(state*8+f)*48,row*64,48,64);for(int previous=0;previous<f;previous++)if(hashes[f]==hashes[previous])throw new Exception("Repeated unit pose at row "+row+" state "+state);}sequences++;}
        int pairedFrames=0,minTeamPixels=9999,minSharedPixels=9999;
        for(int dir=0;dir<4;dir++)for(int cls=0;cls<7;cls++)for(int frame=0;frame<48;frame++){
            int blue=0,red=0,shared=0;
            for(int y=0;y<64;y++)for(int x=0;x<48;x++){
                int a=units.Get(frame*48+x,(cls+dir*14)*64+y),b=units.Get(frame*48+x,(cls+7+dir*14)*64+y);
                if(a==Blue0||a==Blue1||a==Blue2||a==Blue3)blue++;
                if(b==Crimson0||b==Crimson1||b==Crimson2||b==Crimson3)red++;
                if(a==b&&(a==Iron0||a==Iron1||a==Iron2||a==Iron3||a==Skin0||a==Skin1||a==Skin2||a==Wood0||a==Wood1||a==Wood2||a==Wood3))shared++;
            }
            if(blue<4||red<4||shared<8)throw new Exception("Missing team cloth or shared material at class "+cls+" direction "+dir+" frame "+frame);
            minTeamPixels=Math.Min(minTeamPixels,Math.Min(blue,red));minSharedPixels=Math.Min(minSharedPixels,shared);pairedFrames++;
        }
        for(int state=0;state<6;state++)if(atlases[7].Get(state*24+1,1)!=Iron3)throw new Exception("Cursor hotspot differs in state "+state);
        File.WriteAllText(Path.Combine(previews,"faction-checks.json"),"{\"paired_frames\":"+pairedFrames+",\"minimum_team_cloth_pixels\":"+minTeamPixels+",\"minimum_identical_shared_material_pixels\":"+minSharedPixels+",\"insignias\":[\"pale cross\",\"two pale bars\"],\"cursor_states\":6,\"cursor_hotspot\":[1,1],\"runtime_verified_by_this_script\":false}\n");
        var ui=atlases[5];for(int patch=0;patch<3;patch++){int fill=ui.Get(patch*32+8,8);for(int y=8;y<24;y++)for(int x=8;x<24;x++)if(ui.Get(patch*32+x,y)!=fill)throw new Exception("Nonuniform UI stretch center");}
        var props=atlases[1];for(int frame=0;frame<4;frame++)for(int col=3;col<=5;col++)for(int y=0;y<80;y++)for(int x=0;x<64;x++){int color=props.Get(col*64+x,frame*80+y);if(color==Blue0||color==Blue1||color==Blue2||color==Blue3)throw new Exception("Baked blue team cloth on building "+col);}
        report+="]\n";File.WriteAllText(Path.Combine(previews,"validation.json"),report);
        File.WriteAllText(Path.Combine(previews,"checks.json"),"{\"native_pixel_art\":true,\"alpha_values\":[0,255],\"nonempty_frames_checked\":"+framesChecked+",\"unit_sequences\":"+sequences+",\"frames_per_sequence\":8,\"unit_frames_checked\":2688,\"unit_clipped_marks\":0,\"uniform_ui_centers\":true,\"no_baked_blue_building_flags\":true,\"game_integration_verified_by_this_script\":false}\n");
        Console.WriteLine("Pixel verification PASS: "+framesChecked+" nonempty frames; "+sequences+" unit sequences x 8 distinct poses; binary alpha; uniform UI centers; no baked blue building flags.");
    }
    public static void Generate(string target,string previews) {
        var terrain=Atlas(48,32,10,4,(col,row)=>Terrain(col,row));terrain.Save(Path.Combine(target,"terrain.png"));terrain.Zoom(3).Save(Path.Combine(previews,"terrain-3x.png"));
        var props=Atlas(64,80,8,4,(col,row)=>Prop(col,row));props.Save(Path.Combine(target,"props.png"));props.Zoom(3).Save(Path.Combine(previews,"props-3x.png"));
        var units=new Px(48*48,64*56);string clipping="";for(int dir=0;dir<4;dir++)for(int team=0;team<2;team++)for(int cls=0;cls<7;cls++)for(int state=0;state<6;state++)for(int f=0;f<8;f++){var pose=Unit(cls,team,state,f,dir>=2);if(pose.ClippedMarks>0)clipping+="class "+cls+" state "+state+" frame "+f+": "+pose.ClippedMarks+"\n";units.Stamp(pose,(state*8+f)*48,(cls+team*7+dir*14)*64,dir%2==1);}File.WriteAllText(Path.Combine(previews,"clipping.txt"),clipping);if(clipping.Length>0)throw new Exception("Character pixels outside native frame; inspect clipping.txt");units.Save(Path.Combine(target,"units.png"));PreviewUnits(units,previews);
        var effects=Atlas(48,48,8,4,(col,row)=>Effect(col,row));effects.Save(Path.Combine(target,"effects.png"));effects.Zoom(2).Save(Path.Combine(previews,"effects-2x.png"));
        var icons=Atlas(16,16,8,4,(col,row)=>Icon(col+row*8));icons.Save(Path.Combine(target,"icons.png"));icons.Zoom(4).Save(Path.Combine(previews,"icons-4x.png"));
        var ui=UI();ui.Save(Path.Combine(target,"ui.png"));ui.Zoom(4).Save(Path.Combine(previews,"ui-4x.png"));
        var menu=Menu();menu.Save(Path.Combine(target,"menu.png"));menu.Zoom(2).Save(Path.Combine(previews,"menu-2x.png"));
        var cursors=Atlas(24,24,6,1,(col,row)=>Cursor(col));cursors.Save(Path.Combine(target,"cursors.png"));cursors.Zoom(4).Save(Path.Combine(previews,"cursors-4x.png"));
        var badges=Atlas(24,24,7,2,(col,row)=>UnitBadge(col,row));badges.Save(Path.Combine(target,"unit-icons.png"));badges.Zoom(4).Save(Path.Combine(previews,"unit-icons-4x.png"));
        var bodies=Atlas(48,64,7,2,(col,row)=>Unit(col,row,4,7,false));bodies.Zoom(3).Save(Path.Combine(previews,"fallen-teams-3x.png"));
        VerifyPack(target,previews,new Px[]{terrain,props,units,effects,icons,ui,menu,cursors,badges});
    }
}
'@

if (-not ('PlaguefrontPixelArt' -as [type])) {
    if ($PSVersionTable.PSVersion.Major -ge 6) {
        $references = @([AppContext]::GetData('TRUSTED_PLATFORM_ASSEMBLIES') -split [IO.Path]::PathSeparator)
        $references += [System.Drawing.Bitmap].Assembly.Location
        $references += [System.Drawing.Color].Assembly.Location
        $references += [System.Collections.Generic.HashSet[int]].Assembly.Location
        $references += [System.Security.Cryptography.SHA256].Assembly.Location
        Add-Type -TypeDefinition $source -ReferencedAssemblies ($references | Select-Object -Unique)
    } else {
        Add-Type -TypeDefinition $source -ReferencedAssemblies System.Drawing
    }
}
[PlaguefrontPixelArt]::Generate($outputDir,$previewDir)
Get-ChildItem -LiteralPath $outputDir -Filter '*.png' | Select-Object Name,Length
