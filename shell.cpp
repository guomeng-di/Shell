#include <bits/stdc++.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <unistd.h>
#include <sys/stat.h>
using namespace std;
string old_pwd;//上一级
string curr_pwd;//当前目录
void setup_signal_handling(){//屏蔽 Ctrl+C
    signal(SIGINT,SIG_IGN);  
}
void update_pwd(){//获取当前工作目录
    char buf[1024];
    if((getcwd(buf,sizeof(buf)))!=NULL) 
    curr_pwd=buf;
}
void init_shell(){//初始化shell环境
//1.获取当前工作目录
   update_pwd();
//2.不产生僵尸进程
   signal(SIGCHLD,SIG_IGN);
}
void print_prompt(){
//打印提示信息xxx@xxx ~ $
    printf("%s@%s %s ~$ ", getenv("USER"),getenv("USER"),curr_pwd.c_str());
    cout.flush();
    return ;
}
bool read_command(string& s){//判断输入
    if(!getline(cin,s)) return 0;
    return 1;
}
vector<string> trim_input(string& s){
//清理输入字符串两端的多余空白字符
    //1去掉字符串两端空白
    size_t start=s.find_first_not_of(" \t");
    size_t end=s.find_last_not_of(" \t");
    vector<string>s1;
    if(start==string::npos||end==string::npos)
       return s1;
    s=s.substr(start,end-start+1);//从字符串截取子字符串 substr(起始下标,长度)
    
    //2按空格分割字符串
    string temp;
    for(char c:s){
        if(c==' '||c=='\t'){
            if(!temp.empty()){
                s1.push_back(temp);
                temp.clear();
            }
        }else{
            temp+=c;
        }
    }
    if(!temp.empty()){
        s1.push_back(temp);
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
                while(i+1<a.size()&&(a[i+1]==' '||a[i+1]=='\t')) i++;
            }
        }else
            temp.push_back(a[i]);
    }if(!temp.empty()) result.push_back(trim_input(temp));
    return result;
}
void execute_pipeline(vector<vector<string>> &a){//dup2改变输入输出的目的地
    int n=a.size();
    int prev_fd = -1; //上一个管道读端
    int pipe_fd[2];
    for (int i=0;i<n;i++) {
        if (i<n-1) pipe(pipe_fd); //不是最后一个命令就创建管道
        pid_t pid = fork();
        if (pid==0){ //子进程
            //stdin
            if (prev_fd!= -1) dup2(prev_fd, STDIN_FILENO);//输入是上一个的输出
            //如果不是第一条命令(prev_fd!=-1），当前命令的输入就不是终端 而是上一个管道的输出
            //stdout
            if (i<n-1) dup2(pipe_fd[1], STDOUT_FILENO);//输出作为下一个的输入
            //如果不是最后一条命令，当前命令的输出要重定向到管道的写端，供下一个命令读取。
            else{
                //最后一条命令，检测 > >>
                for(size_t j=0;j<a[i].size();j++){
                    if(a[i][j]==">"||a[i][j]==">>"){
                        int fd;
                        if(a[i][j]==">") 
                            fd = open(a[i][j+1].c_str(),O_WRONLY|O_CREAT|O_TRUNC, 0644);
                        else
                            fd = open(a[i][j+1].c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
                        if(fd<0){ perror("open"); exit(1);}
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                        a[i].resize(j); //去掉重定向符号和文件名
                        break;
                    }
                }
            }
            //关闭管道
            if (i<n-1) close(pipe_fd[1]);
            if (prev_fd != -1) close(prev_fd);
            //execvp:执行系统命令
            vector<const char*> args;
            for(auto &arg: a[i]) args.push_back(arg.c_str());
            args.push_back(nullptr);
            execvp(args[0],(char**)args.data());
            exit(0);
        }
        //父进程
        if(prev_fd != -1) close(prev_fd);
        if(i < n-1) close(pipe_fd[1]);
        prev_fd=(i<n-1)?pipe_fd[0]:-1;
    }
    //等待所有子进程
    for(int i=0;i<n;i++) wait(nullptr);
}
void handle_redirect(vector<string>& a,/*const string& file,*/int mode){
    int fd;
    string filename;
        for(size_t i=0;i<a.size();i++) {
        if(a[i]=="<") {filename=a[i+1];a.resize(i);break;}
        else if(a[i]==">") {filename=a[i+1];a.resize(i);break;}
        else if(a[i]==">>") {filename=a[i+1];a.resize(i);break;}
    }
     pid_t pid = fork();
     if(pid==0){
    if(mode==1){//输入重定向 <
        fd=open(filename.c_str(),O_RDONLY);
        if(fd==-1){
            perror("error");
            return ;
        }
        dup2(fd,STDIN_FILENO);
    }else if(mode==2){//输出重定向 >
        fd=open(filename.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
        if(fd==-1){
            perror("error");
            return ;
        }
        dup2(fd,STDOUT_FILENO);
    }else if(mode==3){//输出重定向追加 >>
        fd = open(filename.c_str(),O_WRONLY|O_CREAT|O_APPEND,0644);
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
    exit(0);}else{
        waitpid(pid,nullptr,0);
    }
}
void handle_cd(vector<string>& a){
    string target_dir;
    if (a.size()<2) {
        target_dir=getenv("HOME");
    }else{
        target_dir=a[1];//先决定target_dir
        if (target_dir=="-") {
            // 处理 `cd -`
            if (!old_pwd.empty()) {
                cout<<old_pwd<<endl;
                target_dir=old_pwd;
            }
    }
} char curr_buf[1024];
    getcwd(curr_buf,sizeof(curr_buf));//当前目录curr
    //string temp_old=curr_buf;
    if(chdir(target_dir.c_str())==-1) return ;
    update_pwd();
    //old_pwd=temp_old;
    old_pwd=curr_buf;
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
void execute_command(vector<string>& a){
    pid_t pid=fork();
    if(pid==0){
        vector<const char*> args;
        for(auto& arg:a) args.push_back(arg.c_str());
        args.push_back(nullptr);
        execvp(args[0],(char**)args.data());
        exit(0);
    }else{
        waitpid(pid,nullptr,0);
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
        if(a.size()>1){//处理管道
                execute_pipeline(a);
        }else{//处理单一命令
            vector<string> a0=trim_input(s);
            if(a0[0]=="cd") handle_cd(a0);
            else if(a0.back()=="&") handle_background(a0);
            else if(s.find("<")!=string::npos)
                  handle_redirect(a0/*,a0[2]*/,1);
            else if(s.find(">")!=string::npos)
                  handle_redirect(a0/*,a0[2]*/,2);
            else if(s.find(">>")!=string::npos)
                  handle_redirect(a0/*,a0[2]*/,3);
            else execute_command(a0); 
        }
    }
    return 0;
}
