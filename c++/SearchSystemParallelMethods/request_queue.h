#pragma once

#include "search_server.h"

#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::string query_text;
        size_t count_results;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    int empty_results_ = 0;
    const SearchServer& requester_;
    void AddRequestInDeque(const QueryResult& result);
};


template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    using namespace std;
    try {
        auto result = requester_.FindTopDocuments(std::execution::seq, raw_query, document_predicate);
        QueryResult query_result = { raw_query, result.size() };
        AddRequestInDeque(query_result);
        return result;

    }
    catch (const invalid_argument& error) {
        cout << "Search error: "s << error.what() << endl;
        QueryResult query_result = { raw_query, 0 };
        AddRequestInDeque(query_result);
        return { {} };
    }
}