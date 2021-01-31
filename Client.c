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

#define KEYBOARD 0
#define SCREEN 1
#define DO_FOREVER 1
#define true 1
#define false 0

int CONNECTED = 0;
extern int errno; /* Codul de eroare returnat de unele functii */
int port; /* Portul folosit pentru conectare */
int Comm_sock;			// descriptorul de socket pentru comunicarea cu serverul
char username[25];

void Connect_to_server(char IP[50],char PORT[10])
{
    if ( -1 == (Comm_sock = socket (AF_INET, SOCK_STREAM, 0)) )
    {
        perror ("Error at creating the socket in Config_Socket!\n");
        exit(errno);
    }
    struct sockaddr_in server;
    port = atoi (PORT);
    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(IP);
    /* portul de conectare */
    server.sin_port = htons (port);
    /* ne conectam la server */
    if (connect (Comm_sock, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("Error at connecting in the server!\n");
      exit(errno);
    }
}

void Requesting_registration()
{
    char user_reg[121];
	/* Reading peoples information */
    memset(user_reg,32,121);
	int read_size;
	printf("[CLIENT-specifications]Completezi formularul de inregistrate!\n[CLIENT-specifications]Asigurate ca datele introduse sunt corecte!\n");
    printf("#############################################################################\n");
	printf("[CLIENT]Introduceti numele de utilizator: "); fflush(stdout);
	if( 0 >= (read_size = read(KEYBOARD,user_reg,25) ))
	{
		perror("Error at reading the username from the keyboard in Write_registration!\n");
		exit(errno);
	}
	user_reg[read_size-1] = 32;
	printf("[CLIENT]Introduceti parola: "); fflush(stdout);
	if( 0 >= (read_size = read(KEYBOARD,user_reg+25,25) ))
	{
		perror("Error at reading the password from the keyboard in Write_registration!\n");
		exit(errno);
	}
	user_reg[read_size+25-1] = 32;
	printf("[CLIENT]Introduceti-va prenumele: "); fflush(stdout);
	if( 0 >= (read_size = read(KEYBOARD,user_reg+50,20) ))
	{
		perror("Error at reading the name from the keyboard in Write_registration!\n");
		exit(errno);
	}
	user_reg[read_size+50-1] = 32;
	printf("[CLIENT]Introduceti-va numele: "); fflush(stdout);
	if( 0 >= (read_size = read(KEYBOARD,user_reg+70,20) ))
	{
		perror("Error at reading the surname from the keyboard in Write_registration!\n");
		exit(errno);
	}
	user_reg[read_size+70-1] = 32;
	printf("[CLIENT]Introduceti-va data de nastere(DD/MM/YYYY): "); fflush(stdout);
	if( 0 >= (read_size = read(KEYBOARD,user_reg+90,15) ))
	{
		perror("Error at reading the date of birth from the keyboard in Write_registration!\n");
		exit(errno);
	}
	user_reg[read_size+90-1] = 32;
	printf("[CLIENT]Introduceti-va sexul(MALE/FEMALE): "); fflush(stdout);
	if( 0 >= (read_size = read(KEYBOARD,user_reg+105,10) ))
	{
		perror("Error at reading the sex from the keyboard in Write_registration!\n");
		exit(errno);
	}
	user_reg[read_size+105-1] = 32;
    printf("#############################################################################\n");fflush(stdout);
    /* Sending to the server the information */
	if( -1 == write(Comm_sock,user_reg,120))
	{
		perror("Error at writing the registration in Write_registration!\n");
		exit(errno);
	}
}

int Requesting_login()
{
    char USER[25],PASS[25];
    bzero(USER,25);bzero(PASS,25);
    int size;
    printf("[CLIENT-specification]Completati procedura de logare!\n"); fflush(stdout);
    printf("#############################################################################\n");fflush(stdout);
    printf("[CLIENT]Introduceti username-ul: "); fflush(stdout);
    if(-1 == (size = read(KEYBOARD,USER,25)))/* Reading the username and password for login from the keyboard */
    {
       perror("Error at reading the username in the DO_LOGIN!\n");
       exit(errno);
    }
    USER[size - 1] = 0;
    printf("[CLIENT]Introduceti parola: "); fflush(stdout);
    if(-1 == (size = read(KEYBOARD,PASS,25)))
    {
       perror("Error at reading the password in the DO_LOGIN!\n");
       exit(errno);
    }
    PASS[size - 1] = 0;
    if(-1 == write(Comm_sock,USER,25)) /* Sending the username and password for login to the server */
    {
       perror("Error at writing the username in the DO_LOGIN!\n");
       exit(errno);
    }
    if(-1 == write(Comm_sock,PASS,25))
    {
       perror("Error at writing the password in the DO_LOGIN!\n");
       exit(errno);
    }
    char answer[300];
    bzero(answer,300);
    if(-1 == (size = read(Comm_sock,answer,300)))
    {
       perror("Error at reading the password in the DO_LOGIN!\n");
       exit(errno);
    }
    if( -1 == write(SCREEN,answer,size)) /* Scriem pe ecran mesajul primit */
    {
        perror("Error at writing the message from server to SCREEN in ReceiveFromServer_thread!\n");
        exit(errno);
    }
    if(strstr(answer,"succes") != NULL)
        return true; 
    return false;
}

void Register_Login()
{
    char command[50];
    while(DO_FOREVER)
    {
        printf("[CLIENT]Insereaza comanda: "); fflush(stdout);
        int size_command;
        if( -1 == ( size_command = read(KEYBOARD,command,50)) )/* Citim ce dorim sa facem (register/login) */
        {
            perror("Error at reading the command in the Register_Login!\n");
            exit(errno);
        } 
        command[size_command-1] = 0;
        if( strcmp(command,"login") == 0 )
        {
            if(-1 == write(Comm_sock,command,10))/* Reading the username and password for login from the keyboard */
            {
                perror("Error at reading the username in the DO_LOGIN!\n");
                exit(errno);
            }
            if( true == Requesting_login())
             { CONNECTED = true;  break;}
        }
        else if( strcmp(command,"register") == 0)
        {
            if(-1 == write(Comm_sock,command,10))/* Reading the username and password for login from the keyboard */
            {
                perror("Error at reading the username in the DO_LOGIN!\n");
                exit(errno);
            }
            char response[300];
            bzero(response,300);
            Requesting_registration();
            if(-1 == read(Comm_sock,response,300))
            {
                perror("Error at reading the asnwer from registration!\n");
                exit(errno);
            }
            printf("%s",response);
        }
        else
        {
            printf("[CLIENT-help]Comanda introdusa nu este cunoscuta. Va rog sa incercati din nou.\n[CLIENT-help]Singurele comenzi disponibile sunt <login> si <register>!\n");        
            printf("#############################################################################\n");fflush(stdout);
        }
    }
}

static void * ReceiveFromServer_thread(void * arg)
{
    char received[10000];
    bzero(received,10000);
    int size = 0;
    while(DO_FOREVER)
    { 
        if( -1 == (size = read(Comm_sock,received,10000)) ) /* Citim mesajul de la server */
        {
            perror("Error at receiving a message from server in ReceiveFromServer_thread!\n");
            exit(errno);
        }
        if(size == 0) {printf("[CLIENT]Conexiunea cu serverul s-a inchis..Inchidem aplicatia...\n"); exit(1);}
        if( -1 == write(SCREEN,received,size)) /* Scriem pe ecran mesajul primit */
        {
            perror("Error at writing the message from server to SCREEN in ReceiveFromServer_thread!\n");
            exit(errno);
        }
       
         if(strcmp("[SERVER]Te-ai delogat cu success!\n",received) == 0)
        {
            printf("[CLIENT]Clientul se va inchide...\n");
            exit(EXIT_SUCCESS);
        }
         bzero(received,size);
    }
}

void SendToServer_mainthread()
{
    char sent[10000];
    bzero(sent,10000);
    int size = 0;
    pthread_t receive_thread,send_thread;
    while(DO_FOREVER) 
    {
            bzero(sent,size + 1);
            if( -1 == (size = read(KEYBOARD,sent,10000)) ) /* Citim mesajul de la server */
            {
                perror("Error at reading a message from KEYBOARD in SendToServer_mainthread!\n");
                exit(errno);
            }
            if( -1 == write(Comm_sock,sent,size+1)) /* Scriem pe ecran mesajul primit */
            {
                perror("Error at sending the message to the server in SendToServer_mainthread!\n");
                exit(errno);
            }
    }
}


int main(int argc, char *argv[])
{
    if (argc != 3) /* Verificam daca am primit informatiile necesare pentru conectare */
    {
      printf ("[CLIENT]Sintaxa pentru client este: <adresa_server> <port>\n");
      exit(-1);
    }
    Connect_to_server(argv[1],argv[2]);/* Ne conectam la server */
    Register_Login();
    pthread_t receive_thread,send_thread;
	pthread_create(&receive_thread, NULL, &ReceiveFromServer_thread, NULL);
    SendToServer_mainthread();
    return 0;
}
