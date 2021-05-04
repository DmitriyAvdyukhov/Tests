#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>



using namespace std;

template<class S>
ostream& operator << (ostream& os, const set<S>& s) {
    os << "{";
    bool first = true;
    for (const auto& s_ : s) {
        if (!first) {
            os << ", ";
        }
        first = false;
        os << s_;
    }
    return os << "}";
}

template<class K, class V>
ostream& operator << (ostream& os, const map<K, V>& m) {
    os << "{";
    bool first = true;
    for (const auto& m_ : m) {
        if (!first) {
            os << ", ";
        }
        first = false;
        os << m_.first << ": " << m_.second;
    }
    return os << "}";
}

template <typename T, typename U>
void AssertEqual(const T& t, const U& u, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << "ASSERT_EQUAL"s /*<< t << ", "s << u */ << " failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

void Assert(bool value, const string& hint) {
    if (!value) {
        cerr << "Assertion failed. "s;
        if (!hint.empty()) {
            cerr << "Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}
const int MAX_RESULT_DOCUMENT_COUNT = 5;

std::string ReadLine() {
    std::string s;
    getline(std::cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
            }
            word = "";

        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

enum  DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

struct Document {
    Document(int id_ = 0, double relevance_ = 0, int rating_ = 0) :
        id(id_), relevance(relevance_), rating(rating_) {}
    int id;
    double relevance;
    int rating;
};

class SearchServer {
public:
    SearchServer() {}

    explicit SearchServer(const std::string& stop_words)
    {
        for (const std::string& word : SplitIntoWords(stop_words)) {
            stop_words_.emplace(word);
        }
    }

    template<class StringCollection>
    explicit SearchServer(const StringCollection& stop_words)
    {
        for (const auto& word : stop_words) {
            if (word[0] == ' ') {
                continue;
            }
            else {
                stop_words_.emplace(word);
            }
        }
    }

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const std::string& document, DocumentStatus status,
        const std::vector<int>& score) {
        const std::vector<std::string> words = SplitIntoWordsNoStop(document);

        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(score), status });

        const double inv_word_count = 1.0 / words.size();
        for (const std::string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
    }

    int GetDocumentCount() const { return static_cast<int>(documents_.size()); }

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus current_status,
            int rating) {return current_status == status; });
    }

    template <typename Predicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, const Predicate& predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);
        const double eps = 1e-6;
        sort(matched_documents.begin(), matched_documents.end(),
            [&](const Document& lhs, const Document& rhs) {
                if ((std::abs(lhs.relevance - rhs.relevance)) < eps) {
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

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& text, int document_id) const {
        const Query query = ParseQuery(text);
        std::vector<std::string> matched_words;
        for (const std::string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const std::string& word : query.minus_words) {
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

    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;

    static int ComputeAverageRating(const std::vector<int>& score) {
        return accumulate(score.begin(), score.end(), 0) / static_cast<int>(score.size());
    }

    bool IsStopWord(const std::string& word) const {
        return stop_words_.count(word) != 0;
    }

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const {
        std::vector<std::string> words;
        for (const std::string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) { words.push_back(word); }
        }
        return words;
    }

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const {
        Query query;
        for (const std::string& word : SplitIntoWords(text)) {
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

    double ComputeWordInverseDocumentFreq(const std::string& word) const {
        return log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Predicate>
    std::vector<Document> FindAllDocuments(const Query& query, const Predicate& predicate) const {
        std::map<int, double> document_to_relevance;
        for (const auto& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) { continue; }

            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (predicate(document_id, documents_.at(document_id).status,
                    documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const std::string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) { continue; }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        matched_documents.reserve(document_to_relevance.size());
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }

        return matched_documents;
    }
};

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        AssertEqual(found_docs.size(), 1, "Search doc by word method size()"s);
        const Document& doc0 = found_docs[0];
        Assert((doc0.id == doc_id), "Search doc by word in struct Document"s);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        Assert((server.FindTopDocuments("in"s).empty()), "Search by stop word = null"s);
    }
}

void TestExcludeMinusWords() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    vector<Document> document = search_server.FindTopDocuments("-пушистый -ухоженный -кот"s);
    Assert((document.empty()), "Search doc by minus word return 0"s);

    document = search_server.FindTopDocuments("-пушистый -ухоженный кот"s);
    Assert((document[0].id == 0 && (document.size() == 1)), "Search doc by minus word return 0"s);

    document = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    Assert((document.size() == 3), "Search doc by minus word return 0"s);
}

void TestCorrectMatching() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    string query = "-пушистый -ухоженный кот"s;
    vector<Document> document = search_server.FindTopDocuments("-пушистый -ухоженный кот"s);

    auto a = search_server.MatchDocument(query, 1);
    vector<string> vect = get<vector<string>>(a);

    Assert((vect.empty()), "For minus word Return vector matchdocument empty() for id = 1"s);

    auto b = search_server.MatchDocument(query, 0);
    vect = get<vector<string>>(b);

    Assert((!vect.empty() && vect.size() == 1), "For minus word Return vector matchdocument empty() for id = 0"s);
    AssertEqual(count(vect.begin(), vect.end(), "кот"s), 1, "number of words in Document"s);
}

void TestCorrectSort() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    vector<Document> documents = search_server.FindTopDocuments("пушистый ухоженный кот"s);

    double relevance(0);
    for (const auto& document : documents) {
        if (relevance != 0) Assert((document.relevance <= relevance), "Sort by relevance"s);
        relevance = document.relevance;
    }
    Assert((documents[0].relevance > documents[1].relevance && documents[0].relevance > documents[2].relevance), "Sort by relevance  id0 is greater than id1, id2 "s);
    Assert((documents[1].relevance > documents[2].relevance && documents[0].relevance > documents[1].relevance), "Sort by relevance id1 is greater than id2 and less than id0"s);
}

void TestCorrectRating() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    vector<Document> documents = search_server.FindTopDocuments("пушистый ухоженный кот"s);
    Assert((documents[0].rating != documents[1].rating
        && documents[0].rating != documents[2].rating
        && documents[1].rating != documents[2].rating), "ratings are unequal"s);

    Assert(documents[0].rating > documents[1].rating, "rating id1 is graeter than rating id2"s);
    AssertEqual(documents[0].rating, 5, "For id = 0"s);
    AssertEqual(documents[1].rating, -1, "For id = 1"s);
    AssertEqual(documents[2].rating, 2, "For id = 2"s);

}

void TestResultFromPredicate() {

    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    auto predicate = [](int id, DocumentStatus status, int rating) { return id == 1; };
    vector<Document> documents = search_server.FindTopDocuments("пушистый ухоженный кот"s, predicate);

    AssertEqual(documents[0].id, 1, "documents[0] has id = 1");
    AssertEqual(documents.size(), 1, "documents has a size = 1"s);
    Assert(!(documents[0].id == 2), "documents[0] has no id = 2");
}

void TestSearchDocumentByStatus() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::REMOVED, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    vector<Document> documents = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::REMOVED);

    AssertEqual(documents.size(), 1, "documents has a size = 1 with DocumentStatus::REMOVED"s);
    AssertEqual(documents[0].id, 1, "document with DocumentStatus::REMOVED has id = 1");
    Assert(!(documents[0].id == 2), "document with DocumentStatus::REMOVED has no id = 2");

    vector<Document> documents1 = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);

    AssertEqual(documents1.size(), 2, "documents has a size = 2 with DocumentStatus::ACTUAL"s);
    Assert((documents1[0].id == 2 && documents1[1].id == 0), "document with DocumentStatus::ACTUAL has  id = 2 and d = 0"s);
    Assert(!(documents1[0].id == 1 && documents1[1].id == 3), "document with DocumentStatus::ACTUAL has no id = 1 and id = 3"s);
}

void TestCorrectRelevance() {
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    vector<Document> documents = search_server.FindTopDocuments("пушистый ухоженный кот"s);

    Assert((std::abs(documents[2].relevance) == std::abs(0.13862943611198905)), "relevance for document.id=0"s);
    Assert((std::abs(documents[0].relevance) == std::abs(0.86643397569993164)), "relevance for document.id=1"s);
    Assert((std::abs(documents[1].relevance) == std::abs(0.17328679513998632)), "relevance for document.id=2"s);

    const double eps = 1e-6;
    Assert((std::abs(documents[2].relevance - 0.138629) < eps), "correct relevance for document.id=0"s);
    Assert((std::abs(documents[0].relevance - 0.866434) < eps), "correct relevance for document.id=1"s);
    Assert((std::abs(documents[1].relevance - 0.173287) < eps), "correct relevance for document.id=2"s);
}

template<class TestFunc>
void RunTestImpl(TestFunc func, const string& name_test) {
    func();
    cerr << name_test << " Ok"s << endl;
}

#define RUN_TEST(func)  RunTestImpl(func, #func)

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWords);
    RUN_TEST(TestCorrectMatching);
    RUN_TEST(TestCorrectSort);
    RUN_TEST(TestCorrectRating);
    RUN_TEST(TestResultFromPredicate);
    RUN_TEST(TestSearchDocumentByStatus);
    RUN_TEST(TestCorrectRelevance);
}

int main() {

    TestSearchServer();
    cout << "Search server testing finished"s << endl;
}