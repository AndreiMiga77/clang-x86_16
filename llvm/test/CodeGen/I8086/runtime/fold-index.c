/* Post-RA [base+index] fold: an index kept live across a call lands in a
   callee-saved SI/DI, so `add base,index; mov reg,[base]` folds to
   `mov reg,[base+index]`.  A correct load proves the folded address is right. */
static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}

static volatile int sink;
static void use(int x){ sink = x; }        /* consumes i -> i live across call */
static int arr[6] = {0, 11, 22, 33, 44, 55};

/* i is used before AND after the call, so it survives in SI/DI; the a[i] load
   then folds to [bx+si].  Not inlined into main (has the call + external-ish
   store), so the fold happens here. */
static int idx(int *a, int i){ use(i); return a[i]; }

int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("FI.TXT"):"cx","cc","memory");
  volatile int i = 4;
  putu((unsigned)idx(arr, i)); wr('\n');   /* arr[4] = 44 */
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
