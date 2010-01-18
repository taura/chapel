use BlockDist;

config const n: int(64) = 33;
config const m: int(64) = 2**n;

def testit(type t, m: t) {
  const BD = new dist(new Block(1, t, bbox=[0:t..#m]));
  const D: domain(1, t) distributed BD = [0:t..#m];

  for l in Locales do
    writeln(l.id, ": ", D._value.locDoms[l.id].myBlock);
}

testit(int(64), m);
testit(uint(64), m:uint(64));
