module M1 {
  class C {
    var s: list of int;
  }
}

use M1;

class D: C {
  var i: int;
}

def main() {
  var d = D();

  d.s.append(4);
  d.s.append(5);
  d.s.append(6);
  d.i = 7;

  writeln(d);
}
