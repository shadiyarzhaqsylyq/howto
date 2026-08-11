#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>

template <typename KeyType = int, size_t T = 3>
class BPlusTree {
public:
    // --- Node Hierarchy ---
    struct Node {
        bool is_leaf;
        std::vector<KeyType> keys;

        explicit Node(bool leaf) : is_leaf(leaf) {}
        virtual ~Node() = default;
    };

    struct InternalNode : public Node {
        std::vector<std::unique_ptr<Node>> children;

        InternalNode() : Node(false) {}
    };

    struct LeafNode : public Node {
        LeafNode* next{nullptr};

        LeafNode() : Node(true) {}
    };

private:
    std::unique_ptr<Node> root;

    /* ------------------------------------------------------------------ */
    /*  Split Child Node                                                  */
    /* ------------------------------------------------------------------ */
    void splitChild(InternalNode* parent, size_t i) {
        Node* child = parent->children[i].get();

        if (child->is_leaf) {
            // --- LEAF SPLIT ---
            auto* leaf_child = static_cast<LeafNode*>(child);
            auto new_leaf = std::make_unique<LeafNode>();

            // Copy the upper half of keys to the new leaf node
            new_leaf->keys.assign(
                std::make_move_iterator(leaf_child->keys.begin() + T),
                std::make_move_iterator(leaf_child->keys.end())
            );
            leaf_child->keys.resize(T);

            // Leaf linked-list management
            new_leaf->next = leaf_child->next;
            leaf_child->next = new_leaf.get();

            // In B+ Trees, the first key of the split right leaf is promoted to parent
            KeyType promoted_key = new_leaf->keys[0];

            // Insert new child pointer and promoted key into parent
            parent->children.insert(parent->children.begin() + i + 1, std::move(new_leaf));
            parent->keys.insert(parent->keys.begin() + i, promoted_key);

        } else {
            // --- INTERNAL SPLIT ---
            auto* int_child = static_cast<InternalNode*>(child);
            auto new_internal = std::make_unique<InternalNode>();

            // Key at index (T - 1) is promoted and removed from lower level
            KeyType promoted_key = int_child->keys[T - 1];

            // Move upper half of keys and children to the new internal node
            new_internal->keys.assign(
                std::make_move_iterator(int_child->keys.begin() + T),
                std::make_move_iterator(int_child->keys.end())
            );
            int_child->keys.resize(T - 1);

            new_internal->children.assign(
                std::make_move_iterator(int_child->children.begin() + T),
                std::make_move_iterator(int_child->children.end())
            );
            int_child->children.resize(T);

            // Insert new child pointer and promoted key into parent
            parent->children.insert(parent->children.begin() + i + 1, std::move(new_internal));
            parent->keys.insert(parent->keys.begin() + i, promoted_key);
        }
    }

    /* ------------------------------------------------------------------ */
    /*  Insert Helper                                                     */
    /* ------------------------------------------------------------------ */
    void insertNonFull(Node* node, const KeyType& key) {
        if (node->is_leaf) {
            auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
            node->keys.insert(it, key);
        } else {
            auto* internal = static_cast<InternalNode*>(node);
            auto it = std::upper_bound(internal->keys.begin(), internal->keys.end(), key);
            size_t i = std::distance(internal->keys.begin(), it);

            if (internal->children[i]->keys.size() == 2 * T - 1) {
                splitChild(internal, i);
                if (key >= internal->keys[i]) {
                    i++;
                }
            }
            insertNonFull(internal->children[i].get(), key);
        }
    }

public:
    BPlusTree() = default;

    /* ------------------------------------------------------------------ */
    /*  Public Operations                                                */
    /* ------------------------------------------------------------------ */

    void insert(const KeyType& key) {
        if (!root) {
            auto leaf = std::make_unique<LeafNode>();
            leaf->keys.push_back(key);
            root = std::move(leaf);
            return;
        }

        if (root->keys.size() == 2 * T - 1) {
            auto new_root = std::make_unique<InternalNode>();
            new_root->children.push_back(std::move(root));
            splitChild(new_root.get(), 0);

            size_t i = 0;
            if (key >= new_root->keys[0]) {
                i++;
            }
            insertNonFull(new_root->children[i].get(), key);
            root = std::move(new_root);
        } else {
            insertNonFull(root.get(), key);
        }
    }

    bool contains(const KeyType& key) const {
        if (!root) return false;

        const Node* curr = root.get();
        while (!curr->is_leaf) {
            const auto* internal = static_cast<const InternalNode*>(curr);
            auto it = std::upper_bound(internal->keys.begin(), internal->keys.end(), key);
            size_t i = std::distance(internal->keys.begin(), it);
            curr = internal->children[i].get();
        }

        return std::binary_search(curr->keys.begin(), curr->keys.end(), key);
    }

    // Range Query [low, high] leveraging leaf linked-list
    std::vector<KeyType> rangeSearch(const KeyType& low, const KeyType& high) const {
        std::vector<KeyType> result;
        if (!root) return result;

        const Node* curr = root.get();
        while (!curr->is_leaf) {
            const auto* internal = static_cast<const InternalNode*>(curr);
            auto it = std::upper_bound(internal->keys.begin(), internal->keys.end(), low);
            size_t i = std::distance(internal->keys.begin(), it);
            curr = internal->children[i].get();
        }

        const auto* leaf = static_cast<const LeafNode*>(curr);
        while (leaf) {
            for (const auto& k : leaf->keys) {
                if (k >= low && k <= high) {
                    result.push_back(k);
                } else if (k > high) {
                    return result;
                }
            }
            leaf = leaf->next;
        }
        return result;
    }

    // Print linked leaves sequence
    void printLeaves() const {
        if (!root) return;

        const Node* curr = root.get();
        while (!curr->is_leaf) {
            const auto* internal = static_cast<const InternalNode*>(curr);
            curr = internal->children[0].get();
        }

        const auto* leaf = static_cast<const LeafNode*>(curr);
        std::cout << "Leaf Linked List: ";
        while (leaf) {
            std::cout << "[";
            for (size_t i = 0; i < leaf->keys.size(); ++i) {
                std::cout << leaf->keys[i] << (i + 1 < leaf->keys.size() ? " " : "");
            }
            std::cout << "]";
            if (leaf->next) std::cout << " -> ";
            leaf = leaf->next;
        }
        std::cout << "\n";
    }
};

/* ------------------------------------------------------------------ */
/*  Demo Main                                                         */
/* ------------------------------------------------------------------ */

int main() {
    BPlusTree<int, 3> tree;

    std::vector<int> keys = {10, 20, 5, 6, 12, 30, 7, 17, 3, 1, 15, 25, 27};

    for (int key : keys) {
        tree.insert(key);
    }

    std::cout << "Search 12: " << (tree.contains(12) ? "FOUND" : "NOT FOUND") << "\n";
    std::cout << "Search 99: " << (tree.contains(99) ? "FOUND" : "NOT FOUND") << "\n\n";

    // Show sequential leaves linked list
    tree.printLeaves();

    // Range Query Demo
    std::cout << "\nRange Query [6, 20]: ";
    auto range_result = tree.rangeSearch(6, 20);
    for (int k : range_result) {
        std::cout << k << " ";
    }
    std::cout << "\n";

    return 0;
}
