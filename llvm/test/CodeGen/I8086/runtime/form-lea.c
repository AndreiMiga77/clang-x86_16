/* A base+index+disp computed via a mov/add chain becomes one LEA (operands in
   addressing registers).  A correct sum proves the folded address arithmetic. */
static unsigned fh;
static void wr(char c){char b=c;__asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"::"b"(fh),"d"(&b):"ax","cx","cc","memory");}
static void putu(unsigned v){char b[8];int i=0;if(!v){wr('0');return;}while(v){b[i++]='0'+(char)(v%10);v/=10;}while(i)wr(b[--i]);}

/* Force the operands into SI and BX so the bx+si+8 chain collapses to
   `lea ax,[bx+si+8]`. */
static int lea3(void){
  int si, bx;
  __asm__ volatile("" : "=S"(si) : "0"(1000));
  __asm__ volatile("" : "=b"(bx) : "0"(23));
  return bx + si + 8;
}

int main(void){
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21":"=a"(fh):"d"("LE.TXT"):"cx","cc","memory");
  putu((unsigned)lea3()); wr('\n');          /* 23 + 1000 + 8 = 1031 */
  __asm__ volatile("mov ah,0x3e\n\tint 0x21"::"b"(fh):"ax","memory");
  __asm__ volatile("mov ah,0x4c\n\tint 0x21"); return 0;
}
