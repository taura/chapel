class _sdomain : _domain {
  param rank : integer;
  var adomain : _adomain(rank);

  function _build_array(type elt_type)
    return _sarray(elt_type, rank, dom=this);
}

class _sarray : value {
  type elt_type;
  param rank : integer;

  var dom : _sdomain(rank);
}

function fwrite(f : file, x : _sarray) {
  write("Sparse arrays are not implemented");
}
