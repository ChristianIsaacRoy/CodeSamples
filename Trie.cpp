/*
 * Christian Roy
 * A3: Rule-Of-Three with a Trie
 *
 * A Trie class is a generalized tree createed for storing dictionaries.
 * This trie will be using the lowercase a-z.
 */

#include "Trie.h"

using namespace std;

// Constructor
Trie::Trie(){
    rootNode = new Node();
}

Trie::~Trie(){ delete rootNode; }

// Copy Constructor
Trie::Trie(const Trie& trieToCopy){
    rootNode = nullptr;
    this->rootNode = new Node(*trieToCopy.rootNode);

}


// Add a word to the Trie if it doesn't yet exist in the tree
void Trie::addWord(string word){
    // Check if it is an empty string
    if (word[0] != '\0'){
        rootNode->addWord(word);
    }
}

// Check whether a word is contained in the trie
bool Trie::isWord(string word){
    return rootNode->isWord(word);
}


// Overriding Assignment= operator
Trie& Trie::operator=(const Trie& rhsTrie){
    // Only do assignment if RHS is a different object from this.
    if (this != &rhsTrie) {
        if (rootNode != nullptr){
            delete rootNode;
            rootNode = nullptr;
        }
        // Deallocate, allocate new space, copy values...
        this->rootNode = new Node(*rhsTrie.rootNode);
    }

    return *this;
}


// Returns all words in the tree with the given prefix, including the prefix itself if it is a word.
vector<string> Trie::allWordsWithPrefix(string word){
    vector<string> words;
    vector<int> prefix;

    // Find the pathway to the node containing the last letter of the prefix
    for (auto it = word.begin(); it != word.end(); it++){
        prefix.push_back(*it - 96);
    }

    // Storage for the location of the prefix node
    Node *endOfPrefix = this->rootNode;

    // Follow the path from the rootNode down to where the end of the prefix is
    for (auto it = prefix.begin(); it != prefix.end(); it++){
        endOfPrefix = endOfPrefix->potentialBranches[*it];
    }

    // If the prefix is a word, add it to the list of words
    if (rootNode->isWord(word)){
        words.push_back(word);
    }

    // Get all words with the prefix
    vector<string> subwords = endOfPrefix->getWords();
    string newWord;

    // Go through and append the prefix onto each of the subwords
    for (auto it = subwords.begin(); it != subwords.end(); it++){
        newWord = word;
        words.push_back(newWord.append(*it));
    }

    return words;
}

// Override the << operator to output the Trie class in a neat format
ostream& operator<<(ostream& output, Trie tr){
    output << "Words in the tree: ";

    vector<string> words = tr.allWordsWithPrefix("");
    string temp;

    for (auto it = words.begin(); it != words.end(); it++){
         temp += *it += ", ";
    }

    temp.erase(temp.size()-2);

    output << temp;

    return output;

}
