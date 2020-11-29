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
    LVAL_SEXPR
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

//delete
void lval_del(lval* v){
    switch(v->type){
        case LVAL_NUM:break;
        case LVAL_ERR:free(v->err);break;
        case LVAL_SYM:free(v->sym);break;
        case LVAL_SEXPR:
            for(int i=0;i<v->count;i++){
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    free(v);
}


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

/*
lval eval_op(lval x,char* op,lval y){
    if(strcmp(op,"+")==0){return lval_num(x.num+y.num);}
    if(strcmp(op,"-")==0){return lval_num(x.num-y.num);}
    if(strcmp(op,"*")==0){return lval_num(x.num*y.num);}
    if(strcmp(op,"/")==0){
        return y.num==0?lval_err(LERR_DIV_ZERO):lval_num(x.num/y.num);
    }
    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t){
    if(strstr(t->tag,"number")){
        errno=0;
        double x=strtod(t->contents,NULL);
        return errno!=ERANGE ? lval_num(x):lval_err(LERR_BAD_NUM);
    }

    //表达式
    char* op=t->children[1]->contents;
    lval ret=eval(t->children[2]);
    int i=3;
    while(strstr(t->children[i]->tag,"expr")){
        ret=eval_op(ret,op,eval(t->children[i]));
        i++;
    }
    return ret;
}
*/

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
    mpc_parser_t* Int       = mpc_new("int");
    mpc_parser_t* Float     = mpc_new("float");
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Symbol    = mpc_new("symbol");
    mpc_parser_t* Sexpr     = mpc_new("sexpr");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispy     = mpc_new("lispy");//规则的描述
    //在解析number的时候是存在着先后顺序的 即如果把int
    //提到前面的话则会存在int与float解析为字串的关系，即满足float一定满足int
    //float不会被解析到
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                         \
                int         : /-?[0-9]+/;                               \
                float       : /-?[0-9]+[.][0-9]+/;                      \
                number      : <float> | <int>    ;                      \
                symbol      : '+' | '-' | '*' | '/' ;                   \
                sexpr       : '(' <expr>* ')';                          \
                expr        : <number> |  <symbol> | <sexpr> ;          \
                lispy       : /^/ <expr>* /$/;                          \
              ",
              Number,Int,Float,Symbol,Sexpr,Expr,Lispy);
    /****语法规则的描述******/
    puts("Lispy Version 0.0.0.0.4");
    puts("press Ctrl+c to Exit\n");
    while(1){
        char* input=readline("clisp> ");
        add_history(input);
        //解析器
        mpc_result_t r;
        if(mpc_parse("<stdin>",input,Lispy,&r)){

            lval*x =lval_read(r.output);
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
    mpc_cleanup(7,Int,Float,Number,Symbol,Sexpr,Expr,Lispy);
    return 0;
}

