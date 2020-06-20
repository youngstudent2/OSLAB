#include "lib.h"
#include "types.h"
int uEntry(void)
{
    
    int fd = 0;
    int i = 0;
    char tmp = 0;

    ls("/");      // 列出"/"⽬录下的所有⽂件
    ls("/boot/"); // 列出"/boot/"⽬录下的所有⽂件
    ls("/dev/");  // 列出"/dev/"⽬录下的所有⽂件
    ls("/usr/");  // 列出"/usr/"⽬录下的所有⽂件

    printf("create /usr/test and write alphabets to it\n");
    fd = open("/usr/test", O_WRITE | O_CREATE); // 创建⽂件"/usr/test"
    for (i = 0; i < 512; i++)
    { // 向"/usr/test"⽂件中写⼊字⺟表
        tmp = (char)(i % 26 + 'A');
        write(fd, (uint8_t *)&tmp, 1);
    }
    close(fd);
    ls("/usr/");      // 列出"/usr/"⽬录下的所有⽂件
    cat("/usr/test"); // 在终端中打印"/usr/test"⽂件的内容
    int ret = remove("/usr/test");
    printf("%d\n",ret);
    ls("/usr/");
    exit();
    return 0;
}