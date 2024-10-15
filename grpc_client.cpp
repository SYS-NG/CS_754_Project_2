#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <grpcpp/grpcpp.h>
#include "grpc_service.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace grpc_service;
using namespace std;

#define MB_CHUNK 1024 * 1024
#define ONE_HUNDRED_MB_CHUNK 1024 * 1024 * 100

class GrpcClient {
    private:
        unique_ptr<GrpcService::Stub> stub_;

    public:
        GrpcClient(shared_ptr<Channel> channel): stub_(GrpcService::NewStub(channel)) {}

        void measureRoundTripTime() {
            PingRequest request;
            request.set_message("a");
            PingResponse response;
            ClientContext context;

            auto start = chrono::high_resolution_clock::now();

            Status status = stub_->Ping(&context, request, &response);

            auto end = chrono::high_resolution_clock::now();

            if (status.ok()) {
                chrono::duration<double, milli> elapsed = end - start;
                cout << "Round-trip time: " << elapsed.count() << " ms" << endl;
            } else {
                cout << "RPC failed" << endl;
            }
        }

        void MeasureSendAndWaitAck(int64_t size) {
            // Expects a 1GB payload, split into 1MB chunks
            DataChunk dataChunk;
            int64_t remaining_size = size;
            int64_t normal_chunk_size = MB_CHUNK;
            int64_t chunk_size = min(remaining_size, normal_chunk_size);
            string data(chunk_size, 'a');
            dataChunk.set_data(data);
            PingResponse response;

            auto start = chrono::high_resolution_clock::now();

            while (remaining_size > 0) {  
                // Initialize context for each iteration, as required by gRPC
                ClientContext context;
                chunk_size = min(remaining_size, normal_chunk_size);
                data = string(chunk_size, 'a');
                dataChunk.set_data(data);
                Status status = stub_->SendAndWaitAck(&context, dataChunk, &response);
                if (!status.ok()) {    
                    cout << "RPC failed: " << status.error_code() << ": " << status.error_message() << endl;
                    return; // Exit the function if RPC fails
                }
                remaining_size -= chunk_size;
            }

            auto end = chrono::high_resolution_clock::now();
            chrono::duration<double> elapsed = end - start;

            int64_t bytes_sent = size;
            double throughput = (bytes_sent / (1024.0 * 1024.0)) / elapsed.count();  // MB/s
            cout << "Server streaming completed. Bytes Received: " << bytes_sent << "Time Elapsed (s): " << elapsed.count() << " Throughput: " << throughput << " MB/s" << endl;
            
        }

        void MeasureServerStreaming(int64_t size) {
            ClientContext context;
            DataRequest request;
            request.set_size(size);
            DataChunk chunk;
            int64_t bytes_received = 0;

            auto start = chrono::high_resolution_clock::now();

            unique_ptr<grpc::ClientReader<DataChunk>> reader(
                stub_->ServerStreamData(&context, request)
            );

            while (reader->Read(&chunk)) {
                bytes_received += chunk.data().size();
            }

            auto end = chrono::high_resolution_clock::now();
            Status status = reader->Finish();

            if (status.ok()) {
                chrono::duration<double> elapsed = end - start;
                double throughput = (bytes_received / (1024.0 * 1024.0)) / elapsed.count();  // MB/s
                cout << "Server streaming completed. Bytes Received: " << bytes_received << "Time Elapsed (s): " << elapsed.count() << " Throughput: " << throughput << " MB/s" << endl;
            } else {
                cout << "RPC failed" << endl;
            }
        }

        void MeasureClientStreaming(int64_t size) {
            ClientContext context;
            TransferStatus status;
            unique_ptr<grpc::ClientWriter<DataChunk>> writer(
                stub_->ClientStreamData(&context, &status)
            );
            int64_t remaining_size = size;
            int64_t normal_chunk_size = MB_CHUNK;
            DataChunk chunk;
            int64_t chunk_size = min(remaining_size, normal_chunk_size);
            string data(chunk_size, 'a');
            chunk.set_data(data);

            auto start = chrono::high_resolution_clock::now();

            while (remaining_size > 0) {
                chunk_size = min(remaining_size, normal_chunk_size);
                data = string(chunk_size, 'a');
                chunk.set_data(data);
                writer->Write(chunk);
                remaining_size -= chunk_size;
            }

            writer->WritesDone();
            Status rpc_status = writer->Finish();
            auto end = chrono::high_resolution_clock::now();

            if (rpc_status.ok()) {
                chrono::duration<double> elapsed = end - start;
                double throughput = (status.bytes_received() / (1024.0 * 1024.0)) / elapsed.count();  // MB/s
                cout << "Client streaming completed. Bytes Received by Server: " << status.bytes_received() << " Throughput: " << throughput << " MB/s" << endl;
            } else {
                cout << "RPC failed" << endl;
            }
        }
};

int main(int argc, char** argv) {
    string target_str = "0.0.0.0:50051";
    if (argc > 1) {
        target_str = argv[1];
    }

    GrpcClient client(grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));

    cout << "Measuring round-trip time for small messages:" << endl;
    for (int i = 0; i < 10; ++i) {
        client.measureRoundTripTime();
    }

    cout << "\nMeasuring throughput for 1GB send and wait ack:" << endl;
    for (int i = 0; i < 10; ++i) {
        client.MeasureSendAndWaitAck(1024LL * MB_CHUNK);  // 1GB
    }

    cout << "\nMeasuring throughput for 1GB server streaming:" << endl;
    for (int i = 0; i < 10; ++i) {
        client.MeasureServerStreaming(1024LL * MB_CHUNK);  // 1GB
    }

    cout << "\nMeasuring throughput for 1GB client streaming:" << endl;
    for (int i = 0; i < 10; ++i) {  
        client.MeasureClientStreaming(1024LL * MB_CHUNK);  // 1GB
    }

    return 0;
}