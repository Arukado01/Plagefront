param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))

# Genera arte original reproducible con recursos locales y sin contactar servicios externos.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$target = Join-Path $ProjectRoot 'Content/Textures/Plaguefront'
New-Item -ItemType Directory -Force -Path $target | Out-Null

$source = @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

public static class PlaguefrontArt {
    static readonly Color Ink = C("11191b");
    static readonly Color Silver = C("bcc6b7");
    static readonly Color Stone = C("777e6c");
    static readonly Color LightStone = C("a5ab8b");
    static readonly Color Earth = C("685b41");
    static readonly Color Gold = C("c9ab64");
    static readonly Color Blight = C("a6bf52");
    static Random R = new Random(731051);

    static Color C(string hex) { return Color.FromArgb(255, Convert.ToInt32(hex.Substring(0,2),16), Convert.ToInt32(hex.Substring(2,2),16), Convert.ToInt32(hex.Substring(4,2),16)); }
    static Color A(int alpha, Color color) { return Color.FromArgb(alpha,color); }
    static PointF[] P(params float[] xy) { PointF[] p = new PointF[xy.Length/2]; for(int i=0;i<p.Length;i++) p[i]=new PointF(xy[i*2],xy[i*2+1]); return p; }
    static void Fill(Graphics g, Color c, params float[] xy) { using(var b = new SolidBrush(c)) g.FillPolygon(b,P(xy)); }
    static void Poly(Graphics g, Color c, float width, params float[] xy) { using(var p=new Pen(c,width)) {p.LineJoin=LineJoin.Round; g.DrawPolygon(p,P(xy));} }
    static void Rect(Graphics g, Color c, float x,float y,float w,float h) { using(var b=new SolidBrush(c)) g.FillRectangle(b,x,y,w,h); }
    static void Ell(Graphics g, Color c, float x,float y,float w,float h) { using(var b=new SolidBrush(c)) g.FillEllipse(b,x,y,w,h); }
    static void Ring(Graphics g, Color c, float width,float x,float y,float w,float h) { using(var p=new Pen(c,width)) g.DrawEllipse(p,x,y,w,h); }
    static void Line(Graphics g, Color c, float width, params float[] xy) { using(var p = new Pen(c,width)) {p.StartCap=LineCap.Round;p.EndCap=LineCap.Round;p.LineJoin=LineJoin.Round;g.DrawLines(p,P(xy));} }
    static void Curve(Graphics g, Color c, float width, params float[] xy) {using(var p=new Pen(c,width)) {p.StartCap=LineCap.Round;p.EndCap=LineCap.Round; g.DrawCurve(p,P(xy),0.45f);} }
    static Bitmap Bitmap(int w,int h) { return new Bitmap(w,h,PixelFormat.Format32bppArgb); }
    static Graphics Context(Bitmap b) { Graphics g=Graphics.FromImage(b);g.SmoothingMode=SmoothingMode.AntiAlias;g.PixelOffsetMode=PixelOffsetMode.HighQuality;g.CompositingQuality=CompositingQuality.HighQuality;return g; }
    static void Save(Bitmap b,string root,string file) {b.Save(Path.Combine(root,file),ImageFormat.Png);b.Dispose();}
    static GraphicsPath Hex() {var h=new GraphicsPath();h.AddPolygon(P(32,1,59,16,59,47,32,63,5,47,5,16));return h;}

    static void Ground(Graphics g,int kind) {
        Color[] ground={C("66704c"),C("394e3b"),C("777359"),C("405949"),C("64695a"),C("6f674e"),C("3e6970"),C("49666a"),C("284d5e"),C("444e4e")};
        using(var h=Hex()) {
            using(var br=new LinearGradientBrush(new Point(0,0),new Point(45,64),ground[kind],Ink)) g.FillPath(br,h);
            var state=g.Save();g.SetClip(h);
            for(int i=0;i<75;i++) {int x=R.Next(5,60),y=R.Next(3,62);Ell(g,A(R.Next(12,34),i%2==0?Silver:Ink),x,y,R.Next(1,5),R.Next(1,3));}
            if(kind==0 || kind==1 || kind==2) for(int i=0;i<16;i++) {int x=R.Next(8,56),y=R.Next(8,57); Line(g,A(80,C("a5ab6c")),1,x,y,x+1,y-3,x+3,y-2);}
            if(kind==1) {
                Tree(g,20,32,16); Tree(g,41,30,18); Tree(g,30,45,23); Tree(g,48,45,14);Tree(g,14,46,12);
            } else if(kind==2) {
                Fill(g,C("444b3d"),8,47,24,18,37,44); Fill(g,C("9a9270"),24,18,34,35,28,31,22,33,17,38);
                Fill(g,C("5b604a"),27,52,42,24,60,51); Fill(g,C("b4a982"),42,24,48,35,43,32,38,36,35,39);
                Line(g,A(100,LightStone),1,12,50,23,48,30,51,40,48,54,51);
            } else if(kind==3) {
                Ell(g,C("233e37"),8,23,31,10);Ell(g,C("2c4d43"),25,37,32,11);Line(g,C("718765"),1,10,28,20,29,29,27);
                for(int i=0;i<15;i++) {int x=R.Next(10,54),y=R.Next(20,52);Line(g,C("9c9763"),1,x,y,x+1,y-7);Line(g,C("535737"),2,x+1,y-6,x+1,y-9);}
            } else if(kind==4) {
                for(int y=21;y<47;y+=8) for(int x=12;x<50;x+=12) if(R.Next(3)!=0) {Rect(g,C("444b43"),x,y,11,7);Rect(g,Stone,x,y,10,5);Line(g,LightStone,1,x,y,x+8,y);}
                Fill(g,C("333c34"),11,47,18,35,22,41,29,32,37,45,50,41,55,51);
                for(int i=0;i<8;i++) {int x=R.Next(15,52),y=R.Next(39,52);Fill(g,C("858875"),x,y,x+4,y-3,x+7,y+2,x+1,y+3);}
            } else if(kind==5) {
                Line(g,C("393f34"),20,0,52,21,41,39,25,64,12);Line(g,C("b0a07a"),15,0,52,21,41,39,25,64,12);
                for(int i=0;i<9;i++){float x=i*8,y=52-i*5;Line(g,A(110,Earth),1,x-2,y-5,x+3,y+5);}
            } else if(kind==6 || kind==7 || kind==8) {
                if(kind!=8){Line(g,C("223e49"),26,13,0,30,24,28,39,48,64);Line(g,C("457c82"),20,13,0,30,24,28,39,48,64);}
                for(int i=0;i<10;i++){int x=R.Next(9,53),y=R.Next(8,56);Line(g,A(100,C("9ababb")),1,x,y,x+8,y-2);}
                if(kind==7) {Line(g,C("202c2d"),21,9,39,52,24);for(int i=0;i<8;i++){int x=11+i*5;Line(g,C("a39876"),3,x,31-(i*2),x+4,45-(i*2));}Line(g,C("c1b18a"),3,8,32,49,18);Line(g,C("4c4637"),3,13,47,56,32);}
            } else if(kind==9) {
                Fill(g,C("293536"),7,50,18,14,30,39,41,10,59,49);Fill(g,C("788172"),18,14,24,28,20,25,15,34);Fill(g,C("8a9380"),41,10,49,27,43,23,37,31,31,37);
                Line(g,A(100,Silver),1,15,47,25,49,35,46,48,50);
            }
            g.Restore(state);
            using(var p=new Pen(A(180,C("acb18a")),1.1f))g.DrawPath(p,h);
            Line(g,A(110,C("050b0d")),1.5f,5,47,32,63,59,47);
        }
    }
    static void Tree(Graphics g,float x,float y,float h) {
        Ell(g,A(100,Ink),x-h*.48f,y-2,h,5);Line(g,C("6c6243"),2,x,y,x,y-h*.63f);
        Fill(g,C("1c342a"),x,y-h,x-h*.4f,y-h*.2f,x+h*.4f,y-h*.2f);
        Fill(g,C("284b35"),x,y-h,x-h*.32f,y-h*.44f,x+h*.22f,y-h*.44f);
        Fill(g,C("446443"),x-.5f,y-h,x-h*.26f,y-h*.44f,x,y-h*.55f);
        Line(g,C("6c8050"),1,x-1,y-h+2,x-h*.17f,y-h*.6f);
    }
    static void Masonry(Graphics g,float x,float y,float w,float h) {
        Rect(g,C("434b46"),x,y,w,h);Rect(g,Stone,x,y,w-2,h);Line(g,LightStone,1,x,y,x+w-2,y);
        for(int yy=(int)y+5;yy<y+h;yy+=7){Line(g,A(160,Ink),1,x,yy,x+w,yy);for(int xx=(int)x+((yy/7)%2)*6;xx<x+w;xx+=11)Line(g,A(130,Ink),1,xx,yy,xx,yy+6);}
    }
    static void House(Graphics g) {
        Ell(g,A(100,Ink),12,43,42,12);Masonry(g,18,29,29,20);Fill(g,C("261e1b"),11,29,32,12,53,29);Fill(g,C("684837"),14,28,32,14,49,28);Fill(g,C("8b6442"),15,27,32,14,32,27);
        for(int i=0;i<4;i++)Line(g,C("a77d4e"),1,18+i*3,26-i*2,30,26-i*2);
        Rect(g,Ink,30,37,8,12);Fill(g,Gold,23,33,27,33,27,38,23,38);Line(g,C("4b4332"),1,25,33,25,38);
        Masonry(g,41,15,5,11);Line(g,C("333b32"),2,17,48,47,48);Line(g,C("aea98a"),1,18,30,18,46);
    }
    static void Fortress(Graphics g,bool small) {
        Ell(g,A(120,Ink),5,46,54,12);
        if(small) {
            Masonry(g,14,30,37,20);Masonry(g,23,16,19,33);Fill(g,C("454c45"),19,16,32,7,46,16);Fill(g,C("a69770"),20,15,32,7,32,15);
            for(int i=0;i<4;i++)Rect(g,LightStone,14+i*10,26,6,6);
            Rect(g,Ink,28,38,9,12);Rect(g,C("28322f"),29,22,5,7);Line(g,Gold,1,23,19,41,19);Flag(g,44,17,Gold,9);
        } else {
            Masonry(g,12,29,41,22);Masonry(g,9,21,11,31);Masonry(g,45,21,11,31);Masonry(g,24,16,18,35);
            for(int i=0;i<3;i++){Rect(g,LightStone,8+i*4,17,3,7);Rect(g,LightStone,45+i*4,17,3,7);}
            for(int i=0;i<4;i++)Rect(g,LightStone,23+i*5,11,3,7);
            Fill(g,Ink,27,50,27,37,30,33,35,33,39,37,39,50);Rect(g,C("605342"),29,39,8,11);
            Line(g,Ink,1,31,39,31,49);Line(g,Ink,1,35,39,35,49);Rect(g,C("d7bd77"),28,21,3,5);Rect(g,C("d7bd77"),35,21,3,5);
            Flag(g,34,11,Gold,10);Line(g,LightStone,1,10,26,18,26);Line(g,LightStone,1,46,26,54,26);
        }
    }
    static void Flag(Graphics g,float x,float y,Color c,float size) {Line(g,Silver,1,x,y,x,y-size-2);Fill(g,c,x,y-size-1,x+size,y-size+1,x+size*.7f,y-size+5,x,y-size+4);}
    static void Rot(Graphics g) {
        Ell(g,A(75,Blight),4,17,56,42);Ring(g,A(170,Blight),1,9,21,46,31);
        for(int i=0;i<7;i++){float a=i*.91f;float x=32+(float)Math.Cos(a)*25,y=37+(float)Math.Sin(a)*16;Curve(g,A(200,C("556d34")),3,32,40,20+i*3,26+i%3*9,x,y);}
        for(int i=0;i<18;i++){int x=R.Next(9,55),y=R.Next(19,54),r=R.Next(2,6);Ell(g,C("293b27"),x-1,y-1,r+2,r+2);Ell(g,A(170,Blight),x,y,r,r);Ell(g,C("d5d985"),x,y,1,1);}
        for(int i=0;i<5;i++){int x=R.Next(16,48),y=R.Next(10,32);Ring(g,A(150,Blight),1,x,y,3,3);}
    }
    static void Corpse(Graphics g) {
        Ell(g,A(110,Ink),11,34,43,17);Line(g,C("433932"),8,20,39,40,46);Line(g,C("a29274"),4,17,37,43,48);Line(g,C("7e6b52"),3,25,44,33,33);Line(g,C("7e6b52"),3,25,44,20,48);
        Ell(g,C("cabd91"),13,31,11,9);Ell(g,Ink,15,34,2,2);Ell(g,Ink,20,34,2,2);Fill(g,Ink,18,36,16,39,20,39);Line(g,C("a99877"),2,16,41,21,41);
        Line(g,C("6a4031"),3,37,42,48,39);Line(g,Silver,1,39,37,52,29);Line(g,C("927f51"),2,46,34,49,38);
    }
    static void Tiles(string root) {
        var b=Bitmap(512,128);using(var g=Context(b))for(int i=0;i<15;i++){var s=g.Save();g.TranslateTransform((i%8)*64,(i/8)*64);if(i<10)Ground(g,i);else if(i==10)House(g);else if(i==11)Fortress(g,false);else if(i==12)Fortress(g,true);else if(i==13)Rot(g);else Corpse(g);g.Restore(s);}Save(b,root,"tiles.png");
    }

    static void Head(Graphics g,float x,float y,Color team,bool hood) {
        Ell(g,Ink,x-5,y-6,12,14);Ell(g,C("b9a77f"),x-3,y-3,8,9);
        if(hood){Fill(g,Ink,x-6,y+2,x-4,y-8,x+3,y-10,x+8,y+2,x+4,y-2,x-2,y-2);Line(g,team,1,x-4,y-5,x+1,y-8,x+6,y-2);Line(g,Silver,1,x-1,y+1,x+3,y+1);}
        else {Fill(g,C("84948a"),x-5,y-1,x-5,y-7,x+2,y-9,x+7,y-4,x+7,y+1,x+1,y-1);Fill(g,Silver,x-4,y-6,x+1,y-8,x+1,y-2,x-4,y-2);Line(g,Ink,1,x-4,y,x+5,y);Rect(g,Ink,x+1,y,2,4);Line(g,Gold,1,x-1,y-8,x-1,y-4);}
    }
    static void Body(Graphics g,Color team,float x=31,float y=31) {
        Line(g,Ink,8,x-4,y+6,x-7,y+19);Line(g,Ink,8,x+4,y+6,x+8,y+19);Line(g,C("423e33"),5,x-4,y+7,x-7,y+18);Line(g,C("423e33"),5,x+4,y+7,x+8,y+18);
        Fill(g,Ink,x-11,y-6,x+9,y-6,x+13,y+12,x-13,y+12);Fill(g,team,x-9,y-5,x+8,y-5,x+10,y+9,x-10,y+9);
        Fill(g,C("748279"),x-7,y-5,x+6,y-5,x+7,y+2,x-7,y+2);Line(g,Silver,1,x-5,y-4,x,y-1,x+5,y-4);Line(g,Gold,2,x-9,y+6,x+9,y+6);Rect(g,Gold,x-1,y+4,3,4);
        Line(g,Ink,4,x-8,y+18,x-3,y+18);Line(g,Ink,4,x+5,y+18,x+10,y+18);
    }
    static void Sword(Graphics g,float x,float y,bool diagonal) {
        if(diagonal){Line(g,Ink,5,x,y,x+14,y-23);Line(g,Silver,3,x,y,x+14,y-23);Line(g,C("f0efd4"),1,x+1,y-2,x+14,y-23);Line(g,Gold,3,x-3,y-2,x+6,y+3);Line(g,C("514535"),3,x-3,y+5,x,y);Ell(g,Gold,x-5,y+5,3,3);}
        else{Fill(g,Ink,x-3,y,x-2,y-22,x,y-27,x+3,y-22,x+3,y);Fill(g,Silver,x-1,y,x-1,y-22,x,y-25,x+2,y-21,x+1,y);Line(g,Gold,3,x-5,y,x+6,y);Line(g,C("675c41"),3,x,y,x,y+7);}
    }
    static void Unit(Graphics g,int cls,int teamIndex) {
        Color team=teamIndex==0?C("548ca7"):C("b89151");Color bright=teamIndex==0?C("9bc4d4"):C("e0c588");
        Ell(g,A(140,Ink),11,47,43,12);Ell(g,A(135,team),13,47,39,9);Ring(g,A(210,bright),1,13,47,39,9);
        if(cls==3) {
            // Distingue la silueta del caballo, los arreos, el jinete y el banderín del Rider.
            Line(g,Ink,7,19,40,15,51);Line(g,Ink,7,42,39,49,51);Line(g,C("81765a"),4,19,40,15,50);Line(g,C("81765a"),4,42,39,49,50);
            Ell(g,Ink,10,28,39,18);Ell(g,C("81765a"),12,29,34,14);Fill(g,Ink,37,35,40,22,43,14,51,17,57,26,52,30,46,27,47,39);Fill(g,C("a49370"),40,35,43,22,45,17,50,19,54,25,50,27,45,24,44,37);
            Fill(g,C("3c3932"),42,22,43,14,46,13,46,25);Ell(g,Ink,49,21,2,2);Line(g,C("cab98b"),1,44,24,53,26);Fill(g,team,19,29,34,29,38,41,18,41);Line(g,Gold,1,20,31,33,31,36,39,20,39);
            Line(g,Ink,3,12,32,8,38,9,43);Body(g,team,28,23);Head(g,28,13,team,false);Line(g,Ink,3,40,36,41,4);Line(g,Silver,1,40,36,41,4);Fill(g,bright,42,5,57,9,48,13,42,11);return;
        }
        if(cls==4) {
            Fill(g,Ink,30,15,21,28,15,50,47,50,40,28);Fill(g,C("514a7e"),30,19,24,28,19,48,43,48,37,27);Fill(g,team,31,21,27,29,27,46,38,47,34,29);Line(g,Gold,1,24,44,33,46,42,45);Head(g,30,17,team,true);
            Fill(g,C("4c4669"),19,14,30,0,37,13);Line(g,Gold,1,20,14,38,14);Line(g,Ink,5,48,47,46,13);Line(g,C("b0a077"),2,48,47,46,13);Ell(g,A(80,C("8ba3dc")),39,1,15,17);Fill(g,C("b7c8ef"),46,3,50,9,46,16,42,9);Fill(g,C("6f83bb"),46,5,48,9,46,13,44,9);Line(g,Silver,2,37,28,45,22);return;
        }
        if(cls==5) {
            Fill(g,Ink,29,14,19,29,13,49,32,46,47,50,42,28);Fill(g,C("2c4042"),29,18,21,29,17,47,30,43,43,47,38,29);Body(g,team,30,32);Head(g,30,17,team,true);
            Line(g,Ink,5,21,30,14,37);Line(g,C("7e918a"),3,21,30,15,37);Line(g,Silver,3,12,35,9,25);Line(g,Gold,2,9,35,15,35);Line(g,Ink,5,40,30,47,38);Line(g,C("7e918a"),3,40,30,46,38);Line(g,Silver,3,48,35,55,26);Line(g,Gold,2,44,35,49,39);Line(g,bright,1,25,26,35,26);return;
        }
        Body(g,team);Head(g,31,17,team,cls==1);
        if(cls==0) {Line(g,Ink,5,39,29,46,35);Line(g,C("819087"),3,39,29,46,35);Sword(g,45,32,true);Fill(g,Ink,13,27,26,25,27,40,20,47,12,40);Fill(g,team,15,29,24,28,25,39,20,44,14,39);Poly(g,Gold,1,15,29,24,28,25,39,20,44,14,39);Line(g,Gold,1,20,29,20,42,16,35,23,35);}
        else if(cls==1) {Line(g,Ink,4,41,30,46,34);Line(g,C("a59976"),2,41,30,46,34);Curve(g,Ink,5,48,11,55,20,56,33,49,46);Curve(g,C("b79d65"),2,48,11,55,20,56,33,49,46);Line(g,Silver,1,48,12,48,45);Line(g,Silver,1,40,29,60,29);Fill(g,Silver,60,29,55,26,55,31);Line(g,C("a69367"),3,20,14,15,38);Line(g,C("9cb6ae"),1,15,9,20,24);}
        else if(cls==2) {Line(g,Ink,5,46,50,49,8);Line(g,C("b39b65"),2,46,50,49,8);Fill(g,Ink,45,10,49,0,53,10,50,15);Fill(g,Silver,46,10,49,2,51,10,49,14);Line(g,C("adbab0"),3,39,28,47,32);Fill(g,Ink,14,28,25,28,25,43,16,44);Fill(g,team,16,30,23,30,23,41,17,42);Line(g,Gold,1,18,31,22,31,20,40);}
        else if(cls==6) {
            Fill(g,C("74634c"),19,21,25,25,38,25,43,21,45,28,37,31,26,31,17,28);Ell(g,C("b79e74"),17,26,8,10);Ell(g,C("b79e74"),39,26,8,10);Line(g,Ink,5,46,44,52,13);Line(g,C("a78f5f"),2,46,44,52,13);Fill(g,Ink,50,13,42,9,40,20,47,25,55,20,61,14,56,8);Fill(g,Silver,50,14,43,11,42,19,48,23,55,18,59,14,55,10);Line(g,C("e1dfbb"),1,44,12,43,19,47,22);Fill(g,C("8d7756"),23,11,17,7,18,17,25,19);Fill(g,C("8d7756"),37,11,44,6,44,16,37,19);Rect(g,C("b97b58"),29,19,5,5);
        }
    }
    static void Units(string root) {var b=Bitmap(448,128);using(var g=Context(b))for(int row=0;row<2;row++)for(int col=0;col<7;col++){var s=g.Save();g.TranslateTransform(col*64,row*64);Unit(g,col,row);g.Restore(s);}Save(b,root,"units.png");}

    static void Icon(Graphics g,int i) {
        Color accent=(i>=16 && i<20)?C("abc3c6"):Gold;
        Ell(g,A(70,Ink),2,3,28,27);Ring(g,A(70,accent),.8f,2,3,28,27);
        if(i==0 || i==24){Sword(g,12,22,true);if(i==24){Line(g,Gold,2,23,20,23,28);Line(g,Gold,2,19,24,27,24);}}
        else if(i==1){Curve(g,Gold,3,10,5,23,12,23,21,10,28);Line(g,Silver,1,10,5,10,28);Line(g,Silver,1,6,16,28,16);Fill(g,Silver,29,16,24,13,24,19);}
        else if(i==2 || i==26){Line(g,Gold,2,9,28,22,6);Fill(g,Silver,19,7,25,1,25,10,21,11);if(i==26){Line(g,Silver,1,3,25,16,3);Line(g,Silver,1,16,29,28,7);}}
        else if(i==3 || i==27){Fill(g,Silver,7,27,8,16,16,6,24,8,28,17,22,20,16,16,18,27);Fill(g,Gold,9,15,12,5,16,6,13,14);Ell(g,Ink,22,11,2,2);if(i==27)Line(g,Gold,2,3,22,9,18,3,14);}
        else if(i==4){Fill(g,C("d76a42"),16,3,24,15,24,25,18,29,9,27,6,20,11,12,11,20);Fill(g,C("e6b565"),15,13,20,21,18,27,12,26,10,22);}
        else if(i==5){Line(g,Silver,3,9,7,22,25);Line(g,Silver,3,23,7,10,25);Line(g,Gold,2,5,17,13,13);Line(g,Gold,2,19,13,27,17);}
        else if(i==6){Line(g,Gold,3,9,28,21,5);Fill(g,Silver,17,6,9,8,8,16,17,19,23,13,28,8,25,3);}
        else if(i==7){Fill(g,Gold,5,7,16,3,27,7,25,20,16,29,7,20);Fill(g,C("527d8d"),8,9,16,6,24,9,22,19,16,25,10,19);Line(g,Silver,2,16,10,16,22,11,15,21,15);}
        else if(i>=8 && i<=12){Color liquid=i==8?C("ad5252"):i==9?C("d26a57"):i==10?C("7c85c2"):i==11?C("57a48a"):Blight;Rect(g,Gold,12,3,8,5);Fill(g,Silver,11,7,21,7,21,13,26,20,24,28,8,28,6,20,11,13);Fill(g,liquid,10,17,22,17,24,22,22,26,10,26,8,22);Line(g,A(200,Color.White),1,10,19,10,23);if(i==9){Line(g,Gold,2,16,16,16,24);Line(g,Gold,2,12,20,20,20);}if(i==12){Line(g,Silver,2,16,16,16,24);Line(g,Silver,2,12,20,20,20);}}
        else if(i==13){Ell(g,C("816b3f"),6,5,20,23);Ell(g,Gold,6,4,19,21);Ring(g,C("f0d287"),1,8,6,15,17);Line(g,C("6a5b35"),2,17,9,12,13,19,18,13,23);}
        else if(i==14){Line(g,Silver,2,9,28,9,3);Fill(g,Gold,11,4,28,7,23,12,11,11);Line(g,Gold,2,6,28,13,28);}
        else if(i==15){Line(g,Silver,5,7,25,24,7);Fill(g,Gold,14,6,20,1,29,11,25,15);Line(g,Ink,1,20,4,27,11);}
        else if(i==16){Ell(g,Gold,10,10,12,12);for(int a=0;a<8;a++){double t=a*Math.PI/4;Line(g,Gold,2,16+(float)Math.Cos(t)*10,16+(float)Math.Sin(t)*10,16+(float)Math.Cos(t)*14,16+(float)Math.Sin(t)*14);}}
        else if(i==17 || i==18 || i==19){Ell(g,C("a2b6b5"),5,8,22,12);Ell(g,C("a2b6b5"),10,5,13,15);if(i==17)for(int x=8;x<26;x+=7)Line(g,C("739faf"),2,x,23,x-2,28);if(i==18)for(int y=23;y<30;y+=4)Line(g,Silver,2,5,y,27,y);if(i==19)Fill(g,Gold,18,17,12,25,18,24,14,32,25,21,19,22);}
        else if(i==20){Ell(g,A(100,Blight),3,3,27,27);Fill(g,Blight,16,5,20,13,28,16,21,20,17,28,12,21,4,18,12,13);Ring(g,Ink,2,13,13,7,7);}
        else if(i==21){Ell(g,Silver,8,6,16,17);Rect(g,Silver,11,19,10,8);Ell(g,Ink,10,12,4,5);Ell(g,Ink,18,12,4,5);Fill(g,Ink,16,17,13,21,19,21);Line(g,Ink,1,13,24,20,24);}
        else if(i==22){Line(g,Silver,3,5,24,15,24,15,11,24,11);Fill(g,Gold,24,5,30,11,24,17);Ell(g,Gold,3,22,4,4);}
        else if(i==23){Curve(g,Silver,2,22,6,8,6,4,18,13,27,26,20);Fill(g,Gold,19,4,28,6,24,14);Line(g,Gold,2,16,10,16,18,21,20);}
        else if(i==25){Ring(g,Gold,2,7,7,18,18);Ring(g,Silver,1,12,12,8,8);Line(g,Gold,1,16,3,16,11);Line(g,Gold,1,16,21,16,29);Line(g,Gold,1,3,16,11,16);Line(g,Gold,1,21,16,29,16);}
        else if(i==28){Line(g,C("b7d6df"),2,16,3,16,29);Line(g,C("b7d6df"),2,5,9,27,23);Line(g,C("b7d6df"),2,5,23,27,9);for(int x=0;x<2;x++){Line(g,C("b7d6df"),1,10+12*x,6,10+12*x,12);Line(g,C("b7d6df"),1,10+12*x,20,10+12*x,26);}}
        else if(i==29){Fill(g,C("bdb4e3"),19,2,7,18,16,17,12,30,27,12,18,13);Line(g,C("eee8cf"),1,19,3,10,16,18,15);}
        else if(i==30){Ell(g,A(80,C("6faea0")),3,3,26,26);Rect(g,C("c5d7b6"),13,6,6,20);Rect(g,C("c5d7b6"),6,13,20,6);Line(g,Blight,1,15,8,15,13);}
        else if(i==31){for(int k=0;k<5;k++)Ell(g,A(90+k*20,C("a1a8a0")),4+k*3,18-k*3,11,11);Curve(g,Silver,1,7,24,14,18,13,12,21,6);}
    }
    static void Icons(string root) {var b=Bitmap(256,128);using(var g=Context(b))for(int i=0;i<32;i++){var s=g.Save();g.TranslateTransform((i%8)*32,(i/8)*32);Icon(g,i);g.Restore(s);}Save(b,root,"icons.png");}

    static void Mountain(Graphics g,Color c,float y,float amplitude,int count) {float[] p=new float[(count+4)*2];p[0]=-10;p[1]=900;p[2]=-10;p[3]=y;for(int i=0;i<count;i++){p[(i+2)*2]=i*(1460f/(count-1));p[(i+2)*2+1]=y-R.Next(15,(int)amplitude);}p[(count+2)*2]=1450;p[(count+2)*2+1]=y;p[(count+3)*2]=1450;p[(count+3)*2+1]=900;Fill(g,c,p);}
    static void TallTree(Graphics g,float x,float y,float scale) {
        Color c=C("101d20");Line(g,c,12*scale,x,y,x-10*scale,y-100*scale,x+3*scale,y-220*scale);
        for(int k=0;k<8;k++){float h=(65+k*20)*scale;float side=k%2==0?1:-1;Curve(g,c,(6-k*.4f)*scale,x-5*scale,y-h,x+side*(35+k*3)*scale,y-h-20*scale,x+side*(45+k*6)*scale,y-h-55*scale);Line(g,c,2*scale,x+side*(35+k*3)*scale,y-h-20*scale,x+side*(21+k*4)*scale,y-h-55*scale);}
    }
    static void Tower(Graphics g,int x,int y,int width,int height) {
        using(var br=new LinearGradientBrush(new Rectangle(x,y,width,height),C("51594e"),C("182b2e"),0f))g.FillRectangle(br,x,y,width,height);
        Rect(g,C("14262a"),x+width-8,y,8,height);Rect(g,C("8b8e70"),x,y,2,height);
        for(int xx=x-4;xx<x+width+3;xx+=12){Rect(g,C("566254"),xx,y-12,8,17);Line(g,C("8f9474"),1,xx,y-12,xx+7,y-12);}
        Line(g,C("8b8e70"),1,x-4,y+6,x+width+4,y+6);
        for(int yy=y+28;yy<y+height-18;yy+=36){Rect(g,C("0f1d21"),x+width/2-4,yy,8,17);Rect(g,C("b9a465"),x+width/2-3,yy+1,2,10);Line(g,A(80,Silver),1,x+4,yy+22,x+width-3,yy+22);}
        for(int yy=y+17;yy<y+height;yy+=17)Line(g,A(55,Ink),1,x,yy,x+width,yy);
    }
    static void Menu(string root) {
        var b=Bitmap(1440,900);using(var g=Context(b)) {
            using(var sky=new LinearGradientBrush(new Point(0,0),new Point(350,900),C("111e28"),C("5d6650")))g.FillRectangle(sky,0,0,1440,900);
            // Conserva una luna velada y una columna izquierda despejada para el texto del menú.
            for(int r=180;r>0;r-=8)Ell(g,A(2,C("c4cfb4")),1050-r,178-r,r*2,r*2);
            Ell(g,C("a9b29a"),1000,104,126,126);Ell(g,C("bdc5aa"),1006,106,114,114);
            for(int i=0;i<26;i++){int x=R.Next(1010,1112),y=R.Next(118,211);Ell(g,A(30,C("4b615a")),x,y,R.Next(4,15),R.Next(3,12));}
            for(int i=0;i<170;i++){int x=R.Next(400,1440),y=R.Next(0,330);Ell(g,A(R.Next(20,85),Silver),x,y,1,1);}
            Mountain(g,C("33494b"),520,185,17);Mountain(g,C("2b4141"),605,155,22);
            for(int i=0;i<110;i++){int x=R.Next(550,1440),y=R.Next(480,630);Tree(g,x,y,R.Next(22,70));}
            // Delimita el río y la cuenca contaminada.
            Fill(g,C("526e63"),930,555,1100,567,1051,620,939,656,923,714,1147,900,650,900,791,764,866,678,1010,609);
            for(int i=0;i<100;i++){int x=R.Next(700,1180),y=R.Next(630,900);Line(g,A(R.Next(8,40),C("bfcea1")),R.Next(1,3),x,y,x+R.Next(10,100),y);}
            Fill(g,C("253a36"),652,634,734,557,852,528,1048,547,1153,606,1135,651,1036,674,902,650,832,680,719,667);
            // Compone la fortaleza con torre estrecha y murallas exteriores asimétricas.
            Masonry(g,792,474,312,128);Rect(g,C("263a38"),792,481,312,122);
            for(int xx=788;xx<1105;xx+=23){Rect(g,C("5b6554"),xx,463,15,25);Line(g,C("8f9474"),1,xx,463,xx+14,463);}
            for(int y=495;y<595;y+=22)Line(g,A(100,C("15282b")),1,793,y,1105,y);
            Tower(g,766,388,60,226);Tower(g,1080,370,68,249);Tower(g,902,303,73,298);Tower(g,848,392,46,184);Tower(g,986,420,42,180);
            Fill(g,C("1a292d"),891,290,940,217,985,290);Fill(g,C("535c55"),893,287,939,223,939,287);Line(g,C("808a70"),1,939,223,978,286);
            Rect(g,C("0d1d22"),924,496,35,107);Ell(g,C("0d1d22"),924,480,35,35);Line(g,C("727b63"),2,922,600,922,497,940,478,961,496,961,601);
            for(int x=929;x<959;x+=7)Line(g,C("4c5749"),2,x,501,x,603);
            Flag(g,940,222,C("a18c58"),45);Flag(g,1110,368,C("9a8250"),31);
            Line(g,C("a2a484"),2,782,389,782,359);Line(g,C("a2a484"),2,800,389,800,356);Line(g,C("68735d"),1,782,357,800,357);
            // Extiende la putrefacción por la muralla oriental y hacia el primer plano.
            for(int i=0;i<17;i++){float x=1000+R.Next(250),y=R.Next(600,850);Curve(g,A(160,C("687d38")),R.Next(2,8),1100,581,x-70,642,x+20,718,x,y);}
            for(int i=0;i<60;i++){int x=R.Next(1000,1330),y=R.Next(510,810),r=R.Next(2,9);Ell(g,A(50,C("bbca5b")),x-r,y-r,r*3,r*3);Ell(g,A(R.Next(70,170),Blight),x,y,r,r);}
            // Distribuye la niebla en bandas horizontales.
            for(int i=0;i<25;i++){int y=R.Next(500,705),x=R.Next(500,1350);using(var fog=new LinearGradientBrush(new Rectangle(x,y,230,25),A(0,Silver),A(R.Next(2,9),Silver),0f))g.FillEllipse(fog,x,y,230,25);}
            Fill(g,C("13282a"),0,701,179,674,303,719,429,736,620,715,723,762,761,900,0,900);
            Fill(g,C("102225"),1069,867,1159,756,1241,719,1440,645,1440,900,1090,900);
            TallTree(g,1395,807,2.1f);TallTree(g,1310,760,1.5f);TallTree(g,119,815,1.7f);TallTree(g,47,810,2.1f);
            for(int i=0;i<130;i++){int x=R.Next(0,1440),y=R.Next(798,900);if(x<570 || x>1100){Line(g,C("162d2c"),R.Next(1,4),x,y,x+R.Next(-18,18),y-R.Next(12,62));}}
            // Mezcla brasas cálidas y esporas enfermizas con posiciones deterministas.
            for(int i=0;i<80;i++){int x=R.Next(400,1440),y=R.Next(300,890);Color c=i%4==0?C("c59a60"):Blight;Ell(g,A(15,c),x-3,y-3,9,9);Ell(g,A(R.Next(35,160),c),x,y,2,2);}
            using(var shade=new LinearGradientBrush(new Point(0,0),new Point(900,0),A(245,C("0b171d")),A(0,C("0b171d"))))g.FillRectangle(shade,0,0,900,900);
            using(var bottom=new LinearGradientBrush(new Point(0,625),new Point(0,900),A(0,Ink),A(205,Ink)))g.FillRectangle(bottom,0,625,1440,275);
            // Enmarca las esquinas sin ocupar los bordes necesarios para la interfaz.
            Line(g,A(90,Gold),1,30,90,30,30,110,30);Line(g,A(90,Gold),1,1330,30,1410,30,1410,90);
            Line(g,A(90,Gold),1,30,810,30,870,110,870);Line(g,A(90,Gold),1,1330,870,1410,870,1410,810);
            for(int i=0;i<7;i++)Fill(g,A(120-i*12,Gold),45+i*7,30,48+i*7,27,51+i*7,30,48+i*7,33);
        }Save(b,root,"menu.png");
    }
    static void Emblem(string root) {
        var b=Bitmap(128,128);using(var g=Context(b)) {
            Fill(g,C("14262b"),64,3,118,25,112,86,64,125,16,86,10,25);Poly(g,Gold,3,64,3,118,25,112,86,64,125,16,86,10,25);
            Fill(g,C("3d5350"),64,14,106,33,101,79,64,111,27,79,22,33);Line(g,Gold,2,64,17,64,106);
            var s=g.Save();g.TranslateTransform(17,13);g.ScaleTransform(1.45f,1.45f);Fortress(g,false);g.Restore(s);
            Curve(g,Blight,3,28,83,47,74,72,88,100,71);Curve(g,Blight,2,64,108,76,92,68,70,82,59);Ell(g,Blight,91,70,5,5);Ell(g,Blight,28,80,4,4);
        }Save(b,root,"emblem.png");
    }
    public static void Generate(string root) {R=new Random(731051);Tiles(root);Units(root);Icons(root);Menu(root);Emblem(root);}
}
'@

if (-not ('PlaguefrontArt' -as [type])) {
    if ($PSVersionTable.PSVersion.Major -ge 6) {
        $references = @([AppContext]::GetData('TRUSTED_PLATFORM_ASSEMBLIES') -split [IO.Path]::PathSeparator)
        $references += [System.Drawing.Bitmap].Assembly.Location
        $references += [System.Drawing.Color].Assembly.Location
        Add-Type -TypeDefinition $source -ReferencedAssemblies ($references | Select-Object -Unique)
    } else {
        Add-Type -TypeDefinition $source -ReferencedAssemblies System.Drawing
    }
}
[PlaguefrontArt]::Generate($target)
Get-ChildItem -LiteralPath $target -Filter '*.png' | Select-Object Name,Length
