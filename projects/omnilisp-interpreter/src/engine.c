#include "engine.h"

static Obj* heap_head = NULL;
static size_t heap_count = 0;
static Obj* nil_obj = NULL;

static char* safe_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* d = malloc(len + 1);
    if (d) {
        memcpy(d, s, len + 1);
    }
    return d;
}

void omni_init(void) {
    heap_head = NULL;
    heap_count = 0;
    nil_obj = malloc(sizeof(Obj));
    nil_obj->type = OBJ_NIL;
    nil_obj->marked = true;
    nil_obj->next = NULL;
}

void omni_cleanup(void) {
    Obj* curr = heap_head;
    while (curr) {
        Obj* next = curr->next;
        if (curr->type == OBJ_SYMBOL || curr->type == OBJ_STRING) {
            free(curr->as.sval);
        }
        free(curr);
        curr = next;
    }
    heap_head = NULL;
    heap_count = 0;
    if (nil_obj) {
        free(nil_obj);
        nil_obj = NULL;
    }
}

static Obj* omni_alloc(ObjType type) {
    Obj* obj = malloc(sizeof(Obj));
    if (!obj) {
        omni_gc_collect(NULL, NULL);
        obj = malloc(sizeof(Obj));
        if (!obj) {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }
    }
    obj->type = type;
    obj->marked = false;
    obj->next = heap_head;
    heap_head = obj;
    heap_count++;
    return obj;
}

Obj* omni_nil(void) {
    return nil_obj;
}

Obj* omni_make_bool(bool val) {
    Obj* obj = omni_alloc(OBJ_BOOL);
    obj->as.bval = val;
    return obj;
}

Obj* omni_make_int(int64_t val) {
    Obj* obj = omni_alloc(OBJ_INT);
    obj->as.ival = val;
    return obj;
}

Obj* omni_make_float(double val) {
    Obj* obj = omni_alloc(OBJ_FLOAT);
    obj->as.fval = val;
    return obj;
}

Obj* omni_make_symbol(const char* name) {
    Obj* obj = omni_alloc(OBJ_SYMBOL);
    obj->as.sval = safe_strdup(name);
    return obj;
}

Obj* omni_make_string(const char* val) {
    Obj* obj = omni_alloc(OBJ_STRING);
    obj->as.sval = safe_strdup(val);
    return obj;
}

Obj* omni_make_cons(Obj* car, Obj* cdr) {
    Obj* obj = omni_alloc(OBJ_CONS);
    obj->as.cons.car = car ? car : nil_obj;
    obj->as.cons.cdr = cdr ? cdr : nil_obj;
    return obj;
}

Obj* omni_make_primitive(PrimitiveFn fn) {
    Obj* obj = omni_alloc(OBJ_PRIMITIVE);
    obj->as.pval = fn;
    return obj;
}

Obj* omni_make_closure(Obj* params, Obj* body, Env* env) {
    Obj* obj = omni_alloc(OBJ_CLOSURE);
    obj->as.closure.params = params;
    obj->as.closure.body = body;
    obj->as.closure.env = env;
    return obj;
}

Obj* omni_make_macro(Obj* params, Obj* body, Env* env) {
    Obj* obj = omni_alloc(OBJ_MACRO);
    obj->as.closure.params = params;
    obj->as.closure.body = body;
    obj->as.closure.env = env;
    return obj;
}

Env* omni_env_create(Env* parent) {
    Env* env = malloc(sizeof(Env));
    env->head = NULL;
    env->parent = parent;
    return env;
}

void omni_env_bind(Env* env, const char* name, Obj* obj) {
    Binding* b = malloc(sizeof(Binding));
    b->name = safe_strdup(name);
    b->obj = obj ? obj : nil_obj;
    b->next = env->head;
    env->head = b;
}

Obj* omni_env_lookup(Env* env, const char* name) {
    Env* curr = env;
    while (curr) {
        Binding* b = curr->head;
        while (b) {
            if (strcmp(b->name, name) == 0) {
                return b->obj;
            }
            b = b->next;
        }
        curr = curr->parent;
    }
    return NULL;
}

static void skip_whitespace(Lexer* l) {
    while (l->pos < l->len) {
        char c = l->src[l->pos];
        if (c == ';') {
            while (l->pos < l->len && l->src[l->pos] != '\n') {
                l->pos++;
            }
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',') {
            l->pos++;
        } else {
            break;
        }
    }
}

static bool is_delimiter(char c) {
    return c == '(' || c == ')' || c == ';' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '"';
}

static Obj* parse_expr(Lexer* l);

static Obj* parse_list(Lexer* l) {
    skip_whitespace(l);
    if (l->pos >= l->len) return omni_nil();
    if (l->src[l->pos] == ')') {
        l->pos++;
        return omni_nil();
    }
    Obj* car = parse_expr(l);
    Obj* cdr = parse_list(l);
    return omni_make_cons(car, cdr);
}

static Obj* parse_expr(Lexer* l) {
    skip_whitespace(l);
    if (l->pos >= l->len) {
        Obj* e = omni_alloc(OBJ_EOF);
        return e;
    }
    char c = l->src[l->pos];
    if (c == '(') {
        l->pos++;
        return parse_list(l);
    } else if (c == ')') {
        l->pos++;
        return omni_nil();
    } else if (c == '\'') {
        l->pos++;
        Obj* expr = parse_expr(l);
        return omni_make_cons(omni_make_symbol("quote"), omni_make_cons(expr, omni_nil()));
    } else if (c == '`') {
        l->pos++;
        Obj* expr = parse_expr(l);
        return omni_make_cons(omni_make_symbol("quasiquote"), omni_make_cons(expr, omni_nil()));
    } else if (c == ',') {
        l->pos++;
        Obj* expr = parse_expr(l);
        return omni_make_cons(omni_make_symbol("unquote"), omni_make_cons(expr, omni_nil()));
    } else if (c == '"') {
        l->pos++;
        size_t start = l->pos;
        while (l->pos < l->len && l->src[l->pos] != '"') {
            l->pos++;
        }
        size_t slen = l->pos - start;
        char* buf = malloc(slen + 1);
        memcpy(buf, &l->src[start], slen);
        buf[slen] = '\0';
        if (l->pos < l->len) l->pos++;
        Obj* obj = omni_make_string(buf);
        free(buf);
        return obj;
    } else {
        size_t start = l->pos;
        while (l->pos < l->len && !is_delimiter(l->src[l->pos])) {
            l->pos++;
        }
        size_t slen = l->pos - start;
        char* buf = malloc(slen + 1);
        memcpy(buf, &l->src[start], slen);
        buf[slen] = '\0';

        if (strcmp(buf, "true") == 0) {
            free(buf);
            return omni_make_bool(true);
        }
        if (strcmp(buf, "false") == 0) {
            free(buf);
            return omni_make_bool(false);
        }

        char* endp;
        int64_t ival = strtoll(buf, &endp, 10);
        if (*endp == '\0') {
            free(buf);
            return omni_make_int(ival);
        }
        double fval = strtod(buf, &endp);
        if (*endp == '\0') {
            free(buf);
            return omni_make_float(fval);
        }

        Obj* sym = omni_make_symbol(buf);
        free(buf);
        return sym;
    }
}

Obj* omni_read(const char* input) {
    Lexer l;
    l.src = input;
    l.pos = 0;
    l.len = strlen(input);
    return parse_expr(&l);
}

static bool is_symbol(Obj* obj, const char* name) {
    return obj && obj->type == OBJ_SYMBOL && strcmp(obj->as.sval, name) == 0;
}

static Obj* eval_list(Obj* list, Env* env);

static Obj* macro_expand(Obj* expr, Env* env) {
    if (!expr || expr->type != OBJ_CONS) return expr;
    Obj* car = expr->as.cons.car;
    if (car && car->type == OBJ_SYMBOL) {
        Obj* binding = omni_env_lookup(env, car->as.sval);
        if (binding && binding->type == OBJ_MACRO) {
            Obj* args = expr->as.cons.cdr;
            Env* macro_env = omni_env_create(binding->as.closure.env);
            Obj* params = binding->as.closure.params;
            while (params && params->type == OBJ_CONS && args && args->type == OBJ_CONS) {
                omni_env_bind(macro_env, params->as.cons.car->as.sval, eval_list(args->as.cons.car, env));
                params = params->as.cons.cdr;
                args = args->as.cons.cdr;
            }
            Obj* expanded = omni_eval(binding->as.closure.body, macro_env);
            return macro_expand(expanded, env);
        }
    }
    return expr;
}

static Obj* eval_list(Obj* list, Env* env) {
    if (!list || list->type == OBJ_NIL) return omni_nil();
    if (list->type != OBJ_CONS) return omni_eval(list, env);
    Obj* car = list->as.cons.car;
    Obj* cdr = list->as.cons.cdr;
    Obj* evaluated_car = omni_eval(car, env);
    Obj* head = omni_make_cons(evaluated_car, omni_nil());
    Obj* curr = head;
    while (cdr && cdr->type == OBJ_CONS) {
        Obj* eval_item = omni_eval(cdr->as.cons.car, env);
        curr->as.cons.cdr = omni_make_cons(eval_item, omni_nil());
        curr = curr->as.cons.cdr;
        cdr = cdr->as.cons.cdr;
    }
    return head;
}

Obj* omni_eval(Obj* expr, Env* env) {
    Obj* curr_expr = expr;
    Env* curr_env = env;

tail_call_loop:
    if (!curr_expr) return omni_nil();
    
    switch (curr_expr->type) {
        case OBJ_NIL:
        case OBJ_BOOL:
        case OBJ_INT:
        case OBJ_FLOAT:
        case OBJ_STRING:
        case OBJ_PRIMITIVE:
        case OBJ_CLOSURE:
        case OBJ_MACRO:
            return curr_expr;
            
        case OBJ_SYMBOL:
            {
                Obj* val = omni_env_lookup(curr_env, curr_expr->as.sval);
                if (!val) {
                    fprintf(stderr, "Unbound symbol: %s\n", curr_expr->as.sval);
                    return omni_nil();
                }
                return val;
            }
            
        case OBJ_CONS:
            {
                curr_expr = macro_expand(curr_expr, curr_env);
                if (!curr_expr || curr_expr->type != OBJ_CONS) {
                    return omni_eval(curr_expr, curr_env);
                }
                
                Obj* car = curr_expr->as.cons.car;
                Obj* cdr = curr_expr->as.cons.cdr;
                
                if (is_symbol(car, "quote")) {
                    return cdr ? cdr->as.cons.car : omni_nil();
                }
                if (is_symbol(car, "if")) {
                    Obj* cond = omni_eval(cdr->as.cons.car, curr_env);
                    bool truthy = !(cond->type == OBJ_NIL || (cond->type == OBJ_BOOL && !cond->as.bval));
                    Obj* branch = truthy ? cdr->as.cons.cdr->as.cons.car : cdr->as.cons.cdr->as.cons.cdr->as.cons.car;
                    curr_expr = branch;
                    goto tail_call_loop;
                }
                if (is_symbol(car, "define")) {
                    Obj* sym = cdr->as.cons.car;
                    Obj* val = omni_eval(cdr->as.cons.cdr->as.cons.car, curr_env);
                    omni_env_bind(curr_env, sym->as.sval, val);
                    return val;
                }
                if (is_symbol(car, "lambda")) {
                    return omni_make_closure(cdr->as.cons.car, cdr->as.cons.cdr->as.cons.car, curr_env);
                }
                if (is_symbol(car, "defmacro")) {
                    return omni_make_macro(cdr->as.cons.car, cdr->as.cons.cdr->as.cons.car, curr_env);
                }
                if (is_symbol(car, "begin")) {
                    Obj* body = cdr;
                    while (body && body->type == OBJ_CONS) {
                        Obj* next = body->as.cons.cdr;
                        if (!next || next->type == OBJ_NIL) {
                            curr_expr = body->as.cons.car;
                            goto tail_call_loop;
                        }
                        omni_eval(body->as.cons.car, curr_env);
                        body = next;
                    }
                    return omni_nil();
                }
                
                Obj* fn = omni_eval(car, curr_env);
                Obj* args = eval_list(cdr, curr_env);
                
                if (fn->type == OBJ_PRIMITIVE) {
                    return fn->as.pval(curr_env, args);
                } else if (fn->type == OBJ_CLOSURE) {
                    Env* call_env = omni_env_create(fn->as.closure.env);
                    Obj* params = fn->as.closure.params;
                    Obj* arg_vals = args;
                    while (params && params->type == OBJ_CONS && arg_vals && arg_vals->type == OBJ_CONS) {
                        omni_env_bind(call_env, params->as.cons.car->as.sval, arg_vals->as.cons.car);
                        params = params->as.cons.cdr;
                        arg_vals = arg_vals->as.cons.cdr;
                    }
                    curr_expr = fn->as.closure.body;
                    curr_env = call_env;
                    goto tail_call_loop;
                } else {
                    fprintf(stderr, "Attempt to apply non-procedure\n");
                    return omni_nil();
                }
            }
        default:
            return omni_nil();
    }
}

char* omni_stringify(Obj* obj) {
    if (!obj) return safe_strdup("nil");
    char buf[128];
    switch (obj->type) {
        case OBJ_NIL:
            return safe_strdup("()");
        case OBJ_BOOL:
            return safe_strdup(obj->as.bval ? "true" : "false");
        case OBJ_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)obj->as.ival);
            return safe_strdup(buf);
        case OBJ_FLOAT:
            snprintf(buf, sizeof(buf), "%f", obj->as.fval);
            return safe_strdup(buf);
        case OBJ_SYMBOL:
        case OBJ_STRING:
            return safe_strdup(obj->as.sval);
        case OBJ_CONS:
            {
                size_t cap = 256;
                size_t len = 0;
                char* res = malloc(cap);
                res[0] = '(';
                res[1] = '\0';
                len = 1;
                Obj* curr = obj;
                while (curr && curr->type == OBJ_CONS) {
                    char* elem_str = omni_stringify(curr->as.cons.car);
                    size_t elen = strlen(elem_str);
                    if (len + elen + 4 >= cap) {
                        cap *= 2;
                        res = realloc(res, cap);
                    }
                    if (len > 1) {
                        res[len++] = ' ';
                    }
                    strcpy(res + len, elem_str);
                    len += elen;
                    free(elem_str);
                    curr = curr->as.cons.cdr;
                }
                if (curr && curr->type != OBJ_NIL) {
                    char* tail_str = omni_stringify(curr);
                    size_t tlen = strlen(tail_str);
                    if (len + tlen + 4 >= cap) {
                        cap *= 2;
                        res = realloc(res, cap);
                    }
                    strcpy(res + len, " . ");
                    len += 3;
                    strcpy(res + len, tail_str);
                    len += tlen;
                    free(tail_str);
                }
                if (len + 2 >= cap) {
                    res = realloc(res, len + 2);
                }
                res[len++] = ')';
                res[len] = '\0';
                return res;
            }
        case OBJ_PRIMITIVE:
            return safe_strdup("<primitive>");
        case OBJ_CLOSURE:
            return safe_strdup("<closure>");
        case OBJ_MACRO:
            return safe_strdup("<macro>");
        case OBJ_EOF:
            return safe_strdup("<EOF>");
    }
    return safe_strdup("?");
}

static void mark_object(Obj* obj) {
    if (!obj || obj->marked) return;
    obj->marked = true;
    if (obj->type == OBJ_CONS) {
        mark_object(obj->as.cons.car);
        mark_object(obj->as.cons.cdr);
    } else if (obj->type == OBJ_CLOSURE || obj->type == OBJ_MACRO) {
        mark_object(obj->as.closure.params);
        mark_object(obj->as.closure.body);
    }
}

static void mark_env(Env* env) {
    while (env) {
        Binding* b = env->head;
        while (b) {
            mark_object(b->obj);
            b = b->next;
        }
        env = env->parent;
    }
}

void omni_gc_collect(Env* root_env, Obj* root_obj) {
    if (nil_obj) nil_obj->marked = true;
    mark_env(root_env);
    mark_object(root_obj);

    Obj* curr = heap_head;
    Obj* prev = NULL;
    while (curr) {
        if (curr == nil_obj) {
            prev = curr;
            curr = curr->next;
            continue;
        }
        if (curr->marked) {
            curr->marked = false;
            prev = curr;
            curr = curr->next;
        } else {
            Obj* dead = curr;
            if (prev) {
                prev->next = curr->next;
            } else {
                heap_head = curr->next;
            }
            curr = curr->next;
            if (dead->type == OBJ_SYMBOL || dead->type == OBJ_STRING) {
                free(dead->as.sval);
            }
            free(dead);
            heap_count--;
        }
    }
}

size_t omni_heap_count(void) {
    return heap_count;
}
