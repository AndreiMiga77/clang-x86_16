/* Branch relaxation: a loop whose body exceeds the rel8 range forces the
   back-edge to be relaxed (inverted conditional + near rel16 jump).  A correct
   sum proves the relaxed far branch reaches its target.  The padding is a sled
   of individual NOPs -- executed harmlessly each iteration and, unlike
   `.space N, fill`, correctly length-counted (each `nop` is one instruction) so
   the relaxation pass sees the true body size. */
#define N1  "nop\n\t"
#define N8  N1 N1 N1 N1 N1 N1 N1 N1
#define N64 N8 N8 N8 N8 N8 N8 N8 N8

static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}

int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("RX.TXT"):"cx","cc","memory");
  volatile unsigned n = 5;
  unsigned r = 0, i;
  for (i = 0; i < n; i++) {
    r += i;
    __asm__ volatile(N64 N64 N64);        /* 192 nops: loop body > rel8 range */
  }
  putu(r); wr('\n');                       /* 0+1+2+3+4 = 10 */
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
