#pragma once

#include "document.h"
#include "search_server.h"

#include <deque>
#include <string>
#include <vector>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server) {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query,
                                         DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const {
        return no_result_count_;
    }
private:
    struct QueryResult {
        size_t documents_count;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int no_result_count_ = 0;

    void ProcessRequest(std::vector<Document>& documents);

    // Примечание: В авторском решении зачем-то учитывается текущее время
    // https://pastebin.com/m7YHGX9e
    // возможно это пригодится в дальнейшем
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query,
                                        DocumentPredicate document_predicate) {
    std::vector<Document> documents
        = search_server_.FindTopDocuments(raw_query, document_predicate);
    ProcessRequest(documents);
    return search_server_.FindTopDocuments(raw_query, document_predicate);
}