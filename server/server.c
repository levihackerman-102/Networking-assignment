#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TRUE 1
#define FALSE 0
// data file to store users and emails
#define USERS_DAT_FILE "server/users.dat"
#define MAILS_DAT_FILE "server/mails.dat"

// struct for auth creds 
struct SMTP_AUTH_CREDS {
  char email[50];
  char password[50];
};

// struct for user info
struct USER {
  char name[50];
  char email[50];
  char password[50];
};

// struct for status message
struct STATUS_MSG {
  int status_code;
  char status_msg[50];
};

struct MAIL {
  char from[50];
  char to[50];
  char subject[50];
  char body[100];
};

// struct for inbox details
struct INBOX {
  int count;
  struct MAIL mails[100];
};

int login(int client_sockfd, struct SMTP_AUTH_CREDS *, struct USER *); // login function
int create_user_and_login(int client_sockfd, struct USER *, struct USER *); // signup and login function
void send_status(int client_sockfd, int status_code, char *status_msg); // function to send back status code
void store_mail(int client_sockfd); // function to store emails in the mails.dat file
void send_inbox(int client_sockfd, struct USER *); // function to return inbox to user


int main(int argc, char *argv[])
{
  struct  sockaddr_in server_addr, client_addr;
  struct  SMTP_AUTH_CREDS auth_creds;
  struct  USER user, user_check;
  int     sockfd, client_sockfd, port, client_len, option, is_logged_in;
  FILE    *fileptr;

  // insufficient number of arguments
  if(argc != 3) {
    fprintf(stderr, "Invalid Arguments\n");
    fprintf(stderr, "./server -p <port number>\n");
    return -1;
  }

  port = atoi(argv[2]); // assign port number
  client_len = sizeof(client_addr);

  // create a socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0) {
    perror("Socket error : ");
    return -1;
  }

  server_addr.sin_family = AF_INET; // AF_INET for IPv4
  server_addr.sin_addr.s_addr = INADDR_ANY; // IPv4
  server_addr.sin_port = htons(port); // host to network style change

  // bind socket to a port on localhost
  if ( bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0 ) {
    perror("Bind error : ");
    return -1;
  }
  
  // listen for data on socket
  if ( listen(sockfd, 5) < 0) {
    perror("Listen error : ");
    return -1;
  }
  
  while(TRUE) {
    
    // accept incoming connection 
    if ( (client_sockfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_len)) < 0) {
      perror("Accept error : ");
      continue;
    }

    // receive option from client
    recv(client_sockfd, &option, sizeof(option), 0);

    // set is_logged_in flag 
    switch (option)
    {
      case 1:
        is_logged_in = login(client_sockfd, &auth_creds, &user);
        break;
    
      case 2:
        is_logged_in = create_user_and_login(client_sockfd, &user, &user_check);
        break;

      default:
        break;
    }

    if(is_logged_in == FALSE)
      continue; 

    while (TRUE)
    {

      // receive option from client
      recv(client_sockfd, &option, sizeof(option), 0);

      switch (option)
      {
        case 1:
          send_inbox(client_sockfd, &user);
          break;
      
        case 2:
          store_mail(client_sockfd);
          break;

        case 3:
          break;

        default:
          printf("Choose an option to proceed\n");
          break;
      }

      if(option == 3) break;
    }  

    close(client_sockfd); 
  }
  
  close(sockfd);
}

int login(int client_sockfd,  struct SMTP_AUTH_CREDS *auth_creds, struct USER *user)
{
  FILE   *users_dat;
  recv(client_sockfd, auth_creds, sizeof(*auth_creds), 0);
  users_dat = fopen(USERS_DAT_FILE, "rb");
  
  if(users_dat == NULL)
  {
    send_status(client_sockfd, 500, "NO USER EXISTS!");
    return FALSE;
  }

  while (!feof(users_dat))
  {
    fread(user, sizeof(struct USER), 1, users_dat);
   
    if ( strcmp(user->email, auth_creds->email) == 0)
    {
      if ( strcmp(user->password, auth_creds->password) == 0)
      {
        send_status(client_sockfd, 200, "Logged In Successfully");
        fclose(users_dat);
        return TRUE;
      }
      else
      {
        send_status(client_sockfd, 403, "Password Is Incorrect");
        fclose(users_dat);
        return FALSE;
      }
    }
  
  }

  send_status(client_sockfd, 403, "Invalid Email and Password");
  fclose(users_dat);
  return FALSE;
}

int create_user_and_login(int client_sockfd, struct USER *user, struct USER *user_check)
{
  FILE   *users_dat;
  
  recv(client_sockfd, user, sizeof(*user), 0);
  
  users_dat = fopen(USERS_DAT_FILE, "ab+");
  if(users_dat == NULL)
  {
    send_status(client_sockfd, 500, "Internal Server Error");
    fclose(users_dat);
    return FALSE;
  } 
 fread(user_check, sizeof(struct USER), 1, users_dat);
   
    if ( strcmp(user->name, user_check->name) != 0)
    {
      if ( strcmp(user->email, user_check->email) != 0)
      {
        fwrite(user, sizeof(struct USER), 1, users_dat);
        send_status(client_sockfd, 200, "OK");
        fclose(users_dat);
        return TRUE;
      }
      else
      {
        send_status(client_sockfd, 403, "Email already exists");
        fclose(users_dat);
        return FALSE;
      }
    }
  send_status(client_sockfd, 403, "Username or Email already exists");
  fclose(users_dat);
  return FALSE;  
  
}


void send_status(int client_sockfd, int status_code, char *status_msg)
{
  struct STATUS_MSG status;
  status.status_code = status_code;
  strcpy(status.status_msg, status_msg);
  send(client_sockfd, &status, sizeof(status), 0);
}

void store_mail(int client_sockfd)
{
  struct  MAIL mail;
  struct  USER user;
  FILE    *mails_dat, *users_dat;
  int     found = FALSE;

  recv(client_sockfd, &mail, sizeof(mail), 0);

  users_dat = fopen(USERS_DAT_FILE, "rb");
  mails_dat = fopen(MAILS_DAT_FILE, "ab+");

  while (!feof(users_dat))
  {
    fread(&user, sizeof(struct USER), 1, users_dat);
   // printf("%s - %s\n", user.email, mail.to);
    if ( strcmp(user.email, mail.to) == 0 )
    {
      found = TRUE;
      break;
    }
  }

  if (!found)
  {
    send_status(client_sockfd, 404, "Recipient Address doesn't exist");
    fclose(users_dat);
    fclose(mails_dat);
    return;
  }

  fwrite(&mail, sizeof(struct MAIL), 1, mails_dat);

  perror("fwrite");

  send_status(client_sockfd, 200, "Successfully sent");

  fclose(users_dat);
  fclose(mails_dat);
}


void send_inbox(int client_sockfd, struct USER *user)
{
  struct INBOX inbox;
  struct MAIL temp;
  int    i = 0;
  FILE   *mails_dat;

  mails_dat = fopen(MAILS_DAT_FILE, "rb");

  while(!feof(mails_dat)){
    fread(&temp, sizeof(struct MAIL), 1, mails_dat);
    if( strcmp(temp.to, user->email) == 0 )
    {
      inbox.mails[i++]= temp;
    }
  }
  
  inbox.count = i - 1;
  
  send(client_sockfd, &inbox, sizeof(struct INBOX), 0);

  fclose(mails_dat);
}
