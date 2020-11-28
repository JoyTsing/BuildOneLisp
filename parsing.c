#include <stdio.h>
#include<stdlib.h>
#include "./mpc.h"
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

int main(int argc,char* argv[])
{
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Operator  = mpc_new("operator");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispy     = mpc_new("lispy");//规则的描述
    
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                                     \
                number   : /-?[0-9]+/ ;                             \
                operator : '+' | '-' | '*' | '/';                   \
                expr     : <number> | '(' <operator> <expr>+ ')';   \
                lispy    : /^/ <operator> <expr>| /$/;              \
              ",
              Number,Operator,Expr,Lispy);
    /****语法规则的描述******/
    puts("Lispy Version 0.0.0.0.1");
    puts("press Ctrl+c to Exit\n");
    while(1){
        char* input=readline("clisp> ");        
        add_history(input);
        printf("%s\n",input);
        free(input);
    }
    return 0;
}

