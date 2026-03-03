// =============================================================================
//  rollnumber_firstname.cpp
//  CS253 Assignment 1 — Memory-Efficient Versioned File Indexer
//  Compile: g++ -std=c++17 -O2 -Wall -Wextra -pedantic -o analyzer rollnumber_firstname.cpp
// =============================================================================

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

[[nodiscard]] long long parseSignedLongLong(const std::string& text, const std::string& flag) {
    std::size_t pos = 0;
    long long value = 0;
    try {
        value = std::stoll(text, &pos);
    } catch (const std::invalid_argument&) {
        throw std::runtime_error(flag + " requires a numeric value, got: " + text);
    } catch (const std::out_of_range&) {
        throw std::runtime_error(flag + " value out of range: " + text);
    }
    if (pos != text.size()) {
        throw std::runtime_error(flag + " requires a pure integer, got: " + text);
    }
    return value;
}

} // namespace

// -----------------------------------------------------------------------------
//  Template class: FixedBuffer<T>
//  A fixed-capacity heap buffer that is non-copyable and movable.
// -----------------------------------------------------------------------------
template <typename T>
class FixedBuffer {
public:
    explicit FixedBuffer(std::size_t capacity)
        : capacity_(validate(capacity)), data_(std::make_unique<T[]>(capacity_)) {}

    FixedBuffer(const FixedBuffer&) = delete;
    FixedBuffer& operator=(const FixedBuffer&) = delete;

    FixedBuffer(FixedBuffer&& other) noexcept
        : capacity_(std::exchange(other.capacity_, 0)), data_(std::move(other.data_)) {}

    FixedBuffer& operator=(FixedBuffer&& other) noexcept {
        if (this != &other) {
            capacity_ = std::exchange(other.capacity_, 0);
            data_ = std::move(other.data_);
        }
        return *this;
    }

    T* data() noexcept { return data_.get(); }
    const T* data() const noexcept { return data_.get(); }
    std::size_t capacity() const noexcept { return capacity_; }

private:
    static std::size_t validate(std::size_t c) {
        if (c == 0) {
            throw std::invalid_argument("FixedBuffer: capacity must be > 0");
        }
        return c;
    }

    std::size_t capacity_;
    std::unique_ptr<T[]> data_;
};

// -----------------------------------------------------------------------------
//  BufferedFileReader
//  Reads file chunk-by-chunk with boundary-safe leftover handling.
// -----------------------------------------------------------------------------
class BufferedFileReader {
public:
    BufferedFileReader(const std::string& path, std::size_t bufferBytes)
        : path_(path), buffer_(bufferBytes), ifs_(path, std::ios::binary) {
        if (!ifs_.is_open()) {
            throw std::runtime_error("BufferedFileReader: cannot open file: " + path);
        }
    }

    BufferedFileReader(const BufferedFileReader&) = delete;
    BufferedFileReader& operator=(const BufferedFileReader&) = delete;

    [[nodiscard]] bool readChunk(std::string& out) {
        while (true) {
            if (exhausted_ && leftover_.empty()) {
                return false;
            }

            out.clear();
            if (!leftover_.empty()) {
                out.swap(leftover_);
            }

            if (exhausted_) {
                return !out.empty();
            }

            ifs_.read(buffer_.data(), static_cast<std::streamsize>(buffer_.capacity()));
            const std::streamsize n = ifs_.gcount();

            if (n == 0) {
                exhausted_ = true;
                return !out.empty();
            }

            out.append(buffer_.data(), static_cast<std::size_t>(n));

            if (ifs_.eof()) {
                exhausted_ = true;
                return true;
            }

            // Find last non-alphanumeric boundary in the entire assembled chunk.
            // This is essential when leftover_ already holds a partial token and the
            // newly-read bytes are also all-alphanumeric: in that case, the entire
            // `out` is still one unfinished token and must remain in leftover_.
            std::size_t pos = out.size();
            while (pos > 0 && std::isalnum(static_cast<unsigned char>(out[pos - 1]))) {
                --pos;
            }

            if (pos < out.size()) {
                leftover_ = out.substr(pos);
                out.resize(pos);
            } else {
                // No separator at all; the entire chunk is an unfinished token.
                leftover_.swap(out);
                out.clear();
            }

            if (!out.empty()) {
                return true;
            }
            // Entire assembled block was mid-token; read more and continue stitching.
        }
    }

private:
    std::string path_;
    FixedBuffer<char> buffer_;
    std::ifstream ifs_;
    std::string leftover_;
    bool exhausted_ = false;
};

// -----------------------------------------------------------------------------
//  Tokenizer
//  Stateless parser emitting lowercase alphanumeric words via callback.
// -----------------------------------------------------------------------------
class Tokenizer {
public:
    template <typename Callback>
    void tokenize(std::string_view chunk, Callback&& cb) const {
        tokenize(chunk, 1, std::forward<Callback>(cb));
    }

    template <typename Callback>
    void tokenize(std::string_view chunk, std::size_t minLen, Callback&& cb) const {
        std::size_t i = 0;
        std::string token;
        token.reserve(64);

        while (i < chunk.size()) {
            while (i < chunk.size() && !std::isalnum(static_cast<unsigned char>(chunk[i]))) {
                ++i;
            }
            if (i >= chunk.size()) {
                break;
            }

            const std::size_t start = i;
            while (i < chunk.size() && std::isalnum(static_cast<unsigned char>(chunk[i]))) {
                ++i;
            }

            const std::size_t len = i - start;
            if (len < minLen) {
                continue;
            }

            token.assign(chunk.data() + start, len);
            for (char& c : token) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            cb(token);
        }
    }
};

// -----------------------------------------------------------------------------
//  VersionedIndex
//  Stores independent word-frequency map per version.
// -----------------------------------------------------------------------------
class VersionedIndex {
public:
    using FreqMap = std::unordered_map<std::string, std::uint64_t>;

    void buildIndex(const std::string& version, BufferedFileReader& reader, const Tokenizer& tokenizer) {
        if (indexes_.count(version) != 0) {
            throw std::runtime_error("Version already indexed: " + version);
        }

        FreqMap& freqMap = indexes_[version];
        std::string chunk;
        while (reader.readChunk(chunk)) {
            // Intentionally call the overloaded API with explicit minimum length.
            tokenizer.tokenize(chunk, 1UL, [&freqMap](const std::string& word) { ++freqMap[word]; });
        }
    }

    [[nodiscard]] std::uint64_t frequency(const std::string& version, const std::string& word) const {
        const auto& map = getVersion(version);
        auto it = map.find(word);
        return it == map.end() ? 0ULL : it->second;
    }

    [[nodiscard]] std::int64_t diff(const std::string& v1, const std::string& v2, const std::string& word) const {
        return static_cast<std::int64_t>(frequency(v1, word)) - static_cast<std::int64_t>(frequency(v2, word));
    }

    [[nodiscard]] std::vector<std::pair<std::string, std::uint64_t>>
    topK(const std::string& version, std::size_t k) const {
        const auto& map = getVersion(version);

        using KV = std::pair<const std::string, std::uint64_t>;
        std::vector<const KV*> refs;
        refs.reserve(map.size());
        for (const auto& kv : map) {
            refs.push_back(&kv);
        }

        const std::size_t actualK = std::min(k, refs.size());
        std::partial_sort(
            refs.begin(),
            refs.begin() + static_cast<std::ptrdiff_t>(actualK),
            refs.end(),
            [](const KV* a, const KV* b) {
                if (a->second != b->second) {
                    return a->second > b->second;
                }
                return a->first < b->first;
            });

        std::vector<std::pair<std::string, std::uint64_t>> out;
        out.reserve(actualK);
        for (std::size_t i = 0; i < actualK; ++i) {
            out.emplace_back(refs[i]->first, refs[i]->second);
        }
        return out;
    }

private:
    const FreqMap& getVersion(const std::string& version) const {
        auto it = indexes_.find(version);
        if (it == indexes_.end()) {
            throw std::runtime_error("Unknown version: " + version);
        }
        return it->second;
    }

    std::unordered_map<std::string, FreqMap> indexes_;
};

// -----------------------------------------------------------------------------
//  Abstract query interface + derived queries (runtime polymorphism)
// -----------------------------------------------------------------------------
class Query {
public:
    virtual ~Query() = default;
    virtual void execute(const VersionedIndex& index) const = 0;
    virtual std::string description() const = 0;
};

class WordQuery final : public Query {
public:
    WordQuery(std::string version, std::string word)
        : version_(std::move(version)), word_(toLower(std::move(word))) {}

    void execute(const VersionedIndex& index) const override {
        const auto f = index.frequency(version_, word_);
        std::cout << "  Version   : " << version_ << "\n"
                  << "  Word      : " << word_ << "\n"
                  << "  Frequency : " << f << "\n";
    }

    std::string description() const override {
        return "word [version=" + version_ + ", word=" + word_ + "]";
    }

private:
    std::string version_;
    std::string word_;
};

class DiffQuery final : public Query {
public:
    DiffQuery(std::string version1, std::string version2, std::string word)
        : v1_(std::move(version1)), v2_(std::move(version2)), word_(toLower(std::move(word))) {}

    void execute(const VersionedIndex& index) const override {
        const std::ios::fmtflags savedFlags = std::cout.flags();

        const auto f1 = index.frequency(v1_, word_);
        const auto f2 = index.frequency(v2_, word_);
        const auto d = static_cast<std::int64_t>(f1) - static_cast<std::int64_t>(f2);

        std::cout << "  Version1  : " << v1_ << " (freq = " << f1 << ")\n"
                  << "  Version2  : " << v2_ << " (freq = " << f2 << ")\n"
                  << "  Word      : " << word_ << "\n"
                  << "  Difference: " << std::showpos << d << std::noshowpos
                  << " (" << v1_ << " - " << v2_ << ")\n";

        std::cout.flags(savedFlags);
    }

    std::string description() const override {
        return "diff [v1=" + v1_ + ", v2=" + v2_ + ", word=" + word_ + "]";
    }

private:
    std::string v1_;
    std::string v2_;
    std::string word_;
};

class TopKQuery final : public Query {
public:
    TopKQuery(std::string version, std::size_t k) : version_(std::move(version)), k_(k) {}

    void execute(const VersionedIndex& index) const override {
        const std::ios::fmtflags savedFlags = std::cout.flags();

        const auto rows = index.topK(version_, k_);
        std::cout << "  Version   : " << version_ << "\n"
                  << "  Top-K     : " << k_ << "\n";

        if (rows.empty()) {
            std::cout << "    (empty index)\n";
            std::cout.flags(savedFlags);
            return;
        }

        std::size_t rank = 1;
        for (const auto& [word, count] : rows) {
            std::cout << "    " << std::right << std::setw(4) << rank++
                      << ". " << std::left << std::setw(28) << word
                      << " " << count << "\n";
        }

        std::cout.flags(savedFlags);
    }

    std::string description() const override {
        return "top [version=" + version_ + ", k=" + std::to_string(k_) + "]";
    }

private:
    std::string version_;
    std::size_t k_;
};

// -----------------------------------------------------------------------------
//  QueryProcessor
//  Parses CLI, builds indexes, creates query object, executes and times work.
// -----------------------------------------------------------------------------
class QueryProcessor {
public:
    QueryProcessor(int argc, char* argv[]) { parseArgs(argc, argv); }

    void run() {
        using Clock = std::chrono::high_resolution_clock;

        const auto start = Clock::now();
        printBanner();

        Tokenizer tokenizer;

        if (cfg_.query == "word" || cfg_.query == "top") {
            indexFile(cfg_.file, cfg_.version, tokenizer); // overload usage
        } else {
            indexFile(cfg_.file1, cfg_.version1, cfg_.bufferBytes(), tokenizer);
            indexFile(cfg_.file2, cfg_.version2, cfg_.bufferBytes(), tokenizer);
        }

        std::unique_ptr<Query> query = makeQuery();

        std::cout << "\n[Query] " << query->description() << "\n";
        query->execute(index_);

        const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        std::cout << "\nBuffer size (KB): " << cfg_.buffer_kb << "\n"
                  << "Total execution time (s): " << std::fixed << std::setprecision(6) << elapsed
                  << "\n";
    }

private:
    struct Config {
        std::string file;
        std::string file1;
        std::string file2;
        std::string version;
        std::string version1;
        std::string version2;
        std::string query;
        std::string word;
        std::size_t top_k = 10;
        std::size_t buffer_kb = 512;

        [[nodiscard]] std::size_t bufferBytes() const noexcept { return buffer_kb * 1024; }
    };

    Config cfg_;
    VersionedIndex index_;

    static const std::string& separator() {
        static const std::string s(70, '=');
        return s;
    }

    void printBanner() const {
        std::cout << separator() << "\n"
                  << "Memory-Efficient Versioned File Indexer\n"
                  << separator() << "\n"
                  << "Query type : " << cfg_.query << "\n"
                  << "Buffer     : " << cfg_.buffer_kb << " KB (" << cfg_.bufferBytes() << " bytes)\n\n";
    }

    void parseArgs(int argc, char* argv[]) {
        int i = 1;
        auto next = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value after " + flag);
            }
            return std::string(argv[++i]);
        };

        while (i < argc) {
            const std::string arg = argv[i];
            if (arg == "--file") {
                cfg_.file = next(arg);
            } else if (arg == "--file1") {
                cfg_.file1 = next(arg);
            } else if (arg == "--file2") {
                cfg_.file2 = next(arg);
            } else if (arg == "--version") {
                cfg_.version = next(arg);
            } else if (arg == "--version1") {
                cfg_.version1 = next(arg);
            } else if (arg == "--version2") {
                cfg_.version2 = next(arg);
            } else if (arg == "--query") {
                cfg_.query = next(arg);
            } else if (arg == "--word") {
                cfg_.word = next(arg);
            } else if (arg == "--top") {
                const std::string v = next(arg);
                const long long parsed = parseSignedLongLong(v, "--top");
                if (parsed <= 0) {
                    throw std::runtime_error("--top must be >= 1, got: " + v);
                }
                cfg_.top_k = static_cast<std::size_t>(parsed);
            } else if (arg == "--buffer") {
                const std::string v = next(arg);
                const long long parsed = parseSignedLongLong(v, "--buffer");
                if (parsed < 256 || parsed > 1024) {
                    throw std::runtime_error("--buffer must be between 256 and 1024 KB, got: " + v);
                }
                cfg_.buffer_kb = static_cast<std::size_t>(parsed);
            } else {
                throw std::runtime_error("Unknown argument: " + arg);
            }
            ++i;
        }

        validateConfig();
    }

    void validateConfig() const {
        if (cfg_.query != "word" && cfg_.query != "diff" && cfg_.query != "top") {
            throw std::runtime_error("--query must be one of: word | diff | top");
        }

        if (cfg_.query == "word") {
            if (cfg_.file.empty() || cfg_.version.empty() || cfg_.word.empty()) {
                throw std::runtime_error("word query requires --file --version --word");
            }
        } else if (cfg_.query == "top") {
            if (cfg_.file.empty() || cfg_.version.empty()) {
                throw std::runtime_error("top query requires --file --version");
            }
        } else {
            if (cfg_.version1 == cfg_.version2) {
                throw std::runtime_error(
                    "diff query: --version1 and --version2 must be different");
            }
            if (cfg_.file1.empty() || cfg_.version1.empty() || cfg_.file2.empty() ||
                cfg_.version2.empty() || cfg_.word.empty()) {
                throw std::runtime_error(
                    "diff query requires --file1 --version1 --file2 --version2 --word");
            }
        }
    }

    void indexFile(const std::string& path, const std::string& version, const Tokenizer& tokenizer) {
        indexFile(path, version, cfg_.bufferBytes(), tokenizer);
    }

    void indexFile(const std::string& path,
                   const std::string& version,
                   std::size_t bufferBytes,
                   const Tokenizer& tokenizer) {
        std::cout << "Indexing '" << path << "' as version '" << version << "' ... ";
        std::cout.flush();

        BufferedFileReader reader(path, bufferBytes);
        index_.buildIndex(version, reader, tokenizer);

        std::cout << "done\n";
    }

    std::unique_ptr<Query> makeQuery() const {
        if (cfg_.query == "word") {
            return std::make_unique<WordQuery>(cfg_.version, cfg_.word);
        }
        if (cfg_.query == "diff") {
            return std::make_unique<DiffQuery>(cfg_.version1, cfg_.version2, cfg_.word);
        }
        if (cfg_.query == "top") {
            return std::make_unique<TopKQuery>(cfg_.version, cfg_.top_k);
        }
        throw std::logic_error("Invalid query type after validation");
    }
};

static void printUsage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " --file <path> --version <name> --buffer <kb> --query word --word <token>\n"
        << "  " << prog << " --file <path> --version <name> --buffer <kb> --query top --top <k>\n"
        << "  " << prog
        << " --file1 <path> --version1 <name> --file2 <path> --version2 <name> --buffer <kb> --query diff --word <token>\n\n"
        << "Arguments:\n"
        << "  --file <path>\n"
        << "  --file1 <path>\n"
        << "  --file2 <path>\n"
        << "  --version <name>\n"
        << "  --version1 <name>\n"
        << "  --version2 <name>\n"
        << "  --buffer <kb>    (256..1024, default 512)\n"
        << "  --query word | diff | top\n"
        << "  --word <token>\n"
        << "  --top <k>        (default 10)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        QueryProcessor processor(argc, argv);
        processor.run();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] " << ex.what() << '\n';
        return 1;
    }
}
