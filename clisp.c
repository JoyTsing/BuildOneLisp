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

int number_of_nodes(mpc_ast_t* t);
void printAstNode(mpc_ast_t* a);
double eval(mpc_ast_t* t);
double eval_op(double x,char* op,double y);

int main(int argc,char* argv[])
{
    mpc_parser_t* Int       = mpc_new("int");
    mpc_parser_t* Float     = mpc_new("float");
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Operator  = mpc_new("operator");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispy     = mpc_new("lispy");//规则的描述
    //在解析number的时候是存在着先后顺序的 即如果把int
    //提到前面的话则会存在int与float解析为字串的关系，即满足float一定满足int
    //float不会被解析到
    mpca_lang(MPCA_LANG_DEFAULT,
              "                               \
                int         : /-?[0-9]+/;                               \
                float       : /-?[0-9]+[.][0-9]+/;                      \
                number      : <float> | <int>    ;                      \
                operator    : '+' | '-' | '*' | '/' ;                   \
                expr        : <number> | '(' <operator> <expr>+ ')';    \
                lispy       : /^/ <operator> <expr>+ /$/;               \
              ",
              Number,Int,Float,Operator,Expr,Lispy);
    /****语法规则的描述******/
    puts("Lispy Version 0.0.0.0.3");
    puts("press Ctrl+c to Exit\n");
    while(1){
        char* input=readline("clisp> ");        
        add_history(input);
        //解析器
        mpc_result_t r;
        if(mpc_parse("<stdin>",input,Lispy,&r)){
            
            double result=eval(r.output);
            printf("%f\n",result);
            //mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        }else{
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        //
        free(input);
    }
    mpc_cleanup(6,Int,Float,Number,Operator,Expr,Lispy);
    return 0;
}

double eval(mpc_ast_t* t){
    if(strstr(t->tag,"number")){
        return atof(t->contents);
    }

    //表达式
    char* op=t->children[1]->contents;
    double ret=eval(t->children[2]);
    int i=3;
    while(strstr(t->children[i]->tag,"expr")){
        ret=eval_op(ret,op,eval(t->children[i]));
        i++;
    }
    return ret;
}


double eval_op(double x,char* op,double y){
    if(strcmp(op,"+")==0){return x+y;}
    if(strcmp(op,"-")==0){return x-y;}
    if(strcmp(op,"*")==0){return x*y;}
    if(strcmp(op,"/")==0){return x/y;}
    return 0;
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
