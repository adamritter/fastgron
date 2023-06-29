#include <string>
#include <vector>
#include <optional>
#include <cctype>
#include <variant>
#include "parse_path.hpp"
#include <iostream>
using namespace std;

bool isIdentifierChar(char c)
{
    return std::isalnum(c) || c == '_';
}

inline bool isDigitOrMinus(char c)
{
    return std::isdigit(c) || c == '-';
}

// TODO: Implement batching later
void ObjectAccessors::batchOrInsert(ObjectAccessor object_accessor)
{
    std::vector<ObjectAccessor>::iterator it = std::lower_bound(object_accessors.begin(), object_accessors.end(), object_accessor);
    if (it != object_accessors.end())
    {
        throw std::runtime_error("Cannot insert a value accessor for a key that already exists.");
    }
    else
    {
        object_accessors.emplace(it, std::move(object_accessor));
    }
}

class Parser
{
public:
    Parser(const std::string_view &input) : input_(input), index_(0) {}

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

        accessors.object_accessors.emplace_back(parseObjectAccessor());
        while (match(','))
        {
            if (lookAhead(3) == "...")
            {
                accessors.echo_others = true;
                consume(3);
                break;
            }
            accessors.object_accessors.emplace_back(parseObjectAccessor());
        }
        sort(accessors.object_accessors.begin(), accessors.object_accessors.end(), [](const ObjectAccessor &a, const ObjectAccessor &b)
             { return a.key < b.key; });
        return accessors;
    }

    ObjectAccessor parseObjectAccessor()
    {
        ObjectAccessor accessor;
        match('.'); // Allow one more .
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
            accessor.value_accessor = parseValueAccessor();
        }
        return accessor;
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
                auto allAccessor = std::make_unique<AllAccessor>();
                allAccessor->value_accessor = parseValueAccessor();
                return allAccessor;
            }
            else if (match('"'))
            {
                ObjectAccessors objectAccessors;
                string key = parseJSONString();
                expect(']');
                objectAccessors.object_accessors.emplace_back(ObjectAccessor{key, nullopt, parseValueAccessor()});
                return std::make_unique<ObjectAccessors>(std::move(objectAccessors));
            }
            else
            {
                return std::make_unique<Slice>(parseSlice());
            }
        }
        else if (match('{'))
        {
            ObjectAccessors objectAccessors = parseObjectAccessors();
            expect('}');
            return std::make_unique<ObjectAccessors>(std::move(objectAccessors));
        }
        else if (match('.'))
        {
            if (match('{'))
            {
                ObjectAccessors objectAccessors = parseObjectAccessors();
                expect('}');
                return std::make_unique<ObjectAccessors>(std::move(objectAccessors));
            }
            Slice slice;
            if (match('#'))
            {
                AllAccessor allAccessor;
                allAccessor.value_accessor = parseValueAccessor();
                return std::make_unique<AllAccessor>(std::move(allAccessor));
            }
            else if (tryParseInt(slice.start))
            {
                slice.append_index = true;
                slice.end = slice.start + 1;
                slice.step = 1;
                slice.value_accessor = parseValueAccessor();
                return std::make_unique<Slice>(std::move(slice));
            }
            else if (isIdentifierChar(input_[index_]))
            {
                ObjectAccessor objectAccessor = parseObjectAccessor();
                ObjectAccessors objectAccessors;
                objectAccessors.object_accessors.emplace_back(std::move(objectAccessor));
                return std::make_unique<ObjectAccessors>(std::move(objectAccessors));
            }

            else
            {
                throw std::runtime_error("Expected a value accessor, path: " + std::string(input_) + " index: " + std::to_string(index_));
            }
        }
        else if (index_ == input_.length() || input_[index_] == ',' || input_[index_] == '}')
        {
            return std::monostate{};
        }
        else
        {
            throw std::runtime_error("Expected a value accessor, path: " + std::string(input_) + " index: " + std::to_string(index_));
        }
    }

    Slice
    parseSlice()
    {
        Slice slice;
        if (match('[')) // Double [
        {
            slice.append_index = false;
            if (!tryParseInt(slice.start))
            {
                throw std::runtime_error("Expected a value accessor, path: " + std::string(input_) + " index: " + std::to_string(index_));
            }
            expect(']');
            slice.end = slice.start + 1;
            slice.step = 1;
            slice.value_accessor = parseValueAccessor();
            return slice;
        }
        slice.append_index = true;

        tryParseInt(slice.start);

        if (match(':'))
        {
            tryParseInt(slice.end);
            if (match(':'))
            {
                tryParseInt(slice.step);
            }
        }
        else
        {
            slice.end = slice.start + 1;
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

    bool tryParseInt(int &value)
    {
        skipWhitespace();
        if (index_ >= input_.size() || !isDigitOrMinus(input_[index_]))
        {
            return false;
        }
        std::size_t start = index_;
        while (index_ < input_.size() && isDigitOrMinus(input_[index_]))
        {
            ++index_;
        }
        value = std::stoi(string(input_.substr(start, index_ - start)));
        return true;
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
    const std::string_view &input_;
    std::size_t index_;
};

void debug_print_path(Slice &slice);
void debug_print_path(AllAccessor &allAccessor);
void debug_print_path(ObjectAccessors &objectAccessors);
void debug_print_path(ValueAccessor &valueAccessor);

void debug_print_path(Slice &slice)
{
    cout << "Slice: " << slice.start << " " << slice.end << " " << slice.step << endl;
}

void debug_print_path(AllAccessor &allAccessor)
{
    cout << "AllAccessor" << endl;
}

void debug_print_path(ObjectAccessors &objectAccessors)
{
    cout << "ObjectAccessors" << endl;
    for (auto &objectAccessor : objectAccessors.object_accessors)
    {
        cout << "ObjectAccessor: key: " << objectAccessor.key << endl;
        if (objectAccessor.new_key)
        {
            cout << "New key: " << *objectAccessor.new_key << endl;
        }
        cout << "Value accessor: " << endl;
        debug_print_path(objectAccessor.value_accessor);
    }
}

void debug_print_path(ValueAccessor &valueAccessor)
{
    if (std::holds_alternative<std::monostate>(valueAccessor))
    {
        cout << "monostate" << endl;
    }
    else if (std::holds_alternative<std::unique_ptr<Slice>>(valueAccessor))
    {
        debug_print_path(*std::get<std::unique_ptr<Slice>>(valueAccessor));
    }
    else if (std::holds_alternative<std::unique_ptr<ObjectAccessors>>(valueAccessor))
    {
        debug_print_path(*std::get<std::unique_ptr<ObjectAccessors>>(valueAccessor));
    }
    else if (std::holds_alternative<std::unique_ptr<AllAccessor>>(valueAccessor))
    {
        debug_print_path(*std::get<std::unique_ptr<AllAccessor>>(valueAccessor));
    }
}

ValueAccessor parse_path(std::string_view input)
{
    Parser parser(input);
    ValueAccessor valueAccessor = parser.parse();
    // debug_print_path(valueAccessor);
    return std::move(valueAccessor);
}
