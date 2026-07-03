unsigned counter;
static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}
int main(void){__asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("G.TXT"):"cx","cc","memory");
  counter = 41;              /* store to global -> mov [counter], ax (moffs) */
  counter = counter + 1;     /* load+store global */
  putu(counter); wr('\n');   /* 42 */
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;}
