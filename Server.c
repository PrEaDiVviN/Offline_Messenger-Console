#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#define PORT 3004 /* Portul folosit */
extern int errno; /* Eroarea returnata de unele apeluri */

#define ONLINE 1
#define OFFLINE 0
#define KEYBOARD 0
#define SCREEN 1
#define DO_FOREVER 1
#define true 1
#define false 0
#define DEFAULT_ROOM 0

fd_set readfds; /* Multimea descriptorilor de citire */
fd_set actfds; /* Multimea descriptorilor activi */
int nfds; /* Cel mai mare descriptor la momentul actual */
int ACTUAL_USERS; /* Variabila ce ne spune numarul de conturi inregistrate  */
int LISTENING_SOCKET;
int NOT_CONNECTED = -1;
int NOT_SUCH_USER = -1;

char admins[10][25] = { "administrator" };
int admins_no = 1;

typedef struct UsersInf{
  	char username[25];
  	char password[25];
  	char name[20];
  	char surname[20];
  	char date_of_birth[15];
  	char sex[10];
	int status;/* Daca un user este online sau nu */
  	int commnunicationsocket; /* Socketul cu care comunica serverul cu clientul */
	int room; /* Roomul in care se afla userul la momentul curent! */
	int unread ; /* Daca userul are mesaje necitite sau nu */
} UsersInf;

UsersInf user[100];

void place_end(int pos)
{
	int index;
	for(index = 0; user[pos].username[index] != 32; index ++) ;
	user[pos].username[index] = 0;
	for(index = 0; user[pos].password[index] != 32; index ++) ;
	user[pos].password[index] = 0;
	for(index = 0; user[pos].name[index] != 32; index ++) ;
	if(user[pos].name[index+1]!= ' ')
	{
		index++;
		for(index ; user[pos].name[index] != 32; index ++) ;
	}
	user[pos].name[index] = 0;
	for(index = 0; user[pos].surname[index] != 32; index ++) ;
	user[pos].surname[index] = 0;
	for(index = 0; user[pos].date_of_birth[index] != 32; index ++) ;
	user[pos].date_of_birth[index] = 0;
	for(index = 0; user[pos].sex[index] != 32; index ++) ;
	user[pos].sex[index] = 0;
}

int* getComSockbyname(char find[25],int req) 
{	
	/* In mod sigur aceasta functie returneaza o adresa de memorie intrucat am pus inainte verificarea ca userul exista */	
  	int index=0;
	if(req == true)
	{
  		for( index = 0; index < ACTUAL_USERS; index++ )
		  if(user[index].status == ONLINE)
    		if(strcmp(user[index].username,find)==0)
    			return &user[index].commnunicationsocket; 
		return &NOT_CONNECTED;
	}
	else
	{
		for( index = 0; index < ACTUAL_USERS; index++ )
    		if(strcmp(user[index].username,find)==0)
    			return &user[index].commnunicationsocket; 
		return &NOT_SUCH_USER;
	}
}

UsersInf * getnamebySock(int fd)
{
	int index;
	for( index = 0; index < ACTUAL_USERS; index++ )
		if(user[index].commnunicationsocket == fd)
			return &user[index];
}

int Users_Initialization()
{
	int read_fd;
	if( -1 == ( read_fd = open("users.ini",O_RDONLY)) ) /* Deschidem fisierul */
	{
		perror("Error reading in Users_Initalization!\n");
		exit(errno);
	}
	if( -1 == read(read_fd,&ACTUAL_USERS,sizeof(int)) ) /* Citim numarul de conturi */
	{
		perror("Error at reading ACTUAL_USERS in Users_Initialization!\n");
		exit(errno);
	}
	printf("%d\n",ACTUAL_USERS);
	int index = 0;
	char usINF[121]; /* Folosita pentru a citi din fisier informatiile despre fiecare user in parte */ 
	for( index = 0 ; index < ACTUAL_USERS ; index++) 
	{	if(-1 == read(read_fd,usINF,120))
		{
			perror("Error at reading usINF in Users_Initialization!\n");
			exit(errno);
		}
		memcpy( ((UsersInf *)user) + index,usINF,120);
		place_end(index);
		bzero(usINF,120);
	}
	close(read_fd);
}

void ListenSock_prepare()
{
	struct sockaddr_in server; /* Structura pentru server(ce fel de conexiune
    accepta (relativ la IP) si portul la care asculta) */
    struct sockaddr_in from;/* Structura pentru locatia clientului actual */
    struct timeval tv; /* Structura de timp pentru select */
    int clientfd; /* Descriptorii de socket, socketul de ascultare 
    si cel pentru fiecare client in parte. */
    int optval = 1; /* Optiune folosita pentru setsockopt */
    int fd; /* Descriptorul folosit pentru parcurgerea listelor de descriptori */
    int len; /* Lungimea structurii sockaddr_in */

    /* Cream socketul ce va fi folosit pentru ascultare */
    if(-1 == ( LISTENING_SOCKET = socket(AF_INET,SOCK_STREAM,0) ) )
    {  
		perror("Error at creating the socket in ListenSock_prepare!\n");
		exit(errno);  
	}
    /* Setam pentru socket optiunea ce ne da voia sa mai folosim de mai multe ori
    acelasi port */
    if(-1 == setsockopt(LISTENING_SOCKET,SOL_SOCKET,SO_REUSEPORT ,&optval,sizeof(optval)))
    {
        perror("Error at adding SO_REUSEPORT to socket in ListenSock_prepare!\n");
		exit(errno);
    }
    /* Reinitializam structura de date la 0 */
    bzero(&server,sizeof(server));
    /* Completam structura de date pentru server */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);


    /* Atasam socketul */
    if( -1 == bind(LISTENING_SOCKET,(struct sockaddr *)& server,sizeof(struct sockaddr)))
    {
        perror("Eroare at binding the structure to the socket in ListenSock_prepare\n");
        exit(errno);
    }
    /* punem serverul sa asculte daca vin clientii sa se conecteze */
    if(listen(LISTENING_SOCKET,10) == -1)
    {
        perror("Error at making the socket into a listening one in ListenSock_prepare!\n");
        exit(errno);
    }
}

void Write_registration(char * user_reg)
{
	/* Opening the file for writing */
	int write_fd;
	if( -1 ==  (write_fd = open("users.ini",O_WRONLY)) )
	{
		perror("Error at opening users.ini in Write_registration!\n");
		exit(errno);
	}
	/* Reading peoples information */
	ACTUAL_USERS++;/* The number of users increased! */
	memcpy( ((UsersInf *)user) + (ACTUAL_USERS - 1),user_reg,120);/* Inseram userul care se logheaza in structura globala cu useri */
	place_end(ACTUAL_USERS - 1);
	if( -1 == write(write_fd,&ACTUAL_USERS,sizeof(int)) ) /* Incrementam numarul de conturi atat in fisier, cat si in server */
	{
		perror("Error at reading ACTUAL_USERS in Users_Initialization!\n");
		exit(errno);
	}
	/* Positioning the cursor to the end of the file */
	if(-1 == lseek(write_fd,0,SEEK_END)) 
	{
		perror("Error at lseek to end in Write_registration!\n"); 
		exit(errno);	
	}
	/* Registering people in the file */
	if( -1 == write(write_fd,user_reg,120))
	{
		perror("Error at writing the registration in Write_registration!\n");
		exit(errno);
	}
	close(write_fd);/* Closing the writing descriptor */
}

int NOT_EXISTING_USER(char USERNAME[25])
{
	int index;
	for( index = 0; index < ACTUAL_USERS ; index++)
	{
		if(strcmp(USERNAME,user[index].username) == 0)
			return false;
	}
	return true;
}

int BANNED(char USERNAME[25])
{
	int fd;
	if( -1 == (fd = open("Server/banned_users.txt", O_RDWR))) /* Deschidem fisierul pt a citi */
	{
		perror("Error at opening the file for the banned users in BANNED!\n");
		exit(errno);
	}
	int banned_number = 0;
	if( -1 == read(fd,&banned_number,sizeof(int)))/* Citim numarul de useri banati pana acum */
	{
		perror("Error at reading the banned_number in BANNED!\n");
		exit(errno);
	}
	int index;
	char user_banned[26];
	for(index = 0; index <= banned_number; index ++)
	{
		bzero(user_banned,26);
		if( -1 == read(fd,user_banned,25))
		{
			perror("Error at reading the username in BANNED!\n");
			exit(errno);
		}
		int i = 0; while(user_banned[i]!=' ')i++; user_banned[i] = 0;
		if(strcmp(USERNAME,user_banned) == 0)
			return true;
	}
	return false;
}

int LOGIN_REQUEST(char USERNAME[25],char PASSWORD[25],int * exists)
{
	int index;
	for( index = 0; index < ACTUAL_USERS ; index++)
		if(strcmp(USERNAME,user[index].username) == 0)
		{	(*exists) = true;
			if(strcmp(PASSWORD,user[index].password) == 0)
				return true;
		}
	return false;
}

void Alocate_Folder(char username[])
{
	char path[70];
	bzero(path,70);
	strcat(path,"Conversations/");
	strcat(path,username);
	if(-1 == mkdir(path, S_IRWXU | S_IRGRP | S_IROTH )) 
	{
		perror("Error at creating the folder in Make_folder!\n");
		exit(errno);
	}
	strcat(path,"/unseen.txt");
	int fd;
	if( -1 == (fd = open(path, O_CREAT|O_RDWR|O_TRUNC,S_IRWXU | S_IRGRP | S_IROTH)))
	{
		perror("Error at creating the log for the unseen messages!\n");
		exit(errno);
	}
	close(fd);
}

static void *RegisterLogin_thread(void * arg)
{
	int Comm_sock = *((int *) arg); /* Socket used for speaking with the current client! */
	char command[10];
	while(DO_FOREVER)
	{
		if( 0 >= read(Comm_sock,command,10) )
		{
			perror("Error at reading from the client the command in RegisterLogin_thread");
			exit(errno);
		}
		if(strcmp(command,"login") == 0)
		{
			char username[25];
			char password[25];
			if(-1 == read(Comm_sock,username,25))/* citim numele pe care ni-l da clientul */
  			{
    			perror("Errot at reading the username for Login in RegisterLogin_thread\n");
    			exit(errno);
  			}
			if(-1 == read(Comm_sock,password,25))/* citim numele pe care ni-l da clientul */
  			{
    			perror("Errot at reading the password for Login in RegisterLogin_thread\n");
    			exit(errno);
  			}
			int exists = false;
			if(LOGIN_REQUEST(username,password,&exists) == true) /* Daca se afla printre conturile serverului */
			{
				if(BANNED(username) == true)
				{
					if(-1 == write(Comm_sock,"#############################################################################\n[SERVER]Username-ul a fost banat! Creati alt cont nou pentru a folosi aplicatia!\n#############################################################################\n",238))
					{
						perror("Error at sending the banned message at login in RegisterLogin_thread!\n");
						exit(errno);
					}
				}
				else
				{
					int* address = getComSockbyname(username,false);/* Modificam pt userul curent, socketul in structura globala */
					(*address) = Comm_sock;
					address--; /* Schimbam statusul userului curent in ONLINE */
					(*address) = ONLINE;
  					FD_SET(Comm_sock,&actfds);/* Adaugam socketul curent la coada pt select pt ca ne-am logat! */	
  					if(Comm_sock > nfds) /* Schimbam descriptorul utilizat in select intrucat un nou user s-a logat! */
  						nfds = Comm_sock;
					if(-1 == write(Comm_sock,"#############################################################################\n[SERVER]Logarea s-a desfasurat cu succes!\n#############################################################################\n",200))
					{
						perror("Error at sending the success message at login in RegisterLogin_thread!\n");
						exit(errno);
					}
					address++;address++;address++;
					if((*address) == true)/* Daca userul are mesaje necitite, il notificam! */
						if(-1 == write(Comm_sock,"#############################################################################\n[SERVER]Aveti mesaje necitite!\n[SERVER]Introduceti <inbox> pentru a le vedea!\n#############################################################################\n",235))
						{
							perror("Error at sending the inbox message at login in RegisterLogin_thread!\n");
								exit(errno);
						}
					pthread_detach(pthread_self()); /* Dupa ce thread-ul se termina, memoria este eliberata */
					pthread_exit(NULL); /* thread-ul curent se termina */
				}
			}
			else if(exists == true)
			{ 	
				if(-1 == write(Comm_sock,"#############################################################################\n[SERVER]Numele de utilizator sau parola au fost gresite! Va rugam reincercati!\n#############################################################################\n",236))
				{
					perror("Error at sending the error message at login in RegisterLogin_thread!\n");
					exit(errno);
				}
			}
			else
			{
				if(-1 == write(Comm_sock,"#############################################################################\n[SERVER]Acest nume de utilizator nu este asociat niciunui cont!\n#############################################################################\n",221))
				{
					perror("Error at sending the error message at login in RegisterLogin_thread!\n");
					exit(errno);
				}
			}
			
		}
		else if(strcmp(command,"register") == 0)
		{
			char information [121];/* Unde citim informatiile despre userul curent ce incearca sa se inregistreze */
			if( -1 == read(Comm_sock,information,120) )
			{
				perror("Error at reading the information from the client in RegisterLogin_thread!\n");
				exit(errno);
			}
			/* Verificam ca username-ul nu este deja folosit! */
			char username[25];
			strncpy(username,information,25);/* Citim username-ul userului si verificam daca exista */
			int i=0; while(username[i]!=' ') i++; username[i] = 0; /* Obtinem username-ul */
			if(NOT_EXISTING_USER(username) == true)
			{
				Alocate_Folder(username);
				Write_registration(information);/* Cream inregistrarea in fisier */
				char buffer[200];
				int wsize = 0;
				strcpy(buffer,"[SERVER]Inregistrarea contului s-a desfasurat cu succes!\n#############################################################################\n");
				if(-1 == (wsize = write(Comm_sock,buffer,136)))
				{
					perror("Error at sending the success message at register in RegisterLogin_thread!\n");
					exit(errno);
				}
			}
			else if(-1 == write(Comm_sock,"[SERVER]Acest nume de utilizator a fost deja rezervat! Va rugam reincercati!\n#############################################################################\n",156))
			{
				perror("Error at sending the error message at register in RegisterLogin_thread!\n");
				exit(errno);
			}
			
		}
	}
}

static void *AcceptConnection_thread(void * arg)
{
	struct sockaddr_in from;
  	int comm_sock[100], count = 0, size;
	size = sizeof(from);
	pthread_t REGLOG_thread;
  	while(1)
  	{
      	if( 0 > (comm_sock[count] = accept(LISTENING_SOCKET,(struct sockaddr *) &from,&size)) )
     	{
    		perror("Error at accept call in the Accept_connection!\n");
          	exit(errno);
      	}
		if( -1 == pthread_create(&REGLOG_thread, NULL, &RegisterLogin_thread, &comm_sock[count]) )
     	{
       		perror("Error at creating th thread responsible for login/register in Accept_connection!\n");
       		exit(errno);
     	}	
		count++;/* We accepted another conection */
   }
}

int obtain_username(char * source, char * destination)
{
	int index = 0;
	for(index = 0; source[index] != ' ' && source[index] != 0 && source[index]!='\n'; index ++)
		destination[index] = source[index];
	destination[index] = 0; 
	return index;
}

void write_unseen(int from,char * path,char * message)
{
	int fd;
	if(-1 == (fd = open(path,O_RDWR)))/* Deschidem fisierul unseen.txt pt a scrie pe el */ 
	{
		perror("Error at trying to open the unseen.txt in write_unseen!\n");
		exit(errno);
	}
	int number_messages = 0;/* Citim numarul de mesaje din fisier */
	if( -1 == read(fd,&number_messages,sizeof(int)))
	{
		perror("Error at reading the number_messages from unseen.txt in write_unseen!\n");
		exit(errno);
	}
	if(number_messages != 0)
		if(-1 == lseek(fd,-sizeof(int),SEEK_CUR)) /* Mutam din nou cursorul la inceput */
		{
			perror("Error at lseek to end in write_unseen!\n"); 
			exit(errno);	
		}
	number_messages++;/* Incrementam numarul de mesaje din fisier cu 1 */
	if(-1 == write(fd,&number_messages,sizeof(int))) 
	{
		perror("Error at writing the number_messages in unseen.txt!\n");
		exit(errno);
	}
	if(-1 == lseek(fd,0,SEEK_END)) /* Mutam cursurul la finalul fisierului pentru a mai scrie un mesaj */
	{
		perror("Error at lseek to end in write_unseen!\n"); 
		exit(errno);	
	}
	/* Construim mesajul pe care dorim sa il scriem */
	char Message[300];
	bzero(Message,299);
	strcat(Message,"[FROM -> "); UsersInf * name = getnamebySock(from); strcat(Message,(*name).username);strcat(Message,"]");
	strcat(Message,message);
	/* Scriem intai dimensiunea mesajului */
	int message_size = strlen(Message) - 1;
	if(-1 == write(fd,&message_size,sizeof(int))) /* Writing the size of message in unseen.txt */
	{
		perror("Error at writing the size of the message in unseen.txt!\n");
		exit(errno);
	}
	if(-1 == write(fd,Message,message_size)) /* Writing the message in unseen.txt */
	{
		perror("Error at writing the message in unseen.txt!\n");
		exit(errno);
	}
	close(fd);
}

void write_to(int from,char * message, char * to)
{
	/* Mai intai scriem in fisierul destinatatarului*/
	UsersInf * current_usr = getnamebySock(from);
	char path[70]; bzero(path,70); strcat(path,"Conversations/"); strcat(path,to); strcat(path,"/"); strcat(path,(* current_usr).username);
	if(-1 == access(path,F_OK)) /* Verificam daca fisierul exista, si daca nu exista, il cream */ 
	{
		int fd;/* Fisierul nu exista, deci il cream */
		if( -1 == (fd = open(path, O_CREAT|O_RDWR|O_TRUNC,S_IRWXU | S_IRGRP | S_IROTH)))
		{
			perror("Error at creating the the file for destination messages in write_to!\n");
			exit(errno);
		}
		close(fd);
	}
	int fd;
	if(-1 == (fd = open(path,O_RDWR))) /* Deschidem fisierul pentru destinatar */
	{
		perror("Error at opening the file for the destination in write_to!\n");
		exit(errno);
	}
	int number_messages = 0;/* Citim numarul de mesaje din fisier */
	if( -1 == read(fd,&number_messages,sizeof(int)))
	{
		perror("Error at reading the number_messages from unseen.txt in write_to!\n");
	}
	if(number_messages != 0)
		if(-1 == lseek(fd,-sizeof(int),SEEK_CUR)) /* Mutam din nou cursorul la inceput */
		{
			perror("Error at lseek to end in write_to!\n"); 
			exit(errno);	
		}
	number_messages++;/* Incrementam numarul de mesaje din fisier cu 1 */
	if(-1 == write(fd,&number_messages,sizeof(int))) 
	{
		perror("Error at writing the number_messages in write_to!\n");
		exit(errno);
	}
	if(-1 == lseek(fd,0,SEEK_END)) /* Mutam cursurul la finalul fisierului pentru a mai scrie un mesaj */
	{
		perror("Error at lseek to end in write_to!\n"); 
		exit(errno);	
	}
	/* Obtinem mesajul pe care dorim sa il sriem */
	char Message[300]; bzero(Message,299);
	strcat(Message,"[FROM -> "); UsersInf * name = getnamebySock(from); strcat(Message,(*name).username); strcat(Message,"]");
	strcat(Message,message);
	/* Scriem intai dimensiunea mesajului */
	int message_size = strlen(Message) - 1;
	if(-1 == write(fd,&message_size,sizeof(int))) /* Writing the size of message in unseen.txt */
	{
		perror("Error at writing the size of the message in unseen.txt!\n");
		exit(errno);
	}
	/* Apoi scriem mesajul */
	if( -1 == write(fd,Message,message_size))
	{
		perror("Error at writing the message for destination in write_to!\n");
		exit(errno);
	}
	close(fd);
}

void write_from(int from, char * message,char * to)
{
	UsersInf * current_usr = getnamebySock(from);
	char path[70];
	bzero(path,70); strcat(path,"Conversations/"); strcat(path,(* current_usr).username); strcat(path,"/"); strcat(path,to);
	if(-1 == access(path,F_OK))/* Fisierul nu exista, deci il cream */
	{
		int fd1;
		if( -1 == (fd1 = open(path, O_CREAT|O_RDWR|O_TRUNC,S_IRWXU | S_IRGRP | S_IROTH)))
		{
			perror("Error at creating the the file for destination messages in write_from!\n");
			exit(errno);
		}
		close(fd1);
	}
	int fd;
	if(-1 == (fd = open(path,O_RDWR))) /* Deschidem fisierul pentru destinatar */
	{
		perror("Error at opening the file for the destination in write_from!\n");
		exit(errno);
	}
	int number_messages = 0;/* Citim numarul de mesaje din fisier */
	if( -1 == read(fd,&number_messages,sizeof(int)))
	{
		perror("Error at reading the number_messages from unseen.txt in write_from!\n");
	}
	if(number_messages != 0)
		if(-1 == lseek(fd,-sizeof(int),SEEK_CUR)) /* Mutam din nou cursorul la inceput */
		{
			perror("Error at lseek to end in write_from!\n"); 
			exit(errno);	
		}
	number_messages++;/* Incrementam numarul de mesaje din fisier cu 1 */
	if(-1 == write(fd,&number_messages,sizeof(int))) 
	{
		perror("Error at writing the number_messages in write_from!\n");
		exit(errno);
	}
	if(-1 == lseek(fd,0,SEEK_END)) /* Mutam cursurul la finalul fisierului pentru a mai scrie un mesaj */
	{
		perror("Error at lseek to end in write_from!\n"); 
		exit(errno);	
	}
	/* Obtinem mesajul pe care dorim sa il sriem */
	char Message[300]; bzero(Message,299);
	strcat(Message,"[FROM -> "); UsersInf * name = getnamebySock(from); strcat(Message,(*name).username); strcat(Message,"]");
	strcat(Message,message);
	/* Scriem intai dimensiunea mesajului */
	int message_size = strlen(Message) - 1;
	if(-1 == write(fd,&message_size,sizeof(int))) /* Writing the size of message in unseen.txt */
	{
		perror("Error at writing the size of the message in unseen.txt!\n");
		exit(errno);
	}
	if( -1 == write(fd,Message,message_size))/* scriem apoi mesajul */
	{
		perror("Error at writing the message for destination in write_from!\n");
		exit(errno);
	}
	close(fd);
}

void required_writes(int from,char to[25],char message[] ,int status)
{
	/* Scriem intai in unseen.txt pentru destinatar daca statusul este offline */
	if(status == OFFLINE)
	{
		char path[70];bzero(path,70); strcat(path,"Conversations/"); strcat(path,to); strcat(path,"/unseen.txt");
		write_unseen(from,path,message);
	}
	/* Scriem in fisierele ambilor useri mesajele */
	write_to(from,message,to);

	/* Acum scriem in fisierul expeditorului */
	write_from(from,message,to);
}

void call_send(int Comm_sock, char request[],int size)
{
	char username[25];
	bzero(username,25);
	int start = obtain_username(request+5,username); /* Start se refera la locul in care incepe mesajul */ 
	if(NOT_EXISTING_USER(username) == false)
	{
		int * destination = getComSockbyname(username,true); 

		if((*destination) == NOT_CONNECTED)
		{
			if( -1 == write(Comm_sock,"#############################################################################\n[SERVER]User-ul nu este connectat!\n[SERVER]Acesta va primi mesajele cand se va conecta!\n#############################################################################\n",245))/* Trimitem inapoi faptul ca userul este offline */
			{
					perror("Error at sending the unknown user message in call_send!\n");
					exit(errno);
			}
			destination = getComSockbyname(username,false); 
			destination++;destination++;
			(*destination) = true;/* Modifying unread value to true for the destination user */;
			required_writes(Comm_sock,username,request+5+start,OFFLINE);
		}
		else
		{
			char Message[300];
			bzero(Message,299);
			strcat(Message,"[FROM -> "); UsersInf * name = getnamebySock(Comm_sock); strcat(Message,(*name).username);strcat(Message,"]");
			strcat(Message,request+5+start);
			if( -1 == write((*destination),Message,299))/* Trimitem mesajul */
			{
					perror("Error at sending the message in call_send!\n");
					exit(errno);
			}
			required_writes(Comm_sock,username,request+5+start,ONLINE);
		}	
	}
	else if( -1 == write(Comm_sock,"#############################################################################\n[SERVER]User-ul nu exista!\n#############################################################################\n",184))/* Trimitem inapoi faptul ca username-ul nu exista */
	{
			perror("Error at sending the unknown user message in call_send!\n");
			exit(errno);
		
	}
}

void move_room(int fd,char * command)
{
	char Strnum[15];
	bzero(Strnum,14);
	int index, i=0 ;
	for(index = 5; command[index] != 0 && command[index] != '\n' ; index++)
		Strnum[i++] = command[index];
	Strnum[i] = 0;
	int room_number = atoi(Strnum);
	UsersInf * current_usr = getnamebySock(fd);
	(*current_usr).room = room_number;
	char answer[300];
	bzero(answer,300);
	strcat(answer,"#############################################################################\n[SERVER]Mutat la camera ");
	strcat(answer,Strnum);
	strcat(answer,"!\n#############################################################################\n");
	if(-1 == write(fd,answer,strlen(answer)) )
	{
		perror("Error at sending the room we moved to in move_room!");
		exit(errno);
	}
}

void send_room(char * command, int fd)
{
	UsersInf * current_usr = getnamebySock(fd);
	if( (*current_usr).room == DEFAULT_ROOM ) /* Este interzisa trimiterea de mesaje in room-ul default */
	{
		if(-1 == write(fd,"#############################################################################\n[SERVER]Este interzisa trimiterea de mesaje in room-ul default!\n#############################################################################\n",221))
			{
				perror("Error at sending the error message at register in RegisterLogin_thread!\n");
				exit(errno);
			}
	}
	else
	{
		int index = 0;
		char Message[300];
		bzero(Message,299);
		strcat(Message,"[FROM -> "); strcat(Message,(*current_usr).username);strcat(Message,"][room]");
		strcat(Message,command+5);
		for(index = 0; index < ACTUAL_USERS ; index++)
			if(user[index].room == (*current_usr).room && strcmp(user[index].username,(*current_usr).username) != 0)
				if( -1 == write(user[index].commnunicationsocket,Message, 250) )
				{
					perror("Error at sending the unknown command message in HANDLE_REQUEST!");
					exit(errno);
				}
	}
}

void getuser_Inf(char * command, int fd)
{
	char username[25];
	bzero(username,25);
	strcpy(username,command+8); username[strlen(username)-1] = 0;
	int* userfd = getComSockbyname(username,false);
	if((*userfd) == NOT_SUCH_USER) 
	{
		if( -1 == write (fd,"#############################################################################\n[SERVER]User-ul nu exista!\n#############################################################################\n",184)) 
		{
			perror("Error at writing the unexistent user in getuser_Inf!\n");
			exit(errno);
		}
	}
	else
	{
		UsersInf * current_usr = getnamebySock((*userfd));
		char response[10000];
		bzero(response,10000);
		strcat(response,"###################################[SERVER]##################################\n");
		strcat(response,"Prenumele userului este: ");strcat(response,(* current_usr).name);strcat(response,"\n");
		strcat(response,"Numele userului este: ");strcat(response,(* current_usr).surname);strcat(response,"\n");
		strcat(response,"Data de nastere a userului este: ");strcat(response,(* current_usr).date_of_birth);strcat(response,"\n");
		strcat(response,"Sexul userului este: ");strcat(response,(* current_usr).sex);strcat(response,"\n");
		if((* current_usr).status == ONLINE)
		{ strcat(response,"Statusul userului este: ");strcat(response,"ONLINE\n"); }
		else
		{ strcat(response,"Statusul userului este: ");strcat(response,"OFFLINE\n"); }
		strcat(response,"###################################[SERVER]##################################\n");
		if( -1 == write (fd,response,strlen(response))) 
		{
			perror("Error at reading the command in HANDLE_REQUEST!\n");
			exit(errno);
		}
	}
}

void getONLINE(int fd)
{
	char response[10000];
	bzero(response,10000);
	strcat(response,"###################################[SERVER]##################################\n");
	int index = 0;
	for( index = 0; index < ACTUAL_USERS ; index ++ )
		if(user[index].status == ONLINE)
		{
			strcat(response,user[index].username);
			strcat(response,"\n");
		}
	strcat(response,"###################################[SERVER]##################################\n");
	if( -1 == write (fd,response,strlen(response))) 
	{
		perror("Error at reading the command in HANDLE_REQUEST!\n");
		exit(errno);
	}
}

void getInbox(int from)
{
	/* Cream path-ul pt fisierul din care vom citi */
	UsersInf * current_usr = getnamebySock(from);
	if((*current_usr).unread == true)
	{
		char path[70];bzero(path,70); strcat(path,"Conversations/"); strcat(path,(*current_usr).username); strcat(path,"/unseen.txt");
		/* Deschidem fisierul pentru a citi din el */
		int fd;
		if(-1 == (fd = open(path,O_RDONLY)))
		{
			perror("Error at opening the file in getInbox!\n");
			exit(errno);
		}
		/* Citim numarul de mesaje */
		int number_messages = 0;
		if(-1 == read(fd,&number_messages,sizeof(int)))
		{
			perror("Error at reading the number_messages in getInbox!\n");
			exit(errno);
		}
		/* Trimitem un String ce marcheaza inceputul inbox-ului */
		if(-1 == write(from,"####################################INBOX####################################\n",79)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
		{
			perror("Error at writing some message in getInbox!\n");
			exit(errno);
		}
		/* Citim mesajele si le trimitem */
		int index = 0,size = 0;
		char buffer[300];
		bzero(buffer,300);
		for( index = 0; index < number_messages; index++)
		{
			int message_size = 0;/* Citim dimensiunea mesajului curent */
			if(-1 == read(fd,&message_size,sizeof(int)))
			{
				perror("Error at reading the size of some message in getInbox!\n");
				exit(errno);
			}
			if(-1 == (size = read(fd,buffer,message_size))) /* Citim mesajul curent */
			{
				perror("Error at reading some message in getInbox!\n");
				exit(errno);
			}
			buffer[message_size] = '\n';
			if(-1 == write(from,buffer,message_size+1)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
			{
				perror("Error at writing some message in getInbox!\n");
				exit(errno);
			}
			bzero(buffer,size);
		}
		/* Trimitem un String ce marcheaza finalul inbox-ului */
		if(-1 == write(from,"####################################INBOX####################################\n",79)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
		{
			perror("Error at writing some message in getInbox!\n");
			exit(errno);
		}
		/* Setam faptul ca utilizatorul curent si-a citit mesajele */
		(*current_usr).unread = false;
		close(fd);
		/* Fisierul este golit */
		if( -1 == (fd = open(path,O_WRONLY|O_TRUNC)))
		{
			perror("Error at deleting all content from unseen.txt in getInbox!\n");
			exit(errno);
		}
		close(fd);
	}
	else
	{
		if(-1 == write(from,"#############################################################################\n[SERVER]Nu exista mesaje in inbox-ul tau!\n#############################################################################\n",199)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
		{
			perror("Error at writing empty inbox in getInbox!\n");
			exit(errno);
		}
	}
}

void getConv(int Sock, char message[])
{
	char username[25];
	bzero(username,25);
	int start = obtain_username(message+5,username); /* Start se refera la locul in care incepe mesajul */
	if(NOT_EXISTING_USER(username) == true) /* Daca userul nu exista, nu putem avea o conversatie cu acesta */\
	{
		if(-1 == write(Sock,"#############################################################################\n[SERVER]User-ul nu exista!\n#############################################################################\n",184)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
		{
			perror("Error at writing some message in getConv!\n");
			exit(errno);
		}
	}
	else /* Daca userul exista, verificam daca avem fisier de conversatie cu acesta */
	{
		UsersInf * current_usr = getnamebySock(Sock);
		char path[70]; bzero(path,70); strcat(path,"Conversations/");  strcat(path,(* current_usr).username);  strcat(path,"/"); strcat(path,username);
		int fd;
		if(-1 == access(path,F_OK)) /* Verificam daca fisierul exista, si daca nu exista, il cream */ 
		{
			int fd;/* Fisierul nu exista, deci il cream */
			if( -1 == (fd = open(path, O_CREAT|O_RDWR|O_TRUNC,S_IRWXU | S_IRGRP | S_IROTH)))
			{
				perror("Error at creating the the file for destination messages in getConv!\n");
				exit(errno);
			}
			close(fd);
			if(-1 == write(Sock,"#############################################################################\n[SERVER]Nu ati inceputt o conversatie cu acel user!\n#############################################################################\n",209)) /* Writing the message in unseen.txt */
			{
				perror("Error at writing the message in unseen.txt!\n");
				exit(errno);
			}
			pthread_detach(pthread_self()); /* Dupa ce thread-ul se termina, memoria este eliberata */
			pthread_exit(NULL); /* thread-ul curent se termina */
		}
		else /* Daca fisierul exista, trimitem conversatia*/
		{
			/* Trimitem un String ce marcheaza inceputul Conversatiei */
			if(-1 == write(Sock,"#################################CONVERSATION################################\n",79)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
			{
				perror("Error at writing beggining of conversation in getConv!\n");
				exit(errno);
			}
			/* Des */
			if(-1 == (fd = open(path,O_RDONLY)))
			{
				perror("Error at opening the file in getConv!\n");
				exit(errno);
			}
			/* Citim numarul de mesaje */
			int number_messages = 0;
			if(-1 == read(fd,&number_messages,sizeof(int)))
			{
				perror("Error at reading the number_messages in getConv!\n");
				exit(errno);
			}
			if(number_messages != 0)
			{
				/* Citim mesajele si le trimitem */
				int index = 0,size = 0;
				char buffer[300];
				bzero(buffer,300);
				for( index = 0; index < number_messages; index++)
				{	
					int message_size = 0;/* Citim dimensiunea mesajului curent */
					if(-1 == read(fd,&message_size,sizeof(int)))
					{
						perror("Error at reading the size of some message in getInbox!\n");
						exit(errno);
					}
					if(-1 == (size = read(fd,buffer,message_size))) /* Citim mesajul curent */
					{
						perror("Error at reading some message in getInbox!\n");
						exit(errno);
					}
					buffer[message_size] = '\n';
					if(-1 == write(Sock,buffer,message_size+1)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
					{
						perror("Error at writing some message in getInbox!\n");
						exit(errno);
					}
					bzero(buffer,size);
				}
			}
			else if(-1 == write(Sock,"[SERVER]Conversatia cu acel user este goala!\n",46)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
			{
						perror("Error at writing empty conversation in getConv!\n");
						exit(errno);
			}
			/* Trimitem un String ce marcheaza finalul Conversatiei */
			if(-1 == write(Sock,"#################################CONVERSATION################################\n",79)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
			{
				perror("Error at writing end of conversation in getConv!\n");
				exit(errno);
			}
			close(fd);/* Inchidem descriptorul de fisier */
		}
		
	}
	
}

void manual(int Sock)
{
	char MANUAL[10000];
	bzero(MANUAL,10000);
	strcat(MANUAL,"MANUAL#######################################################################\n");
	strcat(MANUAL,"[register]\n   ->Permite utilizatorului sa le inregistreze. Acesta are de completat 6\ncampuri oblitatorii:(username)(password)(name)(surname)(date of birth)\n(sex) in ordine stabilita. Fiecare camp ce urmeaza completat este\nanuntat.\n   ->In cazul in care username-ul este rezervat deja. Serverul va trimite\nun status de eroare iar user-ul va trebui din nou sa introduca comanda\nregister. \n[login]\n   ->Permite utilizatorului sa se logheze la un cont existent. Acesta are\nde completat username-ul si parola. Fiecare data ce urmeaza a fi\ncompletata este anuntata\n   ->In cazul in care user-ul nu exista sau parola este gresita va\nreturna un status de eroare iar userul va trebui sa introduca\ndin nou comanda login.\n[send:<username> <message>]\n   ->Un utilizator logat poate folosi aceasta comanda pentru a trimite\nmesaje altui utilizator. username-ul trebuie sa fie exact pentru \na putea trimete mesaj.\n   ->In cazul in care userul nu exista, sau este offline va returna un\nstatus de eroare. Pentru cazul in care user-ul este offline,\nmesajul va fi trimis, iar acesta il va primi cand va deveni online.\n[reply:<username> <index> <reply>]\n   ->Primite utilizatorui sa raspunda sa un mesaj pe care il are cu un \nanumit user intr-o conversatie. Username-ul trebuie sa fie exact.\nIndex reprezinta numarul mesajului in conversatie.\n   ->In cazul in care userul nu exista, sau este offline va returna un\nstatus de eroare. Pentru cazul in care user-ul este offline,\nmesajul va fi trimis, iar acesta il va primi cand va deveni online.\n[conv:<username>]\n   ->Permite unui user sa obtina conversatia cu un anumit username.\n   ->In cazul in care, nu are o conversatie cu acesta, userul nu exista\nsau conversatia este goala, va returna un status de eroare.\n[inbox]\n   ->Permite unui user sa vada inbox-ul de mesaje primite cand a fost\noffline.\n   ->In cazul in care inbox-ul este gol, returneaza un status de eroare.\n[who]\n   ->Spune utilizatorului ce utilizatori sunt online la momentul curent.\n[getinfo:<username>]\n   ->Permite unui user sa afle informatii despre un anumit username.\n->In cazul in care userul nu exista, retureaza un status de eroare.\n[goto <number>n]   ->Permite unui itilizator sa intre intr-un anumit room.\n[room <message>]\n   ->Permite unui utilizator sa comunice simulan cu toti userii din \nroom-ul curent. By default, orice user se afla in ROOM_DEFAULT.\n   ->In cazul in care cineva incearca sa trimita mesaj in \nROOM_DEFAULT, va primi un status de eroare.\n[exit]   ->Deconecteaza un user si inchide programul clientului.\n");
	strcat(MANUAL,"MANUAL#######################################################################\n");
	if(-1 == write(Sock,MANUAL,strlen(MANUAL))) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
	{
		perror("Error at writing the manual!\n");
		exit(errno);
	}
}

void reply(int Sock, char * command)
{
	int isonline = false;
	int sent = false; /* Reprezinta daca mesajul a fost trimis sau nu */
	char username[25];/* Userului caruia ii dam reply */
	bzero(username,25);
	int start = obtain_username(command+6,username); /* Start se refera la locul in care incepe numarul */
	int *destination = getComSockbyname(username,false);
	char numberString[25];
	bzero(numberString,25);
	int newstart = obtain_username(command+6+start+1,numberString); /* Obtinem practic numarul */
	int number = atoi(numberString);/* Obtinem mesajul la care dorim sa dam reply */
	if(NOT_EXISTING_USER(username) == true) /* Daca userul nu exista, nu putem avea o conversatie cu acesta */\
	{
		if(-1 == write(Sock,"#############################################################################\n[SERVER]User-ul nu exista!\n#############################################################################\n",184)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
		{
			perror("Error at writing no such user in reply!\n");
			exit(errno);
		}
	}
	else /* Daca userul exista, verificam daca avem fisier de conversatie cu acesta */
	{
		UsersInf * current_usr = getnamebySock(Sock);
		char path[70]; bzero(path,70); strcat(path,"Conversations/");  strcat(path,(* current_usr).username);  strcat(path,"/"); strcat(path,username);
		int fd;
		if(-1 == access(path,F_OK)) /* Verificam daca fisierul exista, si daca nu exista, il cream */ 
		{
			int fd;/* Fisierul nu exista, deci il cream */
			if( -1 == (fd = open(path, O_CREAT|O_RDWR|O_TRUNC,S_IRWXU | S_IRGRP | S_IROTH)))
			{
				perror("Error at creating the the file for destination messages in reply!\n");
				exit(errno);
			}
			close(fd);
		}
		else /* Daca fisierul exista, trimitem un reply*/
		{
			/* Deschidem fisierul pentru a obtine reply-ul cerut */
			if(-1 == (fd = open(path,O_RDONLY)))
			{
				perror("Error at opening the file in reply!\n");
				exit(errno);
			}
			/* Citim numarul de mesaje */
			int number_messages = 0;
			if(-1 == read(fd,&number_messages,sizeof(int)))
			{
				perror("Error at reading the number_messages in reply!\n");
				exit(errno);
			}
			if(number_messages != 0)
			{
				/* Citim mesajele si trimitem reply la cel bun */
				int index = 0,size = 0;
				char buffer[300];
				bzero(buffer,300);
				for( index = 0; index < number_messages; index++)
				{	
					int message_size = 0;/* Citim dimensiunea mesajului curent */
					if(-1 == read(fd,&message_size,sizeof(int)))
					{
						perror("Error at reading the size of some message in reply!\n");
						exit(errno);
					}
					if(-1 == (size = read(fd,buffer,message_size))) /* Citim mesajul curent */
					{
						perror("Error at reading some message in reply!\n");
						exit(errno);
					}
					if(index+1 == number)
					{
						/* Trimitem un String ce marcheaza inceputul reply-ului */
						if(&NOT_CONNECTED != getComSockbyname(username,true))
						if(-1 == write((*destination),"####################################REPLY####################################\n",79)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
						{
							perror("Error at writing beggining of reply in reply!\n");
							exit(errno);
						}
						char Message[500];
						bzero(Message,499);
						strcat(Message,"[FROM -> "); UsersInf * name = getnamebySock(Sock); strcat(Message,(*name).username);strcat(Message,"][REPLY]");
						strcat(Message,"<<<");strcat(Message,buffer);strcat(Message,">>>");strcat(Message,command+start+newstart+8);
						if(&NOT_CONNECTED != getComSockbyname(username,true))
						if(-1 == write((*destination),Message,strlen(Message))) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
						{
							perror("Error at writing some message in reply!\n");
							exit(errno);
						}
						if(&NOT_CONNECTED != getComSockbyname(username,true))
						if(-1 == write((*destination),"####################################REPLY####################################\n",79)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
						{
							perror("Error at writing end of reply in reply!\n");
							exit(errno);
						}
						int widthout = 0;
						int par = 0;
						while(par != 2)
						{	if(Message[widthout] == ']')
								par++;
							widthout++;
						}
						if(&NOT_CONNECTED != getComSockbyname(username,true))
						{	required_writes(Sock,username,Message+widthout,ONLINE); isonline = true;} 
						else
							required_writes(Sock,username,Message+widthout,OFFLINE);
						sent = true;
						break;
					}
					bzero(buffer,size);
				}
			}
			if(sent == false)/* Inseamna ca nu am putut trimite reply-ul */
				if(-1 == write(Sock,"####################################REPLY####################################\n[SERVER]Conversatie goala sau numarul reply-ul nu se afla in limitele actuale!\n####################################REPLY####################################\n",236)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
				{
					perror("Error at writing empty conversation in reply!\n");
					exit(errno);
				}
			if(isonline == false)
				if(-1 == write(Sock,"####################################REPLY####################################\n[SERVER]Userul nu este online!\n[SERVER]Va citi mesajul cand se va conecta!\n####################################REPLY####################################\n",232)) /* Trimitem mesajul curent din inbox inapoi clientului pentru a-l citi */
				{
					perror("Error at writing empty conversation in reply!\n");
					exit(errno);
				}
			/* Trimitem un String ce marcheaza finalul Conversatiei */
			close(fd);/* Inchidem descriptorul de fisier */
		}
		
	}
	
}

void logout(int Sock)
{
	UsersInf * current_usr = getnamebySock(Sock);
	printf("[STATUS]Userul %s s-a deconectat!\n",(* current_usr).username);
	(* current_usr).status = OFFLINE;
	FD_CLR(Sock,&readfds);
	FD_CLR(Sock,&actfds);
	bcopy((char *) &actfds,(char *) &readfds,sizeof(actfds));
	if( -1 == write(Sock,"[SERVER]Te-ai delogat cu success!\n",35))
	{
		perror("Error at logout in logout!\n");
		exit(1);
	}
	pthread_detach(pthread_self()); /* Dupa ce thread-ul se termina, memoria este eliberata */
	pthread_exit(NULL); /* thread-ul curent se termina */
}

int Check_administrator(int fd)
{
	/* Obtinem numele asociat acestui fd */
	 UsersInf * user = getnamebySock(fd); 
	 int i = 0;
	 for( i = 0 ; i < admins_no ; i++)
		if(strcmp(admins[i],(*user).username) == 0)
		{
			return true;
		}
		
	return false;
}

void Ban_user(int Comm_sock, char * command)
{
	int fd;
	if( -1 == (fd = open("Server/banned_users.txt", O_RDWR))) /* Deschidem fisierul pt a citi */
	{
		perror("Error at opening the file for the banned users in Ban_user!\n");
		exit(errno);
	}
	int banned_number = 0;
	if( -1 == read(fd,&banned_number,sizeof(int)))/* Citim numarul de useri banati pana acum */
	{
		perror("Error at reading the banned_number in Ban_user!\n");
		exit(errno);
	}
	if(banned_number != 0)
		if(-1 == lseek(fd,-sizeof(int),SEEK_CUR)) /* Mutam din nou cursorul la inceput */
		{
			perror("Error at lseek to end in Ban_user!\n"); 
			exit(errno);	
		}
	banned_number++;/* Incrementam numarul de user banati cu 1 */
	if(-1 == write(fd,&banned_number,sizeof(int))) 
	{
		perror("Error at writing the banned_number in Ban_user!\n");
		exit(errno);
	}
	if(-1 == lseek(fd,0,SEEK_END)) /* Mutam cursurul la finalul fisierului pentru a mai scrie numele unui user banat */
	{
		perror("Error at lseek to end in write_unseen!\n"); 
		exit(errno);	
	}
	char username[50]; bzero(username,50); /* Obtinem numele userului */
	strcpy(username,command+4);
	username[strlen(username)-1] = 0;
	while(strlen(username)!=25) strcat(username," ");
	if(-1 == write(fd,username,25)) /* Writing the message in unseen.txt */
	{
		perror("Error at writing username in Ban_user!\n");
		exit(errno);
	}
	close(fd);
	if(-1 == write(Comm_sock,"#############################################################################\n[SERVER] Userul a fost banat!\n#############################################################################\n",187)) /* Writing the message in unseen.txt */
	{
		perror("Error at writing the message in unseen.txt!\n");
		exit(errno);
	}
}

void Unban_user(int Comm_sock, char * command)
{
	char username[26];bzero(username,26);
	strcpy(username,command+6); username[strlen(username)-1] = 0;
	int fd;
	if( -1 == (fd = open("Server/banned_users.txt", O_RDWR))) /* Deschidem fisierul pt a citi */
	{
		perror("Error at opening the file for the banned users in Unban_user!\n");
		exit(errno);
	}
	int banned_number = 0;
	if( -1 == read(fd,&banned_number,sizeof(int)))/* Citim numarul de useri banati pana acum */
	{
		perror("Error at reading the banned_number in Unban_user!\n");
		exit(errno);
	}
	int found = false,index;
	char user_banned[26];
	for(index = 0; index < banned_number; index ++)
	{
		bzero(user_banned,26);
		if( -1 == read(fd,user_banned,25))
		{
			perror("Error at reading the username in Unban_user!\n");
			exit(errno);
		}
		int i = 0; while(user_banned[i]!=' ') i++; user_banned[i] = 0;
		if(strcmp(username,user_banned) == 0)
		{
			if(-1 == lseek(fd,-25,SEEK_CUR)) /* Mutam din nou cursorul inainte de a citi numele */
			{
				perror("Error at lseek to end in Ban_user!\n"); 
				exit(errno);	
			}
			if(-1 == write(fd,"                         ",25)) 
			{
				perror("Error at writing the banned_number in Ban_user!\n");
				exit(errno);
			}
			close(fd);
			found = true;
			if(-1 == write(Comm_sock,"#############################################################################\n[SERVER]Userul a fost debanat.\n#############################################################################\n",188)) /* Writing the message in unseen.txt */
			{
				perror("Error at writing the message in unseen.txt!\n");
				exit(errno);
			}
		}
	}
	if(found == false) /* Userul nu a fost gasit in fiser */
	{
		if(-1 == write(Comm_sock,"#############################################################################\n[SERVER]Userul nu a fost gasit in lista utilizatorilor banati!\n#############################################################################\n",220)) /* Writing the message in unseen.txt */
		{
			perror("Error at writing the message in unseen.txt!\n");
			exit(errno);
		}
	}


}

static void * HANDLE_REQUEST(void * arg)
{
  	int Comm_sock = *((int *)arg),size = 0;
	char command[251];
	bzero(command,251);
    if( -1 == (size = read (Comm_sock,command,250)) )
	{
		pthread_detach(pthread_self()); /* Dupa ce thread-ul se termina, memoria este eliberata */
		pthread_exit(NULL); /* thread-ul curent se termina */
	}
	if(size == 0)
	{
		UsersInf * current_usr = getnamebySock(Comm_sock);
		(* current_usr).status = OFFLINE;
		printf("Userul [%s] s-a deconectat subit!\n",(* current_usr).username);
		FD_CLR(Comm_sock,&actfds);
		FD_CLR(Comm_sock,&readfds);
		bcopy((char *) &actfds,(char *) &readfds,sizeof(actfds));
		pthread_detach(pthread_self()); /* Dupa ce thread-ul se termina, memoria este eliberata */
		pthread_exit(NULL); /* thread-ul curent se termina */
	}
	else
	{
		command[size] = 0;
		if(strstr(command,"send") != NULL)	
			call_send(Comm_sock,command,size);
		else if(strstr(command,"goto") != NULL)
			move_room(Comm_sock,command);
		else if(strstr(command,"room") != NULL)
			send_room(command,Comm_sock);
		else if(strstr(command,"getinfo") != NULL)
			getuser_Inf(command,Comm_sock);
		else if(strstr(command,"who") != NULL)
			getONLINE(Comm_sock);
		else if(strstr(command,"inbox") != NULL)
			getInbox(Comm_sock);
		else if(strstr(command,"conv:") != NULL)
			getConv(Comm_sock,command);
		else if(strstr(command,"reply:") != NULL)
			reply(Comm_sock,command);
		else if(strstr(command,"manual") != NULL)
			manual(Comm_sock);
		else if(strstr(command,"exit") != NULL)
			logout(Comm_sock);
		else if(strstr(command,"unban") != NULL && Check_administrator(Comm_sock) == true)
			Unban_user(Comm_sock,command);
		else if (strstr(command,"ban:")!= NULL && Check_administrator(Comm_sock) == true)
			Ban_user(Comm_sock,command);
		else if( -1 == write(Comm_sock,"#############################################################################\n[SERVER]Comanda necunoscuta! Introduceti comanda <manual> pentru ajutor!\n#############################################################################\n",230))
		{
			perror("Error at sending the unknown command message in HANDLE_REQUEST!");
			exit(errno);
		}
		pthread_detach(pthread_self()); /* Dupa ce thread-ul se termina, memoria este eliberata */
		pthread_exit(NULL); /* thread-ul curent se termina */
	}
}

void MULTIPLEXING_SELECT_mainthread()
{
	struct timeval tv; /* Structura de timp pentru select */
	tv.tv_sec = 1; /* Se vor astepta 1 secunda. */
    tv.tv_usec = 0; /* Se va astepta 0 microsecunde */
	FD_ZERO(&actfds); /* Initial multimea este vida(FD_ZERO sterge toti descriptori)
    dintr-o multime de descriptori)*/
    FD_ZERO(&readfds);
	int comm_sock[100], count = 0;
	while(DO_FOREVER)
    {
        /* Ajustam la fiecare pasi numarul de descriptori activi(efectiv utilizati) */
        bcopy((char *) &actfds,(char *) &readfds,sizeof(actfds));/* Primul pas nu copiem nimic */

        if( 0 > select(nfds+1,&readfds,NULL,NULL,&tv) )
        {
            perror("Error at select in MULTIPLEXING_SELECT_mainthread!\n");
            exit(errno);
        }
		int fd;
		pthread_t Request_thread;
        for(fd = 2 ; fd <= nfds; fd++)
        if(FD_ISSET(fd,&readfds)) /* Daca acel descriptor se afla in readfds */
        {
			if(count == 99)
				count = 0;
			comm_sock[count] = fd;
			pthread_create(&Request_thread,NULL,&HANDLE_REQUEST,&comm_sock[count]);/* Un thread separat de va ocupa de requestul acestui user */
			count++;
		}       
    }
}

void Alocate_server_folder()
{
	char path[70];
	bzero(path,70);
	if(-1 == access("Server",F_OK)) /* Daca folderul serverului nu exista, atunci il cream */
	{	strcat(path,"Server");
		if(-1 == mkdir(path, S_IRWXU | S_IRGRP | S_IROTH )) 
		{
			perror("Error at creating the folder in Ban_user!\n");
			exit(errno);
		}
	}
	strcat(path,"/banned_users.txt");
	int fd;
	if(-1 == access("Server/banned_users.txt",F_OK)) /* Daca fisierul nu exista cu useri banati, atunci il */
	{
		if( -1 == (fd = open(path, O_CREAT|O_RDWR|O_TRUNC,S_IRWXU | S_IRGRP | S_IROTH)))
		{
			perror("Error at creating the file for the banned users in Ban_user!\n");
			exit(errno);
		}
	}
	close(fd);
}

int main()
{
	Users_Initialization();
	Alocate_server_folder();
	ListenSock_prepare();
	/* Creating a second thread that only handles Acception conection, Login and Register. */
	pthread_t Connection_thread;
	pthread_create(&Connection_thread, NULL, &AcceptConnection_thread, NULL);
	MULTIPLEXING_SELECT_mainthread();
	return 0;
}
