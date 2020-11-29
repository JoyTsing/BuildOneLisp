#include <stdio.h>
#include <stdlib.h>
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


//值类型
enum{
    LVAL_NUM,
    LVAL_ERR,
    //S-expr
    LVAL_SYM,//symbol
    LVAL_SEXPR,
    LVAL_QEXPR
};

//错误类型
enum{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

typedef struct lval {
    int type;//0:num,1:err
    double num;
    //types string message
    char* err;
    char* sym;
    //count and pointer to a list of lval*
    int count;
    struct lval** cell;//not only one
}lval;

void lval_expr_print(lval*,char,char);
void lval_print(lval*);

//construct
lval* lval_num(double x){
    lval* v=(lval* )malloc(sizeof(lval));
    v->type=LVAL_NUM;
    v->num=x;
    return v;
}

lval* lval_err(char* m){
    lval* v=(lval* )malloc(sizeof(lval));
    v->type=LVAL_ERR;
    v->err=(char* )malloc(strlen(m)+1);
    strcpy(v->err,m);
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

//delete
void lval_del(lval* v){
    switch(v->type){
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

//
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


void lval_print(lval* v){
    switch(v->type){
        case LVAL_NUM:printf("%f",v->num);break;
        case LVAL_ERR:printf("Error :%s",v->err);break;
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

/*******Sexpr*******/
lval* lval_eval(lval*);
lval* lval_eval_sexpr(lval*);
lval* lval_pop(lval*,int);//将操作的S表达式的第i个元素取出并补齐空缺，不删除表达式
lval* lval_take(lval*,int);//取出元素后将剩下的列表删除
lval* builtin_op(lval*,char*);
lval* builtin(lval*,char*);

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


lval* lval_eval(lval* v){
    //evaluate sexpressions
    if(v->type==LVAL_SEXPR){return lval_eval_sexpr(v);}
    //all other lval types reamin the same
    return v;
}

lval* lval_eval_sexpr(lval* v){
    //empty
    if(v->count==0){return v;}
    //single
    if(v->count==1){
        return lval_take(v,0);
    }

    //evaluate children
    for(int i=0;i<v->count;i++){
        v->cell[i]=lval_eval(v->cell[i]);
    }
    //error
    for(int i=0;i<v->count;i++){
        if(v->cell[i]->type==LVAL_ERR){return lval_take(v,i);}
    }

    //保证第一个元素是symbol
    lval* f=lval_pop(v,0);
    if(f->type!=LVAL_SYM){
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with symbol");
    }

    //call builtin with operator
    lval* result=builtin(v,f->sym);
    lval_del(f);
    return result;
}

/*******Qexpr*******/
#define LASSERT(args,cond,err)\
    if(!(cond)){ lval_del(args);return lval_err(err); }

lval* builtin_op(lval* a,char* op){
    for(int i=0;i<a->count;i++){
        if(a->cell[i]->type!=LVAL_NUM){
            lval_del(a);
            return lval_err("can't operator on non-number");
        }
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

lval* builtin_head(lval* a){
    LASSERT(a,a->count==1,
        "Function 'head' passed wrong arguments");

    LASSERT(a,a->cell[0]->type==LVAL_QEXPR,
        "Function 'head' passed incorrect types");

    LASSERT(a,a->cell[0]->count!=0,
        "Function 'head' passed {}");

    lval* v=lval_take(a,0);
    while(v->count>1){
        lval_del(lval_pop(v,1));
    }
    return v;
}

lval* builtin_tail(lval* a){
    LASSERT(a,a->count==1,
        "Function 'tail' passed wrong arguments");

    LASSERT(a,a->cell[0]->type==LVAL_QEXPR,
        "Function 'tail' passed incorrect types");

    LASSERT(a,a->cell[0]->count!=0,
        "Function 'tail' passed {}");

    lval* v=lval_take(a,0);
    lval_del(lval_pop(v,0));
    return v;
}

lval* builtin_list(lval* a){
    a->type=LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lval* a){
    LASSERT(a,a->count==1,
            "Function 'eval' passed worng arguments");
    LASSERT(a,a->cell[0]->type==LVAL_QEXPR,
            "Function 'eval' passed incorrect type");

    lval* x=lval_take(a,0);
    x->type=LVAL_SEXPR;
    return lval_eval(x);
}


lval* lval_join(lval* x,lval* y){
    while(y->count){
        x=lval_add(x,lval_pop(y,0));
    }
    lval_del(y);
    return x;
}

lval* builtin_join(lval* a){
    for(int i=0;i<a->count;i++){
        LASSERT(a,a->cell[i]->type==LVAL_QEXPR,
                "Function 'join' passed incorrect type");
    }

    lval* x=lval_pop(a,0);
    while(a->count){
        x=lval_join(x,lval_pop(a,0));
    }
    lval_del(a);
    return x;
}

lval* builtin(lval* a,char* func){
    if(strcmp("list",func)==0){return builtin_list(a);}
    if(strcmp("head",func)==0){return builtin_head(a);}
    if(strcmp("tail",func)==0){return builtin_tail(a);}
    if(strcmp("join",func)==0){return builtin_join(a);}
    if(strcmp("eval",func)==0){return builtin_eval(a);}
    if(strcmp("+-/*",func)==0){return builtin_op(a,func);}
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

int main(int argc,char* argv[])
{
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
                symbol      : \"list\" | \"head\" |\"tail\"                 \
                            | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ; \
                sexpr       : '(' <expr>* ')';                              \
                qexpr       : '{' <expr>* '}';                              \
                expr        : <number> |  <symbol> | <sexpr> | <qexpr>;     \
                lispy       : /^/ <expr>* /$/;                              \
              ",
              Number,Int,Float,Symbol,Sexpr,Qexpr,Expr,Lispy);
    /****语法规则的描述******/
    puts("Lispy Version 0.0.0.1.0");
    puts("press Ctrl+c to Exit\n");
    while(1){
        char* input=readline("clisp> ");
        add_history(input);
        //解析器
        mpc_result_t r;
        if(mpc_parse("<stdin>",input,Lispy,&r)){

            lval*x =lval_eval(lval_read(r.output));
            lval_println(x);
            //mpc_ast_print(r.output);
            lval_del(x);
        }else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        //
        free(input);
    }
    mpc_cleanup(8,Int,Float,Number,Symbol,Sexpr,Qexpr,Expr,Lispy);
    return 0;
}

