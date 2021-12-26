#pragma once

#include "concurrent_map.h"
#include "document.h"
#include "read_input_functions.h"
#include "string_processing.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <cassert>
#include <tuple>
#include <utility>
#include <execution>
#include <future>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string_view& stop_words_text);
    explicit SearchServer(const std::string& stop_words_text);

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::iterator begin();
    std::set<int>::iterator end();

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy&& policy, const std::string_view& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string> stop_words_;
    std::set<std::string, std::less<>> words_to_server_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;

    bool IsStopWord(const std::string& word) const;
    static bool IsValidWord(const std::string& word);
    std::vector<std::string> SplitIntoWordsNoStop(const std::string_view& text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string& text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string_view& text) const;
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    template <typename Map>
    void DeleteDocumentsFromMap(Map& map_container, const std::set<std::string>& minus_words) const;

};


template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(policy, raw_query, [&status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, const std::string_view& raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename Map>
void SearchServer::DeleteDocumentsFromMap(Map& map_container, const std::set<std::string>& minus_words) const {
    for (const auto& minus_word : minus_words) {
        if (word_to_document_freqs_.count(minus_word) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(minus_word)) {
            map_container.erase(document_id);
        }
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {

    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    DeleteDocumentsFromMap(document_to_relevance, query.minus_words);

    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;

}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const {

    ConcurrentMap<int, double> document_to_relevance_conc(3);
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance_conc[document_id].ref_to_value += term_freq * inverse_document_freq;
            }
        }
    }

    DeleteDocumentsFromMap(document_to_relevance_conc, query.minus_words);

    std::vector<Document> matched_documents;
    std::mutex vector_mutex;
    for (const auto& [document_id, relevance] : document_to_relevance_conc.BuildOrdinaryMap()) {
        std::lock_guard vector_guard(vector_mutex);
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}



template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }
    std::for_each(word_to_document_freqs_.begin(), word_to_document_freqs_.end(), [&document_id](auto& map_element) {

        if (map_element.second.count(document_id) == 0) {
            return;
        }
        map_element.second.erase(document_id);
        });
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
}

template <typename ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(ExecutionPolicy&& policy, const std::string_view& raw_query, int document_id) const {

    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("can't find document id at server");
    }

    const auto query = ParseQuery(raw_query);

    std::vector<std::string_view> matched_words;

    std::for_each(query.plus_words.begin(), query.plus_words.end(), [&matched_words, this, &document_id](const std::string& word) {

        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(*(words_to_server_.find(word)));
        }

        });

    const auto breaking_word = std::find_if(query.minus_words.begin(), query.minus_words.end(), [this, &document_id](const std::string& word) {
        if (!word_to_document_freqs_.count(word)) {
            return false;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return true;
        }
        return false;
        });

    if (breaking_word != query.minus_words.end()) {
        matched_words.clear();
    }

    return { matched_words, documents_.at(document_id).status };
}