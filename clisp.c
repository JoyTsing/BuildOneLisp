#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include"./lib/mpc.h"

#ifdef _WIN32
#include<string.h>

static char buffer[2048];
char* readline(char* prompt){
    fputs(prompt,stdout);
    fgets(buffer,2048,stdin);
    char* cpy=malloc(strlen(buffer)+1);
    strcpy(cpy,buffer);
    cpy[strlen(cpy)-1]='\0';
    return cpy;
}
void add_history(char* unused){}
#else
//解决linux下小键盘移动出现[[^C的问题
//以上两个库解决小键盘移动失效的问题
#include<editline/readline.h>
#include<editline/history.h>
#endif

/*前置声明*/
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

//值类型
enum{
    LVAL_NUM,
    LVAL_ERR,
    LVAL_SYM,//symbol
    LVAL_FUN,
    LVAL_SEXPR,
    LVAL_QEXPR
};

//错误类型
enum{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

typedef lval*(*lbuiltin)(lenv*,lval*);

struct lval {
    int type;//0:num,1:err

    double num;
    //types string message
    char* err;
    char* sym;//symbol

    //function
    lbuiltin builtin;//存储是否为内建函数
    lenv* env;
    lval* foramls;
    lval* body;

    //count and pointer to a list of lval*
    int count;
    struct lval** cell;//not only one
};

struct lenv{
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};


char* ltype_name(int t){
    switch(t){
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

void lval_expr_print(lval*,char,char);
void lval_print(lval*);
lenv* lenv_new();
lenv* lenv_copy(lenv*);

/*
 * construct lval
*/
lval* lval_num(double x){
    lval* v=(lval* )malloc(sizeof(lval));
    v->type=LVAL_NUM;
    v->num=x;
    return v;
}

lval* lval_err(char* fmt,...){
    lval* v=(lval* )malloc(sizeof(lval));
    v->type=LVAL_ERR;

    va_list va;
    va_start(va,fmt);

    v->err=(char*)malloc(512);
    vsnprintf(v->err,511,fmt,va);
    v->err=(char*)realloc(v->err,strlen(v->err)+1);

    va_end(va);
    return v;
}

lval* lval_sym(char* s){
    lval* v=(lval* )malloc(sizeof(lval));
    v->type=LVAL_SYM;
    v->sym=(char* )malloc(strlen(s)+1);
    strcpy(v->sym,s);
    return v;
}

lval* lval_sexpr(){
    lval* v=(lval* )malloc(sizeof(lval));
    v->type=LVAL_SEXPR;
    v->count=0;
    v->cell=NULL;
    return v;
}

lval* lval_qexpr(){
    lval* v=(lval*)malloc(sizeof(lval));
    v->type=LVAL_QEXPR;
    v->count=0;
    v->cell=NULL;
    return v;
}

lval* lval_fun(lbuiltin func){
    lval *v=(lval*)malloc(sizeof(lval));
    v->type=LVAL_FUN;
    v->builtin=func;
    return v;
}

lval* lval_lambda(lval* foramls,lval* body){
    lval* v=(lval*)malloc(sizeof(lval));
    v->type=LVAL_FUN;
    v->builtin=NULL;
    v->env=lenv_new();
    v->foramls=foramls;
    v->body=body;
    return v;
}

/*
 * construct lenv
 */

lenv* lenv_new(){
    lenv* e=(lenv*)malloc(sizeof(lenv));
    e->par=NULL;
    e->count=0;
    e->syms=NULL;
    e->vals=NULL;
    return e;
}

/*
 * delete
 */
void lenv_del(lenv*);

void lval_del(lval* v){
    switch(v->type){
        case LVAL_FUN:
            if(!v->builtin){
                lenv_del(v->env);
                lval_del(v->foramls);
                lval_del(v->body);
            }
            break;
        case LVAL_NUM:break;
        case LVAL_ERR:free(v->err);break;
        case LVAL_SYM:free(v->sym);break;

        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for(int i=0;i<v->count;i++){
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    free(v);
}

void lenv_del(lenv* e){
    for(int i=0;i<e->count;i++){
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

//basic func
lval* lval_read_num(mpc_ast_t* t){
    errno=0;
    double x=strtod(t->contents,NULL);
    return errno!=ERANGE?
        lval_num(x):lval_err("invalid number");
}

lval* lval_add(lval* v,lval* x){
    v->count++;
    v->cell=(struct lval**)realloc(v->cell,sizeof(lval*)*v->count);
    v->cell[v->count-1]=x;
    return v;
}

lval* lval_read(mpc_ast_t* t){
    if(strstr(t->tag,"number")){return lval_read_num(t);}
    if(strstr(t->tag,"symbol")){return lval_sym(t->contents);}

    lval* x=NULL;
    //if root (>) or sexpr, create empty list
    if(strcmp(t->tag,">")==0)   {x=lval_sexpr();}
    if(strcmp(t->tag,"sexpr"))  {x=lval_sexpr();}
    if(strstr(t->tag,"qexpr"))  {x=lval_qexpr();}

    //fill the list with any valid expression ontained within
    for(int i=0;i<t->children_num;i++){
        char* content=t->children[i]->contents;
        if(strcmp(content,"(")==0){continue;}
        if(strcmp(content,")")==0){continue;}
        if(strcmp(content,"{")==0){continue;}
        if(strcmp(content,"}")==0){continue;}
        if(strcmp(t->children[i]->tag,"regex")==0){continue;}
        x=lval_add(x,lval_read(t->children[i]));
    }

    return x;
}

/*
 * 当从environment中放入或取出时候需要用到，注意的是
 * 对于number和func来说直接浅拷贝就行就行
 * 但是对于字符串来说需要额外处理
*/
lval* lval_copy(lval* v){
    lval* x=(lval*)malloc(sizeof(lval));
    x->type=v->type;

    switch(v->type){

        case LVAL_NUM:x->num=v->num;break;
        case LVAL_FUN:
            if(v->builtin){
                x->builtin=v->builtin;
            }else{
                x->builtin=NULL;
                x->env=lenv_copy(v->env);
                x->foramls=lval_copy(v->foramls);
                x->body=lval_copy(v->body);
            }
            break;

        case LVAL_ERR:
            x->err=(char*)malloc(strlen(v->err)+1);
            strcpy(x->err,v->err);
            break;

        case LVAL_SYM:
            x->sym=(char*)malloc(strlen(v->sym)+1);
            strcpy(x->sym,v->sym);
            break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count=v->count;
            x->cell=(lval**)malloc(sizeof(lval*)*x->count);
            for(int i=0;i<x->count;i++)
                x->cell[i]=lval_copy(v->cell[i]);
            break;
    }
    return x;
}

lenv* lenv_copy(lenv* e){
    lenv* n=(lenv*)malloc(sizeof(lenv));
    n->par=e->par;
    n->count=e->count;
    n->syms=(char**)malloc(sizeof(char*)*n->count);
    n->vals=(lval**)malloc(sizeof(lval*)*n->count);

    for(int i=0;i<e->count;i++){
        n->syms[i]=(char*)malloc(strlen(e->syms[i])+1);
        strcpy(n->syms[i],e->syms[i]);
        n->vals[i]=lval_copy(e->vals[i]);
    }

    return n;
}

//local
void lenv_put(lenv* e,lval* k,lval* v){

    for(int i=0;i<e->count;i++){
        //如果相同则被最新的v取替
        if(strcmp(e->syms[i],k->sym)==0){
            lval_del(e->vals[i]);
            e->vals[i]=lval_copy(v);
            return;
        }
    }

    //扩容
    e->count++;
    e->vals=(lval**)realloc(e->vals,sizeof(lval*)*e->count);
    e->syms=(char**)realloc(e->syms,sizeof(char*)*e->count);

    e->vals[e->count-1]=lval_copy(v);
    e->syms[e->count-1]=(char*)malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1],k->sym);
}

//global
void lenv_def(lenv* e,lval* k,lval* v){
    while(e->par){e=e->par;}
    lenv_put(e,k,v);
}


lval* lenv_get(lenv* e,lval* k){
    for(int i=0;i<e->count;i++){
        if(strcmp(e->syms[i],k->sym)==0)
            return lval_copy(e->vals[i]);
    }

    //向上查找作用域
    if(e->par){
        return lenv_get(e->par,k);
    }else{
        return lval_err("Unbound Symbol '%s'",k->sym);
    }
}

void lval_print(lval* v){
    switch(v->type){
        case LVAL_FUN:
            if(v->builtin){
                printf("<builtin>");
            }else{
                printf("(\\");
                lval_print(v->foramls);
                putchar(' ');
                lval_print(v->body);
                putchar(')');
            }
            break;
        case LVAL_NUM:printf("%f",v->num);break;
        case LVAL_ERR:printf("Error : %s",v->err);break;
        case LVAL_SYM:printf("%s",v->sym);break;
        case LVAL_SEXPR:lval_expr_print(v,'(',')');break;
        case LVAL_QEXPR:lval_expr_print(v,'{','}');break;
    }
}

void lval_expr_print(lval* v,char open,char close){
    putchar(open);
    for(int i=0;i<v->count;i++){
        lval_print(v->cell[i]);
        if(i!=(v->count-1)){
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_println(lval* v){
    lval_print(v);
    putchar('\n');
}

/*******lval*******/
lval* lval_eval(lenv*,lval*);
lval* lval_eval_sexpr(lenv*,lval*);
lval* lval_pop(lval*,int);//将操作的S表达式的第i个元素取出并补齐空缺，不删除表达式
lval* lval_take(lval*,int);//取出元素后将剩下的列表删除
//lval* builtin_op(lenv*,lval*,char*);
//lval* builtin(lval*,char*);
lval* builtin_eval(lenv*,lval*);
lval* lval_call(lenv*,lval*,lval*);

lval* lval_pop(lval* v,int i){
    lval* x=v->cell[i];
    memmove(&v->cell[i],&v->cell[i+1],
            sizeof(lval*)*(v->count-i-1));
    v->count--;
    v->cell=(lval**)realloc(v->cell,sizeof(lval*)*v->count);
    return x;
}

lval* lval_take(lval* v,int i){
    lval* x=lval_pop(v,i);
    lval_del(v);
    return x;
}


lval* lval_eval(lenv* e,lval* v){
    //evaluate sexpressions
    if(v->type==LVAL_SYM){
        lval* x=lenv_get(e,v);
        lval_del(v);
        return x;
    }
    if(v->type==LVAL_SEXPR){return lval_eval_sexpr(e,v);}
    //all other lval types reamin the same
    return v;
}

lval* lval_eval_sexpr(lenv* e,lval* v){


    //evaluate children
    for(int i=0;i<v->count;i++){
        v->cell[i]=lval_eval(e,v->cell[i]);
    }
    //error
    for(int i=0;i<v->count;i++){
        if(v->cell[i]->type==LVAL_ERR){
            return lval_take(v,i);
        }
    }

    //empty
    if(v->count==0){return v;}
    //single
    if(v->count==1){
        return lval_take(v,0);
    }

    //保证第一个元素是function
    lval* f=lval_pop(v,0);
    if(f->type!=LVAL_FUN){
        lval* err = lval_err(
            "S-Expression starts with incorrect type. "
            "Got %s, Expected %s.",
            ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f);
        lval_del(v);
        return err;
    }

    //call function to get result
    lval* result=lval_call(e,f,v);
    lval_del(f);
    return result;
}

lval* lval_call(lenv* e,lval* f,lval* a){
    if(f->builtin){return f->builtin(e,a);}

    int given=a->count;
    int total=f->foramls->count;

    //arguments还有
    while(a->count){
        if(f->foramls->count==0){
            lval_del(a);
            return lval_err(
            "Function passed too many arguments."
            "Got %i,Expected %i.",given,total);
        }

        lval* sym=lval_pop(f->foramls,0);
        lval* val=lval_pop(a,0);
        lenv_put(f->env,sym,val);//bind
        lval_del(sym);
        lval_del(val);
    }
    lval_del(a);
    //
    if(f->foramls->count==0){
        f->env->par=e;
        return builtin_eval(f->env,
                            lval_add(lval_sexpr(),lval_copy(f->body)));
    }else{
        return lval_copy(f);
    }
}

/*******Qexpr*******/
#define LASSERT(args,cond,fmt,...)\
    if(!(cond)){ \
        lval* err=lval_err(fmt,##__VA_ARGS__);\
        lval_del(args);\
        return err;\
    }

#define LASSERT_TYPE(func, args, index, expect) \
      LASSERT(args, args->cell[index]->type == expect, \
        "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
        func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
      LASSERT(args, args->count == num, \
        "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
        func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
      LASSERT(args, args->cell[index]->count != 0, \
        "Function '%s' passed {} for argument %i.", func, index);


lval* builtin_op(lenv* e,lval* a,char* op){
    for(int i=0;i<a->count;i++){
        LASSERT_TYPE(op, a, i, LVAL_NUM);
    }

    lval* x=lval_pop(a,0);
    if((strcmp(op,"-")==0)&&a->count==0){
        x->num=-x->num;
    }

    while(a->count>0){
        lval* y=lval_pop(a,0);
        if(strcmp(op,"+")==0){x->num+=y->num;}
        if(strcmp(op,"-")==0){x->num-=y->num;}
        if(strcmp(op,"*")==0){x->num*=y->num;}
        if(strcmp(op,"/")==0){
            if(y->num==0){
                lval_del(x);
                lval_del(y);
                x=lval_err("Division by zero");
                break;
            }
            x->num/=y->num;
        }
        lval_del(y);
    }
    lval_del(a);
    return x;
}

lval* builtin_head(lenv* e,lval* a){

    LASSERT_NUM("head", a, 1);
    LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("head", a, 0);

    lval* v=lval_take(a,0);
    while(v->count>1){
        lval_del(lval_pop(v,1));
    }
    return v;
}

lval* builtin_tail(lenv* e,lval* a){

    LASSERT_NUM("tail", a, 1);
    LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY("tail", a, 0);

    lval* v=lval_take(a,0);
    lval_del(lval_pop(v,0));
    return v;
}

lval* builtin_list(lenv* e,lval* a){
    a->type=LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e,lval* a){

    LASSERT_NUM("eval", a, 1);
    LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

    lval* x=lval_take(a,0);
    x->type=LVAL_SEXPR;
    return lval_eval(e,x);
}


lval* lval_join(lval* x,lval* y){
    for(int i=0;i<y->count;i++){
        x=lval_add(x,y->cell[i]);
    }
    free(y->cell);
    free(y);
    return x;
}

lval* builtin_join(lenv* e,lval* a){
    for(int i=0;i<a->count;i++){
        LASSERT_TYPE("join", a, i, LVAL_QEXPR);
    }

    lval* x=lval_pop(a,0);
    while(a->count){
        lval* y=lval_pop(a,0);
        x=lval_join(x,y);
    }
    lval_del(a);
    return x;
}

/*
lval* builtin(lval* a,char* func){
    if(strcmp("list",func)==0){return builtin_list(a);}
    if(strcmp("head",func)==0){return builtin_head(a);}
    if(strcmp("tail",func)==0){return builtin_tail(a);}
    if(strcmp("join",func)==0){return builtin_join(a);}
    if(strcmp("eval",func)==0){return builtin_eval(a);}
    if(strstr("+-*",func)){return builtin_op(a,func);}
    lval_del(a);
    return lval_err("Unknown Function");
}

int number_of_nodes(mpc_ast_t* t){
    if(t->children_num == 0)
        return 1;
    int total=1;
    for(int i=0;i<t->children_num;i++){
        total+=number_of_nodes(t->children[i]);
    }
    return total;
}
*/

lval* builtin_add(lenv* e,lval* a){
    return builtin_op(e,a,"+");
}


lval* builtin_sub(lenv* e,lval* a){
    return builtin_op(e,a,"-");
}


lval* builtin_div(lenv* e,lval* a){
    return builtin_op(e,a,"/");
}

lval* builtin_mul(lenv* e,lval* a){
    return builtin_op(e,a,"*");
}

void lenv_add_builtin(lenv* e,char* name,lbuiltin func){
    lval* k=lval_sym(name);
    lval* v=lval_fun(func);
    lenv_put(e,k,v);
    lval_del(k);
    lval_del(v);
}

lval* builtin_var(lenv* e,lval* a,char* func){
    LASSERT_TYPE(func,a,0,LVAL_QEXPR);
    lval* syms = a->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
                "Function '%s' cannot define non-symbol. "
                "Got %s, Expected %s.", func,
                ltype_name(syms->cell[i]->type),
                ltype_name(LVAL_SYM)
                )
    }

    LASSERT(a, (syms->count == a->count-1),
        "Function '%s' passed too many arguments for symbols. "
        "Got %i, Expected %i.", func, syms->count, a->count-1);

    for(int i=0;i<syms->count;i++){
        if(strcmp(func,"def")==0){
            lenv_def(e,syms->cell[i],a->cell[i+1]);
        }
        if(strcmp(func,"=")==0){
            lenv_put(e,syms->cell[i],a->cell[i+1]);
        }
    }
    lval_del(a);
    return lval_sexpr();
}

lval* builtin_def(lenv* e,lval* a){
    return builtin_var(e,a,"def");
}

lval* builtin_put(lenv* e,lval* a){
    return builtin_var(e,a,"=");
}

lval* builtin_lambda(lenv* e,lval* a){
      /* Check Two arguments, each of which are Q-Expressions */
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    for(int i=0;i<a->cell[0]->count;i++){
        LASSERT(a,(a->cell[0]->cell[i]->type==LVAL_SYM),
                "Can't define non-symbol. Got %s, Expected %s.",
                ltype_name(a->cell[0]->cell[i]->type),ltype_name(LVAL_SYM));
    }
    lval* foramls=lval_pop(a,0);
    lval* body=lval_pop(a,0);
    lval_del(a);

    return lval_lambda(foramls,body);
}

void lenv_add_builtins(lenv* e){

    lenv_add_builtin(e,"\\",builtin_lambda);

    /*list functions*/
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e,"def",builtin_def);
    lenv_add_builtin(e,"=",builtin_put);
}


int main(int argc,char* argv[]){
    /*
     * 添加新特性的一个典型方式：
     * 语法：为新特性添加新的语法规则
     * 表示：为新特性添加新的数据类型
     * 解析：为新特性添加新的函数，处理AST
     * 语义：为新特性添加新的函数，用于求值和操作
     */
    mpc_parser_t* Int       = mpc_new("int");
    mpc_parser_t* Float     = mpc_new("float");
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Symbol    = mpc_new("symbol");
    mpc_parser_t* Sexpr     = mpc_new("sexpr");//
    mpc_parser_t* Qexpr     = mpc_new("qexpr");//Q表达式不会去求值，而是保持原有，存储在{}中
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispy     = mpc_new("lispy");//规则的描述
    //在解析number的时候是存在着先后顺序的 即如果把int
    //提到前面的话则会存在int与float解析为字串的关系，即满足float一定满足int
    //float不会被解析到

    /*
     * 语法规则：
     * list:接受一个或者多个参数，返回第一个包含所有参数的Q-表达式
     * head:接受一个Q-表达式，返回一个包含其第一个元素的Q-表达式(car)
     * tail:接受一个Q-表达式，返回一个除首元素外的Q-表达式(cdr)
     * join:接受一个或者多个Q-表达式，返回一个将其连在一起的Q-表达式
     * eval:接受一个Q-表达式，将其看做一个S-表达式并运算
     */
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                             \
                int         : /-?[0-9]+/;                                   \
                float       : /-?[0-9]+[.][0-9]+/;                          \
                number      : <float> | <int>    ;                          \
                symbol      : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;             \
                sexpr       : '(' <expr>* ')';                              \
                qexpr       : '{' <expr>* '}';                              \
                expr        : <number> |  <symbol> | <sexpr> | <qexpr>;     \
                lispy       : /^/ <expr>* /$/;                              \
              ",
              Number,Int,Float,Symbol,Sexpr,Qexpr,Expr,Lispy);
    /****语法规则的描述******/
    puts("Lispy Version 0.0.0.1.0");
    puts("press Ctrl+c to Exit\n");

    lenv* e=lenv_new();
    lenv_add_builtins(e);

    while(1){
        char* input=readline("clisp> ");
        add_history(input);
        //解析器
        mpc_result_t r;
        if(mpc_parse("<stdin>",input,Lispy,&r)){

            lval*x =lval_eval(e,lval_read(r.output));
            lval_println(x);
            //mpc_ast_print(r.output);
            lval_del(x);

            mpc_ast_delete(r.output);
        }else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }
    lenv_del(e);
    mpc_cleanup(8,Int,Float,Number,Symbol,Sexpr,Qexpr,Expr,Lispy);
    return 0;
}

