#pragma once

#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : first_(begin)
        , last_(end)
        , size_(distance(first_, last_)) {
    }

    Iterator begin() const {
        return first_;
    }

    Iterator end() const {
        return last_;
    }

    int size() const {
        return size_;
    }

private:
    Iterator first_;
    Iterator last_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& output, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        output << *it;
    }
    return output;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, int page_size) {
        int count = 0;
        Iterator range_end = begin;
        while (range_end++ != end) {
            if (++count == page_size || range_end == end) {
                pages_.push_back(IteratorRange(begin, range_end));
                begin = range_end;
                count = 0;
            }
        }
    }
    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private: 
    std::vector<IteratorRange<Iterator>> pages_;
}; 

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}