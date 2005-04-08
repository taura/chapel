#include <typeinfo>
#define TYPE_EXTERN
#include "expr.h"
#include "files.h"
#include "misc.h"
#include "stringutil.h"
#include "symbol.h"
#include "symtab.h"
#include "type.h"
#include "sym.h"
#include "../traversals/fixup.h"
#include "../traversals/updateSymbols.h"
#include "../traversals/collectASTS.h"

//#define CONSTRUCTOR_WITH_PARAMETERS

//#define USE_VAR_INIT_EXPR


static void genIOReadWrite(FILE* outfile, ioCallType ioType) {
  switch (ioType) {
  case IO_WRITE:
  case IO_WRITELN:
    fprintf(outfile, "_write");
    break;
  case IO_READ:
    fprintf(outfile, "_read");
    break;
  }
}

static void genIODefaultFile(FILE* outfile, ioCallType ioType) {
  switch (ioType) {
  case IO_WRITE:
  case IO_WRITELN:
    fprintf(outfile, "stdout, ");
    break;
  case IO_READ:
    fprintf(outfile, "stdin, ");
    break;
  }
}


Type::Type(astType_t astType, Expr* init_defaultVal) :
  BaseAST(astType),
  symbol(NULL),
  defaultVal(init_defaultVal),
  asymbol(NULL),
  parentType(NULL)
{
  SET_BACK(defaultVal);
}


void Type::addSymbol(Symbol* newsymbol) {
  symbol = newsymbol;
}


bool Type::isComplex(void) {
  return (this == dtComplex);
}


Type* Type::copy(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  Type* new_type = copyType(clone, map, analysis_clone);

  new_type->lineno = lineno;
  new_type->filename = filename;
  if (analysis_clone) {
    analysis_clone->clone(this, new_type);
  }
  if (map) {
    map->put(this, new_type);
  }
  return new_type;
}


Type* Type::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  INT_FATAL(this, "Unanticipated call to Type::copyType");
  return NULL;
}

Type *Type::instantiate_generic(Map<Type *, Type *> &substitutions) {
  if (astType == TYPE_VARIABLE) {
    Type *t = substitutions.get(this);
    if (t)
      return t;
  }
  return 0;
}

void Type::traverse(Traversal* traversal, bool atTop) {
  if (traversal->processTop || !atTop) {
    traversal->preProcessType(this);
  }
  if (atTop || traversal->exploreChildTypes) {
    if (atTop || symbol == NULL) {
      traverseDefType(traversal);
    }
    else {
      traverseType(traversal);
    }
  }
  if (traversal->processTop || !atTop) {
    traversal->postProcessType(this);
  }
}


void Type::traverseDef(Traversal* traversal, bool atTop) {
  if (traversal->processTop || !atTop) {
    traversal->preProcessType(this);
  }
  TRAVERSE(symbol, traversal, false);
  traverseDefType(traversal);
  if (traversal->processTop || !atTop) {
    traversal->postProcessType(this);
  }
}


void Type::traverseType(Traversal* traversal) {
}


void Type::traverseDefType(Traversal* traversal) {
  TRAVERSE(defaultVal, traversal, false);
}


int Type::rank(void) {
  return 0;
}


void Type::print(FILE* outfile) {
  symbol->print(outfile);
}


void Type::printDef(FILE* outfile) {
  print(outfile);
}

void Type::codegen(FILE* outfile) {
  if (this == dtUnknown) {
    INT_FATAL(this, "Cannot generate unknown type");
  }
  symbol->codegen(outfile);
}


void Type::codegenDef(FILE* outfile) {
  INT_FATAL(this, "Don't know how to codegenDef() for all types yet");
}


void Type::codegenPrototype(FILE* outfile) { }


void Type::codegenSafeInit(FILE* outfile) {
  if (this == dtString) {
    fprintf(outfile, " = NULL");
  } else {
    // Most types won't need an initializer to be safe, only correct
  }
}


void Type::codegenStringToType(FILE* outfile) {
}


void Type::codegenIORoutines(FILE* outfile) {
}


void Type::codegenConfigVarRoutines(FILE* outfile) {
}


void Type::codegenDefaultFormat(FILE* outfile, bool isRead) {
  fprintf(outfile, "_default_format");
  if (isRead) {
    fprintf(outfile, "_read");
  } else {
    fprintf(outfile, "_write");
  }
  this->codegen(outfile);
}


void Type::codegenIOCall(FILE* outfile, ioCallType ioType, Expr* arg,
                         Expr* format) {
  genIOReadWrite(outfile, ioType);
  
  if (this == dtUnknown) {
    INT_FATAL(format, "unknown type encountered in codegen");
  }

  this->codegen(outfile);
  fprintf(outfile, "(");

  genIODefaultFile(outfile, ioType);

  if (!format) {
    bool isRead = (ioType == IO_READ);
    codegenDefaultFormat(outfile, isRead);
  } else {
    format->codegen(outfile);
  }
  fprintf(outfile, ", ");

  if (ioType == IO_READ) {
    fprintf(outfile, "&");
  }

  arg->codegen(outfile);
  fprintf(outfile, ")");
}


bool Type::outParamNeedsPtr(void) {
  return true;
}


bool Type::requiresCParamTmp(paramType intent) {
  if (intent == PARAM_BLANK) {
    if (blankIntentImpliesRef()) {
      intent = PARAM_REF;
    } else {
      intent = PARAM_CONST;
    }
  }
  switch (intent) {
  case PARAM_BLANK:
    INT_FATAL(this, "should never have reached PARAM_BLANK case");
  case PARAM_CONST:
  case PARAM_IN:
    // if these are implemented using C's pass-by-value, then C
    // effectively puts in the temp for us
    if (implementedUsingCVals()) {
      return false;
    } else {
      return true;
    }
  case PARAM_INOUT:
    // here a temp is probably always needed in order to avoid
    // affecting the original value
  case PARAM_OUT:
    // and here it's needed to set up the default value of the type
    return true;
  case PARAM_REF:
    // here, a temp should never be needed
    return false;
  default:
    INT_FATAL(this, "case not handled in requiresCParamTmp");
    return false;
  }
}


bool Type::blankIntentImpliesRef(void) {
  return false;
}


bool Type::implementedUsingCVals(void) {
  if (this == dtBoolean ||
      this == dtInteger ||
      this == dtFloat || 
      this == dtComplex) {
    return true;
  } else {
    return false;
 }
}

Type* Type::getType(){
  return this;
}

EnumType::EnumType(EnumSymbol* init_valList) :
  Type(TYPE_ENUM, new Variable(init_valList)),
  valList(init_valList)
{
  Symbol* val = valList;
  while (val) {
    val->type = this;
    val = nextLink(Symbol, val);
  }
}


Type* EnumType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  Type* copy = new EnumType(valList);
  copy->addSymbol(symbol);
  return copy;

    /*
  Symbol* newSyms = valList->copyList(clone, map, analysis_clone);

  if (typeid(*newSyms) != typeid(EnumSymbol)) {
    INT_FATAL(this, "valList is not EnumSymbol in EnumType::copyType()");
    return NULL;
  } else {
    EnumSymbol* newEnums = (EnumSymbol*)newSyms;
    Type* copy = new EnumType(newEnums);
    copy->addSymbol(symbol);
    return copy;
  }
    */
}


void EnumType::traverseDefType(Traversal* traversal) {
  TRAVERSE_LS(valList, traversal, false);
  TRAVERSE(defaultVal, traversal, false);
}


void EnumType::printDef(FILE* outfile) {
  printf("enum ");
  symbol->print(outfile);
  printf(" = ");
  valList->printList(outfile, " | ");
}


void EnumType::codegen(FILE* outfile) {
  symbol->codegen(outfile);
}


void EnumType::codegenDef(FILE* outfile) {
  EnumSymbol* enumSym;
  int last = -1;

  fprintf(outfile, "typedef enum {\n");
  enumSym = valList;
  while (enumSym) {
    enumSym->printDef(outfile);

    if (enumSym->val != last + 1) {
      fprintf(outfile, " = %d", enumSym->val);
    }
    last = enumSym->val;

    enumSym = nextLink(EnumSymbol, enumSym);

    if (enumSym) {
      fprintf(outfile, ",");
    }
    fprintf(outfile, "\n");
  }
  fprintf(outfile, "} ");
  symbol->codegen(outfile);
  fprintf(outfile, ";\n\n");
}


static void codegenIOPrototype(FILE* outfile, Symbol* symbol, bool isRead) {
  fprintf(outfile, "void ");
  if (isRead) {
    fprintf(outfile, "_read");
  } else {
    fprintf(outfile, "_write");
  }
  symbol->codegen(outfile);
  fprintf(outfile, "(FILE* ");
  if (isRead) {
    fprintf(outfile, "infile");
  } else {
    fprintf(outfile, "outfile");
  }
  fprintf(outfile, ", char* format, ");
  symbol->codegen(outfile);
  if (isRead) {
    fprintf(outfile, "*");
  }
  fprintf(outfile, " val)");
}


void EnumType::codegenStringToType(FILE* outfile) {
  EnumSymbol* enumSym = valList;

  fprintf(outfile, "int _convert_string_to_enum");
  symbol->codegen(outfile);
  fprintf(outfile, "(char* inputString, ");
  symbol->codegen(outfile);
  fprintf(outfile, "* val) {\n");
  
  while (enumSym) {
    fprintf(outfile, "if (strcmp(inputString, \"");
    enumSym->codegen(outfile);
    fprintf(outfile, "\") == 0) {\n");
    fprintf(outfile, "*val = ");
    enumSym->codegen(outfile);
    fprintf(outfile, ";\n");
    fprintf(outfile, "} else ");
    enumSym = nextLink(EnumSymbol, enumSym);
  }
  fprintf(outfile, "{ \n");
  fprintf(outfile, "return 0;\n");
  fprintf(outfile, "}\n");
  fprintf(outfile, "return 1;\n}\n\n");
}

void EnumType::codegenIORoutines(FILE* outfile) {
  EnumSymbol* enumSym = valList;
  bool isRead;

  isRead = true;
  codegenIOPrototype(intheadfile, symbol, isRead);
  fprintf(intheadfile, ";\n");
  
  isRead = false;
  codegenIOPrototype(intheadfile, symbol, isRead);
  fprintf(intheadfile, ";\n\n");

  isRead = true;
  codegenIOPrototype(outfile, symbol, isRead);
  fprintf(outfile, " {\n");
  fprintf(outfile, "char* inputString = NULL;\n");
  fprintf(outfile, "_read_string(stdin, format, &inputString);\n");
  fprintf(outfile, "if (!(_convert_string_to_enum");
  symbol->codegen(outfile);
  fprintf(outfile, "(inputString, val))) {\n");
  fprintf(outfile, "fflush(stdout);\n");
  fprintf(outfile, "fprintf (stderr, \"***ERROR:  Not of ");
  symbol->codegen(outfile);
  fprintf(outfile, " type***\\n\");\n");
  fprintf(outfile, "exit(0);\n");
  fprintf(outfile, "}\n");
  fprintf(outfile, "}\n\n");

  isRead = false;
  codegenIOPrototype(outfile, symbol, isRead);
  fprintf(outfile, " {\n");
  fprintf(outfile, "switch (val) {\n");
  while (enumSym) {
    fprintf(outfile, "case ");
    enumSym->codegen(outfile);
    fprintf(outfile, ":\n");
    fprintf(outfile, "fprintf(outfile, format, \"");
    enumSym->codegen(outfile);
    fprintf(outfile, "\");\n");
    fprintf(outfile, "break;\n");

    enumSym = nextLink(EnumSymbol, enumSym);
  }
  fprintf(outfile, "}\n");
  fprintf(outfile, "}\n\n");
}


void EnumType::codegenConfigVarRoutines(FILE* outfile) {
  fprintf(outfile, "int setInCommandLine");
  symbol->codegen(outfile);
  fprintf(outfile, "(char* varName, ");
  symbol->codegen(outfile);
  fprintf(outfile, "* value, char* moduleName) {\n");
  fprintf(outfile, "int varSet = 0;\n");
  fprintf(outfile, "char* setValue = lookupSetValue(varName, moduleName);\n");
  fprintf(outfile, "if (setValue) {\n");
  fprintf(outfile, "int validEnum = _convert_string_to_enum");
  symbol->codegen(outfile);
  fprintf(outfile, "(setValue, value);\n");
  fprintf(outfile, "if (validEnum) {\n");
  fprintf(outfile, "varSet = 1;\n");
  fprintf(outfile, "} else {\n");
  fprintf(outfile, "fprintf(stderr, \"***Error: \\\"%%s\\\" is not a valid "
          "value for a config var \\\"%%s\\\" of type ");
  symbol->codegen(outfile);
  fprintf(outfile, "***\\n\", setValue, varName);\n");
  fprintf(outfile, "exit(0);\n");
  fprintf(outfile, "}\n");
  fprintf(outfile, "}\n");
  fprintf(outfile, "return varSet;\n");
  fprintf(outfile, "}\n\n");
}


void EnumType::codegenDefaultFormat(FILE* outfile, bool isRead) {
  fprintf(outfile, "_default_format");
  if (isRead) {
    fprintf(outfile, "_read");
  } else {
    fprintf(outfile, "_write");
  }
  fprintf(outfile, "_enum");
}


bool EnumType::implementedUsingCVals(void) {
  return true;
}


DomainType::DomainType(Expr* init_expr) :
  Type(TYPE_DOMAIN, NULL),
  numdims(0),
  parent(NULL)
{
  if (init_expr) {
        
    if (typeid(*init_expr) == typeid(IntLiteral)) {
      numdims = init_expr->intVal();
    } else {
      numdims = init_expr->rank();
      parent = init_expr;
      SET_BACK(parent);
    }
    idxType = new IndexType(init_expr);
    ((IndexType*)idxType)->domainType = this;
    /*if (typeid(*init_expr) == typeid(IntLiteral)){
      ((IndexType*)idxType)->idxType = newTType;
    } 
    else*/ 
  }
}


DomainType::DomainType(int init_numdims) :
  Type(TYPE_DOMAIN, NULL),
  numdims(init_numdims),
  parent(NULL)
{}


Type* DomainType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  Type* copy;
  if (!parent) {
    copy = new DomainType(numdims);
  } else {
    copy = new DomainType(parent->copy(clone, map, analysis_clone));
  }
  copy->addSymbol(symbol);
  return copy;
}


int DomainType::rank(void) {
  return numdims;
}


void DomainType::print(FILE* outfile) {
  fprintf(outfile, "domain(");
  if (!parent) {
    if (numdims != 0) {
      fprintf(outfile, "%d", numdims);
    } else {
      fprintf(outfile, "???");
    }
  } else {
    parent->print(outfile);
  }
  fprintf(outfile, ")");
}


void DomainType::codegenDef(FILE* outfile) {
  fprintf(outfile, "typedef struct _");
  symbol->codegen(outfile);
  fprintf(outfile, " {\n");
  fprintf(outfile, "  _dom_perdim dim_info[%d];\n", numdims);
  fprintf(outfile, "} ");
  symbol->codegen(outfile);
  fprintf(outfile, ";\n\n");
}


void DomainType::codegenIOCall(FILE* outfile, ioCallType ioType, Expr* arg,
                               Expr* format) {
  genIOReadWrite(outfile, ioType);

  fprintf(outfile, "_domain");
  fprintf(outfile, "(");

  genIODefaultFile(outfile, ioType);

  if (ioType == IO_READ) {
    fprintf(outfile, "&");
  }

  arg->codegen(outfile);
  fprintf(outfile, ")");
}


bool DomainType::blankIntentImpliesRef(void) {
  return true;
}


IndexType::IndexType(Type* init_idxType):
  Type(TYPE_INDEX, init_idxType->defaultVal),
  idxType(init_idxType) 
{
  domainType = NULL;
}


IndexType::IndexType(Expr* init_expr) :
  Type(TYPE_INDEX, NULL),
  idxExpr(init_expr)
{
  SET_BACK(idxExpr);
  if (typeid(*init_expr) == typeid(IntLiteral)) {
    TupleType* newTType = new TupleType(init_expr->typeInfo());
    for (int i = 1; i < init_expr->intVal(); i++){
      newTType->addType(init_expr->typeInfo());
    }
    idxType = newTType;
  } else {
    idxType = init_expr->typeInfo(); 
  }
}


Type* IndexType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, 
                          CloneCallback* analysis_clone) {
  Type* copy = new IndexType(idxType->copy(clone, map, analysis_clone));
  copy->addSymbol(symbol);
  return copy;
}


void IndexType::print(FILE* outfile) {
  fprintf(outfile, "index(");
  idxExpr->print(outfile);
  fprintf(outfile, ")");
}

void IndexType::codegenDef(FILE* outfile) {
  fprintf(outfile, "typedef ");
  //idxType->codegenDef(outfile);
  //idxType->print(stdout);
  //if (typeid(*idxType) == typeid(Tuple))
  TupleType* tt = dynamic_cast<TupleType*>(idxType);
  if (tt){
    fprintf(outfile, "struct {\n");
    int i = 0;
    forv_Vec(Type, component, tt->components) {
      component->codegen(outfile);
      fprintf(outfile, " _field%d;\n", ++i);
    }
    fprintf(outfile, "} ");
    symbol->codegen(outfile);
    fprintf(outfile, ";\n\n");
  } else {
    idxType->codegenDef(outfile);
    symbol->codegen(outfile);
    fprintf(outfile, ";\n\n");
  }
}


void IndexType::codegenIOCall(FILE* outfile, ioCallType ioType, Expr* arg,
                              Expr* format) {
  idxType->codegenIOCall(outfile, ioType, arg, format);
}
                          

void IndexType::traverseDefType(Traversal* traversal) {
  if (!(typeid(*idxExpr) == typeid(IntLiteral))) {
    TRAVERSE(idxExpr, traversal, false);
  }
  TRAVERSE(idxType, traversal, false);
  TRAVERSE(defaultVal, traversal, false);
}


Type* IndexType::getType(){
  return idxType; 
}


SeqType::SeqType(Type* init_elementType):
  ClassType(TYPE_SEQ),
  elementType(init_elementType)
{}


Type* SeqType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  Type* new_type = new SeqType(elementType->copy(clone, map, analysis_clone));
  new_type->addSymbol(symbol);
  return new_type;
}


void SeqType::traverseDefType(Traversal* traversal) {
  TRAVERSE(elementType, traversal, false);
  ClassType::traverseDefType(traversal);
}


void SeqType::print(FILE* outfile) {
  fprintf(outfile, "seq(");
  elementType->print(outfile);
  fprintf(outfile, ")");
}


void SeqType::codegen(FILE* outfile) {
  symbol->codegen(outfile);
}


void SeqType::codegenDef(FILE* outfile) {
  ClassType::codegenDef(outfile);

  fprintf(outfile, "void _write");
  symbol->codegen(outfile);
  fprintf(outfile, "(FILE* F, char* format, ");
  symbol->codegen(outfile);
  fprintf(outfile, " seq);\n\n");

  fprintf(codefile, "void _write");
  symbol->codegen(codefile);
  fprintf(codefile, "(FILE* F, char* format, ");
  symbol->codegen(codefile);
  fprintf(codefile, " seq) {\n");
  symbol->codegen(codefile);
  fprintf(codefile, "_node tmp = seq->first;\n");
  fprintf(codefile, "while (tmp != nil) {\n");
  fprintf(codefile, "  fprintf(F, format, tmp->element);\n");
  fprintf(codefile, "  tmp = tmp->next;\n");
  fprintf(codefile, "  if (tmp != nil) {\n");
  fprintf(codefile, "  fprintf(F, \" \");\n");
  fprintf(codefile, "}\n");
  fprintf(codefile, "}\n");
  fprintf(codefile, "}\n");
}


void SeqType::codegenDefaultFormat(FILE* outfile, bool isRead) {
  elementType->codegenDefaultFormat(outfile, isRead);
}


void SeqType::codegenIOCall(FILE* outfile, ioCallType ioType, Expr* arg,
                            Expr* format) {
  Type::codegenIOCall(outfile, ioType, arg, format);
}


bool SeqType::implementedUsingCVals(void) {
  return false;
}


SeqType* SeqType::createSeqType(char* new_seq_name, Type* init_elementType) {
  SeqType* new_seq_type = new SeqType(init_elementType);
  Symbol* _seq = Symboltable::lookupInternal("_seq");
  ClassType* _seq_type = dynamic_cast<ClassType*>(_seq->type);
  Symboltable::pushScope(SCOPE_CLASS);
  Stmt* new_decls = dynamic_cast<Stmt*>(_seq_type->declarationList->next)->copyList(true);
  new_seq_type->addDeclarations(new_decls);
  SymScope* new_seq_scope = Symboltable::popScope();
  new_seq_type->setScope(new_seq_scope);

  TypeSymbol* new_seq_sym = new TypeSymbol(new_seq_name, new_seq_type);
  new_seq_type->addSymbol(new_seq_sym);

  /*** set class bindings and this ***/
  forv_Vec(FnSymbol, method, new_seq_type->methods) {
    method->classBinding = new_seq_sym;
    method->_this = method->formals;
    /** mangle cname **/
    method->cname = glomstrings(3, new_seq_sym->name, "_", method->cname);
  }

  /*** update _seq type to new type ***/
  Map<BaseAST*,BaseAST*>* map = new Map<BaseAST*,BaseAST*>();
  map->put(_seq, new_seq_sym);
  map->put(_seq_type, new_seq_type);
  map->put(_seq_type->types.v[0]->type, new_seq_type->elementType);
  TRAVERSE_LS(new_decls, new UpdateSymbols(map), true);

  Symbol* _node = Symboltable::lookupInScope("_node", new_seq_scope);
  _node->cname = glomstrings(2, new_seq_name, _node->name);

  /*
  FnSymbol* copy_fn =
    dynamic_cast<FnSymbol*>(Symboltable::lookupInScope("copy", new_seq_scope));

  if (!copy_fn) {
    INT_FATAL(new_seq_type, "No copy found in sequence type");
  }

  Vec<BaseAST*> asts;
  collect_asts(&asts, copy_fn);

  forv_Vec(BaseAST*, ast, asts) {
    if (ForLoopStmt* for_loop = dynamic_cast<ForLoopStmt*>(ast)) {
      if (DefExpr* def_expr = dynamic_cast<DefExpr*>(for_loop->indices)) {
        def_expr->sym->type = new_seq_type->elementType;
      }
    }
  }
  */
  return new_seq_type;
}


void SeqType::buildImplementationClasses() {
  Symboltable::pushScope(SCOPE_CLASS);

  Symbol* _seq = Symboltable::lookupInternal("_seq");
  StructuralType* _seq_type = dynamic_cast<StructuralType*>(_seq->type);
  // look at next because first one is the variable type
  Stmt* decls = dynamic_cast<Stmt*>(_seq_type->declarationList->next);
  Stmt* new_decls = decls->copyList(true);
  addDeclarations(new_decls);

  Symbol* _node = Symboltable::lookup("_node");
  _node->cname = glomstrings(2, symbol->name, _node->name);
  SymScope* _node_scope = dynamic_cast<StructuralType*>(_node->type)->structScope;
  Symboltable::lookupInScope("element", _node_scope)->type = elementType;
  Symboltable::lookupInScope("next", _node_scope)->type = _node->type;
  Symboltable::lookup("first")->type = _node->type;
  Symboltable::lookup("last")->type = _node->type;

  structScope = Symboltable::popScope();
  structScope->setContext(NULL, symbol, symbol->defPoint);
  TRAVERSE_LS(symbol->defPoint->stmt, new Fixup(), true);
}


ArrayType::ArrayType(Expr* init_domain, Type* init_elementType):
  Type(TYPE_ARRAY, init_elementType->defaultVal),
  domain(init_domain),
  elementType(init_elementType)
{
  SET_BACK(domain);
}


Type* ArrayType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  Type* copy = new ArrayType(domain->copy(clone, map, analysis_clone),
                             elementType->copy(clone, map, analysis_clone));
  copy->addSymbol(symbol);
  return copy;
}


void ArrayType::traverseDefType(Traversal* traversal) {
  TRAVERSE(domain, traversal, false);
  TRAVERSE(elementType, traversal, false);
  TRAVERSE(defaultVal, traversal, false);
}


int ArrayType::rank(void) {
  return domain->rank();
}


void ArrayType::print(FILE* outfile) {
  //  fprintf(outfile, "[");
  domain->print(outfile);
  //  fprintf(outfile, "] ");
  fprintf(outfile, " ");
  elementType->print(outfile);
}


void ArrayType::codegen(FILE* outfile) {
  symbol->codegen(outfile);
}


void ArrayType::codegenDef(FILE* outfile) {
  fprintf(outfile, "struct _");
  symbol->codegen(outfile);
  fprintf(outfile, " {\n");
  fprintf(outfile, "  int elemsize;\n");
  fprintf(outfile, "  int size;\n");
  fprintf(outfile, "  ");
  elementType->codegen(outfile);
  fprintf(outfile, "* base;\n");
  fprintf(outfile, "  ");
  elementType->codegen(outfile);
  fprintf(outfile, "* origin;\n");
  fprintf(outfile, "  ");
  domainType->codegen(outfile);
  fprintf(outfile, "* domain;\n");
  fprintf(outfile, "  _arr_perdim dim_info[%d];\n", domainType->numdims);
  fprintf(outfile, "};\n");

  fprintf(outfile, "void _write");
  symbol->codegen(outfile);
  fprintf(outfile, "(FILE* F, char* format, ");
  symbol->codegen(outfile);
  fprintf(outfile, " arr);\n\n");

  fprintf(codefile, "void _write");
  symbol->codegen(codefile);
  fprintf(codefile, "(FILE* F, char* format, ");
  symbol->codegen(codefile);
  fprintf(codefile, " arr) {\n");
  for (int dim = 0; dim < domainType->numdims; dim++) {
    fprintf(codefile, "  int i%d;\n", dim);
  }
  domainType->codegen(codefile);
  fprintf(codefile, "* const dom = arr.domain;\n\n");
  for (int dim = 0; dim < domainType->numdims; dim++) {
    fprintf(codefile, "for (i%d=dom->dim_info[%d].lo; i%d<=dom"
            "->dim_info[%d].hi; i%d+=dom->dim_info[%d].str) {\n",
            dim, dim, dim, dim, dim, dim);
  }
  fprintf(codefile, "fprintf(F, format, _ACC%d(arr, i0", domainType->numdims);
  for (int dim = 1; dim < domainType->numdims; dim++) {
    fprintf(codefile, ", i%d", dim);
  }
  fprintf(codefile, "));\n");
  fprintf(codefile, "if (i%d<dom->dim_info[%d].hi) {\n",
          domainType->numdims-1, domainType->numdims-1);
  fprintf(codefile, "fprintf(F, \" \");\n");
  fprintf(codefile, "}\n");
  fprintf(codefile, "}\n");
  for (int dim = 1; dim < domainType->numdims; dim++) {
    fprintf(codefile, "fprintf(F, \"\\n\");\n");
    fprintf(codefile, "}\n");
  }
  fprintf(codefile, "}\n");
}


void ArrayType::codegenPrototype(FILE* outfile) {
  fprintf(outfile, "typedef struct _");
  symbol->codegen(outfile);
  fprintf(outfile, " ");
  symbol->codegen(outfile);
  fprintf(outfile, ";\n");
}


void ArrayType::codegenDefaultFormat(FILE* outfile, bool isRead) {
  elementType->codegenDefaultFormat(outfile, isRead);
}


bool ArrayType::blankIntentImpliesRef(void) {
  return true;
}


UserType::UserType(Type* init_definition, Expr* init_defaultVal) :
  Type(TYPE_USER, init_defaultVal),
  definition(init_definition)
{}


Type* UserType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  Type* copy = new UserType(definition,
                            defaultVal->copy(clone, map, analysis_clone));
  copy->addSymbol(symbol);
  return copy;
}


bool UserType::isComplex(void) {
  return definition->isComplex();
}


void UserType::traverseDefType(Traversal* traversal) {
  TRAVERSE(definition, traversal, false);
  TRAVERSE(defaultVal, traversal, false);
}


void UserType::printDef(FILE* outfile) {
  fprintf(outfile, "type ");
  symbol->print(outfile);
  fprintf(outfile, " = ");
  definition->print(outfile);
}


void UserType::codegen(FILE* outfile) {
  symbol->codegen(outfile);
}


void UserType::codegenDef(FILE* outfile) {
  fprintf(outfile, "typedef ");
  definition->codegen(outfile);
  fprintf(outfile, " ");
  symbol->codegen(outfile);
  fprintf(outfile, ";\n");
}


// TODO: We should probably instead have types print out
// their own write routines and have UserType print its
// definition's write routine

static void codegenIOPrototypeBody(FILE* outfile, Symbol* symbol, Type* definition, bool isRead) {
  codegenIOPrototype(outfile, symbol, isRead);
  fprintf(outfile, " {\n");
  if (isRead) {
    fprintf(outfile, " _read");
  } else {
  fprintf(outfile, "  _write");
  }
  definition->codegen(outfile);  
  if (isRead) {
    fprintf(outfile, "(infile, format, val);\n");
  } else {
    fprintf(outfile, "(outfile, format, val);\n");
  }    
  fprintf(outfile, "}\n");
}    


void UserType::codegenIORoutines(FILE* outfile) {
  bool isRead;

  isRead = true;
  codegenIOPrototype(intheadfile, symbol, isRead);
  fprintf(intheadfile, ";\n");

  isRead = false;
  codegenIOPrototype(intheadfile, symbol, isRead);
  fprintf(intheadfile, ";\n\n");

  isRead = true;
  codegenIOPrototypeBody(outfile, symbol, definition, isRead);
  fprintf(outfile, "\n\n");

  isRead = false;
  codegenIOPrototypeBody(outfile, symbol, definition, isRead);
}


void UserType::codegenDefaultFormat(FILE* outfile, bool isRead) {
  definition->codegenDefaultFormat(outfile, isRead);
}


LikeType::LikeType(Expr* init_expr) :
  Type(TYPE_LIKE, NULL),
  expr(init_expr)
{
  SET_BACK(expr);
}


Type* LikeType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  Type* copy = new LikeType(expr->copy(clone, map, analysis_clone));
  copy->addSymbol(symbol);
  return copy;
}


bool LikeType::isComplex(void) {
  return expr->typeInfo()->isComplex();
}


void LikeType::traverseDefType(Traversal* traversal) {
  TRAVERSE(expr, traversal, false);
}


void LikeType::printDef(FILE* outfile) {
  fprintf(outfile, "like type ");
  symbol->print(outfile);
  fprintf(outfile, " = ");
  expr->print(outfile);
}


void LikeType::codegen(FILE* outfile) {
  INT_FATAL(this, "Unanticipated call to LikeType::codegen");
}


void LikeType::codegenDef(FILE* outfile) {
  INT_FATAL(this, "Unanticipated call to LikeType::codegenDef");
}


StructuralType::StructuralType(astType_t astType, Expr* init_defaultVal) :
  Type(astType, init_defaultVal),
  constructor(NULL),
  structScope(NULL),
  declarationList(NULL),
  parentStruct(NULL)
{
  fields.clear();
  methods.clear();
  types.clear();
  SET_BACK(constructor);
}


void StructuralType::copyGuts(StructuralType* copy_type, bool clone, 
                              Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  Symboltable::pushScope(SCOPE_CLASS);
  Stmt* new_decls = NULL;
  for (Stmt* old_decls = declarationList;
       old_decls;
       old_decls = nextLink(Stmt, old_decls)) {
    DefStmt* def = dynamic_cast<DefStmt*>(old_decls);
    FnSymbol* fn;
    if (def && (fn = def->fnDef())) {
      copy_type->methods.add(fn);
    } else {
      new_decls = appendLink(new_decls, old_decls->copy(true, map, analysis_clone));
    }
  }
  copy_type->addDeclarations(new_decls);
  SymScope* copy_scope = Symboltable::popScope();
  copy_type->setScope(copy_scope);
}


void StructuralType::addDeclarations(Stmt* newDeclarations, Stmt* beforeStmt) {
  Stmt* tmp = newDeclarations;
  while (tmp) {
    DefStmt* def_stmt = dynamic_cast<DefStmt*>(tmp);
    FnSymbol* fn;
    TypeSymbol* type_sym;
    VarSymbol* var;
    if (def_stmt && (fn = def_stmt->fnDef())) {
      fn->classBinding = this->symbol;
      fn->method_type = PRIMARY_METHOD;
      methods.add(fn);
    } else if (def_stmt && (type_sym = def_stmt->typeDef())) {
      types.add(type_sym);
    } else if (def_stmt && (var = def_stmt->varDef())) {
      fields.add(var);
    }
    tmp = nextLink(Stmt, tmp);
  }
  if (!declarationList) {
    declarationList = newDeclarations;
    SET_BACK(declarationList);
  } else if (beforeStmt) {
    beforeStmt->insertBefore(newDeclarations);
  } else {
    Stmt* last = dynamic_cast<Stmt*>(declarationList->tail());
    last->insertAfter(newDeclarations);
  }
}


void StructuralType::setScope(SymScope* init_structScope) {
  structScope = init_structScope;
}


void StructuralType::traverseDefType(Traversal* traversal) {
  SymScope* prevScope;
  if (structScope) {
    prevScope = Symboltable::setCurrentScope(structScope);
  }
  TRAVERSE_LS(declarationList, traversal, false);
  TRAVERSE_LS(constructor, traversal, false);
  TRAVERSE(defaultVal, traversal, false);
  if (structScope) {
    Symboltable::setCurrentScope(prevScope);
  }
}


Stmt* StructuralType::buildConstructorBody(Stmt* stmts, Symbol* _this, ParamSymbol* arguments) {
  forv_Vec(VarSymbol, tmp, fields) {
    Expr* lhs = new MemberAccess(new Variable(_this), tmp);
    Stmt* assign_stmt;
    /** stopgap: strings initialized using _init_string;
        this will eventually use VarInitExpr **/
    if (tmp->type == dtString) {
      Expr* args = lhs;
      args->append(new MemberAccess(new Variable(_this), tmp));
      args->append(tmp->type->defaultVal->copy());
      Symbol* init_string = Symboltable::lookupInternal("_init_string");
      FnCall* call = new FnCall(new Variable(init_string), args);
      assign_stmt = new ExprStmt(call);
    } else {
#ifdef USE_VAR_INIT_EXPR
      Expr* assign_expr = new VarInitExpr(lhs);
      assign_stmt = new ExprStmt(assign_expr);
#else
      if (tmp->type->defaultVal) {
        Expr* assign_expr = new AssignOp(GETS_NORM, lhs, tmp->type->defaultVal->copy());
        assign_stmt = new ExprStmt(assign_expr);
      } else {
        continue;
#if 0
        Expr* assign_expr = new AssignOp(
          GETS_NORM, lhs, new Variable(Symboltable::lookupInternal("nil", SCOPE_INTRINSIC)));
        assign_stmt = new ExprStmt(assign_expr);
#endif
      }
#endif
    }
    stmts = appendLink(stmts, assign_stmt);
  }

#ifdef CONSTRUCTOR_WITH_PARAMETERS
  ParamSymbol* ptmp = arguments;
#endif
  forv_Vec(VarSymbol, tmp, fields) {
    Expr* lhs = new MemberAccess(new Variable(_this), tmp);
#ifdef CONSTRUCTOR_WITH_PARAMETERS
    Expr* rhs = new Variable(ptmp);
#else
    Expr* rhs = tmp->init ? tmp->init->copy() : NULL;
    if (!rhs) {
      continue;
    }
#endif

    Stmt* assign_stmt;
    /** stopgap: strings initialized using _init_string;
        this will eventually use VarInitExpr **/
    if (tmp->type == dtString) {
      Expr* args = lhs;
      args->append(new MemberAccess(new Variable(_this), tmp));
      args->append(rhs);
      Symbol* init_string = Symboltable::lookupInternal("_init_string");
      FnCall* call = new FnCall(new Variable(init_string), args);
      assign_stmt = new ExprStmt(call);
    } else {
      Expr* assign_expr = new AssignOp(GETS_NORM, lhs, rhs);
      assign_stmt = new ExprStmt(assign_expr);
    }
    stmts = appendLink(stmts, assign_stmt);

#ifdef CONSTRUCTOR_WITH_PARAMETERS
    ptmp = nextLink(ParamSymbol, ptmp);
#endif
  }
  return stmts;
}


static Stmt* addIOStmt(Stmt* ioStmtList, Expr* argExpr) {
  IOCall* ioExpr = new IOCall(IO_WRITE, new StringLiteral("write"), argExpr);
  Stmt* ioStmt = new ExprStmt(ioExpr);
  if (ioStmtList == NULL) {
    ioStmtList = ioStmt;
  } else {
    ioStmtList->append(ioStmt);
  }
  return ioStmtList;
}


static Stmt* addIOStmt(Stmt* ioStmtList, ParamSymbol* _this, VarSymbol* field) {
  return addIOStmt(ioStmtList, new MemberAccess(new Variable(_this), field));
}


static Stmt* addIOStmt(Stmt* ioStmtList, char* str) {
  return addIOStmt(ioStmtList, new StringLiteral(str));
}


Stmt* StructuralType::buildIOBodyStmtsHelp(Stmt* bodyStmts, ParamSymbol* thisArg) {
  bool firstField = true;

  forv_Vec(VarSymbol, field, fields) {
    if (!firstField) {
      bodyStmts = addIOStmt(bodyStmts, ", ");
    } else {
      firstField = false;
    }
    bodyStmts = addIOStmt(bodyStmts, glomstrings(2, field->name, " = "));
    bodyStmts = addIOStmt(bodyStmts, thisArg, field);
  }
  return bodyStmts;
}


Stmt* StructuralType::buildIOBodyStmts(ParamSymbol* thisArg) {
  Stmt* bodyStmts = NULL;

  bodyStmts = addIOStmt(bodyStmts, "(");
  bodyStmts = buildIOBodyStmtsHelp(bodyStmts, thisArg);
  bodyStmts = addIOStmt(bodyStmts, ")");

  return bodyStmts;
}


void StructuralType::codegen(FILE* outfile) {
  if (symbol->isDead) {
    // BLC: theoretically, this case should only occur when a class
    // is never instantiated -- only nil references are used
    fprintf(outfile, "void* ");
  } else {
    symbol->codegen(outfile);
  }
}


void StructuralType::codegenStartDefFields(FILE* outfile) {}

void StructuralType::codegenStopDefFields(FILE* outfile) {}


void StructuralType::codegenDef(FILE* outfile) {
  forv_Vec(TypeSymbol, type, types) {
    type->codegenDef(outfile);
    fprintf(outfile, "\n");
  }
  fprintf(outfile, "struct __");
  symbol->codegen(outfile);
  fprintf(outfile, " {\n");
  codegenStartDefFields(outfile);
  bool printedSomething = false; // BLC: this is to avoid empty structs, illegal in C
  for (Stmt* tmp = declarationList; tmp; tmp = nextLink(Stmt, tmp)) {
    if (DefStmt* def_stmt = dynamic_cast<DefStmt*>(tmp)) {
      if (VarSymbol* var = def_stmt->varDef()) {
        var->codegenDef(outfile);
        printedSomething = true;
      }
    }
  }
  if (!printedSomething) {
    fprintf(outfile, "int _emptyStructPlaceholder;\n");
  }
  codegenStopDefFields(outfile);
  /*
  forv_Vec(VarSymbol, tmp, fields) {
    tmp->codegenDef(outfile);
    fprintf(outfile, "\n");
  }
  */
  fprintf(outfile, "};\n\n");
  /*
  if (value || union_value) {
    symbol->codegen(outfile);
    fprintf(outfile, ";\n\n");
  }
  else {
    fprintf(outfile, "_");
    symbol->codegen(outfile);
    fprintf(outfile,", *");
    symbol->codegen(outfile);
    fprintf(outfile, ";\n\n");
    }*/
  if (DefStmt* def_stmt = dynamic_cast<DefStmt*>(constructor)) {
    def_stmt->fnDef()->codegenDef(codefile);
  }
  forv_Vec(FnSymbol, fn, methods) {
    // Check to see if this is where it is defined
    if (fn->parentScope->symContext->type == this) {
      fn->codegenDef(codefile);
    }
    fprintf(codefile, "\n");
  }
}


void StructuralType::codegenStructName(FILE* outfile) {
  symbol->codegen(outfile);
  
}


void StructuralType::codegenPrototype(FILE* outfile) {
  forv_Vec(TypeSymbol, type, types) {
    type->codegenPrototype(outfile);
    fprintf(outfile, "\n");
  }
  fprintf(outfile, "typedef struct __");
  symbol->codegen(outfile);
  fprintf(outfile, " ");
  codegenStructName(outfile);
  fprintf(outfile, ";\n");
}


void StructuralType::codegenIOCall(FILE* outfile, ioCallType ioType, Expr* arg,
                                   Expr* format) {
  forv_Symbol(method, methods) {
    if (strcmp(method->name, "write") == 0) {
      method->codegen(outfile);
      fprintf(outfile, "(&(");
      arg->codegen(outfile);
      fprintf(outfile, "));");
      return;
    }
  }
  INT_FATAL("Couldn't find the IO call for a class");
}


void StructuralType::codegenMemberAccessOp(FILE* outfile) {
  fprintf(outfile, ".");
}


bool StructuralType::blankIntentImpliesRef(void) {
  return false;
}


bool StructuralType::implementedUsingCVals(void) {
  return true;
}


ClassType::ClassType(astType_t astType) :
  StructuralType(astType, 
                 new Variable(Symboltable::lookupInternal("nil", 
                                                          SCOPE_INTRINSIC)))
{}


Type* ClassType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, 
                          CloneCallback* analysis_clone) {
  ClassType* copy_type = new ClassType(astType);
  copyGuts(copy_type, clone, map, analysis_clone);
  return copy_type;
}


Stmt* ClassType::buildIOBodyStmts(ParamSymbol* thisArg) {
  Stmt* bodyStmts = NULL;

  bodyStmts = addIOStmt(bodyStmts, "{");
  bodyStmts = buildIOBodyStmtsHelp(bodyStmts, thisArg);
  bodyStmts = addIOStmt(bodyStmts, "}");
  
  return bodyStmts;
}


void ClassType::codegenStructName(FILE* outfile) {
  fprintf(outfile, "_");
  symbol->codegen(outfile);
  fprintf(outfile,", *");
  symbol->codegen(outfile);
}


void ClassType::codegenIOCall(FILE* outfile, ioCallType ioType, Expr* arg,
                              Expr* format) {
  if (symbol->isDead) {
    // BLC: theoretically, this should only happen if no
    // instantiations of a class are ever made
    fprintf(outfile, "fprintf(stdout, \"nil\");\n");
    return;
  }
  fprintf(outfile, "if (");
  arg->codegen(outfile);
  fprintf(outfile, " == nil) {\n");
  fprintf(outfile, "fprintf(stdout, \"nil\");\n");
  fprintf(outfile, "} else {\n");
  StructuralType::codegenIOCall(outfile, ioType, arg, format);
  fprintf(outfile, "}\n");
}


void ClassType::codegenMemberAccessOp(FILE* outfile) {
  fprintf(outfile, "->");
}


bool ClassType::blankIntentImpliesRef(void) {
  return true;
}


bool ClassType::implementedUsingCVals(void) {
  return false;
}


RecordType::RecordType(void) :
  StructuralType(TYPE_RECORD)
{}


Type* RecordType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, 
                          CloneCallback* analysis_clone) {
  RecordType* copy_type = new RecordType();
  copyGuts(copy_type, clone, map, analysis_clone);
  return copy_type;
}


UnionType::UnionType(void) :
  StructuralType(TYPE_UNION),
  fieldSelector(NULL)
{}


Type* UnionType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, 
                          CloneCallback* analysis_clone) {
  UnionType* copy_type = new UnionType();
  copyGuts(copy_type, clone, map, analysis_clone);
  return copy_type;
}


static char* buildFieldSelectorName(UnionType* unionType, Symbol* field, 
                                    bool enumName = false) {
  char* fieldName;
  if (field) {
    fieldName = field->name;
  } else {
    if (enumName) {
      fieldName = "";
    } else {
      fieldName = "_uninitialized";
    }
  }
  return glomstrings(4, "_", unionType->symbol->name, "_union_id_", fieldName);
}


void UnionType::buildFieldSelector(void) {
  EnumSymbol* id_list = NULL;

  /* build list of enum symbols */
  char* id_name = buildFieldSelectorName(this, NULL);
  EnumSymbol* id_symbol = new EnumSymbol(id_name, NULL);
  id_list = appendLink(id_list, id_symbol);
  forv_Vec(VarSymbol, tmp, fields) {
    id_name = buildFieldSelectorName(this, tmp);
    id_symbol = new EnumSymbol(id_name, NULL);
    id_list = appendLink(id_list, id_symbol);
  }
  id_list->set_values();

  /* build enum type */
  EnumType* enum_type = new EnumType(id_list);
  char* enum_name = buildFieldSelectorName(this, NULL, true);
  TypeSymbol* enum_symbol = new TypeSymbol(enum_name, enum_type);
  enum_type->addSymbol(enum_symbol);

  /* build definition of enum */
  DefExpr* def_expr = new DefExpr(enum_symbol);
  enum_symbol->setDefPoint(def_expr);
  id_list->setDefPoint(def_expr);
  DefStmt* def_stmt = new DefStmt(def_expr);
  symbol->defPoint->stmt->insertBefore(def_stmt);

  fieldSelector = enum_type;
}


static char* unionCallName[NUM_UNION_CALLS] = {
  "_UNION_SET",
  "_UNION_CHECK",
  "_UNION_CHECK_QUIET"
};


FnCall* UnionType::buildSafeUnionAccessCall(unionCall type, Expr* base, 
                                            Symbol* field) {
  Expr* args = base->copy();
  char* id_tag = buildFieldSelectorName(this, field);
  args->append(new Variable(Symboltable::lookup(id_tag)));
  if (type == UNION_CHECK) {
    args->append(new StringLiteral(base->filename));
    args->append(new IntLiteral(intstring(base->lineno), base->lineno));
  }
  
  char* fnName = unionCallName[type];
  return new FnCall(new Variable(Symboltable::lookupInternal(fnName)), args);
}


CondStmt* UnionType::buildUnionFieldIO(CondStmt* prevStmt, VarSymbol* field, 
                                       ParamSymbol* thisArg) {
  FnCall* checkFn = buildSafeUnionAccessCall(UNION_CHECK_QUIET, new Variable(thisArg),
                                             field);
  Stmt* condStmts = NULL;
  if (field) {
    condStmts = addIOStmt(condStmts, glomstrings(2, field->name, " = "));
    condStmts = addIOStmt(condStmts, thisArg, field);
  } else {
    condStmts = addIOStmt(condStmts, "uninitialized");
  }
  Stmt* thenClause = new BlockStmt(condStmts);
  CondStmt* newCondStmt = new CondStmt(checkFn, thenClause, NULL);
  if (prevStmt) {
    prevStmt->addElseStmt(newCondStmt);
  }
  return newCondStmt;
}


Stmt* UnionType::buildIOBodyStmtsHelp(Stmt* bodyStmts, ParamSymbol* thisArg) {
  CondStmt* topStmt = buildUnionFieldIO(NULL, NULL, thisArg);
  CondStmt* fieldIOStmt = topStmt;
  forv_Vec(VarSymbol, field, fields) {
    fieldIOStmt = buildUnionFieldIO(fieldIOStmt, field, thisArg);
  }
  bodyStmts->append(topStmt);
  return bodyStmts;
}


Stmt* UnionType::buildConstructorBody(Stmt* stmts, Symbol* _this, ParamSymbol* arguments) {
  Expr* arg1 = new Variable(_this);
  Expr* arg2 = new Variable(fieldSelector->valList);
  arg1 = appendLink(arg1, arg2);
  FnCall* init_function = 
    new FnCall(new Variable(Symboltable::lookupInternal("_UNION_SET")), arg1);
  ExprStmt* init_stmt = new ExprStmt(init_function);
  stmts = appendLink(stmts, init_stmt);
  
  return stmts;
}


void UnionType::codegenStartDefFields(FILE* outfile) {
  fieldSelector->codegen(outfile);
  fprintf(outfile, " _chpl_union_tag;\n");
  fprintf(outfile, "union {\n");
}


void UnionType::codegenStopDefFields(FILE* outfile) {
  fprintf(outfile, "} _chpl_union;\n");
}


void UnionType::codegenMemberAccessOp(FILE* outfile) {
  fprintf(outfile, "._chpl_union.");
}


TupleType::TupleType(Type* firstType) :
  Type(TYPE_TUPLE, NULL)
{
  components.add(firstType);
  defaultVal = new Tuple(firstType->defaultVal->copy());
  SET_BACK(defaultVal);
}


void TupleType::addType(Type* additionalType) {
  components.add(additionalType);
  if (Tuple* tuple = dynamic_cast<Tuple*>(defaultVal)) {
    tuple->exprs = appendLink(tuple->exprs, additionalType->defaultVal->copy());
  }
}


void TupleType::rebuildDefaultVal(void) {
  Tuple* tuple = new Tuple(NULL);
  forv_Vec(Type, component, components) {
    tuple->exprs = appendLink(tuple->exprs, component->defaultVal->copy());
  }
  SET_BACK(tuple->exprs);
  defaultVal->replace(tuple);
}


Type* TupleType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  TupleType* newTupleType =
    new TupleType(components.v[0]->copy(clone, map, analysis_clone));
  for (int i=1; i<components.n; i++) {
    newTupleType->addType(components.v[i]->copy(clone, map, analysis_clone));
  }
  newTupleType->addSymbol(symbol);
  return newTupleType;
}


void TupleType::traverseDefType(Traversal* traversal) {
  for (int i=0; i<components.n; i++) {
    TRAVERSE(components.v[i], traversal, false);
  }
  TRAVERSE(defaultVal, traversal, false);
}


void TupleType::print(FILE* outfile) {
  fprintf(outfile, "(");
  for (int i=0; i<components.n; i++) {
    if (i) {
      fprintf(outfile, ", ");
    }
    components.v[i]->print(outfile);
  }
  fprintf(outfile, ")");
}


void TupleType::codegen(FILE* outfile) {
  symbol->codegen(outfile);
}


void TupleType::codegenDef(FILE* outfile) {
  fprintf(outfile, "typedef struct _");
  symbol->codegen(outfile);
  fprintf(outfile, " {\n");
  int i = 0;
  forv_Vec(Type, component, components) {
    component->codegen(outfile);
    fprintf(outfile, " _field%d;\n", ++i);
  }
  fprintf(outfile, "} ");
  symbol->codegen(outfile);
  fprintf(outfile, ";\n\n");
}


void TupleType::codegenIOCall(FILE* outfile, ioCallType ioType, Expr* arg,
                              Expr* format) {
  fprintf(outfile, "fprintf(stdout, \"(\");\n");
  int compNum = 0;
  char compNumStr[80];
  forv_Vec(Type, component, components) {
    if (compNum) {
      fprintf(outfile, "fprintf(stdout, \", \");\n");
    }

    compNum++;
    sprintf(compNumStr, "%d", compNum);
    component->codegenIOCall(outfile, ioType, new TupleSelect(arg, new IntLiteral(compNumStr, compNum)));
    fprintf(outfile, ";\n");
  }
  fprintf(outfile, "fprintf(stdout, \")\");\n");
}



SumType::SumType(Type* firstType) :
  Type(TYPE_SUM, NULL)
{
  components.add(firstType);
}


void SumType::addType(Type* additionalType) {
  components.add(additionalType);
}


VariableType::VariableType(Type *init_type) :
  Type(TYPE_VARIABLE, NULL), 
  type(init_type)
{}


Type* VariableType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  return new VariableType();
}


void VariableType::codegen(FILE* outfile) {
  INT_FATAL(this, "ERROR:  Cannot codegen a variable type.");
}


UnresolvedType::UnresolvedType(char* init_symbol) :
  Type(TYPE_UNRESOLVED, NULL) {
  symbol = new UnresolvedSymbol(init_symbol);
}


Type* UnresolvedType::copyType(bool clone, Map<BaseAST*,BaseAST*>* map, CloneCallback* analysis_clone) {
  return new UnresolvedType(copystring(symbol->name));
}


void UnresolvedType::codegen(FILE* outfile) {
  INT_FATAL(this, "ERROR:  Cannot codegen an unresolved type.");
}


void initTypes(void) {
  // define built-in types
  dtUnknown = Symboltable::defineBuiltinType("???", "???", NULL);
  dtVoid = Symboltable::defineBuiltinType("void", "void", NULL);

  dtBoolean = Symboltable::defineBuiltinType("boolean", "_boolean",
                                             new BoolLiteral("false", false));
  dtInteger = Symboltable::defineBuiltinType("integer", "_integer64",
                                             new IntLiteral("0", 0));
  dtFloat = Symboltable::defineBuiltinType("float", "_float64",
                                           new FloatLiteral("0.0", 0.0));
  dtComplex = Symboltable::defineBuiltinType("complex", "_complex128",
                                             new FloatLiteral("0.0", 0.0));
  dtString = Symboltable::defineBuiltinType("string", "_string", 
                                            new StringLiteral(""));
  dtNumeric = Symboltable::defineBuiltinType("numeric", "_numeric", NULL);
  dtAny = Symboltable::defineBuiltinType("any", "_any", NULL);
  dtObject = Symboltable::defineBuiltinType("object", "_object", NULL);
  dtLocale = Symboltable::defineBuiltinType("locale", "_locale", NULL);

  // define dtNil.  This doesn't reuse the above because it's a
  // different subclass of Type.  Could come up with a more clever
  // way to generalize
  dtNil = new NilType();
  TypeSymbol* sym = new TypeSymbol("_nilType", dtNil);
  sym->cname = copystring("_nilType");
  dtNil->addSymbol(sym);
  builtinTypes.add(dtNil);
}


void findInternalTypes(void) {
  dtTuple = Symboltable::lookupInternalType("Tuple")->type;
  dtIndex = Symboltable::lookupInternalType("Index")->type;
  dtDomain = Symboltable::lookupInternalType("Domain")->type;
  dtArray = Symboltable::lookupInternalType("Array")->type;
}

// you can use something like the following cache to 
// find an existing SumType.

// eventually this sort of a cache will have to be
// implemented over all 'structural' types, e.g.
// records

class LUBCacheHashFns : public gc {
 public:
  static unsigned int hash(Symbol *a) {
    unsigned int h = 0;
    Sym *s = a->asymbol->sym;
    for (int i = 0; i < s->has.n; i++)
      h += open_hash_multipliers[i % 256] * (uintptr_t)s->has.v[i];
    return h;
  }
  static int equal(Symbol *aa, Symbol *ab) { 
    Sym *a = aa->asymbol->sym, *b = ab->asymbol->sym;
    if (a->has.n != b->has.n)
      return 0;
    for (int i = 0; i < a->has.n; i++)
      if (a->has.v[i] != b->has.v[i])
        return 0;
    return 1;
  }
};

static class BlockHash<Symbol *, LUBCacheHashFns> lub_cache;


Type *find_or_make_sum_type(Vec<Type *> *types) {
  static int uid = 1;
  if (types->n < 2) {
    INT_FATAL("Trying to create sum_type of less than 2 types");
  }
  qsort(types->v, types->n, sizeof(types->v[0]), compar_baseast);
  SumType* new_sum_type = new SumType(types->v[0]);
  for (int i = 1; i <= types->n; i++) {
    new_sum_type->addType(types->v[i]);
  }
  char* name = glomstrings(2, "_sum_type", intstring(uid++));
  Symbol* sym = new TypeSymbol(name, new_sum_type);
  new_sum_type->addSymbol(sym);
  return new_sum_type;
}


NilType::NilType(void) :
  Type(TYPE_NIL, NULL)
{}


void NilType::codegen(FILE* outfile) {
  INT_FATAL(this, "Trying to codegen a nil Type");
}

