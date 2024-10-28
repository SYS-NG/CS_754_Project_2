#include <iostream>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <thread>
#include <mutex>
#include <sys/ioctl.h>
#include <cstring>
#include <numeric>
#include <fstream>

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


// double measureLargeWriteLatency(const char* largeFilePath) {
//     const size_t totalFileSize = 1 * 1024 * 1024 * 1024; // 1 GB
//     const size_t totalWrites = 1024; // Total of 1024 writes
//     const size_t writesPerCycle = 16; // Close file after every 16 writes
//     const size_t bufferSize = totalFileSize / totalWrites; // 1 MB per write
//     char buffer[bufferSize];
//     std::fill_n(buffer, bufferSize, 'A'); // Fill the buffer with the character 'A'

//     auto start = std::chrono::high_resolution_clock::now();
    
//     size_t totalWritten = 0;
//     int fd = -1;

//     for (size_t i = 0; i < totalWrites; ++i) {
//         // Open a new file descriptor every 16 writes
//         if (i % writesPerCycle == 0) {
//             if (fd != -1) {
//                 close(fd);
//             }
//             fd = open(largeFilePath, O_WRONLY | O_CREAT | (i == 0 ? O_TRUNC : 0), 0644);
//             if (fd == -1) {
//                 std::cerr << "Failed to create file: " << largeFilePath << std::endl;
//                 return -1;
//             }
//         }

//         // Write buffer data to file
//         ssize_t bytesWritten = write(fd, buffer, bufferSize);
//         if (bytesWritten < 0) {
//             std::cerr << "Failed to write to file: " << largeFilePath << std::endl;
//             std::cerr << "Error code: " << errno << std::endl;
//             close(fd);
//             return -1;
//         }

//         totalWritten += bytesWritten;

//         // End the test if 1 GB of data has been written
//         if (totalWritten >= totalFileSize) {
//             break;
//         }
//     }

//     auto end1 = std::chrono::high_resolution_clock::now();
//     std::chrono::duration<double, std::milli> latency = end1 - start;
//     std::cout << "Total time taken: " << latency.count() << " ms" << std::endl;

//     // Close the last opened file descriptor
//     if (fd != -1) {
//         close(fd);
//     }

//     auto end = std::chrono::high_resolution_clock::now();

//     std::cout << "Successfully wrote 1 GB in 1024 writes with periodic closes: " << largeFilePath << std::endl;
//     std::chrono::duration<double, std::milli> latency = end - start;
//     return latency.count();
// }

// Large read and write are expected to use 1GB
double measureLargeWriteLatency(const char* largeFilePath) {
    const size_t fileSize = 1 * 1024 * 1024 * 1024; // 1 GB
    // const size_t fileSize = 1 * 1024 * 1024; // 1 MB
    const size_t bufferSize = 1 * 1024 * 1024; // 1 MB buffer
    // const size_t bufferSize = 4 * 1024;
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

    auto end1 = std::chrono::high_resolution_clock::now();

    // cout <<"time taken to write 1GB file: " << std::chrono::duration<double, std::milli>(end1 - start).count() << " ms" << endl;

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

vector<double> createLatencies;
vector<double> writeLatencies;
vector<double> readLatencies;
vector<double> deleteLatencies;
mutex latencyMutex;

void createFile(const char* filePath) {
    auto start = chrono::high_resolution_clock::now();
    
    int fd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        cerr << "Failed to create file: " << filePath << endl;
        return;
    }
    
    close(fd);
    
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, std::milli> latency = end - start;

    lock_guard<mutex> lock(latencyMutex);
    createLatencies.push_back(latency.count());
}

void writeFile(const char* filePath, const char* data) {
    auto start = chrono::high_resolution_clock::now();

    int fd = open(filePath, O_WRONLY);
    if (fd == -1) {
        cerr << "Failed to open file for writing: " << filePath << endl;
        return;
    }

    ssize_t bytesWritten = write(fd, data, strlen(data));
    if (bytesWritten == -1) {
        cerr << "Failed to write to file: " << filePath << endl;
    }

    close(fd);

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, std::milli> latency = end - start;

    lock_guard<mutex> lock(latencyMutex);
    writeLatencies.push_back(latency.count());
}

void readFile(const char* filePath, size_t bufferSize) {
    char buffer[bufferSize];

    auto start = chrono::high_resolution_clock::now();
    int fd = open(filePath, O_RDONLY);
    if (fd == -1) {
        cerr << "Failed to open file for reading: " << filePath << endl;
        return;
    }

    ssize_t bytesRead = read(fd, buffer, bufferSize);
    if (bytesRead == -1) {
        cerr << "Failed to read from file: " << filePath << endl;
    }

    close(fd);

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, std::milli> latency = end - start;

    lock_guard<mutex> lock(latencyMutex);
    readLatencies.push_back(latency.count());
}

void deleteFile(const char* filePath) {
    auto start = chrono::high_resolution_clock::now();

    if (unlink(filePath) != 0) {
        cerr << "Failed to delete file: " << filePath << endl;
    }

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, std::milli> latency = end - start;

    lock_guard<mutex> lock(latencyMutex);
    deleteLatencies.push_back(latency.count());
}

void performFileOperations(int threadId, const vector<char>& buffer, int repeatTimesPerThread) {
    // Create a unique file name for this thread
    string filePath = "./mnt/testfile_" + to_string(threadId) + ".txt";
    
    for (int i = 0; i < repeatTimesPerThread; i++)
    {
        createFile(filePath.c_str());
        writeFile(filePath.c_str(), buffer.data());
        readFile(filePath.c_str(), buffer.size());
        deleteFile(filePath.c_str());
    }
}

vector<double> measureFileOperations(size_t bufferSize, int numThreads, int repeatTimesPerThread) {
    // Clear the latency vectors for fresh measurements
    createLatencies.clear();
    writeLatencies.clear();
    readLatencies.clear();
    deleteLatencies.clear();

    vector<char> buffer(bufferSize, 'A');

    vector<thread> threads;

    for (int threadId = 0; threadId < numThreads; threadId++) {
        threads.emplace_back(performFileOperations, threadId, buffer, repeatTimesPerThread);
    }

    // Wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }

    vector<double> averageLatencies(4); // Create a vector to hold average latencies
    cout << "Operations Cycle Repeated: " << createLatencies.size() << endl;

    averageLatencies[0] = createLatencies.empty() ? 0.0 : accumulate(createLatencies.begin(), createLatencies.end(), 0.0) / createLatencies.size();
    averageLatencies[1] = writeLatencies.empty() ? 0.0 : accumulate(writeLatencies.begin(), writeLatencies.end(), 0.0) / writeLatencies.size();
    averageLatencies[2] = readLatencies.empty() ? 0.0 : accumulate(readLatencies.begin(), readLatencies.end(), 0.0) / readLatencies.size();
    averageLatencies[3] = deleteLatencies.empty() ? 0.0 : accumulate(deleteLatencies.begin(), deleteLatencies.end(), 0.0) / deleteLatencies.size();

    return averageLatencies;
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

void deleteFileBeforeOpen(const char* filePath) {

    const size_t bufferSize = 1024;  // Buffer size for reading
    char buffer[bufferSize];
    
    // First open operation
    int fd = open(filePath, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }
    std::cout << "File opened successfully. Pausing... Press Enter to continue." << std::endl;

    // Pause and wait for user input to continue
    std::cin.get();

    ssize_t bytesRead = read(fd, buffer, bufferSize);
    if (bytesRead == -1) {
        close(fd);
        std::cerr << "Failed to read from file." << std::endl;
    } else {
        std::cout << "Read " << bytesRead << " bytes from file: ";
        std::cout.write(buffer, bytesRead);
        std::cout << std::endl;
    }

    close(fd);

    // // Re-open the file after pause
    // fd = open(filePath, O_WRONLY);
    // if (fd == -1) {
    //     std::cerr << "Failed to re-open file: " << filePath << std::endl;
    //     return;
    // }
    // std::cout << "File re-opened successfully. Continuing execution..." << std::endl;

    // // Close the second open operation
    // close(fd);
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
    const std::string filePath = "testfile.txt";

    const char* testDirPath = "./mnt/testdir";
    const char* testExistFilePath = "./mnt/1255";
    const char* testFilePath = "./mnt/testfile";
    const char* testLargeFilePath = "./mnt/test_1GB_file";

    // Used for a lot of tests
    size_t bufferSize = 4096;

    // Just for operations testing
    int repeatTimesPerThread = 2;

    vector<int> numThreadTrials = {1, 5, 10, 15};

    for (auto &numThreads : numThreadTrials)
    {
        vector<double> avgLatencies = measureFileOperations(bufferSize, numThreads, repeatTimesPerThread);
        cout << "\nAverage Latencies (ms):" << endl;
        cout << "Create: " << avgLatencies[0] << " ms" << endl;
        cout << "Write: " << avgLatencies[1] << " ms" << endl;
        cout << "Read: " << avgLatencies[2] << " ms" << endl;
        cout << "Delete: " << avgLatencies[3] << " ms" << endl;
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

    double closeLatency = testRecoveryAfterCrash(testFilePath, 2, 2);
    if (closeLatency >= 0) {
        std::cout << "Commit latency after recovery: " << closeLatency << " ms" << std::endl;
    } else {
        std::cerr << "Test failed." << std::endl;
    }

    deleteFileBeforeOpen(testFilePath);

    return 0;
}
