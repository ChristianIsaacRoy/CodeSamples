/*
 * Christian Roy
 * A3: Rule-Of-Three with a Trie
 *
 */

#ifndef NODE_H
#define NODE_H

#include <string>
#include <vector>

// This class is meant to be used as nodes in a tree.
class Node {
    public:
    // true if the letters in this node form a word
    bool _isWord;
    // Contains pointers to the other characters that come after this nodes data
    Node *potentialBranches[27];

    // Constructors and deconstructor
    Node();
    Node(const Node&);
    //Node(const Node& nodeToCopy);
    ~Node();


    // Add a word to this node if the word doesn't exist yet
    void addWord(std::string word);

    // Returns true if a word is in the dictionary and false if not
    bool isWord(std::string word);

    // Returns all words in the tree with the given prefix, including the prefix itself if it is a word.
    std::vector< std::string > getWords();

    // Overloaded operators
    Node& operator=(const Node&);

    // Friends
    friend std::ostream& operator<<(std::ostream& output, Node node);


};

#endif
