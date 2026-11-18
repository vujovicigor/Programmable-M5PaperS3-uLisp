// ---------------------------------------------------------------------------
// Macros for uLisp: backquote, defmacro, macroexpand, gensym and the Clojure
// threading macros (-> ->> as-> some-> some->> cond-> cond->>).
//
// A macro is stored as the global value (macro lambda-list . body). When eval
// meets a call to a macro it expands it, then replaces the call in place with
// the expansion ("displacing" macros, as in early Lisps), so each call site is
// expanded only once and the expansion stays reachable for the garbage
// collector. The threading macros are built in, written in C.
// ---------------------------------------------------------------------------

object *bsymbolnamed (const char *name) {
  return bsymbol(lookupbuiltin((char *)name));
}

// ---- Backquote ---------------------------------------------------------------

object *bqexpand (object *x, int level, object *env) {
  if (!consp(x)) return x;
  object *op = car(x);
  if (consp(cdr(x)) && cddr(x) == NULL) {
    if (isbuiltin(op, UNQUOTE)) {
      if (level == 0) return eval(second(x), env);
      return cons(op, cons(bqexpand(second(x), level-1, env), NULL));
    }
    if (isbuiltin(op, UNQUOTESPLICING)) {
      if (level == 0) error2(",@ must be inside a list");
      return cons(op, cons(bqexpand(second(x), level-1, env), NULL));
    }
    if (isbuiltin(op, BACKQUOTE)) return cons(op, cons(bqexpand(second(x), level+1, env), NULL));
  }
  object *head = cons(NULL, NULL);
  protect(head);
  object *tail = head;
  while (consp(x)) {
    // `(a . ,b) reads as (a unquote b): the rest of the list is an unquote
    if ((isbuiltin(car(x), UNQUOTE) || isbuiltin(car(x), UNQUOTESPLICING)) && consp(cdr(x)) && cddr(x) == NULL) {
      if (level == 0 && isbuiltin(car(x), UNQUOTESPLICING)) error2(",@ after a dot");
      cdr(tail) = bqexpand(x, level, env);
      x = NULL;
      break;
    }
    object *item = car(x);
    if (level == 0 && consp(item) && isbuiltin(car(item), UNQUOTESPLICING) && consp(cdr(item))) {
      object *spliced = eval(second(item), env);
      if (!listp(spliced)) error(",@ needs a list", spliced);
      while (consp(spliced)) {
        cdr(tail) = cons(car(spliced), NULL);
        tail = cdr(tail);
        spliced = cdr(spliced);
      }
    } else {
      object *val = bqexpand(item, level, env);
      cdr(tail) = cons(val, NULL);
      tail = cdr(tail);
    }
    x = cdr(x);
  }
  if (x != NULL) cdr(tail) = x;   // dotted atom tail
  unprotect();
  return cdr(head);
}

object *sp_backquote (object *args, object *env) {
  return bqexpand(first(args), 0, env);
}

object *sp_unquote (object *args, object *env) {
  (void) args, (void) env;
  error2("comma not inside a backquote");
  return nil;
}

// ---- defmacro and expansion ----------------------------------------------------

bool macrop (object *val) {
  return consp(val) && isbuiltin(car(val), MACRO);
}

object *sp_defmacro (object *args, object *env) {
  (void) env;
  object *var = first(args);
  if (!symbolp(var)) error(notasymbol, var);
  if (builtinp(var->name)) error("can't redefine a built-in", var);
  object *params = second(args);
  if (!listp(params)) error(notalist, params);
  // &body means &rest, and a dotted lambda list (a . rest) means (a &rest rest)
  for (object *p = params; p != NULL; p = cdr(p)) {
    if (isbuiltin(car(p), AMPBODY)) car(p) = bsymbol(AMPREST);
    if (cdr(p) != NULL && !consp(cdr(p))) {
      if (!symbolp(cdr(p))) error("illegal lambda list", params);
      cdr(p) = cons(bsymbol(AMPREST), cons(cdr(p), NULL));
    }
  }
  object *val = cons(bsymbol(MACRO), cdr(args));
  object *pair = value(var->name, GlobalEnv);
  if (pair != NULL) cdr(pair) = val;
  else push(cons(var, val), GlobalEnv);
  return var;
}

// Calls the macro's expander on the unevaluated arguments. The expander runs in the
// global environment; the caller's env is kept on GCStack, since nothing else reaches it
object *expandmacro (object *macro, object *name, object *args, object *env) {
  protect(env);
  object *menv = NULL;
  object *function = cons(nil, cdr(macro));   // (captured-env lambda-list . body)
  protect(function);
  object *form = closure(0, name->name, function, args, &menv);
  object *expansion = eval(form, menv);
  unprotect(); unprotect();
  return expansion;
}

// Expands form once if it's a macro call; sets *expanded
object *macroexpand1 (object *form, object *env, bool *expanded) {
  *expanded = false;
  if (!consp(form) || !symbolp(car(form))) return form;
  object *function = car(form);
  if (builtinp(function->name)) {
    builtin_t name = builtin(function->name);
    if (name < THREADFIRST || name > CONDTHREADLAST) return form;
    Context = name;
    checkargs(cdr(form));
    *expanded = true;
    return ((fn_ptr_type)lookupfn(name))(cdr(form), env);
  }
  object *pair = findpair(function, env);
  if (pair == NULL || !macrop(cdr(pair))) return form;
  *expanded = true;
  return expandmacro(cdr(pair), function, cdr(form), env);
}

// Replaces the macro call form with its expansion; returns what to evaluate
object *displace (object *form, object *expansion) {
  if (!consp(expansion)) return expansion;
  car(form) = car(expansion);
  cdr(form) = cdr(expansion);
  return form;
}

object *fn_macroexpand1 (object *args, object *env) {
  bool expanded;
  return macroexpand1(first(args), env, &expanded);
}

object *fn_macroexpand (object *args, object *env) {
  object *form = first(args);
  bool expanded = true;
  protect(form);
  while (expanded) {
    form = macroexpand1(form, env, &expanded);
    car(GCStack) = form;
  }
  unprotect();
  return form;
}

// Uninterned symbols: "#:G1" can't be typed in, so it never clashes with a user symbol
unsigned int GensymCounter = 0;

object *fn_gensym (object *args, object *env) {
  (void) env;
  char prefix[16] = "G", buffer[32];
  if (args != NULL) {
    object *arg = first(args);
    if (stringp(arg)) cstring(arg, prefix, sizeof(prefix));
    else if (symbolp(arg)) cstring(princtostring(arg), prefix, sizeof(prefix));
    else error("argument is not a string", arg);
  }
  snprintf(buffer, sizeof(buffer), "#:%s%u", prefix, ++GensymCounter);
  object *obj = lispstring(buffer);
  obj->type = SYMBOL;
  return obj;
}

// ---- Threading macros ------------------------------------------------------------
// (-> x (f a) g) => (g (f x a))      (->> x (f a) g) => (g (f a x))

object *threadstep (object *x, object *step, bool last) {
  if (!consp(step)) return cons(step, cons(x, NULL));
  if (!last) return cons(car(step), cons(x, cdr(step)));
  object *head = cons(car(step), NULL), *tail = head;
  for (object *p = cdr(step); consp(p); p = cdr(p)) { cdr(tail) = cons(car(p), NULL); tail = cdr(tail); }
  cdr(tail) = cons(x, NULL);
  return head;
}

object *threadall (object *args, bool last) {
  object *x = first(args);
  for (object *steps = cdr(args); steps != NULL; steps = cdr(steps))
    x = threadstep(x, car(steps), last);
  return x;
}

object *mc_threadfirst (object *args, object *env) { (void) env; return threadall(args, false); }
object *mc_threadlast (object *args, object *env) { (void) env; return threadall(args, true); }

// Builds (let* ((var init) (var step1) ...) var) from a list of (var form) bindings
object *threadlet (object *var, object *bindings) {
  return cons(bsymbol(LETSTAR), cons(bindings, cons(var, NULL)));
}

// (as-> expr name form*) => (let* ((name expr) (name form1) ...) name)
object *mc_asthread (object *args, object *env) {
  (void) env;
  object *var = second(args);
  if (!symbolp(var)) error(notasymbol, var);
  object *head = cons(cons(var, cons(first(args), NULL)), NULL), *tail = head;
  for (object *forms = cddr(args); forms != NULL; forms = cdr(forms)) {
    cdr(tail) = cons(cons(var, cons(car(forms), NULL)), NULL);
    tail = cdr(tail);
  }
  return threadlet(var, head);
}

// (some-> x f g): like ->, but stops with nil as soon as a step gives nil
object *somethread (object *args, bool last) {
  object *g = fn_gensym(NULL, NULL), *ifsym = bsymbolnamed("if");
  object *head = cons(cons(g, cons(first(args), NULL)), NULL), *tail = head;
  for (object *steps = cdr(args); steps != NULL; steps = cdr(steps)) {
    object *test = cons(ifsym, cons(g, cons(threadstep(g, car(steps), last), NULL)));
    cdr(tail) = cons(cons(g, cons(test, NULL)), NULL);
    tail = cdr(tail);
  }
  return threadlet(g, head);
}

object *mc_somethreadfirst (object *args, object *env) { (void) env; return somethread(args, false); }
object *mc_somethreadlast (object *args, object *env) { (void) env; return somethread(args, true); }

// (cond-> x test1 form1 test2 form2 ...): threads x through each form whose test is true
object *condthread (object *args, bool last) {
  object *g = fn_gensym(NULL, NULL), *ifsym = bsymbolnamed("if");
  object *head = cons(cons(g, cons(first(args), NULL)), NULL), *tail = head;
  for (object *clauses = cdr(args); clauses != NULL; clauses = cddr(clauses)) {
    if (cdr(clauses) == NULL) error2("needs an even number of test and form arguments");
    object *test = cons(ifsym, cons(first(clauses), cons(threadstep(g, second(clauses), last), cons(g, NULL))));
    cdr(tail) = cons(cons(g, cons(test, NULL)), NULL);
    tail = cdr(tail);
  }
  return threadlet(g, head);
}

object *mc_condthreadfirst (object *args, object *env) { (void) env; return condthread(args, false); }
object *mc_condthreadlast (object *args, object *env) { (void) env; return condthread(args, true); }

// ---- Documentation ---------------------------------------------------------------

const char doc_backquote[] = "`form\n"
"Like quote, but parts marked with , are evaluated and parts marked with ,@ are evaluated and spliced in.\n"
"`(a ,(+ 1 2) ,@(list 4 5)) gives (a 3 4 5).";
const char doc_unquote[] = ",form\n"
"Inside a backquote, evaluates form.";
const char doc_unquotesplicing[] = ",@form\n"
"Inside a backquote, evaluates form, which must give a list, and splices its elements in.";
const char doc_macro[] = "macro\n"
"Marks a macro definition made by defmacro.";
const char doc_ampbody[] = "&body\n"
"In a defmacro lambda list, the same as &rest.";
const char doc_threadfirst[] = "(-> x form*)\n"
"Threads x through the forms: inserts it as the first argument of the first form,\n"
"that result as the first argument of the next form, and so on. (-> 5 (- 1) abs) gives 4.";
const char doc_threadlast[] = "(->> x form*)\n"
"Like ->, but inserts each result as the last argument. (->> '(1 2 3) (mapcar 1+) (apply +)) gives 9.";
const char doc_asthread[] = "(as-> expr name form*)\n"
"Binds name to expr, then to each form in turn, so name can go anywhere in each form.\n"
"Returns the last value.";
const char doc_somethreadfirst[] = "(some-> x form*)\n"
"Like ->, but returns nil as soon as a step gives nil.";
const char doc_somethreadlast[] = "(some->> x form*)\n"
"Like ->>, but returns nil as soon as a step gives nil.";
const char doc_condthreadfirst[] = "(cond-> x test form ...)\n"
"Threads x through each form, as with ->, whose test is true. Tests don't see the threaded value.";
const char doc_condthreadlast[] = "(cond->> x test form ...)\n"
"Like cond->, but inserts the value as the last argument.";
const char doc_defmacro[] = "(defmacro name (parameters) form*)\n"
"Defines a macro: calls to name pass the arguments unevaluated to the forms, whose result\n"
"is then evaluated in place of the call. Parameters can use &optional, &rest or &body.";
const char doc_macroexpand1[] = "(macroexpand-1 form)\n"
"If form is a macro call, returns its expansion; otherwise returns form.";
const char doc_macroexpand[] = "(macroexpand form)\n"
"Expands form repeatedly until it is no longer a macro call.";
const char doc_gensym[] = "(gensym [prefix])\n"
"Returns a new symbol, distinct from every other symbol, for use as a variable in a macro expansion.";
