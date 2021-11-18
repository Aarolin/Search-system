#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : requester_(search_server) {

}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    return AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return status == document_status;
        });
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
    return empty_results_;
}

void RequestQueue::AddRequestInDeque(const QueryResult& result) {
    if (result.count_results == 0) {
        ++empty_results_;
    }
    if (requests_.size() >= sec_in_day_) {
        requests_.pop_front();
        requests_.push_back(result);
        --empty_results_;
        return;
    }
    requests_.push_back(result);
}