typedef struct { int x, y; } Point;
typedef struct { long a; int b; char c; } Big;
Point make_point(int x, int y);
int use_point(Point p);
Big make_big(long a, int b, char c);
long sum_big(Big g);

static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void puts_(const char*s){while(*s)wr(*s++);}
static void putl(long v){char b[16];int i=0;unsigned long u;if(v<0){wr('-');u=-(unsigned long)v;}else u=v;if(!u){wr('0');return;}while(u){b[i++]='0'+(char)(u%10);u/=10;}while(i)wr(b[--i]);}

int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("OUT.TXT"):"cx","cc","memory");
  volatile int seven=7, three=3;
  Point p = make_point(seven, three);
  puts_("use_point="); putl(use_point(p)); wr('\n');   /* 73 */
  volatile long hk=100000L; volatile int fh2=500; volatile char cc=20;
  Big g = make_big(hk, fh2, cc);
  puts_("sum_big="); putl(sum_big(g)); wr('\n');        /* 100520 */
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21");
  return 0;
}
