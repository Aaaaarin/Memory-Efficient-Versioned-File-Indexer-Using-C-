Memory-Efficient Versioned File Indexer
Author: Aarin Sheth
Roll Number: 230013
Course: CS253 - Systems Programming
Assignment: 1
Submission Date: March 6, 2026

📋 Table of Contents
Overview

Features

Requirements

Design Architecture

Class Structure

Compilation

Usage

Examples

Memory Management

Error Handling

Testing

Performance

Limitations

References

🔍 Overview
This project implements a memory-efficient versioned file indexer that processes large text files without loading them entirely into memory. The program builds word-frequency indexes for multiple file versions and supports analytical queries including word frequency lookup, version comparison, and top-K frequent words retrieval.

Key Innovation: Fixed-size buffer processing (256KB-1024KB) ensures memory usage remains constant regardless of input file size, making it suitable for terabyte-scale log files.

✨ Features
Core Functionality
✅ Buffer-based file reading - Processes files in fixed-size chunks

✅ Cross-boundary token handling - Correctly reassembles words split across chunks

✅ Case-insensitive word matching - Converts all words to lowercase

✅ Multi-version support - Maintains independent indexes for different file versions

Query Types
Query Type	Description	Example
Word Count	Frequency of a specific word in a version	--query word --word error
Difference	Compare word frequency between two versions	--query diff --word error
Top-K	Most frequent words in a version	--query top --top 10
Technical Features
✅ Object-oriented design with 5+ custom classes

✅ Inheritance with abstract base class

✅ Runtime polymorphism via virtual functions

✅ Function overloading (multiple tokenize methods)

✅ Exception handling for all error conditions

✅ Template class (FixedBuffer<T>)

✅ Move semantics for efficient resource management

✅ RAII principles throughout

📦 Requirements
Software Requirements
Compiler: GCC 7+ or Clang 5+ with C++17 support

Build System: Command-line with g++

OS: Linux/Unix (primary), Windows (with MinGW/Cygwin)

Hardware Constraints
Minimum RAM: 256 KB + index size (grows with vocabulary)

Recommended RAM: 1-2 MB for typical datasets

Disk Space: Sufficient for input files and program binary

Compilation Command
bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -o analyzer rollnumber_firstname.cpp
🏗️ Design Architecture
High-Level Architecture
text
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Command Line  │────▶│  QueryProcessor │────▶│ VersionedIndex  │
│     Parser      │     │                 │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                                 │                        │
                                 ▼                        ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│ BufferedFileRead│────▶│    Tokenizer    │────▶│  Word Frequency │
│                 │     │                 │     │      Maps       │
└─────────────────┘     └─────────────────┘     └─────────────────┘
Data Flow
Command-line arguments → Parsed by QueryProcessor

File reading → BufferedFileReader reads in chunks

Tokenization → Tokenizer extracts words from chunks

Indexing → VersionedIndex stores word frequencies

Query execution → Polymorphic Query objects

Output → Formatted results with timing information

📁 Class Structure
1. FixedBuffer<T> (Template Class)
cpp
template<typename T>
class FixedBuffer {
    // RAII-managed fixed-size heap buffer
    // Non-copyable, movable
    // Provides raw pointer access with bounds safety
};
Responsibility: Provide exception-safe, fixed-size memory buffer

2. BufferedFileReader
cpp
class BufferedFileReader {
    // Uses FixedBuffer<char> internally
    // Maintains leftover partial tokens
    // Handles chunk boundary splitting
};
Responsibility: Stream file in fixed-size chunks with boundary detection

3. Tokenizer
cpp
class Tokenizer {
    // Stateless design
    // Callback-based processing
    // Overloaded tokenize methods
};
Responsibility: Extract alphanumeric words, convert to lowercase

4. VersionedIndex
cpp
class VersionedIndex {
    // unordered_map<string, unordered_map<string, uint64_t>>
    // Supports frequency queries, differences, top-K
};
Responsibility: Store and query word frequencies per version

5. Query (Abstract Base Class)
cpp
class Query {
    virtual void execute(const VersionedIndex&) const = 0;
    virtual std::string description() const = 0;
};
Derived Classes:

WordQuery - Single word frequency lookup

DiffQuery - Compare frequencies between versions

TopKQuery - Most frequent words listing

6. QueryProcessor
cpp
class QueryProcessor {
    // Parses arguments, validates input
    // Coordinates indexing
    // Creates appropriate Query object
    // Times execution
};
Responsibility: Main orchestration class

🔧 Compilation
Basic Compilation
bash
# Compile with optimizations
g++ -std=c++17 -O2 -Wall -Wextra -pedantic -o analyzer rollnumber_firstname.cpp

# Compile with debug symbols
g++ -std=c++17 -g -O0 -Wall -Wextra -pedantic -o analyzer_debug rollnumber_firstname.cpp

# Compile with sanitizers (for debugging)
g++ -std=c++17 -fsanitize=address -fsanitize=undefined -g -O0 -o analyzer_sanitized rollnumber_firstname.cpp
Makefile (Optional)
makefile
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET = analyzer
SOURCE = rollnumber_firstname.cpp

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

debug:
	$(CXX) -std=c++17 -g -O0 -Wall -Wextra -pedantic -o $(TARGET)_debug $(SOURCE)
📖 Usage
Command-Line Arguments
Argument	Required For	Description	Constraints
--file <path>	word, top	Single file path	Must exist, readable
--file1 <path>	diff	First file for comparison	Must exist, readable
--file2 <path>	diff	Second file for comparison	Must exist, readable
--version <name>	word, top	Version identifier	Alphanumeric recommended
--version1 <name>	diff	First version name	Alphanumeric recommended
--version2 <name>	diff	Second version name	Alphanumeric recommended
--buffer <kb>	All	Buffer size in KB	256-1024, default 512
--query <type>	All	Query type	word, diff, top
--word <token>	word, diff	Word to search	Case-insensitive
--top <k>	top	Number of results	≥1, default 10
Command Syntax
Word Query
bash
./analyzer --file <path> --version <name> --buffer <kb> --query word --word <token>
Top-K Query
bash
./analyzer --file <path> --version <name> --buffer <kb> --query top --top <k>
Difference Query
bash
./analyzer --file1 <path> --version1 <name> --file2 <path> --version2 <name> \
           --buffer <kb> --query diff --word <token>
💡 Examples
Example 1: Word Frequency Query
Command:

bash
./analyzer --file sample.log --version v1 --buffer 512 --query word --word error
Sample Output:

text
======================================================================
Memory-Efficient Versioned File Indexer
======================================================================
Query type : word
Buffer     : 512 KB (524288 bytes)

Indexing 'sample.log' as version 'v1' ... done

[Query] word [version=v1, word=error]
  Version   : v1
  Word      : error
  Frequency : 156

Buffer size (KB): 512
Total execution time (s): 0.234567
Example 2: Top-K Query
Command:

bash
./analyzer --file app.log --version production --buffer 1024 --query top --top 5
Sample Output:

text
======================================================================
Memory-Efficient Versioned File Indexer
======================================================================
Query type : top
Buffer     : 1024 KB (1048576 bytes)

Indexing 'app.log' as version 'production' ... done

[Query] top [version=production, k=5]
  Version   : production
  Top-K     : 5
       1. error                         1245
       2. info                          892
       3. debug                          567
       4. warning                        234
       5. critical                        78

Buffer size (KB): 1024
Total execution time (s): 0.456789
Example 3: Difference Query
Command:

bash
./analyzer --file1 v1.log --version1 v1 --file2 v2.log --version2 v2 \
           --buffer 512 --query diff --word timeout
Sample Output:

text
======================================================================
Memory-Efficient Versioned File Indexer
======================================================================
Query type : diff
Buffer     : 512 KB (524288 bytes)

Indexing 'v1.log' as version 'v1' ... done
Indexing 'v2.log' as version 'v2' ... done

[Query] diff [v1=v1, v2=v2, word=timeout]
  Version1  : v1 (freq = 42)
  Version2  : v2 (freq = 38)
  Word      : timeout
  Difference: +4 (v1 - v2)

Buffer size (KB): 512
Total execution time (s): 0.345678
💾 Memory Management
Memory Usage Breakdown
Component	Memory Consumption	Growth Pattern
Fixed Buffer	256-1024 KB	Constant
Index Structure	O(unique_words)	Linear with vocabulary
Temporary Strings	Token size + overhead	Bounded by buffer
Program Code	~100-200 KB	Constant
Why Memory is Independent of File Size
Streaming: File processed in chunks, not loaded entirely

Incremental Indexing: Words counted as they're read

No Look-ahead: Only current chunk in memory

No Backtracking: Once processed, chunk discarded

Worst-Case Memory Scenario
text
1. Maximum buffer: 1024 KB
2. Maximum unique words: 1,000,000 (typical)
3. Average word length: 10 chars
4. Overhead per entry: ~50 bytes
5. Total index memory: ~50 MB

→ Total memory < 51 MB regardless of file size (1KB or 1TB)
⚠️ Error Handling
Error Categories
Category	Examples	Handling
File Errors	File not found, permissions	std::runtime_error with context
Input Validation	Invalid buffer size, missing args	Descriptive error messages
Memory Errors	Allocation failure	std::bad_alloc caught
Logic Errors	Duplicate versions, unknown query	Custom exceptions
Parsing Errors	Invalid numbers	std::invalid_argument
Example Error Messages
text
[ERROR] --buffer must be between 256 and 1024 KB, got: 128
[ERROR] File not found: missing.txt
[ERROR] Missing value after --version
[ERROR] Unknown query type: search
[ERROR] Version already indexed: v1
🧪 Testing
Test Cases
Basic Functionality
Test	Input	Expected Output
Empty file	--file empty.txt	Frequency 0 for any word
Single word	File with "hello" × 100	Word "hello" frequency 100
Case sensitivity	"Hello" and "hello"	Counted as same word
Boundary Tests
Test	Description	Validation
Split word	"hello" across chunk boundary	Counted once
Exact chunk	Word exactly fits buffer	Handled correctly
Multiple splits	Long word across 3 chunks	Reassembled correctly
Query Tests
Query	Test Case	Verification
Word	Non-existent word	Frequency 0
Diff	Same file, different versions	Difference 0
Top-K	K > unique words	All words returned
Sample Test Script
bash
#!/bin/bash
# test.sh - Simple test script

echo "=== Test 1: Word Query ==="
./analyzer --file test.log --version v1 --buffer 512 --query word --word error

echo "=== Test 2: Invalid Buffer ==="
./analyzer --file test.log --version v1 --buffer 100 --query word --word error

echo "=== Test 3: Top-K ==="
./analyzer --file test.log --version v1 --buffer 1024 --query top --top 5
📊 Performance
Benchmark Results (Sample)
File Size	Buffer Size	Indexing Time	Memory Used
10 MB	256 KB	0.15 s	1.2 MB
100 MB	256 KB	1.42 s	2.8 MB
1 GB	256 KB	14.8 s	5.6 MB
10 GB	1024 KB	152 s	8.3 MB
Performance Characteristics
Time Complexity: O(n) for indexing, where n = file size

Query Time: O(1) for word lookup, O(u log k) for top-k where u = unique words

I/O Efficiency: Sequential reads maximize disk throughput

CPU Usage: Minimal, dominated by I/O for large files

⚡ Limitations
Current Limitations
Word Definition: Only alphanumeric characters; punctuation separates words

Case Handling: Completely case-insensitive (no mixed-case preservation)

Encoding: Assumes ASCII/UTF-8; no Unicode normalization

Version Names: Must be unique within execution

Buffer Size: Fixed per execution, cannot be changed dynamically

Future Improvements (If Extended)
Unicode support with proper grapheme clustering

Configurable word delimiters

Persistent index storage between runs

Parallel processing for multi-core systems

Memory-mapped I/O for very large files

📚 References
C++ Standards Used
C++17 (std::filesystem would be used if extended)

RAII and smart pointers

Move semantics and perfect forwarding

STL containers and algorithms

Key Concepts Applied
Streaming Algorithms: Processing data in single pass

Object-Oriented Design: SOLID principles

Memory Efficiency: Fixed buffers, no unnecessary copies

Exception Safety: Strong guarantee where possible

External Resources
C++ Reference - STL documentation

Google C++ Style Guide - Coding standards

CS253 Course Materials - Course website

