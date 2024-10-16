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

class grpcServices final : public grpc_service::GrpcService::Service {
    private:
        std::string directory_path_; // Where All the files will get mounted

    public: 
        grpcServices(const std::string& directory_path) : directory_path_(directory_path) {}

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
            int file_descriptor = request->filehandle(); // Get the file descriptor from the request
            cout << "NfsRead called with file descriptor: " << file_descriptor << endl; // Debug log
            
            struct stat st;
            if (fstat(file_descriptor, &st) != 0) { // Get file info using fstat
                cerr << "Failed to get file status for descriptor: " << file_descriptor << endl; // Debug log
                response->set_success(false);
                response->set_message("File status retrieval failed");
                return Status::OK;
            }

            response->set_size(st.st_size); // Set the size from fstat

            // Allocate a buffer to hold the file content
            std::vector<char> buffer(st.st_size);
            ssize_t bytes_read = pread(file_descriptor, buffer.data(), buffer.size(), 0); // Read from the file descriptor
            
            if (bytes_read < 0) {
                cerr << "Failed to read file descriptor: " << file_descriptor << endl; // Debug log
                response->set_success(false);
                response->set_message("File Read Failed");
                return Status::OK;
            }

            response->set_success(true);
            response->set_message("File Read successfully");
            response->set_content(std::string(buffer.data(), bytes_read)); // Use buffer.data() and bytes_read
            cout << "File content: " << response->content() << endl; // Debug log

            return Status::OK;
        }

        Status NfsOpen(
            ServerContext* context,
            const grpc_service::NfsOpenRequest* request,
            grpc_service::NfsOpenResponse* response
        ) override {
            const std::string path = request->path();
            cout << "NfsOpen called with path: " << path << endl; // Debug log

            // Open the file and get the file descriptor
            int file_descriptor = open((directory_path_ + path).c_str(), O_RDONLY);
            if (file_descriptor < 0) {
                cout << "File not found: " << path << endl; // Debug log
                response->set_success(false);
                response->set_message("File not found");
                return Status::OK;
            }

            // If the file exists, we can open it successfully
            cout << "File opened successfully: " << path << endl; // Debug log
            response->set_success(true);
            response->set_message("File opened successfully");
            response->set_filehandle(file_descriptor); // Return the file descriptor
            return Status::OK;
        }

        Status NfsRelease(
            ServerContext* context,
            const grpc_service::NfsReleaseRequest* request,
            grpc_service::NfsReleaseResponse* response
        ) override {
            int file_descriptor = request->filehandle(); // Get the file descriptor from the request
            cout << "NfsRelease called with file descriptor: " << file_descriptor << endl; // Debug log

            if (close(file_descriptor) == 0) {
                cout << "File descriptor " << file_descriptor << " closed successfully." << endl;
                response->set_success(true);
                response->set_message("File released successfully");
            } else {
                cerr << "Failed to close file descriptor: " << file_descriptor << endl;
                response->set_success(false);
                response->set_message("File release failed");
            }

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

            int file_descriptor = open((directory_path_ + path).c_str(), O_CREAT | O_WRONLY, mode);
            if (file_descriptor < 0) {
                cerr << "Failed to create file: " << path << endl;
                response->set_success(false);
                response->set_message("File creation failed");
                return Status::OK;
            }

            cout << "File created successfully: " << path << endl;
            response->set_success(true);
            response->set_message("File created successfully");
            response->set_filehandle(file_descriptor);
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
                response->set_message("Failed to update timestamps");
                cerr << "Failed to update timestamps for " << path << endl;
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