#include <iostream>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

// Measure latency of `getattr` operation
double measureGetattrLatency(const char* filePath) {
    struct stat st;

    auto start = std::chrono::high_resolution_clock::now();
    int result = stat(filePath, &st);
    auto end = std::chrono::high_resolution_clock::now();

    if (result != 0) {
        std::cerr << "Failed to get attributes for: " << filePath << std::endl;
        return -1.0;
    }

    std::chrono::duration<double, std::milli> latency = end - start;
    return latency.count();
}

// Measure latency of `mkdir` operation
double measureMkdirLatency(const char* dirPath, mode_t mode = 0755) {
    auto start = std::chrono::high_resolution_clock::now();
    int result = mkdir(dirPath, mode);
    auto end = std::chrono::high_resolution_clock::now();

    if (result != 0) {
        std::cerr << "Failed to create directory: " << dirPath << std::endl;
        return -1.0;
    }

    std::chrono::duration<double, std::milli> latency = end - start;
    return latency.count();
}

// Measure latency of `unlink` operation
double measureUnlinkLatency(const char* filePath) {
    auto start = std::chrono::high_resolution_clock::now();
    int result = unlink(filePath);
    auto end = std::chrono::high_resolution_clock::now();

    if (result != 0) {
        std::cerr << "Failed to unlink file: " << filePath << std::endl;
        return -1.0;
    }

    std::chrono::duration<double, std::milli> latency = end - start;
    return latency.count();
}

// Measure latency of `rmdir` operation
double measureRmdirLatency(const char* dirPath) {
    auto start = std::chrono::high_resolution_clock::now();
    int result = rmdir(dirPath);
    auto end = std::chrono::high_resolution_clock::now();

    if (result != 0) {
        std::cerr << "Failed to remove directory: " << dirPath << std::endl;
        return -1.0;
    }

    std::chrono::duration<double, std::milli> latency = end - start;
    return latency.count();
}

// Measure latency of `open` operation
double measureOpenLatency(const char* filePath) {
    auto start = std::chrono::high_resolution_clock::now();
    int fd = open(filePath, O_RDONLY);
    auto end = std::chrono::high_resolution_clock::now();

    if (fd == -1) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return -1.0;
    }
    close(fd);

    std::chrono::duration<double, std::milli> latency = end - start;
    return latency.count();
}

// Measure latency of `read` operation
double measureReadLatency(const char* filePath, size_t bufferSize) {
    char buffer[bufferSize];

    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return -1.0;
    }

    auto start = std::chrono::high_resolution_clock::now();
    ssize_t bytesRead = read(fd, buffer, bufferSize);
    auto end = std::chrono::high_resolution_clock::now();

    close(fd);

    if (bytesRead < 0) {
        return -1.0;
    }

    std::chrono::duration<double, std::milli> latency = end - start;
    return latency.count();
}

// Measure latency of `readdir` operation
double measureReaddirLatency(const char* dirPath) {
    auto start = std::chrono::high_resolution_clock::now();
    DIR* dir = opendir(dirPath);
    if (!dir) {
        std::cerr << "Failed to open directory: " << dirPath << std::endl;
        return -1.0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Process directory entry here if needed
    }
    closedir(dir);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> latency = end - start;
    return latency.count();
}

// Measure latency of `create` operation
double measureCreateLatency(const char* filePath, mode_t mode = 0644) {
    auto start = std::chrono::high_resolution_clock::now();
    int fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, mode);
    auto end = std::chrono::high_resolution_clock::now();

    if (fd == -1) {
        std::cerr << "Failed to create file: " << filePath << std::endl;
        return -1.0;
    }
    close(fd);

    std::chrono::duration<double, std::milli> latency = end - start;
    return latency.count();
}

int main() {
    const char* testDirPath = "./mnt/testdir";
    const char* testExistFilePath = "./mnt/1255";
    const char* testFilePath = "./mnt/testdir/testfile";
    const size_t bufferSize = 4096;

    // Test getattr
    double getattrLatency = measureGetattrLatency(testExistFilePath);
    if (getattrLatency >= 0) {
        std::cout << "Getattr latency: " << getattrLatency << " ms" << std::endl;
    }

    // Test mkdir
    double mkdirLatency = measureMkdirLatency(testDirPath);
    if (mkdirLatency >= 0) {
        std::cout << "Mkdir latency: " << mkdirLatency << " ms" << std::endl;
    }

    // Test create
    double createLatency = measureCreateLatency(testFilePath);
    if (createLatency >= 0) {
        std::cout << "Create latency: " << createLatency << " ms" << std::endl;
    }

    // Test open
    double openLatency = measureOpenLatency(testFilePath);
    if (openLatency >= 0) {
        std::cout << "Open latency: " << openLatency << " ms" << std::endl;
    }

    // Test read
    double readLatency = measureReadLatency(testExistFilePath, bufferSize);
    if (readLatency >= 0) {
        std::cout << "Read latency: " << readLatency << " ms" << std::endl;
    }

    // Test readdir
    double readdirLatency = measureReaddirLatency(testDirPath);
    if (readdirLatency >= 0) {
        std::cout << "Readdir latency: " << readdirLatency << " ms" << std::endl;
    }

    // Test unlink
    double unlinkLatency = measureUnlinkLatency(testFilePath);
    if (unlinkLatency >= 0) {
        std::cout << "Unlink latency: " << unlinkLatency << " ms" << std::endl;
    }

    // Test rmdir
    double rmdirLatency = measureRmdirLatency(testDirPath);
    if (rmdirLatency >= 0) {
        std::cout << "Rmdir latency: " << rmdirLatency << " ms" << std::endl;
    }

    return 0;
}
