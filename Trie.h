/*
 * Christian Roy
 * A3: Rule-Of-Three with a Trie
 *
 */

#ifndef TRIE_H
#define TRIE_H

#include <iostream>
#include "Node.h"

// A Trie class is a generalized tree createed for storing dictionaries.
// This trie will be using the lowercase a-z.
class Trie {
    // This node is the first and empty node
    Node* rootNode;

    // This is how many words have been added to the Trie
    int numOfWords;

    // How many nodes have been added to the dictionary
    int numOfNodes;

    public:
    // Constructors and deconstructor
    Trie();
    ~Trie();
    Trie(const Trie&);

    // Overloding the assignment operator
    Trie& operator=(const Trie&);

    // Friends
    friend std::ostream& operator<<(std::ostream& output, Trie tr);

    // Add a word to the Trie. Duplicates do not affect the trie.
    // Only lower-case characters from a-z are recommended.
    void addWord(std::string word);

    // Returns true if a given word is in the Trie, otherwise returns false.
    // Only lower-case characters from a-z are recommended.
    bool isWord(std::string word);

    // Returns all words in the tree with the given prefix, including the prefix itself if it is a word.
    std::vector<std::string> allWordsWithPrefix(std::string);
};

#endif
