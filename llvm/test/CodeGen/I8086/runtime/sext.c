/* i8->i16 sign extension via both paths: the plain CBW (AX free) and the
   AX-avoiding rol/sbb/ror (AX pinned live across the sext).  Negative values
   check that the high byte is correctly filled with the sign. */
static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}

/* keep a value pinned in AX across the sext so it is done in another reg pair */
static int sext_noax(signed char c){
  register int k asm("ax");
  __asm__ volatile("":"=a"(k):"0"(1000));
  int e = c;
  return e + k;
}

int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("SX.TXT"):"cx","cc","memory");
  volatile signed char n = -5;
  putu((unsigned)(int)n); wr(' ');            /* cbw path: -5 -> 0xFFFB = 65531 */
  putu((unsigned)sext_noax(-5)); wr(' ');     /* rol/sbb/ror path: -5 + 1000 = 995 */
  putu((unsigned)sext_noax(-128)); wr('\n');  /* -128 + 1000 = 872 */
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
