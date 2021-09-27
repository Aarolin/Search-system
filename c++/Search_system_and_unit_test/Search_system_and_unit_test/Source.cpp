
#include <iostream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            words.push_back(word);
            word = "";
        }
        else {
            word += c;
        }
    }
    words.push_back(word);

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus document_status) const {
        auto StatusPred = [&document_status](int document_id, DocumentStatus status, int rating) { return status == document_status; };
        return FindTopDocuments(raw_query, StatusPred);
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    template<typename Pred>
    vector<Document> FindTopDocuments(const string& raw_query, Pred predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Pred>
    vector<Document> FindAllDocuments(const Query& query, Pred predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                DocumentData document_properties = documents_.at(document_id);
                if (predicate(document_id, document_properties.status, document_properties.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
                });
        }
        return matched_documents;
    }
};

/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
void AssertImpl(bool expr, const string& t_expr, const string& file, const string& func, unsigned line, const string& hint) {

    if (!expr) {

        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << t_expr << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }

}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

template <typename FunctionType>
void RunTestImpl(FunctionType function, const string& func) {

    function();
    cerr << func << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

#define ASSERT(expr) AssertImpl(expr, #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

/*
Разместите код остальных тестов здесь

*/
void TestAddDocument() {

    const string content = "white cat and fluffy tail"s;
    const int doc_id = 10;
    const vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    ASSERT_EQUAL(server.GetDocumentCount(), 0);
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
    const auto docs = server.FindTopDocuments("white cat"s);
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    server.AddDocument(12, "black dog", DocumentStatus::BANNED, ratings);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);
}

void TestSupportStopWords() {
    SearchServer server;
    const string doc1_content = "white cat and fluffy tail"s;
    const string doc2_content = "black and gray"s;
    const int doc1_id = 1;
    const int doc2_id = 2;
    const vector<int> ratings = { 1, 2, 3 };
    server.SetStopWords("black and"s);
    server.AddDocument(doc1_id, doc1_content, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc2_id, doc2_content, DocumentStatus::ACTUAL, ratings);
    {
        const auto finded_docs = server.FindTopDocuments("black and white"s);
        ASSERT_EQUAL(finded_docs.size(), 1);
        ASSERT_EQUAL(finded_docs.at(0).id, doc1_id);
    }

    {
        const auto finded_docs = server.FindTopDocuments("black and"s);
        ASSERT(finded_docs.empty());
    }
}

void TestSupportMinusWords() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    {
        const auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(docs.size(), 3);
        ASSERT_EQUAL(docs.at(0).id, 1);
        ASSERT_EQUAL(docs.at(1).id, 0);
        ASSERT_EQUAL(docs.at(2).id, 2);
    }

    {
        const auto docs = search_server.FindTopDocuments("-пушистый -ухоженный кот"s);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs.at(0).id, 0);
    }

    {
        const auto docs = search_server.FindTopDocuments("-пушистый -ухоженный -кот"s);
        ASSERT(docs.empty());
    }

    {
        const auto docs = search_server.FindTopDocuments("-пушистый -ухоженный -кот собака"s);
        ASSERT(docs.empty());
    }
}

void TestMatchingDocuments() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    {
        const auto matching = search_server.MatchDocument("белый кот", 0);
        ASSERT_EQUAL(get<0>(matching).size(), 2);
        ASSERT_EQUAL(get<0>(matching)[0], "белый"s);
        ASSERT_EQUAL(get<0>(matching)[1], "кот"s);
        ASSERT(get<1>(matching) == DocumentStatus::ACTUAL);
    }

    {
        const auto matching = search_server.MatchDocument("белый кот и", 0);
        ASSERT_EQUAL(get<0>(matching).size(), 2);
        ASSERT_EQUAL(get<0>(matching)[0], "белый"s);
        ASSERT_EQUAL(get<0>(matching)[1], "кот"s);
        ASSERT(get<1>(matching) == DocumentStatus::ACTUAL);
    }

    {
        const auto matching = search_server.MatchDocument("белый -кот и", 0);
        ASSERT(get<0>(matching).empty());
    }
}

void TestSortFindedDocuments() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    const auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_EQUAL(docs.size(), 3);
    ASSERT_EQUAL(docs.at(0).id, 1);
    ASSERT_EQUAL(docs.at(1).id, 0);
    ASSERT_EQUAL(docs.at(2).id, 2);

}


void TestAverageRating() {

    {
        SearchServer search_server;
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
        const auto docs = search_server.FindTopDocuments("пушисый ухоженный кот"s);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs.at(0).rating, 2);
    }

    {
        SearchServer search_server;
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { -3, -3 });
        const auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].rating, -3);
    }
    {
        SearchServer search_server;
        search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 0, 0, 0 });
        const auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].rating, 0);
    }
}

void TestFilterByPredicat() {

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    {
        const auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs.at(0).id, 3);
    }

    {
        const auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return  rating > 4; });
        ASSERT_EQUAL(docs.size(), 2);
        ASSERT_EQUAL(docs.at(0).id, 1);
        ASSERT_EQUAL(docs.at(1).id, 3);
    }

    {
        const auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id < 1; });
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs.at(0).id, 0);
    }

}

void TestFindDocumentsByStatus() {
    SearchServer search_server;

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый ухоженный хвост"s, DocumentStatus::REMOVED, { 7, 2, 7 });

    {
        const auto docs = search_server.FindTopDocuments("ухоженный белый"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs.at(0).id, 0);
    }

    {
        const auto docs = search_server.FindTopDocuments("белый кот"s, DocumentStatus::BANNED);
        ASSERT(docs.empty());
    }

}

void TestCorrectRelevance() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    const auto docs = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    ASSERT_EQUAL(docs.size(), 3);
    ASSERT((docs[0].relevance - 0.866434) < 1e-6);
    ASSERT((docs[1].relevance - 0.173287) < 1e-6);
    ASSERT((docs[2].relevance - 0.173287) < 1e-6);
}



// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestSupportStopWords);
    RUN_TEST(TestSupportMinusWords);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestSortFindedDocuments);
    RUN_TEST(TestAverageRating);
    RUN_TEST(TestFilterByPredicat);
    RUN_TEST(TestFindDocumentsByStatus);
    RUN_TEST(TestCorrectRelevance);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}