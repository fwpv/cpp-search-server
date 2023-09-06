#include <algorithm>
#include <cmath>
#include <iostream>
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
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        
        map<string, int> words_frequency;
        for (const string& word: words) {
            words_frequency[word]++;
        }
        
        for (const string& word: words) {
            double tf = words_frequency[word] / static_cast<double>(words.size());
            words_of_documents_[word][document_id] = tf;
        }
        
        document_count_ += 1;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    set<string> stop_words_;
	// key - word, value - map (key - document_id, value - TF)
    map<string, map<int, double>> words_of_documents_;
    int document_count_ = 0;
    
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

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }
 
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

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> relevance_map; // key - document_id, value - relevance
        
        for (const string& plus_word: query.plus_words) {
            if (words_of_documents_.count(plus_word)) {
                const map<int, double>& docs = words_of_documents_.at(plus_word);
                
                // Calculate IDF of a plus_word in a query
                double idf = document_count_ / static_cast<double>(docs.size());
                idf = log(idf);
                
                for (const auto& [id, tf] : docs) {
                    // Calculate IDF-TF of a plus_word in that document
                    double idf_tf = idf * tf;
                    
                    // Accumulate all IDF-TF of that document
                    relevance_map[id] += idf_tf; 
                }
            }
        }
           
        for (const string& minus_word: query.minus_words) {
            if (words_of_documents_.count(minus_word)) {
                const map<int, double>& docs = words_of_documents_.at(minus_word);
                for (const auto& [id, tf] : docs) {
                    // Exclude a minus_word
                    relevance_map[id] = -1; 
                }
            }
        }
        
        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : relevance_map) {
            if (relevance >= 0) {
                matched_documents.push_back({document_id, relevance});
            }
        }
        
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}