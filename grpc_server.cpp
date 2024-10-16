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

// Directory Manipulating
#include <dirent.h>

// File Handler
#include <fstream>

using grpc::Status;
using grpc::ServerContext;
using grpc::ServerBuilder;
using grpc::Server;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::InsecureServerCredentials;
using namespace std;

#define MB_CHUNK 1024 * 1024
#define ONE_HUNDRED_MB_CHUNK 1024 * 1024 * 100
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
            const std::string path = request->path();
            cout << "NfsRead called with path: " << path << endl; // Debug log
            
            ifstream file(directory_path_ + path, ios::binary); // Open file in binary mode
            if (!file) {
                cerr << "File not found: " << directory_path_ + path << endl; // Debug log
                response->set_success(false);
                response->set_message("File not found");
                return Status::OK;
            }

            // Get the size of the file
            file.seekg(0, std::ios::end); // Move to the end to get the size
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg); // Move back to the beginning
            cout << "File size determined: " << size << " bytes" << endl; // Debug log

            // Update Size for the return message
            response->set_size(size);

            // Allocate a buffer to hold the file content
            std::vector<char> buffer(size);
            // Read the file into the buffer
            if (file.read(buffer.data(), size)) {
                response->set_success(true);
                response->set_message("File Read successfully");
                cout << "File read successfully: " << path << endl; // Debug log
                response->set_content(std::string(buffer.data(), size)); // Use buffer.data() and size
                cout << "File content: " << response->content() << endl; // Debug log
            } else {
                response->set_success(false);
                response->set_message("File Read Failed");
                cerr << "Failed to read file: " << path << endl; // Debug log
            }  

            file.close();
            return Status::OK;
        }

        Status NfsOpen(
            ServerContext* context,
            const grpc_service::NfsOpenRequest* request,
            grpc_service::NfsOpenResponse* response
        ) override {
            const std::string path = request->path();
            cout << "NfsOpen called with path: " << path << endl; // Debug log

            // Check if the file exists
            ifstream file(directory_path_ + path);
            if (!file) {
                cout << "File not found: " << path << endl; // Debug log
                response->set_success(false);
                response->set_message("File not found");
                return Status::OK;
            }

            // If the file exists, we can open it successfully
            cout << "File opened successfully: " << path << endl; // Debug log
            response->set_success(true);
            response->set_message("File opened successfully");
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