typedef struct { int x, y; } Point;
typedef struct { long a; int b; char c; } Big;
Point make_point(int x, int y) { Point p; p.x = x; p.y = y; return p; }
int use_point(Point p) { return p.x * 10 + p.y; }
Big make_big(long a, int b, char c) { Big r; r.a = a; r.b = b; r.c = c; return r; }
long sum_big(Big g) { return g.a + g.b + g.c; }
