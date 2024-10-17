#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>

int main() {
    const char* filename = "./mnt/newfile.txt"; // FUSE 挂载点下的文件路径
    mode_t mode = 0640; // 文件权限

    // 使用 creat 系统调用单独触发 create 操作
    int fd = creat(filename, mode);

    if (fd == -1) {
        std::cerr << "Failed to create file: " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "File created successfully." << std::endl;

    // 关闭文件描述符
    close(fd);

    return 0;
}