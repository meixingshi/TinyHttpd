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
/*��487�У���356�У���168��
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
//isspace��� c ��һ���հ��ַ�����ú������ط���ֵ��true�������򷵻� 0��false����
//ʹ��define�����ʱ��Ϊ�˱�������⾡����������

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
����const char *filename    //�ļ������ļ��е�·��
����, struct stat *buf      //��ȡ����Ϣ�������ڴ���
); Ҳ����ͨ���ṩ�ļ���������ȡ�ļ����� 
stat�ṹ���Ԫ��
struct stat {
        mode_t     st_mode;       //�ļ���Ӧ��ģʽ���ļ���Ŀ¼��
        ino_t      st_ino;        //inode�ڵ��
        dev_t      st_dev;        //�豸����
        dev_t      st_rdev;       //�����豸����
        nlink_t    st_nlink;      //�ļ���������
        uid_t      st_uid;        //�ļ�������
        gid_t      st_gid;        //�ļ������߶�Ӧ����
        off_t      st_size;       //��ͨ�ļ�����Ӧ���ļ��ֽ���
        time_t     st_atime;      //�ļ���󱻷��ʵ�ʱ��
        time_t     st_mtime;      //�ļ���������޸ĵ�ʱ��
        time_t     st_ctime;      //�ļ�״̬�ı�ʱ��
        blksize_t  st_blksize;    //�ļ����ݶ�Ӧ�Ŀ��С
        blkcnt_t   st_blocks;     //ΰ�����ݶ�Ӧ�Ŀ�����
      };   
  */
  int cgi = 0; /* becomes true if server decides this is a CGI
                    * program */
  char *query_string = NULL;

  numchars = get_line(client, buf, sizeof(buf));
  //��ȡһ��http����buf��
  i = 0;
  j = 0;
  //����buf��method�У����ǲ��ܳ���method��С255��
  while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
  {
    method[i] = buf[j];
    i++;
    j++;
  }
  method[i] = '\0';//��method����һ����β

  //strcasecmp�������Դ�Сд�ıȽ��ַ���������
  //������s1��s2�ַ�������򷵻�0��s1����s2�򷵻ش���0 ��ֵ��s1 С��s2 �򷵻�С��0��ֵ
  //�˺���ֻ��Linux���ṩ���൱��windowsƽ̨�� stricmp��
  //����Ȳ���getҲ����post���������޷�����
  if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
  {
    unimplemented(client);
    return;
  }


  //�����post������cgi
  if (strcasecmp(method, "POST") == 0)
    cgi = 1;
  
  //����get����post����Ҫ���ж�ȡurl��ַ
  i = 0;//������ַ�
  while (ISspace(buf[j]) && (j < sizeof(buf)))
    j++;
  while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
  {
    url[i] = buf[j];
    i++;
    j++;
  }
  url[i] = '\0';

  //�����get������Ҫ��url����Ϊ����
  //get������ܴ��У���ʾ��ѯ����
  if (strcasecmp(method, "GET") == 0)
  {
    query_string = url;
    while ((*query_string != '?') && (*query_string != '\0'))
      query_string++;
    if (*query_string == '?')
    {//����У�˵����Ҫ����cgi
      cgi = 1;//��Ҫִ��cgi���������������ñ�־λΪ1
      *query_string = '\0';//������url��β
      query_string++;//query_string��Ϊʣ�µ����
    }
  }

  sprintf(path, "htdocs%s", url);//��url��ӵ�htdocs�󸳸�path
  if (path[strlen(path) - 1] == '/')//�������/��β����Ҫ��index.html��ӵ�����
    //��/��β��˵��pathֻ��һ��Ŀ¼����ʱ��Ҫ����Ϊ��ҳindex.html
    strcat(path, "index.html");//strcat���������ַ���
  if (stat(path, &st) == -1)//ִ�гɹ��򷵻�0��ʧ�ܷ���-1
  {//��������ڣ���ô��ȡʣ�µ����������ݶ���
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
    not_found(client);//���ô������404
  }
  else
  {
    /*����
      #define _S_IFMT 0xF000
      #define _S_IFDIR 0x4000
      #define _S_IFCHR 0x2000
      #define _S_IFIFO 0x1000
      #define _S_IFREG 0x8000
      #define _S_IREAD 0x0100
      #define _S_IWRITE 0x0080
      #define _S_IEXEC 0x0040
    */
    if ((st.st_mode & S_IFMT) == S_IFDIR)//mode_t     st_mode;��ʾ�ļ���Ӧ��ģʽ���ļ���Ŀ¼�ȣ����������ܵõ����
      strcat(path, "/index.html");//�����Ŀ¼����ʾindex.html�����ǰ���Ƿ��ظ���
    if ((st.st_mode & S_IXUSR) ||
        (st.st_mode & S_IXGRP) ||
        (st.st_mode & S_IXOTH))//IXUSR X��ִ�У�R����Wд��USR�û�,GRP�û��飬OTH�����û�
      cgi = 1;
    

    if (!cgi)//�������cgi,ֱ�ӷ���
      serve_file(client, path);
    else
      execute_cgi(client, path, method, query_string);//�ǵĻ���ִ��cgi
  }

  close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)//�����������400
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

  fgets(buf, sizeof(buf), resource);//fgetsÿ�ζ�ȡһ��
  while (!feof(resource))//ʹ��feofָʾ�Ƿ����ͷ
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
{//�������500������������
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
  perror(sc);//perror(s) ��������һ���������������ԭ���������׼�豸(stderr)
  //����    
  /*fp=fopen("/root/noexitfile","r+");
    if(NULL==fp)
    {
        perror("/root/noexitfile");
    }
    ����ӡ /root/noexitfile: No such file or directory
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
  //��֤����
  if (strcasecmp(method, "GET") == 0)//�����get����ֱ�Ӷ�����
    while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
      numchars = get_line(client, buf, sizeof(buf));
  else /* POST */
  {
    numchars = get_line(client, buf, sizeof(buf));
    while ((numchars > 0) && strcmp("\n", buf))//���û������һֱ��
    {
      buf[15] = '\0';
      if (strcasecmp(buf, "Content-Length:") == 0)
        content_length = atoi(&(buf[16]));//�ҵ�content-length����
      numchars = get_line(client, buf, sizeof(buf));
    }
    if (content_length == -1)//���Ϊ-1����������
    {
      bad_request(client);//����400
      return;
    }
  }
  //��ʼд
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  send(client, buf, strlen(buf), 0);

//#include<unistd.h>
//int pipe(int filedes[2]);
//����ֵ���ɹ�������0�����򷵻�-1�������������pipeʹ�õ������ļ�����������fd[0]:���ܵ���fd[1]:д�ܵ���
//������fork()�е���pipe()�������ӽ��̲���̳��ļ���������
//�������̲��������Ƚ��̣��Ͳ���ʹ��pipe�����ǿ���ʹ�������ܵ���


  if (pipe(cgi_output) < 0)
  {
    cannot_execute(client);//�������500
    return;
  }
  if (pipe(cgi_input) < 0)
  {
    cannot_execute(client);
    return;
  }
  //�ɹ������ܵ�
  if ((pid = fork()) < 0)
  {//�ӽ��̽���ʧ��
    cannot_execute(client);
    return;
  }
  //�����ӽ���
  if (pid == 0) /* child: CGI script */
  {
    char meth_env[255];//����request_method �Ļ�������
    char query_env[255];//GET �Ļ����� query_string �Ļ�������
    char length_env[255];//POST �Ļ����� content_length �Ļ�������
    //��׼�����ļ����ļ���������0
    //��׼����ļ����ļ���������1
    //��׼����������ļ���������2

    //dup2����һ��newfd��newfdָ��oldfd��λ��
    dup2(cgi_output[1], 1);//����ǽ���׼����ض���output�ܵ���д��ˣ�Ҳ����������ݽ��������outputд��
    dup2(cgi_input[0], 0);//����׼�����ض���input��ȡ�ˣ�Ҳ���ǽ���input[0]�����ݵ�input����
    close(cgi_output[0]);//�ر�output�ܵ��ĵĶ�ȡ��
    close(cgi_input[1]);//�ر�input�ܵ���д���
    sprintf(meth_env, "REQUEST_METHOD=%s", method);//��method���浽����������
    //����˵����int putenv(const char * string)�����ı�����ӻ�������������.
    //����û�������ԭ�ȴ���, ��������ݻ�������string �ı�, ����˲������ݻ��Ϊ�µĻ�������.
    //����ֵ��ִ�гɹ��򷵻�0, �д������򷵻�-1.
    putenv(meth_env);
    if (strcasecmp(method, "GET") == 0)//�����get
    {
      sprintf(query_env, "QUERY_STRING=%s", query_string);//�洢query_string��query_env
      putenv(query_env);
    }
    else
    { /* POST */
      sprintf(length_env, "CONTENT_LENGTH=%d", content_length);//�洢tontent_length��length_env
      putenv(length_env);
    }
    // ��ͷ�ļ�#include<unistd.h>
    // int execl(const char * path,const char * arg,....);
    // ����˵��:  execl()����ִ�в���path�ַ�����������ļ�·�����������Ĳ�������ִ�и��ļ�ʱ���ݹ�ȥ��argv(0)��argv[1]���������һ�����������ÿ�ָ��(NULL)��������
    // ����ֵ:    ���ִ�гɹ��������᷵�أ�ִ��ʧ����ֱ�ӷ���-1
    execl(path, path, NULL);//ִ��path���Ľű�
    exit(0);//�˳�
  }


  else
  { /* parent */
    close(cgi_output[1]);//�ر�output��д��
    close(cgi_input[0]);//�ر�input�Ķ�ȡ
    if (strcasecmp(method, "POST") == 0)//�����post
      for (i = 0; i < content_length; i++)
      {
        recv(client, &c, 1, 0);//һ���ֽ�һ���ֽڶ�ȡ
        write(cgi_input[1], &c, 1);//д�뵽input��д��
      }
    //��inputд��󣬽����ӽ��̣��ӽ��̵���excelִ��

    //���η��͵��ͻ���
    while (read(cgi_output[0], &c, 1) > 0)
      send(client, &c, 1, 0);

    close(cgi_output[0]);//ouput�Ķ�
    close(cgi_input[1]);//�ر�input��д
    waitpid(pid, &status, 0);//�ȴ��ӽ�����ֹ
    //���庯����pid_t waitpid(pid_t pid, int * status, int options);
    //����˵����wait()����ʱֹͣĿǰ���̵�ִ��, ֱ�����ź��������ӽ��̽���. 
    //waitpid��������
    //������������״ֵ̬, �����status �������NULL. ����pid Ϊ���ȴ����ӽ���ʶ����, ������ֵ�������£�
    //1��pid<-1 �ȴ�һ��ָ���������е��κ��ӽ��̣�����������ID����pid�ľ���ֵ
    //2��pid=-1 �ȴ��κ��ӽ���, �൱��wait().
    //3��pid=0 �ȴ�������ʶ������Ŀǰ������ͬ���κ��ӽ���.
    //4��pid>0 �ȴ��κ��ӽ���ʶ����Ϊpid ���ӽ���.
  }
}

/**********************************************************************/
/*����һ���ַ��������������\n��������ô��Ȼ���Ƕ���\n�����������\r\n��\r��β����ת��Ϊ\n��β
 �����Ƿ����\n����������ַ��������ˣ������\0����
 Parameters: the socket descriptor
              the buffer to save the data in
              the size of the buffer
  Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
int get_line(int sock, char *buf, int size)
{
  int i = 0;
  char c = '\0';//���⣬��������Ϊ'\0'�����
  int n;

  while ((i < size - 1) && (c != '\n'))
  {
    n = recv(sock, &c, 1, 0);//��sock�������1���ֽڣ����ĸ�����һ����0��
    /* DEBUG printf("%02X\n", c); */
    if (n > 0)
    {
      if (c == '\r')
      {
        n = recv(sock, &c, 1, MSG_PEEK);
        /*ͨ��flags����Ϊ0��
        ��ʱrecv()������ȡtcp �������е����ݵ�buf�У�����tcp ���������Ƴ��Ѷ�ȡ�����ݡ�
        �����flags����ΪMSG_PEEK�������ǰ�tcp �������е����ݶ�ȡ��buf�У�
        û�а��Ѷ�ȡ�����ݴ�tcp ���������Ƴ�������ٴε���recv()������Ȼ���Զ����ղŶ��������ݡ�
        �������Ϊ����Ϊ0��ʱ���Ǽ��У�MSG_PEEK���Ǽ���
        */
        /* DEBUG printf("%02X\n", c); */
        //��ô����յ�����һ��\n����ô�ٵ���һ��recv(sock, &c, 1, 0)��\n���յ���
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

  return (i);//���ض�����ַ���
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
  char buf[1024];
  (void)filename; //�����������ڣ�
  //2��ʾ�ɹ�
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
{//�ͻ��˴���404
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
  //�������õ�������ʲô����֤����ѭ��
  while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
    numchars = get_line(client, buf, sizeof(buf));//��ȡ������

  resource = fopen(filename, "r");//��ֻ��ģʽ��
  if (resource == NULL)
    not_found(client);//�����ڵ���404
  else
  {
    headers(client, filename);//дhttpͷ
    cat(client, resource);//�����ļ�
  }
  fclose(resource);//�ر�
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
  struct sockaddr_in name;//�׽��ֵ�ַ

  httpd = socket(PF_INET, SOCK_STREAM, 0);//����TCP�׽���
  if (httpd == -1)
    error_die("socket");//����ʧ��
  memset(&name, 0, sizeof(name));//��ʼ���׽��ֵ�ַ
  name.sin_family = AF_INET;//address family Internet
  name.sin_port = htons(*port);//ת������host to net short port 
  name.sin_addr.s_addr = htonl(INADDR_ANY);//host to net long 
  /*
  socket INADDR_ANY ����0.0.0.0��ַ socketֻ�󶨶˿���·�ɱ���������ĸ�ip
  ����INADDR_ANY����ָ����ַΪ0.0.0.0�ĵ�ַ,�����ַ��ʵ�ϱ�ʾ��ȷ����ַ,�����е�ַ�����������ַ����
  ���ָ��ip��ַΪͨ���ַ(INADDR_ANY)����ô�ں˽��ȵ��׽���������(TCP)�������׽����Ϸ������ݱ�ʱ��ѡ��һ������IP��ַ��
  һ������£������Ҫ�������������������Ҫ֪ͨ����������ϵͳ��
  ����ĳ��ַ xxx.xxx.xxx.xxx�ϵ�ĳ�˿� yyyy�Ͻ������������Ұ������������ݰ����͸��ҡ�
  ������̣�����ͨ��bind����ϵͳ������ɵġ�����Ҳ����˵����ĳ���Ҫ�󶨷�������ĳ��ַ��
  ����˵���ѷ�������ĳ��ַ�ϵ�ĳ�˿�ռΪ���á�����������ϵͳ���Ը������ָ���ĵ�ַ��Ҳ���Բ����㡣

  �����ķ������ж��������
  ����ķ��񣨲�������udp�˿���������������tcp�˿���������������ĳ��ԭ�򣺿�������ķ���������ϵͳ������ʱ����IP��ַ��
  Ҳ�п�����Ϊ��ʡȥȷ������������ʲô����˿ڣ����������鷳 ���� 
  ����Ҫ�ڵ���bind()��ʱ�򣬸��߲���ϵͳ��������Ҫ�� yyyy �˿���������
  ���Է��͵�������������˿ڣ��������ĸ�����/�ĸ�IP��ַ���յ������ݣ������Ҵ���ġ�����ʱ�򣬷���������0.0.0.0�����ַ�Ͻ���������
  */
  if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)//ʧ�ܷ���-1
    error_die("bind");
  if (*port == 0)//�����port=0��˵��������˿ڣ����������˿ڣ���ô��ȡ
  {
    /*�˿ں� 0 ��һ��Ԥ���Ķ˿ںţ��������˼��������TCP����UDP���紫����Ӧ�ò��ᱻ�õ���
    �������������У���������unix socket��̵��У�����һЩ����ĺ��塣
    ��unix socket��̵��У��˿ں� 0 ��һ����ϵͳָ����̬���ɵĶ˿ڡ�
    �����µ�TCP��UDP socket����ʱ����Ҫ������ָ���˿ںš� 
    Ϊ�˱�������д���˿ںŵ���������˵Ϊ�˴ӱ���ϵͳ���ҵ����ö˿ڡ�������Ա�ǿ����Զ˿ں�0����Ϊ���Ӳ�����
    �����Ļ�����ϵͳ�ͻ�Ӷ�̬�˿ںŷ�Χ����������������ʹ�õĶ˿ںš�windowsϵͳ����������ϵͳ�ڴ���˿ں�0ʱ��һЩϸ΢�Ĳ��
    */
    int namelen = sizeof(name);
    if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)//��ȡһ���׽��ֵı��ؽӿ�
      error_die("getsockname");
    //��ô����connect���ӳɹ���ʹ��getsockname������ȷ��õ�ǰ����ͨ�ŵ�socket��IP�Ͷ˿ڵ�ַ��?
    *port = ntohs(name.sin_port);//ת���˿ں�
  }
  if (listen(httpd, 5) < 0)
    error_die("listen");
  return (httpd);//�����׽��ֵ�ַ
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
  implemented.
 * Parameter: the client socket */
/**********************************************************************/
void unimplemented(int client)
{//�ش���ִ�еķ���
  char buf[1024];
  //sprintf��ʽ������ַ�����printf���ֵܣ���������spintf���������Ŀ�껺����
  //״̬��
  //http/1.1�汾�� 501������� ��ԭ�� 
  /*
  ״̬��������λ������ɣ���һ�����ֶ�������Ӧ����𣬹����������:
  1xx��ָʾ��Ϣ--��ʾ�����ѽ��գ���������
  2xx���ɹ�--��ʾ�����ѱ��ɹ����ա���⡢����
  3xx���ض���--Ҫ������������и���һ���Ĳ���
  4xx���ͻ��˴���--�������﷨����������޷�ʵ��
  5xx���������˴���--������δ��ʵ�ֺϷ�������
  */
  sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");//501��Ӧ not implement
  send(client, buf, strlen(buf), 0);//����

  //��Ϣͷ:
  //����������
  sprintf(buf, SERVER_STRING);
  send(client, buf, strlen(buf), 0);
  //��������
  sprintf(buf, "Content-Type: text/html\r\n");
  send(client, buf, strlen(buf), 0);
  //��Ϊ�Ǵ�������û�г���
  //����
  sprintf(buf, "\r\n");
  send(client, buf, strlen(buf), 0);
  
  //��Ϣ��
  sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</TITLE></HEAD>\r\n");
  send(client, buf, strlen(buf), 0);
  //html��ʽ��ʹ�� /ͬ�� ��ʾ����
  //��ҳbody�����<p>��ǩ�������ǩ���������ǩ������html�е�һ����ǩ
  //ֻҪ���������ǩ�����־ͻ��Զ����в��ֶ�
  sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
  send(client, buf, strlen(buf), 0);
  sprintf(buf, "</BODY></HTML>\r\n");
  send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
  int server_sock = -1;//�������׽���
  u_short port = 0;//�˿ں�
  int client_sock = -1;//
  struct sockaddr_in client_name;//��������ַ
  int client_name_len = sizeof(client_name);//����̶�
  pthread_t newthread;//�߳�
  //�߳�ID�������ǣ� pthread_t����һ���ṹ����������
  //typedef unsigned long int pthread_t;sizeof(pthread_t) =8

  server_sock = startup(&port);//��̬�����׽���
  printf("httpd running on port %d\n", port);

  while (1)
  {
    client_sock = accept(server_sock,
                         (struct sockaddr *)&client_name,
                         &client_name_len);//���ܿͻ�����������
    if (client_sock == -1)
      error_die("accept");
    /* accept_request(client_sock); */
    //pthread_creat(pthread_t *restrict thread,const pthread_attr_t* restrict attr
    //void*(*start_routine)(void*),void* restrict arg)
    //��������ĵ��õ��ĸ������ǲ���Ӧ����(void*)&client_sock.Ҳ���������ظ��ˣ�û���ܹ��������
    if (pthread_create(&newthread, NULL, accept_request, client_sock) != 0)
    //para1=���߳�ָ����ڴ浥Ԫ��para2=�߳�����Ĭ��ΪNULL��
    //para3=void *(*start_rtn)(void *), �´������̴߳�start_rtn�����ĵ�ַ��ʼ����
    //para4=
    //�����̣߳��ɹ�����0��ʧ�ܷ���-1
      perror("pthread_create");
  }

  close(server_sock);//�ر��׽���

  return (0);
}
