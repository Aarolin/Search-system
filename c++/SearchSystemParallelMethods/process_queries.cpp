#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const std::string& query) {
        return std::move(search_server.FindTopDocuments(query));
        });

    return result;
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::list<Document> result;
    for (auto vector_to_docs : ProcessQueries(search_server, queries)) {
        result.splice(result.end(), std::move(std::list<Document>(std::make_move_iterator(vector_to_docs.begin()), std::make_move_iterator(vector_to_docs.end()))));
    }
    return result;
}