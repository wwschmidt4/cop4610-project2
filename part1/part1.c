// add five system calls

#include <unistd.h>

int main(void)
{
   getpid();
   getppid();
   getgid();
   getuid();
   chdir("./");
}