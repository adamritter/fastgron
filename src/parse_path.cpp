#include <string>
#include <vector>
#include <optional>
#include <cctype>
#include <variant>
#include "parse_path.hpp"

bool isIdentifierChar(char c)
{
    return std::isalnum(c) || c == '_';
}

class Parser
{
public:
    Parser(const std::string &input) : input_(input), index_(0) {}

    ValueAccessor parse()
    {
        return parseValueAccessor();
    }

private:
    std::string_view lookAhead(int N)
    {
        // Ensure we're not exceeding the bounds of the input string.
        if (index_ + N > input_.size())
        {
            throw std::runtime_error("Attempted to look ahead beyond the end of input.");
        }

        // Return a view of the next N characters from the input string without advancing the index.
        return std::string_view(input_).substr(index_, N);
    }

    void consume(int N)
    {
        // Ensure we're not exceeding the bounds of the input string.
        if (index_ + N > input_.size())
        {
            throw std::runtime_error("Attempted to consume beyond the end of input.");
        }

        // Advance the index by N characters.
        index_ += N;
    }

    ObjectAccessors parseObjectAccessors()
    {
        ObjectAccessors accessors;
        parseObjectAccessor(accessors.object_accessors.emplace_back());
        while (match(','))
        {
            if (lookAhead(3) == "...")
            {
                accessors.echo_others = true;
                consume(3);
                break;
            }
            parseObjectAccessor(accessors.object_accessors.emplace_back());
        }
        sort(accessors.object_accessors.begin(), accessors.object_accessors.end(), [](const ObjectAccessor &a, const ObjectAccessor &b)
             { return a.key < b.key; });
        return accessors;
    }

    void parseObjectAccessor(ObjectAccessor &accessor)
    {
        parseIdentifier(accessor.key);
        if (match(':'))
        {
            accessor.new_key.emplace();         // Here we separate the emplace operation
            parseIdentifier(*accessor.new_key); // We dereference the optional to get the string reference
            if (match(':'))
            {
                accessor.value_accessor = parseValueAccessor();
            }
        }
        else
        {
            accessor.value_accessor = std::monostate{};
        }
    }

    string parseJSONString()
    {
        string s;
        while (index_ < input_.size())
        {
            if (match('"'))
            {
                break;
            }
            match('\\'); // Ignore escape
            s += input_[index_++];
        }
        return s;
    }

    ValueAccessor parseValueAccessor()
    {
        if (match('['))
        {
            if (match('#'))
            {
                expect(']');
                AllAccessor allAccessor;
                allAccessor.value_accessor = parseValueAccessor();
                return allAccessor;
            }
            else if (match('"'))
            {
                ObjectAccessors objectAccessors;
                string key = parseJSONString();
                expect(']');
                objectAccessors.object_accessors.emplace_back(ObjectAccessor{key, nullopt, parseValueAccessor()});
            }
            else
            {
                return parseSlice();
            }
        }
        else if (match('{'))
        {
            ObjectAccessors objectAccessors = parseObjectAccessors();
            expect('}');
            return objectAccessors;
        }
        else if (match('.'))
        {
            if (match('#'))
            {
                AllAccessor allAccessor;
                allAccessor.value_accessor = parseValueAccessor();
                return allAccessor;
            }
            else if (isIdentifierChar(input_[index_]))
            {
                ObjectAccessors objectAccessors;
                string key = parseJSONString();
                objectAccessors.object_accessors.emplace_back(ObjectAccessor{key, nullopt, parseValueAccessor()});
                return objectAccessors;
            }
            else
            {
                Slice slice;
                slice.append_index = true;
                parseInt(slice.start);
                slice.end = slice.start;
                slice.step = 1;
                slice.value_accessor = parseValueAccessor();
                return slice;
            }
        }
    }

    Slice parseSlice()
    {
        Slice slice;
        if (match('['))
        {
            slice.append_index = false;
            parseInt(slice.start);
            expect(']');
            slice.end = slice.start;
            slice.step = 1;
            slice.value_accessor = parseValueAccessor();
            return slice;
        }
        slice.append_index = true;

        expect('[');
        parseInt(slice.start);

        expect(':');
        parseInt(slice.step);
        if (match(':'))
        {
            parseInt(slice.end);
        }
        else
        {
            slice.end = slice.step;
            slice.step = 1;
        }
        expect(']');
        slice.value_accessor = parseValueAccessor();
        return slice;
    }

    void parseIdentifier(std::string &identifier)
    {
        skipWhitespace();
        std::size_t start = index_;
        while (index_ < input_.size() && isIdentifierChar(input_[index_]))
        {
            ++index_;
        }
        identifier = input_.substr(start, index_ - start);
    }

    void parseInt(int &value)
    {
        skipWhitespace();
        std::size_t start = index_;
        while (index_ < input_.size() && std::isdigit(input_[index_]))
        {
            ++index_;
        }
        value = std::stoi(input_.substr(start, index_ - start));
    }

    void expect(char c)
    {
        skipWhitespace();
        if (index_ >= input_.size() || input_[index_] != c)
        {
            throw std::runtime_error("Expected character '" + std::string(1, c) + "' not found.");
        }
        ++index_;
    }

    bool match(char c)
    {
        skipWhitespace();
        if (index_ < input_.size() && input_[index_] == c)
        {
            ++index_;
            return true;
        }
        return false;
    }

    void skipWhitespace()
    {
        while (index_ < input_.size() && std::isspace(input_[index_]))
        {
            ++index_;
        }
    }

private:
    const std::string &input_;
    std::size_t index_;
};
