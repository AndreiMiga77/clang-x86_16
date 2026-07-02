//===-- i8086_builtins.c - runtime helpers for the i8086 target ----------===//
//
// Freestanding implementations of the libgcc/compiler-rt helper routines that
// the i8086 backend calls for arithmetic wider than the machine's native
// 16 bits.  The 8086 has native 16x16->32 MUL and 32/16 DIV, so the backend
// synthesises 16-bit (and 32-bit multiply) inline; only 32-bit division,
// 64-bit multiply/division, and variable-count wide shifts become calls.
//
// Everything here is written to bottom out in 16-bit operations so that no
// routine ends up calling itself: e.g. __ashlsi3 never uses a 32-bit shift,
// it splits the value into two 16-bit halves.
//
// On this target: int/short = 16 bits, long = 32 bits, long long = 64 bits.
//===----------------------------------------------------------------------===//

typedef unsigned short u16;
typedef unsigned long u32;
typedef signed long s32;
typedef unsigned long long u64;
typedef signed long long s64;

//===----------------------------------------------------------------------===//
// 32-bit shifts.  cnt is taken modulo 32 the way libgcc leaves it (the caller
// only produces defined shift amounts 0..31).
//===----------------------------------------------------------------------===//

u32 __ashlsi3(u32 v, int cnt) {
  u16 lo = (u16)v, hi = (u16)(v >> 16);
  u16 rlo, rhi;
  if (cnt == 0)
    return v;
  if (cnt >= 16) {
    rlo = 0;
    rhi = lo << (cnt - 16);
  } else {
    rlo = lo << cnt;
    rhi = (hi << cnt) | (lo >> (16 - cnt));
  }
  return ((u32)rhi << 16) | rlo;
}

u32 __lshrsi3(u32 v, int cnt) {
  u16 lo = (u16)v, hi = (u16)(v >> 16);
  u16 rlo, rhi;
  if (cnt == 0)
    return v;
  if (cnt >= 16) {
    rhi = 0;
    rlo = hi >> (cnt - 16);
  } else {
    rhi = hi >> cnt;
    rlo = (lo >> cnt) | (hi << (16 - cnt));
  }
  return ((u32)rhi << 16) | rlo;
}

u32 __ashrsi3(u32 v, int cnt) {
  u16 lo = (u16)v, hi = (u16)(v >> 16);
  u16 rlo, rhi;
  short shi = (short)hi; // arithmetic on the sign half
  if (cnt == 0)
    return v;
  if (cnt >= 16) {
    rhi = (u16)(shi >> 15);            // sign fill
    rlo = (u16)(shi >> (cnt - 16));
  } else {
    rhi = (u16)(shi >> cnt);
    rlo = (lo >> cnt) | (hi << (16 - cnt));
  }
  return ((u32)rhi << 16) | rlo;
}

//===----------------------------------------------------------------------===//
// 64-bit shifts, built the same way from two 32-bit halves (which themselves
// use the 32-bit shifts above).
//===----------------------------------------------------------------------===//

u64 __ashldi3(u64 v, int cnt) {
  u32 lo = (u32)v, hi = (u32)(v >> 32);
  u32 rlo, rhi;
  if (cnt == 0)
    return v;
  if (cnt >= 32) {
    rlo = 0;
    rhi = __ashlsi3(lo, cnt - 32);
  } else {
    rlo = __ashlsi3(lo, cnt);
    rhi = __ashlsi3(hi, cnt) | __lshrsi3(lo, 32 - cnt);
  }
  return ((u64)rhi << 32) | rlo;
}

u64 __lshrdi3(u64 v, int cnt) {
  u32 lo = (u32)v, hi = (u32)(v >> 32);
  u32 rlo, rhi;
  if (cnt == 0)
    return v;
  if (cnt >= 32) {
    rhi = 0;
    rlo = __lshrsi3(hi, cnt - 32);
  } else {
    rhi = __lshrsi3(hi, cnt);
    rlo = __lshrsi3(lo, cnt) | __ashlsi3(hi, 32 - cnt);
  }
  return ((u64)rhi << 32) | rlo;
}

u64 __ashrdi3(u64 v, int cnt) {
  u32 lo = (u32)v, hi = (u32)(v >> 32);
  u32 rlo, rhi;
  if (cnt == 0)
    return v;
  if (cnt >= 32) {
    rhi = __ashrsi3(hi, 31);           // sign fill
    rlo = __ashrsi3(hi, cnt - 32);
  } else {
    rhi = __ashrsi3(hi, cnt);
    rlo = __lshrsi3(lo, cnt) | __ashlsi3(hi, 32 - cnt);
  }
  return ((u64)rhi << 32) | rlo;
}

//===----------------------------------------------------------------------===//
// Wide multiply.  A 32x32->64 helper (built from four native 16x16 products)
// underpins the 64-bit multiply.
//===----------------------------------------------------------------------===//

static u64 umul32to64(u32 x, u32 y) {
  u16 xl = (u16)x, xh = (u16)(x >> 16);
  u16 yl = (u16)y, yh = (u16)(y >> 16);
  // Each of these is a native 16x16->32 multiply.
  u32 ll = (u32)xl * yl;
  u32 lh = (u32)xl * yh;
  u32 hl = (u32)xh * yl;
  u32 hh = (u32)xh * yh;
  u32 cross = (ll >> 16) + (lh & 0xFFFF) + (hl & 0xFFFF);
  u32 lo = (ll & 0xFFFF) | (cross << 16);
  u32 hi = hh + (lh >> 16) + (hl >> 16) + (cross >> 16);
  return ((u64)hi << 32) | lo;
}

u64 __muldi3(u64 a, u64 b) {
  u32 al = (u32)a, ah = (u32)(a >> 32);
  u32 bl = (u32)b, bh = (u32)(b >> 32);
  u64 lo = umul32to64(al, bl);
  // The cross terms only affect the high 32 bits, so 32-bit products suffice.
  u32 cross = al * bh + ah * bl;
  return lo + ((u64)cross << 32);
}

//===----------------------------------------------------------------------===//
// Unsigned division/remainder via restoring shift-subtract, done at the natural
// width (no recursion: these never invoke the same-width '/' operator).
//===----------------------------------------------------------------------===//

u32 __udivmodsi4(u32 n, u32 d, u32 *rem) {
  u32 q = 0, r = 0;
  int i;
  for (i = 31; i >= 0; i--) {
    r = (r << 1) | ((n >> i) & 1);
    if (r >= d) {
      r -= d;
      q |= (u32)1 << i;
    }
  }
  if (rem)
    *rem = r;
  return q;
}

u32 __udivsi3(u32 n, u32 d) { return __udivmodsi4(n, d, 0); }

u32 __umodsi3(u32 n, u32 d) {
  u32 r;
  __udivmodsi4(n, d, &r);
  return r;
}

s32 __divsi3(s32 a, s32 b) {
  int neg = 0;
  u32 ua = a, ub = b, q;
  if (a < 0) { ua = -ua; neg ^= 1; }
  if (b < 0) { ub = -ub; neg ^= 1; }
  q = __udivsi3(ua, ub);
  return neg ? -(s32)q : (s32)q;
}

s32 __modsi3(s32 a, s32 b) {
  int neg = a < 0;
  u32 ua = a, ub = b, r;
  if (a < 0) ua = -ua;
  if (b < 0) ub = -ub;
  __udivmodsi4(ua, ub, &r);
  return neg ? -(s32)r : (s32)r;
}

u64 __udivmoddi4(u64 n, u64 d, u64 *rem) {
  u64 q = 0, r = 0;
  int i;
  for (i = 63; i >= 0; i--) {
    r = (r << 1) | ((n >> i) & 1);
    if (r >= d) {
      r -= d;
      q |= (u64)1 << i;
    }
  }
  if (rem)
    *rem = r;
  return q;
}

u64 __udivdi3(u64 n, u64 d) { return __udivmoddi4(n, d, 0); }

u64 __umoddi3(u64 n, u64 d) {
  u64 r;
  __udivmoddi4(n, d, &r);
  return r;
}

s64 __divdi3(s64 a, s64 b) {
  int neg = 0;
  u64 ua = a, ub = b, q;
  if (a < 0) { ua = -ua; neg ^= 1; }
  if (b < 0) { ub = -ub; neg ^= 1; }
  q = __udivdi3(ua, ub);
  return neg ? -(s64)q : (s64)q;
}

s64 __moddi3(s64 a, s64 b) {
  int neg = a < 0;
  u64 ua = a, ub = b, r;
  if (a < 0) ua = -ua;
  if (b < 0) ub = -ub;
  __udivmoddi4(ua, ub, &r);
  return neg ? -(s64)r : (s64)r;
}
