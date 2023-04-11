#include "lib.h"
#include "dev.h"
#include "utf8.h"
using namespace quanteda;

typedef tbb::concurrent_unordered_multimap<Type, int> MultiMapIndex;
typedef std::string Pattern;
typedef std::vector<Pattern> Patterns;

void index_glob(Types &types, MultiMapIndex &index,
                std::string wildcard, int side, int len) {
    
    std::size_t H = types.size();
    Types temp(H);
    
    for (size_t h = 0; h < H; h++) {
        std::string value;
        if (side == 1) {
            if (len > 0) {
                value = utf8_sub_left(types[h], len);
            } else {
                value = utf8_sub_left(types[h], utf8_length(types[h]) + len);
            }
            if (value != "") {
                index.insert(std::pair<Type, int>(value + wildcard, h));
                //Rcout << "Insert: " << value + wildcard << " " << h << "\n";
            }
        } else if (side == 2) {
            if (len > 0) {
                value = utf8_sub_right(types[h], len);
            } else {
                value = utf8_sub_right(types[h], utf8_length(types[h]) + len);
            }
            if (value != "") {
                index.insert(std::pair<Type, int>(wildcard + value, h));
                //Rcout << "Insert: " << wildcard + value << " " << h << "\n";
            }
        }
    }
}


// [[Rcpp::export]]
List cpp_index_types(const CharacterVector &patterns_, 
                     const CharacterVector &types_) {
    
    dev::Timer timer;
    dev::start_timer("Convert", timer);
    
    Patterns patterns = Rcpp::as<Patterns>(patterns_);
    Types types = Rcpp::as<Types>(types_);
    
    std::vector<int> len;
    len.reserve(patterns.size());
    for (size_t i = 0; i < patterns.size(); i++) {
        std::string pattern = patterns[i];
        //Rcout << "Pattern: " << pattern << "\n";
        //Rcout << utf8_sub_left(pattern, 1) << "\n";
        //Rcout << utf8_sub_right(pattern, 1) << "\n";
        if ((utf8_sub_left(pattern, 1) == "*") != (utf8_sub_right(pattern, 1) == "*")) {
            len.push_back(utf8_length(pattern) - 1);
        }
    }
    sort(len.begin(), len.end());
    len.erase(unique(len.begin(), len.end()), len.end());
    

    dev::stop_timer("Convert", timer);
    
    dev::start_timer("Index", timer);
    
    MultiMapIndex index;
    std::size_t H = len.size();
    Texts temp(H);
#if QUANTEDA_USE_TBB
    tbb::parallel_for(tbb::blocked_range<int>(0, H), [&](tbb::blocked_range<int> r) {
        for (int h = r.begin(); h < r.end(); ++h) {
            index_glob(types, index, "*", 1, len[h]);
            index_glob(types, index, "*", 2, len[h]);
        }
    });
#else
    for (size_t h = 0; h < H; h++) {
        index_glob(types, index, "*", 1, len[h]);
        index_glob(types, index, "*", 2, len[h]);
    }
#endif
    index_glob(types, index, "?", 1, -1);
    index_glob(types, index, "?", 2, -1);
    dev::stop_timer("Index", timer);

    dev::start_timer("List", timer);
    List result_(patterns.size());
    for (size_t i = 0; i < patterns.size(); i++) {
        std::string pattern = patterns[i];
        auto range = index.equal_range(pattern);
        //Rcout << pattern << ": " << index.count(pattern) << "\n";
        int j = 0;
        IntegerVector value_(index.count(pattern));
        for (auto it = range.first; it != range.second; ++it) {
            value_[j] = it->second;
            j++;
        }
        result_[i] = unique(value_);
    }
    result_.attr("key") = encode(patterns);
    dev::stop_timer("List", timer);
    
    return result_;
}

/*** R
#out <- cpp_index_types(c("a*", "*b", "*c*", "跩*"), 
#                       c("bbb", "aaa", "跩购鹇", "ccc", "aa", "bb"))
cpp_index_types("跩*", c("跩购鹇", "跩"))
*/
