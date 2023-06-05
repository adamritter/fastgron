# GRON File Format Specification

## 1. Introduction

GRON, standing for Greppable Object Notation, is a format derived from JavaScript designed to simplify JSON data for grep-like operations. The structure flattens JSON to enhance parsing with tools such as `grep`. This is particularly beneficial in shell scripting and similar contexts.

## 2. File Format Structure

### 2.1. Root Object

A GRON file SHOULD commence with an empty root object defined as `json = {}`, `json = []` or `jsonl = []`

### 2.2. Primitive Data Types

GRON supports both string and numeric data types, in accordance with the standard syntax of JavaScript.

```javascript
json.name = "Joe";
json.age = 30;
```

### 2.3. String Indexing

GRON allows for string-based indexing to assign values to objects. This feature enables the setting and accessing of properties that might contain special characters or spaces.

```javascript
json["hello world"] = 3;
```

### 2.4. Arrays

Arrays in GRON are expressed across multiple lines, with each entry specified individually. Array indices are numeric, starting from `0`.

```javascript
json.likes = [];
json.likes[0] = "code";
json.likes[1] = "cheese";
```

### 2.5. Nested Objects

GRON supports nested objects. These objects are defined in a manner similar to the root object, and they are assigned as properties to their parent object.

```javascript
json.contact = {};
json.contact.email = "joe@gmail.com";
```

## 3. Syntax Rules

- Each field MUST be declared and assigned a value on its own line.
- Values MUST be valid JSON values (string, number, boolean or null)
- Arrays SHOULD be declared as empty arrays (`[]`), and then populated with values on subsequent lines.
- Nested objects SHOULD be declared as empty objects (`{}`), and then populated with fields on subsequent lines.
- All lines MAY terminate with a semicolon (`;`).

## 4. Example

Here's an example demonstrating the GRON structure:

```javascript
json = {};
json.name = "Adam";
json.github = "https://github.com/adamritter/";
json["hello world"] = 3;
json.likes = [];
json.likes[0] = "code";
json.likes[1] = "cheese";
json.likes[2] = "meat";
```

The main aim of this specification is to provide a more streamlined approach for handling JSON data in contexts where parsing standard JSON may be impractical, such as shell scripting.

## 5. Relaxations to the specification

By default fastgron will produce a file that conforms to the specification, but there are some practical circumstances
that changes what it produces / what it accepts in the future:

- When parsing, all empty object / array definitions are optional, as they can be lost when using grep as a tool
- jsonl will by default produce JSON lines instead of array output
- in the future some custom paths may produce array accesses in non-sequential order to keep parsing order (for speed efficiency),
  but it is not yet implemented
- When parsing with ungron, objects that were created and populated may be deleted / their type changed (not yet implemented)
- In the future the output may be scalar (json = "Hello, world") if the input is a scalar object, even though it doesn't conform
  to the JSON standard.
