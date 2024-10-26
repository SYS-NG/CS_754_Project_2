#include <iostream>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/ioctl.h>

#define SET_RUN_SYNC _IO('f', 2)

using namespace std;

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

// Measure Latency of `write` operation
double measureWriteLatency(const char* filePath, size_t bufferSize) {
    char buffer[bufferSize];
    std::fill_n(buffer, bufferSize, 'A'); // Fill the buffer with the character 'A'

    int fd = open(filePath, O_WRONLY);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return -1.0;
    }

    auto start = std::chrono::high_resolution_clock::now();

    ssize_t bytesRead = write(fd, buffer, bufferSize);
    close(fd);
    
    auto end = std::chrono::high_resolution_clock::now();

    if (bytesRead < 0) {
        return -1.0;
    }

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

// Large read and write are expected to use 1GB
double measureLargeWriteLatency(const char* largeFilePath) {
    const size_t fileSize = 1 * 1024 * 1024 * 1024; // 1 GB
    //const size_t fileSize = 1 * 1024 * 1024; // 1 MB
    const size_t bufferSize = 1 * 1024 * 1024; // 1 MB buffer
    char buffer[bufferSize];
    std::fill_n(buffer, bufferSize, 'A'); // Fill the buffer with the character 'A'

    auto start = std::chrono::high_resolution_clock::now();

    // Open the file for writing
    int fd = open(largeFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        std::cerr << "Failed to create file: " << largeFilePath << std::endl;
        return -1;
    }

    size_t totalWritten = 0;
    while (totalWritten < fileSize) {
        //ssize_t bytesToWrite = std::min(bufferSize, fileSize - totalWritten);
        ssize_t bytesToWrite = bufferSize;
        ssize_t bytesWritten = write(fd, buffer, bufferSize);
        // cout << "bytes to write: " << bytesToWrite << " bytes write: " << bytesWritten << endl;
        if (bytesWritten < 0) {
            std::cerr << "Failed to write to file: " << largeFilePath << std::endl;
            std::cerr << "Error code: " << errno << std::endl;
            close(fd);
            return -1;
        }
        totalWritten += bytesWritten;
        // std::cout << "Written " << totalWritten << " bytes to " << largeFilePath << std::endl;
    }

    close(fd);
    auto end = std::chrono::high_resolution_clock::now();

    struct stat fileStat;
    if (stat(largeFilePath, &fileStat) == 0) {
        if (fileStat.st_size == fileSize) {
            std::cout << "File size verified: " << fileStat.st_size << " bytes." << std::endl;
        } else {
            std::cerr << "File size mismatch: expected " << fileSize << " bytes, but got " << fileStat.st_size << " bytes." << std::endl;
        }
    } else {
        std::cerr << "Failed to get file status for: " << largeFilePath << std::endl;
    }
    
    std::chrono::duration<double, std::milli> latency = end - start;
    std::cout << "Successfully created 1GB file: " << largeFilePath << std::endl;
    return latency.count();
}

double measureLargeReadLatency(const char* largeFilePath) {
    const size_t fileSize = 1 * 1024 * 1024 * 1024; // 1 GB
    const size_t bufferSize = 4096; // 4 KB buffer
    char buffer[bufferSize];

    auto start = std::chrono::high_resolution_clock::now();

    // Open the file for reading
    int fd = open(largeFilePath, O_RDONLY);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << largeFilePath << std::endl;
        return -1.0;
    }

    size_t totalRead = 0;
    while (totalRead < fileSize) {
        ssize_t bytesRead = read(fd, buffer, bufferSize);
        if (bytesRead < 0) {
            std::cerr << "Failed to read from file: " << largeFilePath << std::endl;
            close(fd);
            return -1.0;
        }
        if (bytesRead == 0) {
            break; // End of file reached
        }
        totalRead += bytesRead;
    }

    close(fd);
    auto end = std::chrono::high_resolution_clock::now();

    int result = unlink(largeFilePath);

    if (result != 0) {
        std::cerr << "Failed to unlink file: " << largeFilePath << std::endl;
        return -1.0;
    }

    std::chrono::duration<double, std::milli> latency = end - start;
    std::cout << "Successfully read 1GB file: " << largeFilePath << " total bytes read: " << totalRead << std::endl;
    return latency.count();
}

// Function to test recovery after crash with parameterized write counts
double testRecoveryAfterCrash(const char* filePath, int preCrashWrites, int postCrashWrites) {
    const char data = 'a';  // 1 byte of data to write
    int fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
        return -1.0;
    }

    // Perform writes before the simulated crash
    for (int i = 0; i < preCrashWrites; ++i) {
        if (write(fd, &data, 1) == -1) {
            std::cerr << "Write operation " << (i + 1) << " before crash failed." << std::endl;
            close(fd);
            return -1.0;
        }
    }

    // Pause and wait for user input to simulate server recovery
    std::cout << "Pause for server recovery. Press Enter to continue..." << std::endl;
    std::cin.get();

    // Perform writes after the simulated crash
    for (int i = 0; i < postCrashWrites; ++i) {
        if (write(fd, &data, 1) == -1) {
            std::cerr << "Write operation " << (i + 1) << " after crash failed." << std::endl;
            close(fd);
            return -1.0;
        }
    }

    // Measure close operation latency
    auto start = std::chrono::high_resolution_clock::now();
    if (close(fd) == -1) {
        std::cerr << "Close operation failed." << std::endl;
        return -1.0;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> latency = end - start;

    // Delete the file
    if (unlink(filePath) == -1) {
        std::cerr << "Failed to delete file: " << filePath << std::endl;
        return -1.0;
    }

    // Return close operation latency in milliseconds
    return latency.count();
}

int changeMode(int fd, int mode) {
    if (ioctl(fd, SET_RUN_SYNC, reinterpret_cast<void*>(mode)) == -1) { 
        perror("ioctl");
        close(fd);
        return -1;
    }
    return 0;
}

int main() {
    const char* testDirPath = "./mnt/testdir";
    const char* testExistFilePath = "./mnt/1255";
    const char* testFilePath = "./mnt/testdir/testfile";
    const char* testLargeFilePath = "./mnt/test_1GB_file";
    const char* testFilePath2 = "./mnt/testfile2";

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

    // Test getattr
    double getattrLatency = measureGetattrLatency(testFilePath);
    if (getattrLatency >= 0) {
        std::cout << "Getattr latency: " << getattrLatency << " ms" << std::endl;
    }

    // Test open
    double openLatency = measureOpenLatency(testFilePath);
    if (openLatency >= 0) {
        std::cout << "Open latency: " << openLatency << " ms" << std::endl;
    }

    
    size_t bufferSize = 4096;
    // The order of create, write, and read matters
    // Test write
    double writeLatency = measureWriteLatency(testFilePath, bufferSize);
    if (writeLatency >= 0) {
        std::cout << "Write latency: " << writeLatency << " ms" << std::endl;
    }

    // Test read
    double readLatency = measureReadLatency(testFilePath, bufferSize);
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

    const double fileSizeGB = 1.0;  // 1 GB file size
    const double fileSizeMB = fileSizeGB * 1024.0; // Convert to MB if needed

    // Test Large Write
    double largeWriteLatency = measureLargeWriteLatency(testLargeFilePath);
    if (largeWriteLatency >= 0) {
        std::cout << "Large Write latency: " << largeWriteLatency << " ms" << std::endl;
        double writeThroughputMBps = (fileSizeMB / largeWriteLatency) * 1000.0;
        double writeThroughputGBps = (fileSizeGB / largeWriteLatency) * 1000.0;
        std::cout << "Large Write throughput: " << writeThroughputMBps << " MB/s (" << writeThroughputGBps << " GB/s)" << std::endl;
    }   

    // Test Large Read
    double largeReadLatency = measureLargeReadLatency(testLargeFilePath);
    if (largeReadLatency >= 0) {
        std::cout << "Large Read latency: " << largeReadLatency << " ms" << std::endl;
        double readThroughputMBps = (fileSizeMB / largeReadLatency) * 1000.0;
        double readThroughputGBps = (fileSizeGB / largeReadLatency) * 1000.0;
        std::cout << "Large read throughput: " << readThroughputMBps << " MB/s (" << readThroughputGBps << " GB/s)" << std::endl;
    }

    // Test change mode
    int fd = open(testExistFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        std::cerr << "Failed to create file: " << testExistFilePath << std::endl;
        return -1;
    }
    
    if (changeMode(fd, 1) == -1) {
        return -1;
    }
    
    close(fd);

    // Test sync write latency
    double largeSyncWriteLatency = measureLargeWriteLatency(testLargeFilePath);
    if (largeSyncWriteLatency >= 0) {
        std::cout << "Large Sync Write latency: " << largeSyncWriteLatency << " ms" << std::endl;
        double writeThroughputMBps = (fileSizeMB / largeSyncWriteLatency) * 1000.0;
        double writeThroughputGBps = (fileSizeGB / largeSyncWriteLatency) * 1000.0;
        std::cout << "Large Sync Write throughput: " << writeThroughputMBps << " MB/s (" << writeThroughputGBps << " GB/s)" << std::endl;
    }

    //change mode back
    fd = open(testExistFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        std::cerr << "Failed to create file: " << testExistFilePath << std::endl;
        return -1;
    }

    if (changeMode(fd, 0) == -1) {
        return -1;
    }
    
    close(fd);

    double closeLatency = testRecoveryAfterCrash(testFilePath2, 2, 2);
    if (closeLatency >= 0) {
        std::cout << "Commit latency after recovery: " << closeLatency << " ms" << std::endl;
    } else {
        std::cerr << "Test failed." << std::endl;
    }

    return 0;
}
