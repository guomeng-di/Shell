#include <bits/stdc++.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/stat.h>
using namespace std;
string old_pwd;
string curr_pwd;
void setup_signal_handling(){//屏蔽 Ctrl+C
    signal(SIGINT,SIG_IGN);  
}
void update_pwd(){//获取当前工作目录
    char buf[1024];
    if((getcwd(buf,sizeof(buf)))!=NULL) 
       curr_pwd=buf;
    old_pwd=curr_pwd;
}
void init_shell(){//初始化shell环境
//1.获取当前工作目录
   update_pwd();
//2.不产生僵尸进程
   signal(SIGCHLD,SIG_IGN);
}
void print_prompt(){
//打印提示信息xxx@xxx ~ $
    printf("%s@%s ~$",getenv("USER"),getenv("USER"));
    cout.flush();
    return ;
}
bool read_command(string& s){//判断输入
    if(!getline(cin,s)) return 0;
    return 1;
}
vector<string> trim_input(string& s){
//清理输入字符串两端的多余空白字符
    size_t start=s.find_first_not_of("\t");
    size_t end=s.find_last_not_of("\t");
    vector<string>s1;
    if(start!=string::npos&&end!=string::npos){
    for(int i=start;i<=end;i++)
        s1.push_back(string(1, s[i]));
    }
    return s1;
}
vector<vector<string>> split_pipe(string& a){
//按管道符 | 分割字符串向量
    vector<vector<string>>result;
    string temp;
    for(int i=0;i<a.size();i++){
        if(a[i]=='|'){
            if(!temp.empty()){
                result.push_back(trim_input(temp));
                temp.clear();
            }
        }else
            temp.push_back(a[i]);
    }if(!temp.empty()) result.push_back(trim_input(temp));
    return result;
}
vector<string> split_command(string& s){
//将输入命令按空格分割成多个部分
    vector<string>s1;
    string temp;
    for(int i=0;i<s.size();i++){ 
        if(s[i]!=' ')
            temp.push_back(s[i]);
        else if (!temp.empty()){
        s1.push_back(temp);
        temp.clear();
    }
}
    if (!temp.empty())
        s1.push_back(temp);
    return s1;
}
void execute_command_with_pipe(vector<string>& a,vector<string>& b){
//通过管道将子进程的输出连接到下一个子进程的输入
    int pipe_fd[2];
    pipe(pipe_fd);
    pid_t pid1=fork();
    if(pid1==0){
        close(pipe_fd[0]);
        dup2(pipe_fd[1],STDOUT_FILENO);
        close(pipe_fd[1]);
        vector<const char*>args1;
        for (auto& arg :a) args1.push_back(arg.c_str());
        args1.push_back(nullptr);
        execvp(args1[0],(char**)args1.data());
        exit(0);
    }pid_t pid2=fork();
    if(pid2==0){
        close(pipe_fd[1]);
        dup2(pipe_fd[0],STDIN_FILENO);
        close(pipe_fd[0]);
        vector<const char*>args2;
        for (auto& arg :b) args2.push_back(arg.c_str());
        args2.push_back(nullptr);
        execvp(args2[0],(char**)args2.data());
        exit(0);
    }
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    waitpid(pid1,nullptr,0);
    waitpid(pid2,nullptr,0);
}
void handle_redirect(vector<string>&a,const string& file,int mode){
    int fd;
    if(mode==1){//输入重定向 <
        fd=open(file.c_str(),O_RDONLY);
        if(fd==-1){
            perror("error");
            return ;
        }
        dup2(fd,STDIN_FILENO);
    }else if(mode==2){//输出重定向 >
        fd=open(file.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
        if(fd==-1){
            perror("error");
            return ;
        }
        dup2(fd,STDOUT_FILENO);
    }else if(mode==3){//输出重定向追加 >>
        fd = open(file.c_str(),O_WRONLY|O_CREAT|O_APPEND,0644);
        if(fd==-1){
            perror("error");
            return ;
        } 
        dup2(fd,STDOUT_FILENO);        
    }close(fd);
    vector<const char*> args;
    for (auto& arg :a) args.push_back(arg.c_str());
    args.push_back(nullptr);
    execvp(args[0], (char**)args.data());
    exit(0);
}
void handle_cd(vector<string>& a){
    if (a.size()<2) {
        chdir(getenv("HOME"));
    }else{
        string path=a[1];
        if (path=="-") {
            // 处理 `cd -`
            if (!old_pwd.empty()) {
                chdir(old_pwd.c_str());
            }
        }else
            chdir(path.c_str());
    }
}
void handle_background(vector<string>& a) {
    if (a.back()=="&") {
        a.pop_back(); // 删除 `&`
        if (fork()==0) {
            vector<const char*> args;
            for (auto& arg : a) args.push_back(arg.c_str());
            args.push_back(nullptr);
            execvp(args[0],(char**)args.data());
            exit(0);
        }
    } else {
        vector<const char*> args;
        for (auto& arg : a) args.push_back(arg.c_str());
        args.push_back(nullptr);
        execvp(args[0], (char**)args.data());
    }
}
int main(){
    setup_signal_handling();
    while(1){
        init_shell();//初始化shell环境
        print_prompt();//输出提示符
        string s; 
        if(!read_command(s)||s=="exit")  break;//判断输入
        vector<vector<string>> a=split_pipe(s);
        //把整条命令按 | 分割成多个子命令
        bool background=0;
        if(a.size()>1){
            for(int i=0;i<a.size()-1;i++)//处理管道
                execute_command_with_pipe(a[i],a[i+1]);
        }else{//处理单一命令
            vector<string> a0=split_command(s);
            if(a0[0]=="cd") handle_cd(a0);
            else if(a0.back()=="&") handle_background(a0);
            else{
                if(s.find("<")!=string::npos)
                  handle_redirect(a0,a0[2],1);
                else if(s.find(">")!=string::npos)
                  handle_redirect(a0,a0[2],2);
                else if(s.find(">>")!=string::npos)
                  handle_redirect(a0,a0[2],3);
            }
        }
    }
    return 0;
}