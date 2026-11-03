#include "godxplr/core/query_parser.h"
#include <iostream>
#include <cstdlib>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << "\n"; \
            std::abort(); \
        } \
    } while(0)

void test_basic_term_and_phrase() {
    auto node = gx::query::Parser::parse("report \"annual review 2026\"");
    TEST_ASSERT(node != nullptr);
    TEST_ASSERT(node->type == gx::query::NodeType::And);

    std::string dumpStr = gx::query::Parser::dump(node.get());
    TEST_ASSERT(dumpStr == "AND(Term(report) Phrase(\"annual review 2026\"))");
    std::cout << "[PASS] test_basic_term_and_phrase: " << dumpStr << "\n";
}

void test_or_and_precedence() {
    // "cat dog | bird fish" -> OR( AND(cat, dog) , AND(bird, fish) )
    auto node = gx::query::Parser::parse("cat dog | bird fish");
    TEST_ASSERT(node != nullptr);
    TEST_ASSERT(node->type == gx::query::NodeType::Or);

    std::string dumpStr = gx::query::Parser::dump(node.get());
    TEST_ASSERT(dumpStr == "OR(AND(Term(cat) Term(dog)) | AND(Term(bird) Term(fish)))");
    std::cout << "[PASS] test_or_and_precedence: " << dumpStr << "\n";
}

void test_filters_and_sizes() {
    auto node = gx::query::Parser::parse("ext:cpp;h size:>10mb file:");
    TEST_ASSERT(node != nullptr);
    TEST_ASSERT(node->type == gx::query::NodeType::And);

    std::string dumpStr = gx::query::Parser::dump(node.get());
    TEST_ASSERT(dumpStr.find("Ext(cpp,h)") != std::string::npos);
    TEST_ASSERT(dumpStr.find("Size(10485760..0)") != std::string::npos);
    TEST_ASSERT(dumpStr.find("Flag(File)") != std::string::npos);
    std::cout << "[PASS] test_filters_and_sizes: " << dumpStr << "\n";
}

void test_not_and_double_negation() {
    auto node = gx::query::Parser::parse("!temp !(!final)");
    TEST_ASSERT(node != nullptr);
    node = gx::query::Parser::normalize(std::move(node));

    std::string dumpStr = gx::query::Parser::dump(node.get());
    // !(!final) should be normalized to final
    TEST_ASSERT(dumpStr == "AND(NOT(Term(temp)) Term(final))");
    std::cout << "[PASS] test_not_and_double_negation: " << dumpStr << "\n";
}

int main() {
    std::cout << "Running Query Parser unit tests...\n";
    test_basic_term_and_phrase();
    test_or_and_precedence();
    test_filters_and_sizes();
    test_not_and_double_negation();
    std::cout << "All Query Parser unit tests passed successfully.\n";
    return 0;
}
