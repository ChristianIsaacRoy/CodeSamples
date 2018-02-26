/*
 * Christian Roy
 * A3: Rule-Of-Three with a Trie
 *
 * This class is meant to be used as nodes in a tree.
 */

#include "Node.h"
#include <iostream>

using namespace std;

// Constructors

// Create an empty node with a word flag with a false bool value
Node::Node(){
    for (size_t i = 0; i < 27; i++){
        potentialBranches[i] = nullptr;
    }
    _isWord = false;
}


// Copy constructor
Node::Node(const Node& nodeToCopy){
    this->_isWord = nodeToCopy._isWord;

    for (size_t i = 0; i < 27; i++){
        potentialBranches[i] = nullptr;
    }

    // Go through and copy each node
    for (size_t i = 0; i < 27; i++){
        if (nodeToCopy.potentialBranches[i] != nullptr){
            this->potentialBranches[i] = new Node(*nodeToCopy.potentialBranches[i]);
        }

    }
}


// Destructor
Node::~Node(){
    // Go through and delete all branching nodes
    for (int i = 0; i < 27; i++){
        // Don't delete nullptr
        if (potentialBranches[i] != nullptr) {
            delete potentialBranches[i];
            potentialBranches[i] = nullptr;
        }
    }
}

// Add a word to this node if the word doesn't exist yet
void Node::addWord(string word){
    // Check if it is an empty string
    if (word[0] != '\0'){
        // save the word excluding the first char
        string restOfWord = word.substr(1,word.size());

        // Get the index of the char in the array of pointers
        int index = word.at(0) - 96;

        // If the index contains a nullptr, create a new node
        if ( potentialBranches[index] == nullptr ){
            potentialBranches[index] = new Node();
        }

        // check if there is more of the word to add
        if (word[1] != '\0'){
            // Add the rest of the word recursively
            this->potentialBranches[index]->addWord(restOfWord);
        }
        // If there is no more word to add, then this is a word
        else{
            potentialBranches[index]->_isWord = true;
        }
    }
}

bool Node::isWord(string word){
    // Check if the word is an empty string
    if (word[0] != '\0'){

        // save the word excluding the first char
        string restOfWord = word.substr(1,word.size());
        // Get the index of the char in the array of pointers
        int index = word.at(0) - 96;

        // If the index contains a nullptr, then the word hasn't been added. Return false
        if ( this->potentialBranches[index] == nullptr ){
            return false;
        }

        // check if there is more of the word to check
        if (restOfWord.size() != 0){
            // check for the rest of the word recursively
            return this->potentialBranches[index]->isWord(restOfWord);
        }
        // If there is no more word to check, check if the letter we are looking at is the end of a word.
        else{
            return this->potentialBranches[index]->_isWord;
        }
    }
    return false;
}

// Returns all words in the tree with the given prefix, including the prefix itself if it is a word.
vector<string> Node::getWords(){
    vector<string> words;
    vector<string> subwords;

    for (int i=0; i<27; i++){
        if (potentialBranches[i] != nullptr){
            const char firstChar = i + 96;

            string newWord;
            newWord += firstChar;

            if (this->isWord(newWord)){
                words.push_back(newWord);
            }

            subwords = potentialBranches[i]->getWords();
            for (auto it = subwords.begin(); it != subwords.end(); it++){
                string temp = newWord;
                temp.append(*it);
                words.push_back(temp);
            }

        }
    }

    return words;


}


// Overloading the operator=
Node& Node::operator=(const Node& rhsNode) {
    // Only copy if it is not the same node
    if (this != &rhsNode){
         // Allocate new memory
         Node* newPotentialBranches [27];

         // Delete old branches
         for (int i=0; i<27; i++){
             delete potentialBranches[i];
             potentialBranches[i] = nullptr;
         }

         // Copy branches from rhs
         for (size_t index = 0; index < 27; index++){
             newPotentialBranches[index] = new Node(*rhsNode.potentialBranches[index]);
         }

         // Assign Potential Branches to copy of potential branches from rhs
         for (int i =0; i<27; i++){
             potentialBranches[i] = newPotentialBranches[i];
         }

         // Copy value of isWord
         this->_isWord = rhsNode._isWord;
    }

    return *this;
}

// Overloading the operator<<
ostream& operator<<(ostream& output, Node nd){
    if (nd._isWord){
        output << "Node is word";
    }
    else
        output << "Node is not Word";

    return output;
}
