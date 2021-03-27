/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
/*第487行，第356行，第168行
*/
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>

typedef unsigned short u_short;

#define ISspace(x) isspace((int)(x))
//isspace如果 c 是一个空白字符，则该函数返回非零值（true），否则返回 0（false）。
//使用define代替的时候，为了避免出问题尽量都加括号

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);
void bad_request(int);
void cat(int, FILE *);
void cannot_execute(int);
void error_die(const char *);
void execute_cgi(int, const char *, const char *, const char *);
int get_line(int, char *, int);
void headers(int, const char *);
void not_found(int);
void serve_file(int, const char *);
int startup(u_short *);
void unimplemented(int);

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client)
{
  char buf[1024];
  int numchars;
  char method[255];
  char url[255];
  char path[512];
  size_t i, j;
  struct stat st;
  /*
  int stat(
　　const char *filename    //文件或者文件夹的路径
　　, struct stat *buf      //获取的信息保存在内存中
); 也就是通过提供文件描述符获取文件属性 
stat结构体的元素
struct stat {
        mode_t     st_mode;       //文件对应的模式，文件，目录等
        ino_t      st_ino;        //inode节点号
        dev_t      st_dev;        //设备号码
        dev_t      st_rdev;       //特殊设备号码
        nlink_t    st_nlink;      //文件的连接数
        uid_t      st_uid;        //文件所有者
        gid_t      st_gid;        //文件所有者对应的组
        off_t      st_size;       //普通文件，对应的文件字节数
        time_t     st_atime;      //文件最后被访问的时间
        time_t     st_mtime;      //文件内容最后被修改的时间
        time_t     st_ctime;      //文件状态改变时间
        blksize_t  st_blksize;    //文件内容对应的块大小
        blkcnt_t   st_blocks;     //伟建内容对应的块数量
      };   
  */
  int cgi = 0; /* becomes true if server decides this is a CGI
                    * program */
  char *query_string = NULL;

  numchars = get_line(client, buf, sizeof(buf));
  //读取一行http请求到buf中
  i = 0;
  j = 0;
  //复制buf到method中，但是不能超过method大小255个
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
  {
    method[i] = buf[j];
    i++;
    j++;
  }
  method[i] = '\0';//给method增加一个结尾

  //strcasecmp函数忽略大小写的比较字符串函数。
  //若参数s1和s2字符串相等则返回0。s1大于s2则返回大于0 的值，s1 小于s2 则返回小于0的值
  //此函数只在Linux中提供，相当于windows平台的 stricmp。
  //如果既不是get也不是post，服务器无法处理
  if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
  {
    unimplemented(client);
    return;
  }


  //如果是post，激活cgi
  if (strcasecmp(method, "POST") == 0)
    cgi = 1;
  
  //不管get还是post，都要进行读取url地址
  i = 0;//处理空字符
  while (ISspace(buf[j]) && (j < sizeof(buf)))
    j++;
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
  {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';

  //如果是get可能需要把url划分为两段
  //get请求可能带有？表示查询参数
  if (strcasecmp(method, "GET") == 0)
  {
    query_string = url;
    while ((*query_string != '?') && (*query_string != '\0'))
      query_string++;
    if (*query_string == '?')
    {//如果有？说明需要激活cgi
      cgi = 1;//需要执行cgi，解析参数，设置标志位为1
      *query_string = '\0';//这里变成url结尾
      query_string++;//query_string作为剩下的起点
    }
  }

  sprintf(path, "htdocs%s", url);//把url添加到htdocs后赋给path
  if (path[strlen(path) - 1] == '/')//如果是以/结尾，需要把index.html添加到后面
    //以/结尾，说明path只是一个目录，此时需要设置为首页index.html
    strcat(path, "index.html");//strcat连接两个字符串
  if (stat(path, &st) == -1)//执行成功则返回0，失败返回-1
  {//如果不存在，那么读取剩下的请求行内容丢弃
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
    not_found(client);//调用错误代码404
  }
  else
  {
    /*掩码
      #define _S_IFMT 0xF000
      #define _S_IFDIR 0x4000
      #define _S_IFCHR 0x2000
      #define _S_IFIFO 0x1000
      #define _S_IFREG 0x8000
      #define _S_IREAD 0x0100
      #define _S_IWRITE 0x0080
      #define _S_IEXEC 0x0040
    */
    if ((st.st_mode & S_IFMT) == S_IFDIR)//mode_t     st_mode;表示文件对应的模式，文件，目录等，做与运算能得到结果
      strcat(path, "/index.html");//如果是目录，显示index.html这里和前面是否重复？
    if ((st.st_mode & S_IXUSR) ||
        (st.st_mode & S_IXGRP) ||
        (st.st_mode & S_IXOTH))//IXUSR X可执行，R读，W写，USR用户,GRP用户组，OTH其他用户
      cgi = 1;
    

    if (!cgi)//如果不是cgi,直接返回
      serve_file(client, path);
    else
      execute_cgi(client, path, method, query_string);//是的话，执行cgi
  }

  close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)//错误请求代码400
{
  char buf[1024];

  sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "<P>Your browser sent a bad request, ");
  send(client, buf, sizeof(buf), 0);
  sprintf(buf, "such as a POST without a Content-Length.\r\n");
  send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
  char buf[1024];

  fgets(buf, sizeof(buf), resource);//fgets每次读取一行
  while (!feof(resource))//使用feof指示是否读到头
  {
    send(client, buf, strlen(buf), 0);
    fgets(buf, sizeof(buf), resource);
  }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{//错误代码500，服务器问题
  char buf[1024];

  sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
  perror(sc);//perror(s) 用来将上一个函数发生错误的原因输出到标准设备(stderr)
  //例如    
  /*fp=fopen("/root/noexitfile","r+");
    if(NULL==fp)
    {
        perror("/root/noexitfile");
    }
    将打印 /root/noexitfile: No such file or directory
    */
  exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
                 const char *method, const char *query_string)
{
  char buf[1024];
  int cgi_output[2];
  int cgi_input[2];
  pid_t pid;
  int status;
  int i;
  char c;
  int numchars = 1;
  int content_length = -1;

  buf[0] = 'A';
  buf[1] = '\0';
  //保证进入
  if (strcasecmp(method, "GET") == 0)//如果是get，则直接丢弃？
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
  else /* POST */
  {
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf))//如果没到结束一直读
    {
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&(buf[16]));//找到content-length长度
      numchars = get_line(client, buf, sizeof(buf));
    }
    if (content_length == -1)//如果为-1，错误请求
    {
      bad_request(client);//返回400
      return;
    }
  }
  //开始写
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);

//#include<unistd.h>
//int pipe(int filedes[2]);
//返回值：成功，返回0，否则返回-1。参数数组包含pipe使用的两个文件的描述符。fd[0]:读管道，fd[1]:写管道。
//必须在fork()中调用pipe()，否则子进程不会继承文件描述符。
//两个进程不共享祖先进程，就不能使用pipe。但是可以使用命名管道。


  if (pipe(cgi_output) < 0)
  {
    cannot_execute(client);//错误代码500
    return;
  }
  if (pipe(cgi_input) < 0)
  {
    cannot_execute(client);
    return;
  }
  //成功建立管道
  if ((pid = fork()) < 0)
  {//子进程建立失败
    cannot_execute(client);
    return;
  }
  //进入子进程
  if (pid == 0) /* child: CGI script */
  {
    char meth_env[255];//设置request_method 的环境变量
    char query_env[255];//GET 的话设置 query_string 的环境变量
    char length_env[255];//POST 的话设置 content_length 的环境变量
    //标准输入文件的文件描述符：0
    //标准输出文件的文件描述符：1
    //标准错误输出的文件描述符：2

    //dup2创建一个newfd，newfd指向oldfd的位置
    dup2(cgi_output[1], 1);//这就是将标准输出重定向到output管道的写入端，也就是输出内容将会输出到output写入
    dup2(cgi_input[0], 0);//将标准输入重定向到input读取端，也就是将从input[0]读内容到input缓冲
    close(cgi_output[0]);//关闭output管道的的读取端
    close(cgi_input[1]);//关闭input管道的写入端
    sprintf(meth_env, "REQUEST_METHOD=%s", method);//把method保存到环境变量中
    //函数说明：int putenv(const char * string)用来改变或增加环境变量的内容.
    //如果该环境变量原先存在, 则变量内容会依参数string 改变, 否则此参数内容会成为新的环境变量.
    //返回值：执行成功则返回0, 有错误发生则返回-1.
    putenv(meth_env);
    if (strcasecmp(method, "GET") == 0)//如果是get
    {
      sprintf(query_env, "QUERY_STRING=%s", query_string);//存储query_string到query_env
      putenv(query_env);
    }
    else
    { /* POST */
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);//存储tontent_length到length_env
      putenv(length_env);
    }
    // 表头文件#include<unistd.h>
    // int execl(const char * path,const char * arg,....);
    // 函数说明:  execl()用来执行参数path字符串所代表的文件路径，接下来的参数代表执行该文件时传递过去的argv(0)、argv[1]……，最后一个参数必须用空指针(NULL)作结束。
    // 返回值:    如果执行成功则函数不会返回，执行失败则直接返回-1
    execl(path, path, NULL);//执行path出的脚本
    exit(0);//退出
  }


  else
  { /* parent */
    close(cgi_output[1]);//关闭output的写入
    close(cgi_input[0]);//关闭input的读取
    if (strcasecmp(method, "POST") == 0)//如果是post
      for (i = 0; i < content_length; i++)
      {
        recv(client, &c, 1, 0);//一个字节一个字节读取
        write(cgi_input[1], &c, 1);//写入到input的写入
      }
    //从input写入后，进入子进程，子进程调用excel执行

    //依次发送到客户端
    while (read(cgi_output[0], &c, 1) > 0)
      send(client, &c, 1, 0);

    close(cgi_output[0]);//ouput的读
    close(cgi_input[1]);//关闭input的写
    waitpid(pid, &status, 0);//等待子进程中止
    //定义函数：pid_t waitpid(pid_t pid, int * status, int options);
    //函数说明：wait()会暂时停止目前进程的执行, 直到有信号来到或子进程结束. 
    //waitpid不会阻塞
    //如果不在意结束状态值, 则参数status 可以设成NULL. 参数pid 为欲等待的子进程识别码, 其他数值意义如下：
    //1、pid<-1 等待一个指定进程组中的任何子进程，这个进程组的ID等于pid的绝对值
    //2、pid=-1 等待任何子进程, 相当于wait().
    //3、pid=0 等待进程组识别码与目前进程相同的任何子进程.
    //4、pid>0 等待任何子进程识别码为pid 的子进程.
  }
}

/**********************************************************************/
/*读入一行字符串。首先如果有\n结束，那么自然就是读到\n结束，如果是\r\n或\r结尾则是转化为\n结尾
 不管是否读到\n结束，如果字符串读完了，则会以\0结束
 Parameters: the socket descriptor
              the buffer to save the data in
              the size of the buffer
  Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
  int i = 0;
  char c = '\0';//问题，这里设置为'\0'可以嘛？
  int n;

  while ((i < size - 1) && (c != '\n'))
  {
    n = recv(sock, &c, 1, 0);//从sock接收最大1个字节，第四个参数一般置0。
    /* DEBUG printf("%02X\n", c); */
    if (n > 0)
    {
      if (c == '\r')
      {
        n = recv(sock, &c, 1, MSG_PEEK);
        /*通常flags设置为0，
        此时recv()函数读取tcp 缓冲区中的数据到buf中，并从tcp 缓冲区中移除已读取的数据。
        如果把flags设置为MSG_PEEK，仅仅是把tcp 缓冲区中的数据读取到buf中，
        没有把已读取的数据从tcp 缓冲区中移除，如果再次调用recv()函数仍然可以读到刚才读到的数据。
        可以类比为设置为0的时候，是剪切，MSG_PEEK则是剪切
        */
        /* DEBUG printf("%02X\n", c); */
        //那么如果收到的是一个\n，那么再调用一次recv(sock, &c, 1, 0)把\n吸收掉；
        if ((n > 0) && (c == '\n'))
          recv(sock, &c, 1, 0);
        else
          c = '\n';
      }


      buf[i] = c;
      i++;
    }
    else
      c = '\n';
  }
  buf[i] = '\0';

  return (i);//返回读入的字符数
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
  char buf[1024];
  (void)filename; //这里的意义何在？
  //2表示成功
  strcpy(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  strcpy(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client)
{//客户端错误404
  char buf[1024];
  
  sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "your request because the resource specified\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "is unavailable or nonexistent.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename)
{
  FILE *resource = NULL;
  int numchars = 1;
  char buf[1024];

  buf[0] = 'A';
  buf[1] = '\0';
  //这里设置的意义是什么，保证进入循环
  while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
    numchars = get_line(client, buf, sizeof(buf));//读取并丢弃

  resource = fopen(filename, "r");//以只读模式打开
  if (resource == NULL)
    not_found(client);//不存在调用404
  else
  {
    headers(client, filename);//写http头
    cat(client, resource);//复制文件
  }
  fclose(resource);//关闭
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
  int httpd = 0;
  struct sockaddr_in name;//套接字地址

  httpd = socket(PF_INET, SOCK_STREAM, 0);//创建TCP套接字
  if (httpd == -1)
    error_die("socket");//创建失败
  memset(&name, 0, sizeof(name));//初始化套接字地址
  name.sin_family = AF_INET;//address family Internet
  name.sin_port = htons(*port);//转换类型host to net short port 
  name.sin_addr.s_addr = htonl(INADDR_ANY);//host to net long 
  /*
  socket INADDR_ANY 监听0.0.0.0地址 socket只绑定端口让路由表决定传到哪个ip
  其中INADDR_ANY就是指定地址为0.0.0.0的地址,这个地址事实上表示不确定地址,或“所有地址”、“任意地址”。
  如果指定ip地址为通配地址(INADDR_ANY)，那么内核将等到套接字已连接(TCP)或已在套接字上发出数据报时才选择一个本地IP地址。
  一般情况下，如果你要建立网络服务器，则你要通知服务器操作系统：
  请在某地址 xxx.xxx.xxx.xxx上的某端口 yyyy上进行侦听，并且把侦听到的数据包发送给我。
  这个过程，你是通过bind（）系统调用完成的。——也就是说，你的程序要绑定服务器的某地址，
  或者说：把服务器的某地址上的某端口占为已用。服务器操作系统可以给你这个指定的地址，也可以不给你。

  如果你的服务器有多个网卡，
  而你的服务（不管是在udp端口上侦听，还是在tcp端口上侦听），出于某种原因：可能是你的服务器操作系统可能随时增减IP地址，
  也有可能是为了省去确定服务器上有什么网络端口（网卡）的麻烦 —— 
  可以要在调用bind()的时候，告诉操作系统：“我需要在 yyyy 端口上侦听，
  所以发送到服务器的这个端口，不管是哪个网卡/哪个IP地址接收到的数据，都是我处理的。”这时候，服务器则在0.0.0.0这个地址上进行侦听。
  */
  if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)//失败返回-1
    error_die("bind");
  if (*port == 0)//如果是port=0，说明是随机端口，如果是随机端口，那么获取
  {
    /*端口号 0 是一个预留的端口号，代表的意思就是它在TCP或者UDP网络传输中应该不会被用到。
    但是在网络编程中，尤其是在unix socket编程当中，它有一些特殊的含义。
    在unix socket编程当中，端口号 0 是一种由系统指定动态生成的端口。
    建立新的TCP和UDP socket连接时，需要给它们指定端口号。 
    为了避免这种写死端口号的做法或者说为了从本地系统中找到可用端口。网络编程员们可以以端口号0来作为连接参数。
    这样的话操作系统就会从动态端口号范围内搜索接下来可以使用的端口号。windows系统和其他操作系统在处理端口号0时有一些细微的差别。
    */
    int namelen = sizeof(name);
    if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)//获取一个套接字的本地接口
      error_die("getsockname");
    //那么调用connect连接成功后，使用getsockname可以正确获得当前正在通信的socket的IP和端口地址。?
    *port = ntohs(name.sin_port);//转换端口号
  }
  if (listen(httpd, 5) < 0)
    error_die("listen");
  return (httpd);//返回套接字地址
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
  implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{//回答不能执行的方法
  char buf[1024];
  //sprintf格式化输出字符串，printf的兄弟，区别在于spintf是输出到了目标缓冲区
  //状态行
  //http/1.1版本号 501错误代码 及原因 
  /*
  状态代码有三位数字组成，第一个数字定义了响应的类别，共分五种类别:
  1xx：指示信息--表示请求已接收，继续处理
  2xx：成功--表示请求已被成功接收、理解、接受
  3xx：重定向--要完成请求必须进行更进一步的操作
  4xx：客户端错误--请求有语法错误或请求无法实现
  5xx：服务器端错误--服务器未能实现合法的请求
  */
  sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");//501对应 not implement
  send(client, buf, strlen(buf), 0);//发送

  //消息头:
  //服务器端名
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  //数据类型
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  //因为是错误，所以没有长度
  //空行
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  
  //消息体
  sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  //html格式，使用 /同名 表示结束
  //网页body里插入<p>标签，这个标签叫做段落标签，它是html中的一个标签
  //只要插入这个标签，文字就会自动换行并分段
  sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
  int server_sock = -1;//服务器套接字
  u_short port = 0;//端口号
  int client_sock = -1;//
  struct sockaddr_in client_name;//服务器地址
  int client_name_len = sizeof(client_name);//计算程度
  pthread_t newthread;//线程
  //线程ID的类型是： pthread_t，是一个结构体数据类型
  //typedef unsigned long int pthread_t;sizeof(pthread_t) =8

  server_sock = startup(&port);//动态创建套接字
  printf("httpd running on port %d\n", port);

  while (1)
  {
    client_sock = accept(server_sock,
                         (struct sockaddr *)&client_name,
                         &client_name_len);//接受客户端请求连接
    if (client_sock == -1)
      error_die("accept");
    /* accept_request(client_sock); */
    //pthread_creat(pthread_t *restrict thread,const pthread_attr_t* restrict attr
    //void*(*start_routine)(void*),void* restrict arg)
    //所以下面的调用第四个参数是不是应该是(void*)&client_sock.也可能是我迂腐了，没有能够灵活运用
    if (pthread_create(&newthread, NULL, accept_request, client_sock) != 0)
    //para1=新线程指向的内存单元，para2=线程属性默认为NULL，
    //para3=void *(*start_rtn)(void *), 新创建的线程从start_rtn函数的地址开始运行
    //para4=
    //创建线程，成功返回0，失败返回-1
      perror("pthread_create");
  }

  close(server_sock);//关闭套接字

  return (0);
}
