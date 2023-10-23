#pragma once

#include <iostream>

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

std::ostream& operator<<(std::ostream& output, const Document& document);