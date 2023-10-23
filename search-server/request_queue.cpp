#include "request_queue.h"

#include <deque>
#include <string>
#include <vector>

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query,
                                                   DocumentStatus status) {
    std::vector<Document> documents = search_server_.FindTopDocuments(raw_query, status);
    ProcessRequest(documents);
    return documents;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    std::vector<Document> documents = search_server_.FindTopDocuments(raw_query);
    ProcessRequest(documents);
    return documents;
}

void RequestQueue::ProcessRequest(std::vector<Document>& documents) {
    requests_.push_back(QueryResult{documents.size()});
    if (documents.empty()) {
        ++no_result_count_;
    }
    if (requests_.size() > min_in_day_) {
        QueryResult& front = requests_.front();
        if (front.documents_count == 0) {
            --no_result_count_;
        }
        requests_.pop_front();
    }
}