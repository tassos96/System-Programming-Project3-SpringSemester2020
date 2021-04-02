#ifndef WHO_SERVER_H
#define WHO_SERVER_H

void child_server(int newsock);
void perror_exit(char *message);
void sigchld_handler (int sig);
int getMaximumFD(int fd1, int fd2);
void read_data (int fd, char *msg_buffer, int msgTotalBytes);
int write_data(int fd, char* message);

#endif