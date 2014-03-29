Source files
============ 

1. Query.py

Descriptions
============

Query the inverted index as described in the assignment.

Shows a text snippet from the found location(s).

Use the keywords AND, OR, or NOT to combine 2 search terms
Use quotes to search for consecutive terms.

Useage:

python ./src/Query.py [query]

Examples:

> ls ../../invertedindex/input/
file01.txt file02.txt

> cat ../../invertedindex/input/*
1 Hello World Goodbye World 
2 Hello World Bye World 
3 Hello Hadoop Goodbye Hadoop
4 Hello Hadoop Goodbye Hadoop

> py Query.py bob
...
bob
    Terms in this query: ['bob']
    Mode: QUOTE
    NOT found

> py Query.py hello
...
hello
    Terms in this query: ['hello']
    Mode: QUOTE

    found in file02.txt, line 1, field 2: "...3 Hello Hadoop Goodbye Hadoop ... "
    found in file02.txt, line 2, field 2: "...4 Hello Hadoop Goodbye Hadoop ... "
    found in file01.txt, line 1, field 2: "...1 Hello World Goodbye World  ... "
    found in file01.txt, line 2, field 2: "...2 Hello World Bye World  ... "

> py Query.py hello world
...
hello
    Terms in this query: ['hello']
    Mode: QUOTE

    found in file02.txt, line 1, field 2: "...3 Hello Hadoop Goodbye Hadoop ... "
    found in file02.txt, line 2, field 2: "...4 Hello Hadoop Goodbye Hadoop ... "
    found in file01.txt, line 1, field 2: "...1 Hello World Goodbye World  ... "
    found in file01.txt, line 2, field 2: "...2 Hello World Bye World  ... "

world
    Terms in this query: ['world']
    Mode: QUOTE

    found in file01.txt, line 1, field 3: "...1 Hello World Goodbye World  ... "
    found in file01.txt, line 1, field 5: "...Hello World Goodbye World  ... "
    found in file01.txt, line 2, field 3: "...2 Hello World Bye World  ... "
    found in file01.txt, line 2, field 5: "...Hello World Bye World  ... "

> py Query.py "Hello World"
...
Hello World
    Terms in this query: ['hello', 'world']
    Mode: QUOTE

    found in file01.txt, line 1, field 2: "...1 Hello World Goodbye World  ... "
    found in file01.txt, line 1, field 3: "...1 Hello World Goodbye World  ... "
    found in file01.txt, line 2, field 2: "...2 Hello World Bye World  ... "
    found in file01.txt, line 2, field 3: "...2 Hello World Bye World  ... "

> py Query.py "Hello World Bye"
...
Hello World Bye
    Terms in this query: ['hello', 'world', 'bye']
    Mode: QUOTE

    found in file01.txt, line 2, field 2: "...2 Hello World Bye World  ... "
    found in file01.txt, line 2, field 3: "...2 Hello World Bye World  ... "
    found in file01.txt, line 2, field 4: "...2 Hello World Bye World  ... "

> py Query.py Hello AND Hadoop
...
Hello AND Bye
    Terms in this query: ['hello', 'bye']
    Mode: AND

    found in file01.txt, line 2: "...2 Hello World Bye ... "

> py Query.py Hello OR Hadoop
...
Hello OR Hadoop
    Terms in this query: ['hello', 'hadoop']
    Mode: OR

    found in file02.txt, line 1: "...3 Hello Hadoop Goodbye ... "
    found in file02.txt, line 2: "...4 Hello Hadoop Goodbye ... "
    found in file01.txt, line 1: "...1 Hello World Goodbye ... "
    found in file01.txt, line 2: "...2 Hello World Bye ... "

> py Query.py Hello NOT Hadoop
...
hello NOT Hadoop
    Terms in this query: ['hello', 'hadoop']
    Mode: NOT

    found in file01.txt, line 2: "...2 Hello World Bye ... "
    found in file01.txt, line 1: "...1 Hello World Goodbye ... "