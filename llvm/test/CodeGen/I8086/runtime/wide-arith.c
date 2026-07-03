/* Freestanding .COM: exercise runtime 16/32/64-bit arithmetic, writing the
   results to RESULT.TXT via DOS file I/O so the host can read them back. */
static unsigned fh;

static int dos_create(const char *name) {
  unsigned h;
  __asm__ volatile("mov ah,0x3c\n\txor cx,cx\n\tint 0x21"
                   : "=a"(h) : "d"(name) : "cx", "cc", "memory");
  return h;
}
static void dos_write(char c) {
  char b = c;
  __asm__ volatile("mov ah,0x40\n\tmov cx,1\n\tint 0x21"
                   : : "b"(fh), "d"(&b) : "ax", "cx", "cc", "memory");
}
static void dos_close(unsigned h) {
  __asm__ volatile("mov ah,0x3e\n\tint 0x21" : : "b"(h) : "ax", "cc", "memory");
}
static void puts(const char *s) { while (*s) dos_write(*s++); }
static void putu32(unsigned long v) {
  char buf[12]; int i = 0;
  if (!v) { dos_write('0'); return; }
  while (v) { buf[i++] = '0' + (char)(v % 10); v /= 10; }
  while (i) dos_write(buf[--i]);
}
static void putu64(unsigned long long v) {
  char buf[24]; int i = 0;
  if (!v) { dos_write('0'); return; }
  while (v) { buf[i++] = '0' + (char)(v % 10); v /= 10; }
  while (i) dos_write(buf[--i]);
}

int main(void) {
  volatile unsigned long a = 100000UL, b = 7UL;
  volatile unsigned long long x = 1000000000ULL, y = 999UL;
  volatile long sa = -100000L, sb = 3L;

  fh = dos_create("RESULT.TXT");
  puts("u32mul="); putu32(a * b); puts("\n");   /* 700000 */
  puts("u32div="); putu32(a / b); puts("\n");   /* 14285  */
  puts("u32mod="); putu32(a % b); puts("\n");   /* 5      */
  puts("s32div="); { long q = sa / sb; putu32((unsigned long)q); } puts(" (=-33333 as u32=4294933963)\n");
  puts("u64mul="); putu64(x * y); puts("\n");   /* 999000000000 */
  puts("u64div="); putu64(x / y); puts("\n");   /* 1001001 */
  puts("shift ="); putu32((a << 5) >> 2); puts("\n"); /* 800000 */
  dos_close(fh);
  __asm__ volatile("mov ah,0x4c\n\tint 0x21");
  return 0;
}
