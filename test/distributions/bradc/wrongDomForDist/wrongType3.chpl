use DummyArithDist;

enum color { red, blue, green };

var Dist = new dist(new MyDist());
var DArith: domain(1) distributed(Dist);
var DAssoc: domain(color) distributed(Dist);
