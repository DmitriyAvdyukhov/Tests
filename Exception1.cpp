#include<optional>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

std::string ReadLine() 
{
    std::string s;
    std::getline(std::cin, s);
    return s;
}

int ReadLineWithNumber()
{
    int result;
    std::cin >> result;
    ReadLine();
    return result;
}

std::vector<std::string> SplitIntoWords(const std::string& text) 
{
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) 
    {
        if (c == ' ')
        {
            if (!word.empty()) 
            {
                words.push_back(word);
                word.clear();
            }
        }
        else 
        {
            word += c;
        }
    }
    if (!word.empty()) 
    {
        words.push_back(word);
    }
    return words;
}

struct Document 
{
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating)
    {  }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) 
{
    set<string> non_empty_strings;
    for (const string& str : strings) 
    {
        if (!str.empty()) 
        {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:    

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        
        for (const string& word : MakeUniqueNonEmptyStrings(stop_words)) {
            if (!IsValidWord(word)) {
                throw invalid_argument(" присутствуют недопустимые символы"s);
            }
        }         
        
    }

    explicit SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text))
    { }

    void AddDocument(int document_id, const std::string& document, DocumentStatus status,
        const std::vector<int>& ratings) 
    {
        if (document_id < 0)  
        {
            throw std::invalid_argument(" ID документа отриательный"s);
        }
        std::vector<std::string> words = SplitIntoWordsNoStop(document);
       
        if (documents_.count(document_id) > 0)
        {
            throw std::invalid_argument(" уже существует документ с таким ID"s);
        }

        const double inv_word_count = 1.0 / words.size();
        for (const std::string& word : words)
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        document_ids_.push_back(document_id);
        
    }

    template <typename DocumentPredicate>
    std::vector<Document>FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const
    {
         Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        const double eps = 1e-6;

        sort(matched_documents.begin(), matched_documents.end(), [&](const Document& lhs, const Document& rhs)
            {
            if (abs(lhs.relevance - rhs.relevance) < eps)
            {
                return lhs.rating > rhs.rating;
            }
            else 
            {
                return lhs.relevance > rhs.relevance;
            }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) 
        {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

   std::vector<Document>FindTopDocuments(const std::string& raw_query, DocumentStatus status) const
   {
        return FindTopDocuments(
            raw_query,
            [status](int document_id, DocumentStatus document_status, int rating)
            {
                return document_status == status;
            });
    }

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const
    {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const
    {
        return documents_.size();
    }

    int GetDocumentId(int index) const
    {
        if (index >= 0 && index < GetDocumentCount()) 
        {
            return document_ids_[index];
        }
        throw std::out_of_range(" индекс документа выходит за пределы допустимого диапазона"s);
    }

    std::tuple<std::vector<std::string>, DocumentStatus>MatchDocument(const std::string& raw_query, int document_id) const
    {
         Query query = ParseQuery(raw_query);
         std::vector<std::string> matched_words;
        for (const std::string& word : query.plus_words) 
        {
            if (word_to_document_freqs_.count(word) == 0) 
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                matched_words.push_back(word);
            }
        }
        for (const std::string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) 
            {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) 
            {
                matched_words.clear();
                break;
            }
        }        
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData 
    {
        int rating;
        DocumentStatus status;
    };
    const std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;

    bool IsStopWord(const std::string& word) const
    {
        return stop_words_.count(word) > 0;
    }

    static bool IsValidWord(const std::string& word)
    {
        return none_of(word.begin(), word.end(), [](char c) 
            {
            return c >= '\0' && c < ' ';
            });
    }

    

    std::vector<std::string>SplitIntoWordsNoStop(const std::string& text) const 
    {
         std::vector<std::string> words;
        for (const std::string& word : SplitIntoWords(text))
        {
            if (!IsValidWord(word))
            {
               throw std::invalid_argument(" присутствуют недопустимые символы"s);
            }
            if (!IsStopWord(word))
            {
                words.push_back(word);
            }
        }        
        return words;
    }

    static int ComputeAverageRating(const std::vector<int>& ratings) 
    {
        if (ratings.empty())
        {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) 
        {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord
    {        
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    

    QueryWord ParseQueryWord(std::string text) const
    {        
        bool is_minus = false;
        if (text[0] == '-') 
        {
            is_minus = true;
            text = text.substr(1);
        }
        if ( text[0] == '-')
        {
            throw std::invalid_argument(" в запросе присутствует второй минус"s);
        }
        if (!IsValidWord(text)) 
        {
            throw std::invalid_argument(" в запросе присутствует недопустимые символы"s);
        }     
        if (text.empty()) 
        {
            throw std::invalid_argument(" в запросе присутствует одинарный минус"s);
        }
        return{ text, is_minus, IsStopWord(text) };
    }

    struct Query
    {        
        std::set<std::string> plus_words ;
        std::set<std::string> minus_words ;
    };

    Query ParseQuery(const std::string& text) const
    {        
        Query query;
        for (const std::string& word : SplitIntoWords(text))
        {
            QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop)
            {
                if (query_word.is_minus)
                {
                    query.minus_words.insert(query_word.data);
                }
                else
                {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const std::string& word) const
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const 
    {
        std::map<int, double> document_to_relevance;
        for (const std::string& word : query.plus_words)
        {
            if (word_to_document_freqs_.count(word) == 0) 
            {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
            {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating))
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) 
        {
            if (word_to_document_freqs_.count(word) == 0)
            {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word))
            {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) 
        {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};






void PrintDocument(const Document& document)
{
    std::cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << std::endl;
}

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status)
{
    std::cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const std::string& word : words)
    {
        std::cout << ' ' << word;
    }
    std::cout << "}"s << std::endl;
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings)
{
    try 
    {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка добавления документа "s << document_id << ": "s << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query) 
{
    std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try
    {
        for (const Document& document : search_server.FindTopDocuments(raw_query))
        {
            PrintDocument(document);
        }
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Ошибка поиска: "s << e.what() << std::endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const std::string& query)
{
    try
    {
        std::cout << "Матчинг документов по запросу: "s << query << std::endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index)
        {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << std::endl;
    }
}

int main() {
    setlocale(LC_ALL, "Russian");
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    FindTopDocuments(search_server, ""s);
    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);
   

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}