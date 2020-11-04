#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT "8080" //  port
#define BUFSIZE 8096

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpeg"},
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{"exe","text/plain" },
	{"mp3","audio/mp3" },
	{0,0} };
int open_listenfd();
void load_file (int fd,char* buffer,int ret);
void handle_socket(int fd);
char* check_filetype(char* buffer);
int main(int argc, char **argv) 
{
	int listenfd, clientfd,pid,length;
	char hostname[BUFSIZE], port[BUFSIZE];
	socklen_t clientlen;
	struct sockaddr_in client_addr;
	signal(SIGCLD, SIG_IGN);
	listenfd = open_listenfd();
	fprintf(stderr,"Server waiting connect... \n");
	while (1)
	{
		length = sizeof(client_addr);
		/* 等待客戶端連線 */
		if ((clientfd = accept(listenfd, (struct sockaddr *)&client_addr, &length)) < 0)
			exit(3);

		/* 分出子行程處理要求 */
		if ((pid = fork()) < 0) 
		{
			exit(3);
		} 
		else
		{
			if (pid == 0) {  /* 子行程 */
				close(listenfd);
				handle_socket(clientfd);
			} 	
			else { /* 父行程 */
				close(clientfd);
			}
		}
	}
}
void handle_socket(int fd)
{
	int ret;
	int mod_f = 0; // 0:GET 1:POST
	int file_fd;
	char* fstr;
	static char buf[BUFSIZE+1],tmp[BUFSIZE];
	ret = read(fd,buf,BUFSIZE); // read browser
	if(ret <= 0) // connect error
	{
		fprintf(stderr,"Client: %d\nConnect Error!!\n\n",fd);
		exit(3);
	}
	//read client upload file
	memcpy(tmp,buf,ret);
	fprintf(stderr,"Client: %d\n%s\n\n",fd,buf);
	//download file
	load_file(fd,buf,ret);
	// end = '\0'
	if(ret < BUFSIZE)
		buf[ret] = '\0';
	else
		buf[0] = '\0';
	//remove \r \n
	for(int i=0;i<ret;i++)
	{
		if(buf[i] == '\r' || buf[i] == '\n')
			buf[i] = '\0';
	}
	/* 若為 GET */
	if (strncmp(buf,"GET ",4) == 0 || strncmp(buf,"get ",4) == 0)
	{
		file_fd = 0;		// Meaning it is GET now.
	}
	/* 若為 POST */
	else if(strncmp(tmp,"POST ",5) == 0 || strncmp(tmp,"post ",5) == 0){
		file_fd = 1; // Meaning it is POST now.
	}
	else
	{
		fprintf(stderr,"Client: %d\n Not ask GET or POST Error!!\n\n",fd);
		exit(3);
	}

	if(file_fd == 0)
	{
		// 我們要把 GET /index.html HTTP/1.0 後面的 HTTP/1.0 用空字元隔開
		for(int i=4;i<BUFSIZE;i++)
		{
			if(buf[i] == ' ') 
			{
				buf[i] = 0;
				break;
			}
		}

	}
	//(GET)當客戶端要求根目錄時讀取 index.html
	if (!strncmp(&buf[0],"GET /\0",6)||!strncmp(&buf[0],"get /\0",6) )
		strcpy(buf,"GET /index.html\0");

	/* 檢查客戶端所要求的檔案格式 */
	fstr = check_filetype(buf);


	if(file_fd == 1)
	{
		file_fd=open(&buf[6],O_RDONLY);
		file_fd = open("index.html", O_RDONLY);  // index.html
		sprintf(tmp,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
		write(fd,tmp,strlen(tmp));
	}
	else
	{
		file_fd=open(&buf[5],O_RDONLY);
		/* 傳回瀏覽器成功碼 200 和內容的格式 */
		sprintf(buf,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
		write(fd,buf,strlen(buf));
	}

	/* 讀取檔案內容輸出到客戶端瀏覽器 */
	while ((ret=read(file_fd, buf, BUFSIZE))>0) {
		write(fd,buf,ret);
	}

	exit(1);

	return;
}
int open_listenfd() 
{
	int listenfd;
	static struct sockaddr_in serv_addr;

	if ((listenfd=socket(AF_INET, SOCK_STREAM,0))<0)
		exit(1);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(8080);
	if (bind(listenfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr))<0)
	{
		fprintf(stderr,"bind Error!!!\n");
		exit(2);
	}
		
	if (listen(listenfd,64)<0)
	{
		fprintf(stderr,"listen Error!!!\n");
		exit(2);
	}
	return listenfd;
}
void load_file (int fd,char* buffer,int ret)
{
	//check upload file
	char *tmp = strstr (buffer,"filename=\"");
	if (tmp == 0 ) return;
	char filename[BUFSIZE+1],data[BUFSIZE+1],store[BUFSIZE+1];	
	//get file name
	tmp+=10;
	int i=7;
	strcpy(store,"upload/");
	char *e=strstr(tmp,"\""),*j;
	for(j=tmp;j!=e;i++,j++)
	{
		store[i] = *j;
	}
	store[i]=0;
	tmp = strstr(tmp,"\n");
	tmp = strstr(tmp+1,"\n");
	tmp = strstr(tmp+1,"\n");
	tmp++;

	int download_fd = open(store,O_CREAT|O_WRONLY|O_TRUNC|O_SYNC,S_IRWXO|S_IRWXU|S_IRWXG);

	write(download_fd,tmp,BUFSIZE-(tmp-buffer));
	while((ret=read(fd,buffer,BUFSIZE)) > 0)
	{
		//ret=read(fd,buffer,BUFSIZE);
		write(download_fd,buffer,ret);
	} 
	close(download_fd);
	fprintf (stderr,"client: %d UPLOAD FILE NAME :%s\n\n",fd,filename);
	return ;
}
char* check_filetype(char* buffer)
{
	int i,len;
	int buflen = strlen(buffer);
	char* fstr = (char*)0;
	for(i=0;extensions[i].ext!=0;i++) {
		len = strlen(extensions[i].ext);
		if(!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr = extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0) {
		fstr = extensions[i-1].filetype;
	}
	return fstr;
}
