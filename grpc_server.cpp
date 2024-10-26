#include <iostream>
#include <memory>
#include <string>   
#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"

// For getting the server IP
#include <ifaddrs.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h> // For open and pread
#include <cstring> // For memset

// Directory Manipulating
#include <dirent.h>

using grpc::Status;
using grpc::ServerContext;
using grpc::ServerBuilder;
using grpc::Server;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::InsecureServerCredentials;
using namespace std;

struct WriteCommand {
    off_t offset;
    size_t size;
    std::string content;
};

class grpcServices final : public grpc_service::GrpcService::Service {
    private:
        std::string directory_path_; // Where All the files will get mounted
        std::unordered_map<std::string, vector<WriteCommand>> file_handle_map_; // Maps file handle to their write commands
        std::string write_verifier_; // Write verifier for the current server instance

    public: 
        grpcServices(const std::string& directory_path) : directory_path_(directory_path) {
            auto now = std::chrono::system_clock::now();
            auto now_c = std::chrono::system_clock::to_time_t(now);
            write_verifier_ = std::to_string(now_c);
            cout << "Write verifier initialized: " << write_verifier_ << endl;
        }

        Status Ping(
            ServerContext*                   context,
            const grpc_service::PingRequest* request,
            grpc_service::PingResponse*      response
        ) override {
            response->set_message("Ack");
            return Status::OK;
        }

        Status NfsGetAttr(
            ServerContext* context,
            const grpc_service::NfsGetAttrRequest* request,
            grpc_service::NfsGetAttrResponse* response
        ) override {
            const std::string path = request->path();
            struct stat st;
            if (stat((directory_path_ + path).c_str(), &st) != 0) {
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File not found");
                return Status::OK;
            }
            response->set_success(true);
            response->set_size(st.st_size);
            response->set_mode(st.st_mode);
            response->set_nlink(st.st_nlink);
            return Status::OK;
        }

        Status NfsReadDir(
            ServerContext* context,
            const grpc_service::NfsReadDirRequest* request,
            grpc_service::NfsReadDirResponse* response
        ) override {
            const std::string path = request->path();
            DIR* dir = opendir((directory_path_ + path).c_str());
            if (dir == nullptr) {
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("Directory not found");
                return Status::OK;
            }

            response->set_success(true);
            response->set_message("Directory read successfully");

            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                // DT_REG = regular file
                // DT_DIR = directory
                if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
                    cout << entry->d_name << endl;
                    response->add_files(entry->d_name);
                }
            }
            closedir(dir);
            return Status::OK;
        }

        Status NfsRead(
            ServerContext* context,
            const grpc_service::NfsReadRequest* request,
            grpc_service::NfsReadResponse* response
        ) override {
            //Read first

            const std::string path  = request->path();
            const int64_t     flags = request->flags(); 
            // cout << "READ: NfsOpen called with path: " << path << endl; // Debug log

            // Open the file and get the file descriptor
            int file_descriptor = open((directory_path_ + path).c_str(), flags);
            if (file_descriptor < 0) {
                cout << "File not found: " << path << endl; // Debug log
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File not found");
                return Status::OK;
            }

            // If the file exists, we can open it successfully
            // cout << "READ: File opened successfully: " << path << endl; // Debug log

            // int file_descriptor = request->filedescriptor(); // Get the file descriptor from the request
            off_t offset = request->offset();
            off_t size   = request->size();

            // cout << "READ: NfsRead called with file descriptor: " << file_descriptor << endl; // Debug log
            
            struct stat st;
            if (fstat(file_descriptor, &st) != 0) { // Get file info using fstat
                cerr << "Failed to get file status for descriptor: " << file_descriptor << endl; // Debug log
                close(file_descriptor);
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File status retrieval failed");
                return Status::OK;
            }
            
            int fileSize = st.st_size;

            if (offset >= fileSize) {
                close(file_descriptor);
                response->set_success(false);
                response->set_errorcode(0); // No data to read
                response->set_message("Offset is beyond the file size");
                return Status::OK;
            }
            if (size > fileSize - offset) {
                size = fileSize - offset; // Adjust size to read only up to the file size
            }

            std::vector<char> buffer(size);
            ssize_t bytes_read = pread(file_descriptor, buffer.data(), size, offset); // Read from the file descriptor
            
            if (bytes_read < 0) {
                cerr << "Failed to read file descriptor: " << file_descriptor << endl; // Debug log
                close(file_descriptor);
                response->set_success(false);
                response->set_message("File Read Failed");
                response->set_errorcode(errno);
                return Status::OK;
            }

            response->set_size(bytes_read);
            response->set_success(true);
            response->set_message("File Read successfully");
            response->set_content(std::string(buffer.data(), bytes_read));
            // cout << "File content: " << response->content() << endl;


            // Close the file descriptor
            // cout << "READ: NfsRelease called with file descriptor: " << file_descriptor << endl; // Debug log

            if (close(file_descriptor) != 0) {
                cerr << "Failed to close file descriptor: " << file_descriptor << ", error: " << strerror(errno) << endl;
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File close failed");
            } else {
                // cout << "File descriptor " << file_descriptor << " closed successfully." << endl;
            }

            return Status::OK;
        }

        Status NfsOpen(
            ServerContext* context,
            const grpc_service::NfsOpenRequest* request,
            grpc_service::NfsOpenResponse* response
        ) override {
            const std::string path  = request->path();
            const int64_t     flags = request->flags(); 
            cout << "NfsOpen called with path: " << path << endl; // Debug log

            // check permissions
            int access_mode = 0;
            if (flags & O_RDONLY) {
                access_mode |= R_OK;
            }
            if (flags & O_WRONLY) {
                access_mode |= W_OK;
            }
            if (flags & O_RDWR) {
                access_mode |= R_OK | W_OK;
            }

            if (access((directory_path_ + path).c_str(), access_mode) != 0) {
                cout << "Permission denied or file not found: " << path << endl; // Debug log
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("Permission denied or file not found");
                return Status::OK;
            }

            cout << "File access check passed for path: " << path << endl; // Debug log
            response->set_success(true);
            response->set_message("File access check passed");
            return Status::OK;
        }

        WriteCommand mergeContent(const WriteCommand& a, const WriteCommand& b) {
            // Log the input arguments
            // cout << "Merging WriteCommands: a.offset = " << a.offset << ", a.size = " << a.size << ", a.content = " << a.content << endl;
            // cout << "b.offset = " << b.offset << ", b.size = " << b.size << ", b.content = " << b.content << endl;

            // Check if the commands overlap or are continuous
            if (b.offset > a.offset + a.size) {
                throw std::invalid_argument("WriteCommands do not overlap or are not continuous");
            }

            // Create a new WriteCommand for the merged content
            WriteCommand merged_command;
            merged_command.offset = a.offset; // Start at the beginning of the first command
            merged_command.size = std::max(a.offset + a.size, b.offset + b.size) - a.offset; // Calculate the new size

            // Create the merged content
            merged_command.content = a.content;
            // Overwrite the overlapping region with the content of the second command
            size_t overlap_start = b.offset - a.offset; // Calculate the start of the overlap in the first command
            merged_command.content.replace(overlap_start, b.size, b.content); // Replace the overlapping part

            // cout << "Final merged command: offset = " << merged_command.offset 
            //      << ", size = " << merged_command.size 
            //      << ", content = " << merged_command.content << endl;

            return merged_command;
        }

        Status NfsCommit(
            ServerContext* context,
            const grpc_service::NfsCommitRequest* request,
            grpc_service::NfsCommitResponse* response
        ) override {
            const std::string path  = request->path();
            const int64_t     flags = request->flags();
            const std::string& valid_write_verifier = write_verifier_;
            cout << "NfsCommit called with file handle: " << path << endl;

            cout << "Retrieving write commands for file handle: " << path << endl;
            std::vector<WriteCommand> write_commands = file_handle_map_[path];
            cout << "Number of write commands retrieved: " << write_commands.size() << endl;

            response->set_current_write_verifier("-1");

            if(request->write_verifiers().empty()) {
                response->set_success(true);
                response->set_message("File released successfully");
                cout<< "File released no write verifiers found" << endl;
                return Status::OK;
            }

            if (request->write_verifiers().size() == 1) {
                cout << "Checking write verifier: " << request->write_verifiers(0) << endl;
                if (request->write_verifiers(0) != valid_write_verifier) {
                    cout << "Invalid write_verifier detected" << endl;
                    response->set_success(false);
                    response->set_message("Invalid write_verifier detected");
                    response->set_errorcode(EIO);
                    response->set_current_write_verifier(valid_write_verifier);
                    return Status::OK;
                }
            } else {
                cout << "Multiple write_verifiers detected, assuming a mismatch" << endl;
                response->set_success(false);
                response->set_message("Multiple write_verifiers detected, assuming a mismatch");
                response->set_errorcode(EIO);
                response->set_current_write_verifier(valid_write_verifier);
                return Status::OK;
            }

            // bool all_verifiers_match = true;
            // for (const auto& ver : request->write_verifiers()) {
            //     cout << "Checking write verifier: " << ver << endl;
            //     if (ver != valid_write_verifier) {
            //         all_verifiers_match = false;
            //         break;
            //     }
            // }

            // if (!all_verifiers_match) {
            //     response->set_success(false);
            //     response->set_message("Invalid write_verifier(s) detected");
            //     response->set_errorcode(EIO);
            //     response->set_current_write_verifier(valid_write_verifier);
            //     return Status::OK;
            // }

            // Open the file and get the file descriptor
            int file_descriptor = open((directory_path_ + path).c_str(), flags);
            if (file_descriptor < 0) {
                cout << "File not found: " << path << endl; // Debug log
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File not found");
                return Status::OK;
            }
            // cout << "File opened successfully: " << "fd: "<< file_descriptor << "with: " << path << endl; // Debug log
            
            
            // Need to push to disk
            // First sort the WriteCommands under the file descriptor by offset
            // Then start merging the Write Commands when possible
            // if nextOffset < currentOffset + currentSize
            // then it can be merged
            // currentSize = nextOffset + nextSize - currentOffset
            // content that overlaps should be overwritten by the new content
            // 
            // else: this overlap is done, and is one writeCommand, add to a final write command vector
            // on to the next commands
            //
            // At the end using the final write command vector, write to the file descriptor

            cout << "Sorting write commands by offset." << endl;
            std::sort(write_commands.begin(), write_commands.end(), [](const WriteCommand& a, const WriteCommand& b) {
                return a.offset < b.offset; // Sort by offset
            });

            std::vector<WriteCommand> final_write_commands;
            WriteCommand current_command = {0, 0, ""}; // Initialize with default values

            for (const auto& command : write_commands) {
                // cout << "Processing command: offset = " << command.offset 
                //      << ", size = " << command.size 
                //      << ", content = " << command.content << endl;

                if (current_command.size == 0) {
                    cout << "Initializing current_command with the first command." << endl;
                    current_command = command;
                } else {
                    // cout << "Comparing current_command: offset = " << current_command.offset 
                    //      << ", size = " << current_command.size 
                    //      << " with command: offset = " << command.offset 
                    //      << ", size = " << command.size << endl;

                    if (command.offset < current_command.offset + current_command.size) {
                        // cout << "Commands overlap. Merging commands." << endl;
                        current_command = mergeContent(current_command, command);
                    } else {
                        // cout << "No overlap detected. Finalizing current command." << endl;
                        final_write_commands.push_back(current_command);
                        current_command = command; // Start a new command
                    }
                }
            }

            // Add the last command if it exists
            if (current_command.size > 0) {
                cout << "Adding the last command to final write commands." << endl;
                final_write_commands.push_back(current_command);
            }

            // Write the final commands to the file descriptor
            for (const auto& cmd : final_write_commands) {
                // cout << "Writing to file descriptor " << file_descriptor 
                //      << " at offset " << cmd.offset 
                //      << " with size " << cmd.size 
                //      << " and content: " << cmd.content << endl;

                if (lseek(file_descriptor, cmd.offset, SEEK_SET) == (off_t)-1) {
                    cerr << "Failed to seek to offset: " << cmd.offset << " for file descriptor: " << file_descriptor << endl;
                    close(file_descriptor);
                    response->set_success(false);
                    response->set_errorcode(errno);
                    response->set_message("File seek failed");
                    return Status::OK;
                }
                ssize_t bytes_written = write(file_descriptor, cmd.content.data(), cmd.size);
                cout << "Bytes Written to File: " << bytes_written << endl;
                if (bytes_written < 0) {
                    cerr << "Failed to write to file descriptor: " << file_descriptor 
                         << ", error: " << strerror(errno) << endl;
                    close(file_descriptor);
                    response->set_success(false);
                    response->set_errorcode(errno);
                    response->set_message("File write failed");
                    return Status::OK;
                }
                cout << "Successfully wrote " << bytes_written << " bytes." << endl;
            }

            cout << "Clearing the file descriptor map." << endl;
            file_handle_map_.erase(path); // Clear the file descriptor map

            if (close(file_descriptor) == 0) {
                cout << "File descriptor " << file_descriptor << " closed successfully." << endl;
                response->set_success(true);
                response->set_message("File released successfully");
            } else {
                cerr << "Failed to close file descriptor: " << file_descriptor << ", error: " << strerror(errno) << endl;
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File release failed");
            }

            return Status::OK;
        }

        Status NfsRelease(
            ServerContext* context,
            const grpc_service::NfsReleaseRequest* request,
            grpc_service::NfsReleaseResponse* response
        ) override {

            const std::string path  = request->path();
            struct stat buffer;

            if (stat((directory_path_ + path).c_str(), &buffer) != 0) {
                // If stat fails, the file does not exist or there is another error
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File not found: " + path);
                return Status::OK;
            }

            response->set_success(true);
            response->set_message("File released successfully");
            return Status::OK;
        }

        // Status NfsRelease(
        //     ServerContext* context,
        //     const grpc_service::NfsReleaseRequest* request,
        //     grpc_service::NfsReleaseResponse* response
        // ) override {
        //     // int file_descriptor = request->filedescriptor(); // Get the file descriptor from the request
        //     // cout << "NfsRelease called with file descriptor: " << file_descriptor << endl; // Debug log

        //     // if (close(file_descriptor) == 0) {
        //     //     cout << "File descriptor " << file_descriptor << " closed successfully." << endl;
        //     //     response->set_success(true);
        //     //     response->set_message("File released successfully");
        //     // } else {
        //     //     cerr << "Failed to close file descriptor: " << file_descriptor << endl;
        //     //     response->set_success(false);
        //     //     response->set_errorcode(errno);
        //     //     response->set_message("File release failed");
        //     // }

        //     return Status::OK;
        // }

        Status NfsWrite(
            ServerContext* context,
            const grpc_service::NfsWriteRequest* request,
            grpc_service::NfsWriteResponse* response
        ) override {
            // int file_descriptor = request->filedescriptor();
            // const std::string content = request->content();
            // int64_t size = request->size();
            // off_t offset = request->offset(); 

            // cout << "NfsWrite invoked with file descriptor: " << file_descriptor << ", content size: " << size << ", and offset: " << offset << endl; // Debug log
            // cout << "Writing content: " << content << " to file descriptor: " << file_descriptor << " at offset: " << offset << endl; // Log the content being written
            // if (lseek(file_descriptor, offset, SEEK_SET) == (off_t)-1) {
            //     cerr << "Failed to seek to offset: " << offset << " in file descriptor: " << file_descriptor << endl;
            //     response->set_success(false);
            //     response->set_errorcode(errno);
            //     response->set_message("File seek failed");
            //     return Status::OK;
            // }

            // // Write the content to the file
            // ssize_t bytes_written = write(file_descriptor, content.c_str(), size);
            // if (bytes_written < 0) {
            //     cerr << "Failed to write to file descriptor: " << file_descriptor << ", error: " << strerror(errno) << endl; // Debug log with error message
            //     response->set_success(false);
            //     response->set_errorcode(errno);
            //     response->set_message("File write failed");
            //     return Status::OK;
            // }

            // cout << "Successfully wrote " << bytes_written << " bytes to file descriptor: " << file_descriptor << endl; // Debug log
            // response->set_success(true);
            // response->set_message("File written successfully");
            // response->set_bytes_written(bytes_written); // Return the number of bytes written
            return Status::OK;
        }

        Status NfsWriteAsync(
            ServerContext* context,
            const grpc_service::NfsWriteRequest* request,
            grpc_service::NfsWriteResponse* response
        ) override {
            const std::string path = request->path();
            const std::string data = request->content();
            int64_t offset = request->offset();
            int32_t size = request->size();

            // cout << "NfsWriteAsync invoked with fh: " << path 
            //      << ", data size: " << size << ", and offset: " << offset << endl; // Debug log
        
            WriteCommand command;
            command.offset = offset;
            command.size = size;
            command.content = data;

            // Store the write command in the file handle map
            file_handle_map_[path].push_back(command);

            response->set_success(true);
            response->set_message("Data buffered successfully");
            // cout << size << endl;
            response->set_bytes_written(size); // Return the number of bytes intended to be written
            response->set_write_verifier(write_verifier_); // Return the current write verifier
            return Status::OK;
        }

        Status NfsUnlink(
            ServerContext* context,
            const grpc_service::NfsUnlinkRequest* request,
            grpc_service::NfsUnlinkResponse* response
        ) override {
            const std::string path = request->path();
            cout << "NfsUnlink called with path: " << path << endl; // Debug log

            if (unlink((directory_path_ + path).c_str()) == 0) {
                cout << "File unlinked successfully: " << path << endl;
                response->set_success(true);
                response->set_message("File unlinked successfully");
            } else {
                cerr << "Failed to unlink file: " << path << " - " << strerror(errno) << endl;
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File unlink failed");
            }
            return Status::OK;
        }

        Status NfsRmdir(
            ServerContext* context,
            const grpc_service::NfsRmdirRequest* request,
            grpc_service::NfsRmdirResponse* response
        ) override {
            const std::string path = request->path();
            cout << "NfsRmdir called with path: " << path << endl; // Debug log

            // Perform rmdir operation
            if (rmdir((directory_path_ + path).c_str()) == 0) {
                cout << "Directory removed successfully: " << path << endl;
                response->set_success(true);
                response->set_message("Directory removed successfully");
            } else {
                cerr << "Failed to remove directory: " << path << " - " << strerror(errno) << endl;
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("Directory removal failed");
            }

            return Status::OK;
        }

        Status NfsCreate(
            ServerContext* context,
            const grpc_service::NfsCreateRequest* request,
            grpc_service::NfsCreateResponse* response
        ) override {
            const std::string path = request->path();
            mode_t mode = request->mode();
            cout << "NfsCreate called with path: " << path << " and mode: " << oct << mode << endl;

            // open the file and get the file descriptor
            int file_descriptor = open((directory_path_ + path).c_str(), O_CREAT | O_WRONLY, mode);
            if (file_descriptor < 0) {
                cerr << "Failed to create file: " << path << endl;
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File creation failed");
                return Status::OK;
            }

            cout << "File created successfully: " << path << endl;
            response->set_success(true);
            response->set_message("File created successfully");

            if (close(file_descriptor) != 0) {
                cerr << "Failed to close file descriptor: " << file_descriptor << ", error: " << strerror(errno) << endl;
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("File close failed");
            } else {
                cout << "File descriptor " << file_descriptor << " closed successfully." << endl;
            }

            return Status::OK;
        }

        Status NfsUtimens(
            ServerContext* context,
            const grpc_service::NfsUtimensRequest* request,
            grpc_service::NfsUtimensResponse* response
        ) override {
            const std::string path = request->path();
            struct timespec times[2];

            // Setting the access time (atime)
            times[0].tv_sec = request->atime();
            times[0].tv_nsec = 0;

            // Setting the modification time (mtime)
            times[1].tv_sec = request->mtime();
            times[1].tv_nsec = 0;

            if (utimensat(AT_FDCWD, (directory_path_ + path).c_str(), times, 0) == 0) {
                response->set_success(true);
                response->set_message("Timestamps updated successfully");
                cout << "Timestamps for " << path << " updated successfully." << endl;
            } else {
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("Failed to update timestamps");
                cerr << "Failed to update timestamps for " << path << endl;
            }

            return Status::OK;
        }

        Status NfsMkdir(
            ServerContext* context,
            const grpc_service::NfsMkdirRequest* request,
            grpc_service::NfsMkdirResponse* response
        ) override {
            const std::string path = request->path();
            mode_t mode = request->mode();

            cout << "NfsMkdir called with path: " << path << " and mode: " << mode << endl; // Debug log

            // Create the directory using mkdir system call
            if (mkdir((directory_path_ + path).c_str(), mode) == 0) {
                response->set_success(true);
                response->set_message("Directory created successfully");
                cout << "Directory created: " << path << endl;
            } else {
                response->set_success(false);
                response->set_errorcode(errno);
                response->set_message("Failed to create directory: " + std::string(strerror(errno)));
                cerr << "Failed to create directory: " << path << " - " << strerror(errno) << endl;
            }

            return Status::OK;
        }
};

std::string getServerIP() {
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];
    std::string ip_address;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (s != 0) {
            printf("getnameinfo() failed: %s\n", gai_strerror(s));
            continue;
        }

        // Skip loopback interface
        if (strcmp(ifa->ifa_name, "lo") != 0) {
            ip_address = host;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return ip_address;
}

void RunServer(string remote_storage_dir_path) {

    // Check that the remote storage directory exists, if not create it
    struct stat st;
    if (stat(remote_storage_dir_path.c_str(), &st) != 0) {
        // Directory does not exist, create it
        if (mkdir(remote_storage_dir_path.c_str(), 0777) != 0) {
            perror("mkdir");
            return;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        // Path exists but is not a directory
        cerr << "Error: " << remote_storage_dir_path << " is not a directory." << endl;
        return;
    }

    cout << "Running Storage at: " << remote_storage_dir_path << endl;

    // Create GRPC Server
    string server_address = getServerIP() + ":50051";
    grpcServices service(remote_storage_dir_path);
    ServerBuilder builder;
    builder.AddListeningPort(server_address, InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Server listening on " << server_address << endl;
    server->Wait();
}

int main(int argc, char** argv) {
    string remote_storage_dir_path = "./remoteStore";
    if (argc > 1) {
        remote_storage_dir_path = argv[1];
    }
    RunServer(remote_storage_dir_path);
    return 0;
}