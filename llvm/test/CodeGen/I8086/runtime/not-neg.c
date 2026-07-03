/* ~x lowers to NOT, -x to NEG (16- and 8-bit).  volatile keeps the values in
   registers so the ops are not constant-folded. */
static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}

int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("NN.TXT"):"cx","cc","memory");
  volatile unsigned short x=0x1234; volatile unsigned char b=0x12;
  unsigned short n16 = (unsigned short)~x;    /* 0xEDCB = 60875 */
  unsigned short g16 = (unsigned short)-x;    /* 0xEDCC = 60876 */
  unsigned char  n8  = (unsigned char)~b;     /* 0xED = 237 */
  unsigned char  g8  = (unsigned char)-b;     /* 0xEE = 238 */
  putu(n16);wr(' ');putu(g16);wr(' ');putu(n8);wr(' ');putu(g8);wr('\n');
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
