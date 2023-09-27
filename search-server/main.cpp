#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
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
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == ' ') {
            continue;
        }
        const size_t space_pos = text.find(' ', i);
        if (space_pos == text.npos) {
            words.push_back(text.substr(i));
            break;
        } else {
            words.push_back(text.substr(i, space_pos - i));
            i = space_pos;
        }
    }
    return words;
}

struct Document {
    Document() = default;

    Document(int s_id, double s_relevance, int s_rating)
        : id(s_id)
        , relevance(s_relevance)
        , rating(s_rating) {        
    }
    int id = 0;
    double relevance = 0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    //SearchServer() = default;
    
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text))
    {
    }

    //void SetStopWords(const string& stop_words_text) {
    //    vector<string> stop_words = SplitIntoWords(stop_words_text);
    //    stop_words_ = MakeUniqueNonEmptyStrings(stop_words);
    //}

    //template <typename StringContainer>
    //void SetStopWords(const StringContainer& stop_words) {
    //    stop_words_ = MakeUniqueNonEmptyStrings(stop_words);
    //}
    
    bool AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        if (document_id < 0)
            return false;
        
        if (documents_.count(document_id))
            return false;

        if (HasSpecialCharacters(document)) {
            return false;
        }

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.insert({document_id, DocumentData{ComputeAverageRating(ratings), status}});
        document_ids_.push_back(document_id);
        return true;
    }

    template <typename DocumentPredicate>
    optional<vector<Document>> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        if (HasSpecialCharacters(raw_query)) {
            return nullopt;
        }

        if (HasWrongMinuses(raw_query)) {
            return nullopt;
        }
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 const double EPSILON = 1e-6;
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    optional<vector<Document>> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status](int, DocumentStatus document_status, int) {
                return document_status == status;
            });
    }

    optional<vector<Document>> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    optional<tuple<vector<string>, DocumentStatus>> MatchDocument(const string& raw_query, int document_id) const {
        if (HasSpecialCharacters(raw_query)) {
            return nullopt;
        }

        if (HasWrongMinuses(raw_query)) {
            return nullopt;
        }

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
        tuple<vector<string>, DocumentStatus> result = {matched_words, documents_.at(document_id).status};
        return result;
    }

    int GetDocumentId(int index) const {
        if (index >= 0 && index < static_cast<int>(documents_.size()))
            return document_ids_[index];
        else
            return INVALID_DOCUMENT_ID;
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;

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
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    static bool HasWrongMinuses(const string& text) {
        if (text.empty()) {
            return false;
        }

        if (text[text.size() - 1] == '-') {
            return true;
        }

        for (size_t i = 0; i < text.size() - 1; ++i) {
            if (text[i] == '-' && (text[i+1] == '-' || text[i+1] == ' ')) {
                return true;
            }
        }    

        return false;
    }

    static bool HasSpecialCharacters(const string& text) {
        for (unsigned char c : text) {
            if (c <= 31 ) {
                return true;
            }
        }
        return false;
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
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
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document = documents_.at(document_id);
                if (document_predicate(document_id, document.status, document.rating)) {
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
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// Фреймворк для юнит-тестирования

// Оператор вывода в поток для vector
template <typename T>
ostream& operator<<(ostream& output, const vector<T>& items) {
    output << "["s;
    bool first_item = true;
    for (const T& item : items) {
        if (!first_item) {
            output << ", "s;
        }
        output << item;
        first_item = false;
    }
    output << "]"s;
    return output;
} 

// Оператор вывода в поток для set
template <typename T>
ostream& operator<<(ostream& output, const set<T>& items) {
    output << "{"s;
    bool first_item = true;
    for (const T& item : items) {
        if (!first_item) {
            output << ", "s;
        }
        output << item;
        first_item = false;
    }
    output << "}"s;
    return output;
}

// Оператор вывода в поток для map
template <typename K, typename V>
ostream& operator<<(ostream& output, const map<K, V>& items) {
    output << "{"s;
    bool first_item = true;
    for (const auto& [key, value] : items) {
        if (!first_item) {
            output << ", "s;
        }
        output << key << ": "s << value;
        first_item = false;
    }
    output << "}"s;
    return output;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    
    bool equality = false;
    if constexpr (std::is_same_v<T, double> || std::is_same_v<U, double>) {
        const double EPSILON = 1e-6;
        equality = abs(t - u) < EPSILON;
    } else {
        equality = t == u;
    }
    
    if (!equality) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << noboolalpha;
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
} 

#define ASSERT(value) AssertImpl((value), #value, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(value, hint) AssertImpl((value), #value, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TestFunc>
void RunTestImpl(const TestFunc& func, const string& test_name) {
    func();
    cerr << test_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func) 
/*
// -------- Начало модульных тестов поисковой системы ----------

 
// Проверить следующие функции поисковой системы:
 
// 1. Добавление документов. Добавленный документ должен находиться по поисковому
// запросу, который содержит слова из документа.
void TestAddDocument() {
    SearchServer server;
    server.AddDocument(0, "a colorful parrot with green wings and red tail is lost", DocumentStatus::ACTUAL, {});
    server.AddDocument(15, "a grey hound with black ears is found at the railway station", DocumentStatus::ACTUAL, {});
    server.AddDocument(99, "a white cat with long furry tail is found near the red square", DocumentStatus::ACTUAL, {});
 
    {
        // Найти документ по словам только из 15 документа
        const vector<Document> found_docs = server.FindTopDocuments("grey hound railway"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(15, found_docs[0].id);
    }
    
    {
        // Найти документы по словам, которых нет ни в одном документе
        const vector<Document> found_docs = server.FindTopDocuments("some random words"s);
        ASSERT(found_docs.empty());
    }
 
    {
        // Найти документы по словам, которые есть в двух документах
        const vector<Document> found_docs = server.FindTopDocuments("white cat long tail"s);
        ASSERT_EQUAL(found_docs.size(), 2u);
    }
}
 
// 2. Поддержка стоп-слов. Стоп-слова исключаются из текста документов. 
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const vector<Document> found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
 
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
 
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}
 
// 3. Поддержка минус-слов. Документы, содержащие минус-слова из поискового запроса,
// не должны включаться в результаты поиска.
void TestExcludeDocumentsWithMinusWords() {
    SearchServer server;
    server.AddDocument(0, "a colorful parrot with green wings and red tail is lost", DocumentStatus::ACTUAL, {});
    server.AddDocument(1, "a grey hound with black ears is found at the railway station", DocumentStatus::ACTUAL, {});
    server.AddDocument(2, "a white cat with long furry tail is found near the red square", DocumentStatus::ACTUAL, {});
 
    {
        // Найти документы по словам из 0 и 2, но исключить 0 с помощью минус слова
        const vector<Document> found_docs = server.FindTopDocuments("white cat long tail -parrot"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(2, found_docs[0].id);
    }
 
    {
        // Найти документы по словам из всех документов, и исключить по словам из всех документов
        const vector<Document> found_docs = server.FindTopDocuments("a with -parrot -found"s);
        ASSERT(found_docs.empty());
    }
 
}
 
// 4. Соответствие документов поисковому запросу. При этом должны быть возвращены все
// слова из поискового запроса, присутствующие в документе. Если есть соответствие
// хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatchDocument() {
    SearchServer server;
    server.AddDocument(0, "a colorful parrot with green wings and red tail is lost", DocumentStatus::ACTUAL, {});
    server.AddDocument(1, "a grey hound with black ears is found at the railway station", DocumentStatus::ACTUAL, {});
 
    {
        const auto [words, status] = server.MatchDocument("colorful hound parrot black with -railway", 0);
        ASSERT_EQUAL(words.size(), 3u);
        ASSERT(find(words.begin(), words.end(), "colorful") != words.end());
        ASSERT(find(words.begin(), words.end(), "parrot") != words.end());
        ASSERT(find(words.begin(), words.end(), "with") != words.end());
 
    }
 
    {
        const auto [words, status] = server.MatchDocument("colorful hound parrot black with -lost", 0);
        ASSERT(words.empty());
    }
}
 
// 5. Сортировка найденных документов по релевантности. Возвращаемые при поиске
// документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSortingByRelevance() {
    SearchServer server;
    server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {});
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {});
    server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::ACTUAL, {});
 
    {
        const vector<Document> found_docs = server.FindTopDocuments("groomed cat"s);
        ASSERT_EQUAL(found_docs.size(), 3u);
        ASSERT_EQUAL(found_docs[0].id, 2);
        ASSERT(found_docs[1].id == 1 || found_docs[1].id == 0);
    }
 
    {
        const vector<Document> found_docs = server.FindTopDocuments("groomed fluffy cat"s);
        ASSERT_EQUAL(found_docs.size(), 3u);
        ASSERT_EQUAL(found_docs[0].id, 1);
        ASSERT_EQUAL(found_docs[1].id, 2);
        ASSERT_EQUAL(found_docs[2].id, 0);
    }
    
}
 
// 6. Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему
// арифметическому оценок документа.
void TestRatingCalculation() {
    SearchServer server;
    server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
 
    {
        const vector<Document> found_docs = server.FindTopDocuments("groomed fluffy cat"s);
        ASSERT_EQUAL(found_docs.size(), 3u);
        ASSERT_EQUAL(found_docs[0].rating, 5);
        ASSERT_EQUAL(found_docs[1].rating, -1);
        ASSERT_EQUAL(found_docs[2].rating, 2);
    }
}
 
// 7. Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
// Поиск документов, имеющих заданный статус.
void TestFiltering() {
    SearchServer server;
    server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::BANNED, {7, 2, 7});
    server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::IRRELEVANT, {5, -12, 2, 1});
    
    {
        auto predicate = [](int document_id, DocumentStatus, int) {
            return document_id == 0; };
 
        const vector<Document> found_docs = server.FindTopDocuments("cat dog"s, predicate);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 0);
    }
 
    {
        auto predicate = [](int, DocumentStatus status, int) {
            return status == DocumentStatus::BANNED; };
 
        const vector<Document> found_docs = server.FindTopDocuments("cat dog"s, predicate);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 1);
    }
 
    {
        auto predicate = [](int, DocumentStatus, int rating) {
            return rating == -1; };
 
        const vector<Document> found_docs = server.FindTopDocuments("cat dog"s, predicate);
        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, 2);
    }
    
}
 
// 8. Корректное вычисление релевантности найденных документов.
void TestRelevanceCalculation() {
    SearchServer server;
    server.SetStopWords("and in on"s);
    server.AddDocument(0, "white cat and fashionable collar"s, DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "groomed dog expressive eyes"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
 
    {
        const vector<Document> found_docs = server.FindTopDocuments("groomed fluffy cat"s);
        ASSERT_EQUAL(found_docs.size(), 3u);
        ASSERT_EQUAL(found_docs[0].relevance, 0.65067242136109593);
        ASSERT_EQUAL(found_docs[1].relevance, 0.27465307216702745);
        ASSERT_EQUAL(found_docs[2].relevance, 0.1013662770270411);
    }
}
 
// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortingByRelevance);
    RUN_TEST(TestRatingCalculation);
    RUN_TEST(TestFiltering);
    RUN_TEST(TestRelevanceCalculation);
}

// --------- Окончание модульных тестов поисковой системы -----------
*/
void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    //TestSearchServer();

    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;

    SearchServer search_server("и в на"s);
    // Явно игнорируем результат метода AddDocument, чтобы избежать предупреждения
    // о неиспользуемом результате его вызова
    (void) search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    if (!search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2})) {
        cout << "Документ не был добавлен, так как его id совпадает с уже имеющимся"s << endl;
    }
    if (!search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2})) {
        cout << "Документ не был добавлен, так как его id отрицательный"s << endl;
    }
    if (!search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, {1, 3, 2})) {
        cout << "Документ не был добавлен, так как содержит спецсимволы"s << endl;
    }
    if (const auto documents = search_server.FindTopDocuments("--пушистый"s)) {
        for (const Document& document : *documents) {
            PrintDocument(document);
        }
    } else {
        cout << "Ошибка в поисковом запросе"s << endl;
    }

    return 0;
}