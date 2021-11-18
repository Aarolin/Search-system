#include "remove_duplicates.h"
#include <algorithm>

void RemoveDuplicates(SearchServer& search_server) {

    std::vector<int> documents_to_delete;
    auto& documents_to_words = search_server.documents_to_words_;

    for (auto search_iterator = documents_to_words.begin(); search_iterator != documents_to_words.end(); ) {
        const auto& [doc_id, curr_words_set] = *search_iterator;
        ++search_iterator;
        for (auto bg = search_iterator; bg != documents_to_words.end(); ++bg) {
            const auto& [pos_duplicate_doc_id, possible_duplicate_words_set] = *bg;
            if (curr_words_set == possible_duplicate_words_set) {
                documents_to_delete.push_back(pos_duplicate_doc_id);
            }
        }
    }

    for (const int id_to_del : documents_to_delete) {
        search_server.RemoveDocument(id_to_del);
    }
}