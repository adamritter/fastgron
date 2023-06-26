// This file contains the data structure for storing path grammars.
// Example paths are:
// .a[1].b.2[4:][-3:]  that contain slices and object accessors.
// Multiple object accessors can be created like this .{id,users[1].{name,address}}
// It's also equivalent to this: .{id,users[1].name,users[1].address}
// Renaming can be supported multiple ways:
// .{id,user:users[1]:.{name,address}}   or .{id,name:users[1].name,address:users[1].address}
// or {.id,user.name:users[1].name,user.address:users[1].address}
// Rewinding or multipath support is needed to support this:
// .{id,user:users[1],name:users[1].name,address:users[1].address}
// Let's go with multipath. rewriting the last expression looks like this:
// .{id,user:users[1]:{name,address}}
// [[1]] is an index accessor without outputting on the path.

#pragma once
#include <optional>
#include <cctype>
#include <variant>
using namespace std;

struct AllAccessor;
struct Slice;
struct ObjectAccessor;
struct ObjectAccessors;

using ValueAccessor = std::variant<std::monostate,
                                   Slice,
                                   ObjectAccessors,
                                   AllAccessor>;
// Foreach object key or foreach array index
struct AllAccessor
{
    ValueAccessor value_accessor;
};

struct Slice
{
    int start = 0;
    int step = 1;
    int end = -1;
    bool append_index;
    ValueAccessor value_accessor;
};

struct ObjectAccessors
{
    std::vector<ObjectAccessor> object_accessors;
    bool echo_others;
};

struct ObjectAccessor
{
    std::string key;
    std::optional<std::string> new_key;
    ValueAccessor value_accessor;
};

/*

// encode the example paths in the given data structure. Don't use helper functions.
// For the path ".a[1].b.2[4:][-3:]"
vector<ValueAccessor> getPath1()
{
    Slice slice2 = {-3, -1, 1, true, std::monostate{}};
    Slice slice1 = {4, -1, 1, true, slice2};
    // .2 is equivalent to [2]
    Slice slice0 = {2, 2, 1, true, slice1};
    ObjectAccessor accessorB = {"b", "", false, slice0};
    Slice sliceForA = {1, 1, 1, true, accessorB};
    ObjectAccessor rootAccessor1 = {"a", "", false, sliceForA};
    std::vector<ValueAccessor> rootAccessors1 = {rootAccessor1};
}

// For the path ".{id,users[1].name,users[1].address}"
vector<ValueAccessor> getPath2()
{
    Slice sliceForName = {1, 1, 1, true, ObjectAccessor{"name", "name", std::monostate{}}};
    Slice sliceForAddress = {1, 1, 1, true, ObjectAccessor{"address", "address", std::monostate{}}};
    std::vector<ValueAccessor> multipleAccessorsForUsers = {sliceForName, sliceForAddress};
    ObjectAccessor accessorUsers = {"users", "users", multipleAccessorsForUsers};
    std::vector<ValueAccessor> rootAccessors2 = {ObjectAccessor{"id", "id", std::monostate{}}, accessorUsers};
    return rootAccessors2;
}

// For renaming ".{id,user:users[1]:.{name,address}}"
vector<ValueAccessor> getPath3()
{
    ValueAccessor inner = {{ObjectAccessor{"name", "name", std::monostate{}},
                            ObjectAccessor{"address", "address", std::monostate{}}},
                           false};
    Slice sliceForAddressWithRename = {1, 1, 1, false, inner};
    std::vector<ValueAccessor> renamedAccessorsForUsers = {sliceForNameWithRename, sliceForAddressWithRename};
    ObjectAccessor accessorUsersWithRename = {"users", "user", renamedAccessorsForUsers};
    std::vector<ValueAccessor> rootAccessors3 = {
        {ObjectAccessor{"id", "id", std::monostate{}}, accessorUsersWithRename},
        false};
    return rootAccessors3;
}
*/