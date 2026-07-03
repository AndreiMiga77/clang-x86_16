/* Rotations lower to native ROL/ROR.  These are non-static (placed before main
   in .text), which also exercises the main-first .COM layout. */
unsigned short rol16(unsigned short x, unsigned n){ return (x << n) | (x >> (16-n)); }
unsigned short ror16(unsigned short x, unsigned n){ return (x >> n) | (x << (16-n)); }
unsigned char  rol8 (unsigned char  x, unsigned n){ return (unsigned char)((x << n) | (x >> (8-n))); }

static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}
int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("ROT.TXT"):"cx","cc","memory");
  volatile unsigned short x=0x1234; volatile unsigned char y=0x12; volatile unsigned n=4;
  putu(rol16(x,n)); wr(' '); putu(ror16(x,n)); wr(' '); putu(rol8(y,n)); wr('\n');
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
