/* Constant shifts and rotates by more than 8 (bytes: more than 4).  A shift of
   9..15 becomes a byte op plus a small residual shift; a rotate flips direction
   for the shorter count.  volatile keeps the values register-resident so the
   shift is not folded into a narrowed load. */
static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}

static unsigned short rol16(unsigned short x, unsigned n){ return (unsigned short)((x << n) | (x >> (16-n))); }
static unsigned short ror16(unsigned short x, unsigned n){ return (unsigned short)((x >> n) | (x << (16-n))); }
static unsigned char  rol8 (unsigned char  x, unsigned n){ return (unsigned char)((x << n) | (x >> (8-n))); }

int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("SL.TXT"):"cx","cc","memory");
  volatile unsigned short x=0x1234; volatile short s=(short)0x8234; volatile unsigned char yb=0x12;
  unsigned short a = x << 9;             /* 26624 */
  unsigned short b = x << 12;            /* 16384 */
  unsigned short c = x >> 11;            /* 2 */
  short          d = s >> 11;            /* -16 -> 65520 as u16 */
  unsigned short e = rol16(x, 12);       /* == ror16(x,4) = 16675 */
  unsigned short f = ror16(x, 11);       /* == rol16(x,5) = 18050 */
  unsigned char  g = rol8(yb, 6);        /* == ror8(y,2) = 132 */
  putu(a);wr(' ');putu(b);wr(' ');putu(c);wr(' ');putu((unsigned short)d);wr(' ');
  putu(e);wr(' ');putu(f);wr(' ');putu(g);wr('\n');
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
