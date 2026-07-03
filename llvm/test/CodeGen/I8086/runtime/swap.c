static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}
int use2(int a,int b);  /* not defined; keep the loop's a,b live in regs */
static int swapn(int n,int a,int b,int*ob){ while(n-->0){int t=a;a=b;b=t;} *ob=b; return a; }
int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("SW.TXT"):"cx","cc","memory");
  volatile int n3=3,n4=4,a=11,b=22; int o1,o2;
  int r1=swapn(n3,a,b,&o1);   /* n=3 odd -> a=22,b=11 */
  int r2=swapn(n4,a,b,&o2);   /* n=4 even -> a=11,b=22 */
  putu(r1);wr(',');putu(o1);wr(' ');putu(r2);wr(',');putu(o2);wr('\n');
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
