#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <chrono>


int main() {
    const char* filePath = "./mnt/125"; // 替换为你挂载的FUSE文件路径
    const size_t bufferSize = 4096; // 假设读取4KB的数据
    char buffer[bufferSize];

    // 打开文件
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return 1;
    }

    // 记录读取开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 执行读取操作，触发FUSE的read调用
    ssize_t bytesRead = read(fd, buffer, bufferSize);

    // 记录读取结束时间
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> latency = end - start;

    // 检查读取结果
    if (bytesRead >= 0) {
        std::cout << "Read successful. Bytes read: " << bytesRead << std::endl;
        std::cout << "Read latency: " << latency.count() << " ms" << std::endl;
    } else {
        std::cerr << "Read failed with error." << std::endl;
    }

    // 关闭文件
    // close(fd);

    return 0;
}

// int main() {
//     const char* filename = "./mnt/newfile.txt"; // FUSE 挂载点下的文件路径
//     mode_t mode = 0640; // 文件权限

//     // 使用 creat 系统调用单独触发 create 操作
//     int fd = creat(filename, mode);

//     if (fd == -1) {
//         std::cerr << "Failed to create file: " << strerror(errno) << std::endl;
//         return 1;
//     }

//     std::cout << "File created successfully." << std::endl;

//     // 关闭文件描述符
//     close(fd);

//     return 0;
// }