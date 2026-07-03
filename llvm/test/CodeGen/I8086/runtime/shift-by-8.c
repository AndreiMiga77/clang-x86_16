static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}
int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("S8.TXT"):"cx","cc","memory");
  volatile unsigned short x=0x1234; volatile short s=(short)0x8234;
  unsigned short a=x<<8, b=x>>8, d=(unsigned short)((x<<8)|(x>>8));
  short c = s>>8;
  unsigned char t = (unsigned char)(x>>8);
  putu(a);wr(' ');putu(b);wr(' ');putu((unsigned short)c);wr(' ');putu(d);wr(' ');putu(t);wr('\n');
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
