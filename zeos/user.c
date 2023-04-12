#include <libc.h>


int pid;

void fork_test() {
  char *buff;
  buff = "\n\nFORK TEST\n";
  if (write(1, buff, strlen(buff)) == -1) perror();
  buff = "Creando hijo\n";
  if (write(1, buff, strlen(buff)) == -1) perror();

  int pid = fork();
  char pidbuff[16];
  itoa(pid, pidbuff);
  /* if (write(1, pidbuff, strlen(pidbuff)) == -1) perror(); */
  char ownpid[16];

  char *testing_data = "fragmento de datos del padre copiado en el hijo\n";

  switch(pid) {
  case -1:
    perror();
    break;
  case 0:
    buff = "\nHIJO: getpid == ";
    if (write(1, buff, strlen(buff)) == -1) perror();
    itoa(getpid(), ownpid);
    if (write(1, ownpid, strlen(ownpid)) == -1) perror();
    buff = "\n";
    if (write(1, buff, strlen(buff)) == -1) perror();
    if (write(1, testing_data, strlen(testing_data)) == -1) perror();
    break;
  default:
    buff = "PADRE: getpid == ";
    if (write(1, buff, strlen(buff)) == -1) perror();
    itoa(getpid(), ownpid);
    if (write(1, ownpid, strlen(ownpid)) == -1) perror();

    buff = " -- pid de mi hijo == ";
    if (write(1, buff, strlen(buff)) == -1) perror();
    if (write(1, pidbuff, strlen(pidbuff)) == -1) perror();
    buff = "\n";
    if (write(1, buff, strlen(buff)) == -1) perror();



    break;
  }

  buff = "INFO: El codigo siguiente lo tendrian que ejecutar padre e hijo\n";
  if (write(1, buff, strlen(buff)) == -1) perror();

  buff = "Saludos desde getpid == ";
  if (write(1, buff, strlen(buff)) == -1) perror();

  itoa(getpid(), ownpid);
  if (write(1, ownpid, strlen(ownpid)) == -1) perror();

  buff = "\n";
  if (write(1, buff, strlen(buff)) == -1) perror();
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
  
fork_test();
struct stats st;
get_stats(getpid(), &st);
char *buff = "User ticks ";
if (write(1,buff,strlen(buff))==-1) perror();
itoa(st.elapsed_total_ticks,buff);
if (write(1,buff,strlen(buff))==-1) perror();
  while(1) { }
}
 