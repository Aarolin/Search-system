#include "remove_duplicates.h"
#include <algorithm>

void RemoveDuplicates(SearchServer& search_server) {

    std::vector<int> documents_to_delete;
    std::set<std::set<std::string>> unic_words_controller;

    for (const int document_id : search_server) {

        std::map<std::string, double> word_freqs = search_server.GetWordFrequencies(document_id);
        std::set<std::string> words;
        for (const auto& [word, freq] : word_freqs) {
            words.insert(word);
        }

        if (unic_words_controller.count(words) > 0) {
            documents_to_delete.push_back(document_id);
            continue;
        }

        unic_words_controller.insert(words);

    }

    for (const int id_to_del : documents_to_delete) {
        search_server.RemoveDocument(id_to_del);
    }
}