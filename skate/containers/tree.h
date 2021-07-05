#ifndef SKATE_TREE_H
#define SKATE_TREE_H

#include <vector>
#include <algorithm>

namespace skate {
    template<typename T, size_t children_per_node, bool stack_recursion = true>
    class tree {
        tree *children[children_per_node];              // Children, owned by this instance
        tree *parent_;                                  // Pointer to parent node (if NULL, the tree is empty, if equal to `this`, this is the root)
        T val;                                          // Value of this node

        // Creates tree with specified value and parent
        tree(T value, tree *parent) : parent_(parent), val(value) {
            for (size_t i = 0; i < children_per_node; ++i)
                children[i] = NULL;
        }

        // Upgrade from empty to single element tree
        void upgrade() {
            if (parent_ == NULL)
                parent_ = this;
        }

        // Base class for all iterators
        template<typename R, typename Node>
        class iterator_base {
        protected:
            Node *n;
            size_t depth_;                              // Invariant should always hold that depth_ refers to the number of unique parents of n, 0 if at the root or n is NULL

            static size_t index_in_parent(Node *node) {
                Node * const * const children_ = node->parent_->children;
                for (size_t i = 0; i < children_per_node; ++i) {
                    if (children_[i] == node)
                        return i;
                }

                // Shouldn't ever get here
                return 0;
            }

            iterator_base(Node *n, size_t depth = 0) : n(n), depth_(depth) {}
            iterator_base() : n(NULL), depth_(0) {}

        public:
            bool operator==(const iterator_base &other) const { return n == other.n; }
            bool operator!=(const iterator_base &other) const { return !(*this == other); }

            iterator_base parent() const { return iterator_base(n->parent_, depth_ - 1); }
            iterator_base child(size_t index) const { return iterator_base(n->children[index], depth_ + 1); }

            // Returns index of this node in its parent (optimized in traversal iterator)
            size_t child_index() const { return depth_? index_in_parent(n): 0; }

            // Returns how many levels this node is down from the root
            size_t depth() const { return depth_; }

            R &operator*() const { return n->value(); }
            R *operator->() const { return &(n->value()); }
        };

        /////////////////////////////////////////////////////////////////////////
        ///
        /// \brief The traversal_iterator_base class
        ///
        /// Base class for entire-tree traversal iterators, tracking indexes in parents
        ///
        /////////////////////////////////////////////////////////////////////////
        template<typename R, typename Node>
        class traversal_iterator_base : public iterator_base<R, Node>, public std::iterator<std::forward_iterator_tag, R, std::ptrdiff_t, R *, R> {
        protected:
            typedef iterator_base<R, Node> base;

            std::vector<size_t> children_of_parents;

            size_t pop_child_of_parent() {
                --base::depth_;

                // Cached index in parent
                if (children_of_parents.size()) {
                    const size_t child_of_parent = children_of_parents.back();
                    children_of_parents.pop_back();
                    return child_of_parent;
                }

                // No cached index, search for index the long way
                return base::index_in_parent(base::n);
            }

            void move_to_child(size_t index) {
                base::n = base::n->children[index];
                ++base::depth_;
                children_of_parents.push_back(index);
            }

            traversal_iterator_base(Node *n) : iterator_base<R, Node>(n) {}
            traversal_iterator_base(const iterator_base<R, Node> &other) : iterator_base<R, Node>(other) {}
            traversal_iterator_base() {}

        public:
            size_t child_index() const { // Shadows the implementation in the base class, but since iterators shouldn't be used as pointers it shouldn't matter
                if (base::depth() == 0)
                    return 0;
                else if (children_of_parents.size())
                    return children_of_parents.back();

                return base::index_in_parent(base::n);
            }

            bool has_same_ancestor_heirarchy(const traversal_iterator_base<R, Node> &other) const {
                using namespace std; // Just for macro min/max

                // If depth from root is different, it cannot have the same heirarchy
                if (base::depth() != other.base::depth())
                    return false;

                const size_t common_size = min(children_of_parents.size(), other.children_of_parents.size());

                // Check the closest (cached) parent indexes to see if they're all equal, if not then the heirarchy is different
                if (!std::equal(children_of_parents.begin() + children_of_parents.size() - common_size,
                                children_of_parents.end(),
                                other.children_of_parents.begin() + other.children_of_parents.size() - common_size,
                                other.children_of_parents.end()))
                    return false;
                // Heirarchy caches are equal here, but check if they're actually equal to the depth of the trees, if so, then the caches hold all values to the root
                else if (common_size == base::depth())
                    return true;

                // Heirarchy caches are equal here, but don't go all the way to the root, so backtrack to the last known root parent
                Node *p_this = base::n, *p_other = other.base::n;

                for (size_t i = 0; i < common_size; ++i) {
                    p_this = p_this->parent_;
                    p_other = p_other->parent_;
                }

                // Then walk p_this and p_other from known common parent to the actual root
                while (p_this->parent_ != p_this) { // Both must not be root if only one is checked, since the depths are equal
                    if (index_in_parent(p_this) != index_in_parent(p_other))
                        return false;

                    p_this = p_this->parent_;
                    p_other = p_other->parent_;
                }

                return true;
            }
        };

        /////////////////////////////////////////////////////////////////////////
        ///
        /// \brief The sibling_iterator_base class
        ///
        /// Iterates only immediate siblings of the current node
        ///
        /////////////////////////////////////////////////////////////////////////
        template<typename R, typename Node>
        class sibling_iterator_base : public traversal_iterator_base<R, Node> {
        protected:
            typedef iterator_base<R, Node> base;

            sibling_iterator_base(Node *n) : traversal_iterator_base<R, Node>(n) {}
            sibling_iterator_base(const iterator_base<R, Node> &other) : traversal_iterator_base<R, Node>(other) {}
            sibling_iterator_base() {}

        public:
            // Pre-order traverses parent first, then the children (V -> L -> R)
            sibling_iterator_base &operator++() {
                const size_t next_child_number = base::pop_child_of_parent() + 1;

                base::n = base::n->parent_;

                for (size_t i = next_child_number; i < children_per_node; ++i) {
                    if (base::n->children[i]) {
                        base::n = base::n->children[i];
                        base::move_to_child(i);
                        return *this;
                    }
                }

                // If no right sibling, the iterator is at the end
                base::n = NULL;

                return *this;
            }
            sibling_iterator_base operator++(int) {
                const sibling_iterator_base ret = *this;
                ++(*this);
                return ret;
            }
        };

        /////////////////////////////////////////////////////////////////////////
        ///
        /// \brief The preorder_iterator_base class
        ///
        /// Iterates parent first, then the children (parent -> left -> right)
        ///
        /////////////////////////////////////////////////////////////////////////
        template<typename R, typename Node>
        class preorder_iterator_base : public traversal_iterator_base<R, Node> {
        protected:
            typedef traversal_iterator_base<R, Node> base;

            preorder_iterator_base(Node *n) : traversal_iterator_base<R, Node>(n) {}
            preorder_iterator_base(const iterator_base<R, Node> &other) : traversal_iterator_base<R, Node>(other) {}
            preorder_iterator_base() {}

        public:
            // Pre-order traverses parent first, then the children (V -> L -> R)
            preorder_iterator_base &operator++() {
                // Check for any child, if any child visit it
                for (size_t i = 0; i < children_per_node; ++i) {
                    if (base::n->children[i]) {
                        base::move_to_child(i);
                        return *this;
                    }
                }

                // If no child, go back to parent and get next child
                while (base::depth_) {
                    const size_t next_child_number = base::pop_child_of_parent() + 1;

                    base::n = base::n->parent_;

                    for (size_t i = next_child_number; i < children_per_node; ++i) {
                        if (base::n->children[i]) {
                            base::move_to_child(i);
                            return *this;
                        }
                    }
                }

                // If no parent, the iterator is at end
                base::n = NULL;

                return *this;
            }
            preorder_iterator_base operator++(int) {
                const preorder_iterator_base ret = *this;
                ++(*this);
                return ret;
            }
        };

        /////////////////////////////////////////////////////////////////////////
        ///
        /// \brief The postorder_iterator class
        ///
        /// Iterates children first, then the parent (left -> right -> parent)
        ///
        /////////////////////////////////////////////////////////////////////////
        template<typename R, typename Node>
        class postorder_iterator_base : public traversal_iterator_base<R, Node> {
        protected:
            typedef traversal_iterator_base<R, Node> base;

            // Find the deepest, leftmost leaf of node n
            void visit_leftmost_leaf() {
                while (base::n) {
                    bool is_leaf = true;

                    // Get the left-most child and continue looking for leaf
                    for (size_t i = 0; i < children_per_node; ++i) {
                        if (base::n->children[i]) {
                            base::move_to_child(i);
                            is_leaf = false;
                            break;
                        }
                    }

                    if (is_leaf)
                        return;
                }
            }

            // Constructor reinitializes iterator to point to the deepest, leftmost child node of root
            postorder_iterator_base(Node *root) : traversal_iterator_base<R, Node>(root) {
                visit_leftmost_leaf();
            }
            postorder_iterator_base(const iterator_base<R, Node> &other) : traversal_iterator_base<R, Node>(other) {}
            postorder_iterator_base() {}

        public:
            // Post-order traverses children first, then the parent (L -> R -> V)
            // Calling operator++() with exists() == false is undefined
            postorder_iterator_base &operator++() {
                if (base::depth_) {
                    // Does this node have a sibling to the right? Move to the deepest, leftmost child of it
                    const size_t next_child_number = base::pop_child_of_parent() + 1;

                    base::n = base::n->parent_;

                    for (size_t i = next_child_number; i < children_per_node; ++i) {
                        if (base::n->children[i]) {
                            base::move_to_child(i);

                            visit_leftmost_leaf();
                            return *this;
                        }
                    }

                    // Node has no sibling to the right, return the parent
                    return *this;
                }

                // If no parent, the iterator is at end
                base::n = NULL;

                return *this;
            }
            postorder_iterator_base operator++(int) {
                const postorder_iterator_base ret = *this;
                ++(*this);
                return ret;
            }
        };

    public:
        class iterator : public iterator_base<T, tree> {
        protected:
            friend class tree;

            iterator(tree *root) : iterator_base<T, tree>(root) {}

        public:
            iterator(const iterator &other) : iterator_base<T, tree>(other) {}
            iterator(const iterator_base<T, tree> &other) : iterator_base<T, tree>(other) {}
            iterator() {}
        };

        class const_iterator : public iterator_base<const T, const tree> {
        protected:
            friend class tree;

            const_iterator(const tree *root) : iterator_base<const T, const tree>(root) {}

        public:
            const_iterator(const const_iterator &other) : iterator_base<const T, const tree>(other) {}
            const_iterator(const iterator_base<const T, const tree> &other) : iterator_base<const T, const tree>(other) {}
            const_iterator() {}
        };

        class preorder_iterator : public preorder_iterator_base<T, tree> {
            friend class tree;

            preorder_iterator(tree *root) : preorder_iterator_base<T, tree>(root) {}

        public:
            preorder_iterator(const iterator_base<T, tree> &other) : preorder_iterator_base<T, tree>(other) {}
            preorder_iterator() {}
        };

        class const_preorder_iterator : public preorder_iterator_base<const T, const tree> {
            friend class tree;

            const_preorder_iterator(const tree *root) : preorder_iterator_base<const T, const tree>(root) {}

        public:
            const_preorder_iterator(const iterator_base<const T, const tree> &other) : preorder_iterator_base<const T, const tree>(other) {}
            const_preorder_iterator() {}
        };

        class postorder_iterator : public postorder_iterator_base<T, tree> {
            friend class tree;

            postorder_iterator(tree *root) : postorder_iterator_base<T, tree>(root) {}

        public:
            postorder_iterator(const iterator_base<T, tree> &other) : postorder_iterator_base<T, tree>(other) {}
            postorder_iterator() {}
        };

        class const_postorder_iterator : public postorder_iterator_base<const T, const tree> {
            friend class tree;

            const_postorder_iterator(const tree *root) : postorder_iterator_base<const T, const tree>(root) {}

        public:
            const_postorder_iterator(const iterator_base<const T, const tree> &other) : postorder_iterator_base<const T, const tree>(other) {}
            const_postorder_iterator() {}
        };

        iterator root() { return iterator(empty()? NULL: this); }
        const_iterator root() const { return const_iterator(empty()? NULL: this); }
        const_iterator croot() const { return const_iterator(empty()? NULL: this); }
        iterator end() { return iterator(); }
        const_iterator end() const { return const_iterator(); }
        const_iterator cend() const { return const_iterator(); }

        preorder_iterator preorder_begin() { return preorder_iterator(empty()? NULL: this); }
        preorder_iterator preorder_end() { return preorder_iterator(); }
        const_preorder_iterator preorder_begin() const { return const_preorder_iterator(empty()? NULL: this); }
        const_preorder_iterator preorder_end() const { return const_preorder_iterator(); }
        const_preorder_iterator preorder_cbegin() const { return const_preorder_iterator(empty()? NULL: this); }
        const_preorder_iterator preorder_cend() const { return const_preorder_iterator(); }
        postorder_iterator postorder_begin() { return postorder_iterator(empty()? NULL: this); }
        postorder_iterator postorder_end() { return postorder_iterator(); }
        const_postorder_iterator postorder_begin() const { return const_postorder_iterator(empty()? NULL: this); }
        const_postorder_iterator postorder_end() const { return const_postorder_iterator(); }
        const_postorder_iterator postorder_cbegin() const { return const_postorder_iterator(empty()? NULL: this); }
        const_postorder_iterator postorder_cend() const { return const_postorder_iterator(); }

        // Create default empty tree
        tree() : parent_(NULL) {
            for (size_t i = 0; i < children_per_node; ++i)
                children[i] = NULL;
        }
        // Create tree with single node with specified value
        tree(T value) : parent_(this), val(value) {
            for (size_t i = 0; i < children_per_node; ++i)
                children[i] = NULL;
        }
        // Copy another tree, without copying its parent
        tree(const tree &other) : parent_(other.empty()? NULL: this), val(other.val) {
            // Clear all children first in case an exception is thrown while creating them
            for (size_t i = 0; i < children_per_node; ++i) {
                children[i] = NULL;
            }

            try {
                if (stack_recursion) {
                    for (size_t i = 0; i < children_per_node; ++i) {
                        if (other.children[i]) {
                            children[i] = new tree(*other.children[i]);
                            children[i]->parent_ = this;
                        }
                    }
                } else {
                    const tree *original = &other;
                    tree *clone = this;

                    while (true) {
                        bool all_children_copied = true;

                        for (size_t j = 0; j < children_per_node; ++j) {
                            if (original->children[j] && clone->children[j] == NULL) {
                                clone->children[j] = new tree(original->children[j]->val, clone);

                                original = original->children[j];
                                clone = clone->children[j];
                                all_children_copied = false;
                                break;
                            }
                        }

                        if (all_children_copied) {
                            if (original == &other)
                                break;

                            original = original->parent_;
                            clone = clone->parent_;
                        }
                    }
                }
            } catch (...) { // Exception thrown, so delete the existing children
                for (size_t i = 0; i < children_per_node; ++i) {
                    delete children[i];
                }

                throw;
            }
        }
#if __cplusplus >= 201103L
        // Move from other node to this node
        tree(tree &&other) : parent_(other.empty()? NULL: this), val(std::move(other.val)) {
            for (size_t i = 0; i < children_per_node; ++i) {
                children[i] = other.children[i];

                if (children[i])
                    children[i]->parent_ = this;

                other.children[i] = NULL;
            }
        }
        template<typename... Args>
        tree(Args&&... args) : parent_(this), val(std::forward<Args>(args)...) {}
#endif

        ~tree() {
            if (stack_recursion) {
                for (size_t i = 0; i < children_per_node; ++i)
                    delete children[i];
            } else { // No use of stack recursion in deletion routine, purely iteration (necessary for extremely deep trees)
                tree *n = this;

                while (true) {
                    bool all_children_deleted = true;

                    for (size_t j = 0; j < children_per_node; ++j) {
                        if (n->children[j]) {
                            tree *temp = n->children[j];
                            n->children[j] = NULL;
                            n = temp;
                            all_children_deleted = false;
                            break;
                        }
                    }

                    if (all_children_deleted) {
                        if (n == this)
                            break;

                        tree *temp = n;
                        n = n->parent_;

                        delete temp;
                    }
                }
            }
        }

        // WARNING! Assigning an empty tree to a child will NOT delete the child from its parent
        // It will instead just make the child a default-initialized node
        tree &operator=(const tree &other) {
            if (&other == this)
                return *this;

            erase_children();

            val = other.val;
            for (size_t i = 0; i < children_per_node; ++i) {
                if (other.children[i]) {
                    children[i] = new tree(*other.children[i]);
                    children[i]->parent_ = this;
                }
            }

            if (is_root())
                parent_ = other.empty()? NULL: this;

            return *this;
        }
#if __cplusplus >= 201103L
        tree &operator=(tree &&other) {
            if (&other == this)
                return *this;

            val = std::move(other.val);
            for (size_t i = 0; i < children_per_node; ++i) {
                delete children[i];
                children[i] = other.children[i];

                if (children[i])
                    children[i]->parent_ = this;

                other.children[i] = NULL;
            }

            if (is_root())
                parent_ = other.empty()? NULL: this;

            return *this;
        }
#endif

        size_t max_children() const { return children_per_node; }
        size_t child_count() const {
            size_t count = 0;

            for (size_t i = 0; i < children_per_node; ++i)
                count += children[i] != NULL;

            return count;
        }
        bool is_leaf() const {
            for (size_t i = 0; i < children_per_node; ++i)
                if (children[i] != NULL)
                    return false;

            return true;
        }
        bool has_child(size_t index) const { return children[index] != NULL; }
        bool has_left_child() const { return has_child(0); }
        bool has_right_child() const { return has_child(children_per_node - 1); }

        bool empty() const { return parent_ == NULL; }

        // WARNING! If this function is called on a child object (is_child() == true), the child object will be removed from the parent and will be deleted
        // This makes any further references to the instantiated object invalid
        //
        // If called on the root, the tree is set to empty
        //
        // Calling this function on an already empty tree does nothing
        //
        // Invalidates iterators to any siblings of this tree
        void clear_to_empty() {
            if (parent_ == this) { // Root element, just clear all children and erase to empty
                erase_children();
                parent_ = NULL;
            } else if (parent_) { // If not empty, remove from parent and this object is now deleted
                for (size_t i = 0; i < children_per_node; ++i) {
                    if (parent().children[i] == this) {
                        parent().erase_child(i);
                        return;
                    }
                }
            }
        }

        // Erase immediate children with specified value
        // Invalidates iterators to any children of this tree
        void erase(const T &value) {
            for (size_t i = 0; i < children_per_node; ++i) {
                if (children[i] && children[i]->val == value)
                    erase_child(i);
            }
        }
        // Erase immediate child with specific index, including its children
        // Invalidates iterators to any children of this tree
        void erase_child(size_t index) {
            delete children[index];
            children[index] = NULL;
        }
        // Invalidates iterators to any children of this tree
        void erase_left() { erase_child(0); }
        // Invalidates iterators to any children of this tree
        void erase_right() { erase_child(children_per_node - 1); }
        // Erase all children of this tree
        // Invalidates iterators to any children of this tree
        void erase_children() {
            for (size_t i = 0; i < children_per_node; ++i)
                erase_child(i);
        }

        // Return the value of the current node
        T &value() { upgrade(); return val; }
        T &operator*() { upgrade(); return val; }
        T *operator->() { upgrade(); return &val; }
        // An empty element if empty() is true
        const T &value() const { return val; }
        // An empty element if empty() is true
        const T &operator*() const { return val; }
        // An empty element if empty() is true
        const T &operator->() const { return &val; }

        // Returns true if this node is the root. Returns true for empty trees as well
        bool is_root() const { return parent_ == NULL || parent_ == this; }

        // Returns true if this node is a child of a larger tree
        bool is_child() const { return !is_root(); }

        // Returns the parent of this node
        // Undefined if empty() is true
        // If at the root, the root itself is returned
        tree &parent() { return *parent_; }
        const tree &parent() const { return *parent_; }

        // Always well defined if index < max_children(), creates a single-node child if it doesn't exist
        // No iterators are invalidated
        tree &child(size_t index) {
            if (children[index] == NULL)
                children[index] = new tree(T(), this);

            return *children[index];
        }
        tree &left() { return child(0); }
        tree &right() { return child(children_per_node - 1); }

        // Undefined if has_child(index) is false
        const tree &child(size_t index) const { return *children[index]; }
        // Undefined if has_left() is false
        const tree &left() const { return child(0); }
        // Undefined if has_right() is false
        const tree &right() const { return child(children_per_node - 1); }

        // Defined only if iterator != end()
        // No iterators are invalidated
        // Undefined if iterator is invalid
        tree &subtree(iterator it) { return *it.n; }
        const tree &subtree(const_iterator it) const { return *it.n; }

        // Slices the subtree from the tree, using the specified node as the root, and returns it
        // Invalidates only the provided iterator and any of its children
        // Undefined if iterator is invalid
        tree slice(iterator it) {
            tree result;

            // Slice everything since root is disappearing
            if (it.n == this) {
                result.swap(*this);
                return result;
            }

            // Parent must be valid since it's not the root
            const size_t index = iterator::index_in_parent(it.n);

            it.n->parent_->children[index] = NULL;

            result.swap(*it.n);
            delete it.n;

            return result;
        }

        // Follows a path of values down the tree and returns an iterator to the element, or an invalid iterator if the path doesn't exist
        template<typename It>
        iterator follow_path(It begin, It end) const {
            iterator it = root();

            // Does begin match the root?
            if (it == this->end() || begin == end || !(*begin == *it))
                return iterator();

            // Check each child for the next path element
            for (++begin; begin != end; ++begin) {
                bool matched = false;

                for (size_t i = 0; i < children_per_node; ++i) {
                    if (it.n->children[i] && *begin == it.n->children[i]->val) {
                        matched = true;
                        it = it.child(i);
                        break;
                    }
                }

                if (!matched)
                    return iterator();
            }

            return it;
        }

        // Follows a path of values down the tree and returns an iterator to the element, or an invalid iterator if the path doesn't exist
        template<typename It>
        const_iterator follow_path(It begin, It end) const {
            const_iterator it = root();

            // Does begin match the root?
            if (it == this->end() || begin == end || !(*begin == *it))
                return const_iterator();

            // Check each child for the next path element
            for (++begin; begin != end; ++begin) {
                bool matched = false;

                for (size_t i = 0; i < children_per_node; ++i) {
                    if (it.n->children[i] && *begin == it.n->children[i]->val) {
                        matched = true;
                        it = it.child(i);
                        break;
                    }
                }

                if (!matched)
                    return const_iterator();
            }

            return it;
        }

        bool operator==(const tree &other) const {
            if (!empty() && !other.empty()) {
                const_preorder_iterator current = preorder_begin();
                const_preorder_iterator alternate = other.preorder_begin();

                for (; current != end() && alternate != other.end(); ++current, ++alternate) {
                    if (!(*current == *alternate) || !current.has_same_ancestor_heirarchy(alternate))
                        return false;
                }

                return current != end() || alternate != other.end();
            }
            else if (!empty() || !other.empty())
                return false;
            else
                return true;
        }
        bool operator!=(const tree &other) const { return !(*this == other); }

        // Returns the number of levels in the tree (empty = 0, just root = 1, etc.)
        size_t height() const {
            size_t height = 0;

            for (const_preorder_iterator it = preorder_begin(); it != end(); ++it) {
                using namespace std;

                height = max(height, it.depth() + 1);
            }

            return height;
        }
        // Returns the number of nodes in the tree
        size_t size() const { return std::distance(preorder_begin(), preorder_end()); }
        // Returns the number of leaf nodes in the tree
        size_t leaf_count() const {
            size_t leaves = 0;

            for (const_preorder_iterator it = preorder_begin(); it != end(); ++it) {
                leaves += subtree(it).is_leaf();
            }

            return leaves;
        }
        // Returns the number of non-leaf nodes in the tree
        size_t branch_count() const {
            size_t branches = 0;

            for (const_preorder_iterator it = preorder_begin(); it != end(); ++it) {
                branches += !subtree(it).is_leaf();
            }

            return branches;
        }

        // WARNING! Swapping an empty tree with a child will NOT delete the child from its parent
        // It will instead just make the child a default-initialized node
        void swap(tree &other) {
            std::swap(val, other.val);

            for (size_t i = 0; i < children_per_node; ++i) {
                std::swap(children[i], other.children[i]);

                if (children[i]) {
                    children[i]->parent_ = this;
                }

                if (other.children[i]) {
                    other.children[i]->parent_ = &other;
                }
            }

            // Only change parents if empty or the root node
            if (is_root() && other.is_root()) {
                // Fix self reference if not empty tree
                if (parent_ == this)
                    parent_ = &other;

                // Then do the same for the other tree
                if (other.parent_ == &other)
                    other.parent_ = this;

                // Then do actual swap
                std::swap(parent_, other.parent_);
            }
        }
    };

    template<typename T, bool stack_recursion>
    class tree<T, 0, stack_recursion> {
        std::vector<tree *> children;                   // Children, owned by this instance (children are never NULL)
        tree *parent_;                                  // Pointer to parent node (if NULL, the tree is empty, if equal to `this`, this is the root)
        T val;                                          // Value of this node

        // Creates tree with specified value and parent
        tree(T value, tree *parent) : parent_(parent), val(value) {}

        // Upgrade from empty to single element tree
        void upgrade() {
            if (parent_ == NULL)
                parent_ = this;
        }

        // Base class for all iterators
        template<typename R, typename Node>
        class iterator_base {
        protected:
            Node *n;
            size_t depth_;                              // Invariant should always hold that depth_ refers to the number of unique parents of n, 0 if at the root or n is NULL

            size_t index_in_parent(Node *node) const {
                Node * const parent = node->parent_;
                for (size_t i = 0; i < parent->children.size(); ++i) {
                    if (parent->children[i] == node)
                        return i;
                }

                // Shouldn't ever get here
                return 0;
            }

            iterator_base(Node *n, size_t depth = 0) : n(n), depth_(depth) {}
            iterator_base() : n(NULL), depth_(0) {}

        public:
            bool operator==(const iterator_base &other) const { return n == other.n; }
            bool operator!=(const iterator_base &other) const { return !(*this == other); }

            iterator_base parent() const { return iterator_base(n->parent_, depth_ - 1); }
            iterator_base child(size_t index) const { return iterator_base(n->children[index], depth_ + 1); }

            // Returns index of this node in its parent (optimized in traversal iterator)
            size_t child_index() const { return depth_? index_in_parent(n): 0; }

            // Returns how many levels this node is down from the root
            size_t depth() const { return depth_; }

            R &operator*() const { return n->value(); }
            R *operator->() const { return &(n->value()); }
        };

        /////////////////////////////////////////////////////////////////////////
        ///
        /// \brief The traversal_iterator_base class
        ///
        /// Base class for entire-tree traversal iterators, tracking indexes in parents
        ///
        /////////////////////////////////////////////////////////////////////////
        template<typename R, typename Node>
        class traversal_iterator_base : public iterator_base<R, Node>, public std::iterator<std::forward_iterator_tag, R, std::ptrdiff_t, R *, R> {
        protected:
            typedef iterator_base<R, Node> base;

            std::vector<size_t> children_of_parents;

            size_t pop_child_of_parent() {
                --base::depth_;

                // Cached index in parent
                if (children_of_parents.size()) {
                    const size_t child_of_parent = children_of_parents.back();
                    children_of_parents.pop_back();
                    return child_of_parent;
                }

                // No cached index, search for index the long way
                return base::index_in_parent(base::n);
            }

            void move_to_child(size_t index) {
                base::n = base::n->children[index];
                ++base::depth_;
                children_of_parents.push_back(index);
            }

            traversal_iterator_base(Node *n) : iterator_base<R, Node>(n) {}
            traversal_iterator_base(const iterator_base<R, Node> &other) : iterator_base<R, Node>(other) {}
            traversal_iterator_base() {}

        public:
            size_t child_index() const { // Shadows the implementation in the base class, but since iterators shouldn't be used as pointers it shouldn't matter
                if (base::depth() == 0)
                    return 0;
                else if (children_of_parents.size())
                    return children_of_parents.back();

                return base::index_in_parent(base::n);
            }

            bool has_same_ancestor_heirarchy(const traversal_iterator_base<R, Node> &other) const {
                using namespace std; // Just for macro min/max

                // If depth from root is different, it cannot have the same heirarchy
                if (base::depth() != other.base::depth())
                    return false;

                const size_t common_size = min(children_of_parents.size(), other.children_of_parents.size());

                // Check the closest (cached) parent indexes to see if they're all equal, if not then the heirarchy is different
                if (!std::equal(children_of_parents.begin() + children_of_parents.size() - common_size,
                                children_of_parents.end(),
                                other.children_of_parents.begin() + other.children_of_parents.size() - common_size,
                                other.children_of_parents.end()))
                    return false;
                // Heirarchy caches are equal here, but check if they're actually equal to the depth of the trees, if so, then the caches hold all values to the root
                else if (common_size == base::depth())
                    return true;

                // Heirarchy caches are equal here, but don't go all the way to the root, so backtrack to the last known root parent
                Node *p_this = base::n, *p_other = other.base::n;

                for (size_t i = 0; i < common_size; ++i) {
                    p_this = p_this->parent_;
                    p_other = p_other->parent_;
                }

                // Then walk p_this and p_other from known common parent to the actual root
                while (p_this->parent_ != p_this) { // Both must not be root if only one is checked, since the depths are equal
                    if (base::index_in_parent(p_this) != base::index_in_parent(p_other))
                        return false;

                    p_this = p_this->parent_;
                    p_other = p_other->parent_;
                }

                return true;
            }
        };

        /////////////////////////////////////////////////////////////////////////
        ///
        /// \brief The preorder_iterator_base class
        ///
        /// Iterates parent first, then the children (parent -> left -> right)
        ///
        /////////////////////////////////////////////////////////////////////////
        template<typename R, typename Node>
        class preorder_iterator_base : public traversal_iterator_base<R, Node> {
        protected:
            typedef traversal_iterator_base<R, Node> base;

            preorder_iterator_base(Node *n) : traversal_iterator_base<R, Node>(n) {}
            preorder_iterator_base(const iterator_base<R, Node> &other) : traversal_iterator_base<R, Node>(other) {}
            preorder_iterator_base() {}

        public:
            // Pre-order traverses parent first, then the children (V -> L -> R)
            preorder_iterator_base &operator++() {
                // Check for any child, if any child visit it
                if (base::n->children.size()) {
                    base::move_to_child(0);
                    return *this;
                }

                // If no child, go back to parent and get next child
                while (base::depth_) {
                    const size_t next_child_number = base::pop_child_of_parent() + 1;

                    base::n = base::n->parent_;

                    if (next_child_number < base::n->children.size()) {
                        base::move_to_child(next_child_number);
                        return *this;
                    }
                }

                // If no parent, the iterator is at end
                base::n = NULL;

                return *this;
            }
            preorder_iterator_base operator++(int) {
                const preorder_iterator_base ret = *this;
                ++(*this);
                return ret;
            }
        };

        /////////////////////////////////////////////////////////////////////////
        ///
        /// \brief The postorder_iterator class
        ///
        /// Iterates children first, then the parent (left -> right -> parent)
        ///
        /////////////////////////////////////////////////////////////////////////
        template<typename R, typename Node>
        class postorder_iterator_base : public traversal_iterator_base<R, Node> {
        protected:
            typedef traversal_iterator_base<R, Node> base;

            // Find the deepest, leftmost leaf of node n
            void visit_leftmost_leaf() {
                if (base::n) {
                    while (base::n->children.size()) {
                        base::move_to_child(0);
                    }
                }
            }

            // Constructor reinitializes iterator to point to the deepest, leftmost child node of root
            postorder_iterator_base(Node *root) : traversal_iterator_base<R, Node>(root) {
                visit_leftmost_leaf();
            }
            postorder_iterator_base(const iterator_base<R, Node> &other) : traversal_iterator_base<R, Node>(other) {}
            postorder_iterator_base() {}

        public:
            // Post-order traverses children first, then the parent (L -> R -> V)
            // Calling operator++() with exists() == false is undefined
            postorder_iterator_base &operator++() {
                if (base::depth_) {
                    // Does this node have a sibling to the right? Move to the deepest, leftmost child of it
                    const size_t next_child_number = base::pop_child_of_parent() + 1;

                    base::n = base::n->parent_;

                    if (next_child_number < base::n->children.size()) {
                        base::move_to_child(next_child_number);
                        visit_leftmost_leaf();
                        return *this;
                    }

                    // Node has no sibling to the right, return the parent
                    return *this;
                }

                // If no parent, the iterator is at end
                base::n = NULL;

                return *this;
            }
            postorder_iterator_base operator++(int) {
                const postorder_iterator_base ret = *this;
                ++(*this);
                return ret;
            }
        };

    public:
        class iterator : public iterator_base<T, tree> {
        protected:
            friend class tree;

            iterator(tree *root) : iterator_base<T, tree>(root) {}

        public:
            iterator(const iterator &other) : iterator_base<T, tree>(other) {}
            iterator(const iterator_base<T, tree> &other) : iterator_base<T, tree>(other) {}
            iterator() {}
        };

        class const_iterator : public iterator_base<const T, const tree> {
        protected:
            friend class tree;

            const_iterator(const tree *root) : iterator_base<const T, const tree>(root) {}

        public:
            const_iterator(const const_iterator &other) : iterator_base<const T, const tree>(other) {}
            const_iterator(const iterator_base<const T, const tree> &other) : iterator_base<const T, const tree>(other) {}
            const_iterator() {}
        };

        class preorder_iterator : public preorder_iterator_base<T, tree> {
            friend class tree;

            preorder_iterator(tree *root) : preorder_iterator_base<T, tree>(root) {}

        public:
            preorder_iterator(const iterator_base<T, tree> &other) : preorder_iterator_base<T, tree>(other) {}
            preorder_iterator() {}
        };

        class const_preorder_iterator : public preorder_iterator_base<const T, const tree> {
            friend class tree;

            const_preorder_iterator(const tree *root) : preorder_iterator_base<const T, const tree>(root) {}

        public:
            const_preorder_iterator(const iterator_base<const T, const tree> &other) : preorder_iterator_base<const T, const tree>(other) {}
            const_preorder_iterator() {}
        };

        class postorder_iterator : public postorder_iterator_base<T, tree> {
            friend class tree;

            postorder_iterator(tree *root) : postorder_iterator_base<T, tree>(root) {}

        public:
            postorder_iterator(const iterator_base<T, tree> &other) : postorder_iterator_base<T, tree>(other) {}
            postorder_iterator() {}
        };

        class const_postorder_iterator : public postorder_iterator_base<const T, const tree> {
            friend class tree;

            const_postorder_iterator(const tree *root) : postorder_iterator_base<const T, const tree>(root) {}

        public:
            const_postorder_iterator(const iterator_base<const T, const tree> &other) : postorder_iterator_base<const T, const tree>(other) {}
            const_postorder_iterator() {}
        };

        iterator root() { return iterator(empty()? NULL: this); }
        const_iterator root() const { return const_iterator(empty()? NULL: this); }
        const_iterator croot() const { return const_iterator(empty()? NULL: this); }
        iterator end() { return iterator(); }
        const_iterator end() const { return const_iterator(); }
        const_iterator cend() const { return const_iterator(); }

        preorder_iterator preorder_begin() { return preorder_iterator(empty()? NULL: this); }
        preorder_iterator preorder_end() { return preorder_iterator(); }
        const_preorder_iterator preorder_begin() const { return const_preorder_iterator(empty()? NULL: this); }
        const_preorder_iterator preorder_end() const { return const_preorder_iterator(); }
        const_preorder_iterator preorder_cbegin() const { return const_preorder_iterator(empty()? NULL: this); }
        const_preorder_iterator preorder_cend() const { return const_preorder_iterator(); }
        postorder_iterator postorder_begin() { return postorder_iterator(empty()? NULL: this); }
        postorder_iterator postorder_end() { return postorder_iterator(); }
        const_postorder_iterator postorder_begin() const { return const_postorder_iterator(empty()? NULL: this); }
        const_postorder_iterator postorder_end() const { return const_postorder_iterator(); }
        const_postorder_iterator postorder_cbegin() const { return const_postorder_iterator(empty()? NULL: this); }
        const_postorder_iterator postorder_cend() const { return const_postorder_iterator(); }

        // Create default empty tree
        tree() : parent_(NULL) { }
        // Create tree with single node with specified value
        tree(T value) : parent_(this), val(value) { }
        // Copy another tree, without copying its parent
        tree(const tree &other) : parent_(other.empty()? NULL: this), val(other.val) {
            children.reserve(other.children.size());

            try {
                if (stack_recursion) {
                    for (size_t i = 0; i < other.children.size(); ++i) {
                        tree *p = new tree(*other.children[i]);
                        p->parent_ = this;

                        try {
                            children.push_back(p);
                        } catch (...) {
                            delete p;
                            throw;
                        }
                    }
                } else {
                    const tree *original = &other;
                    tree *clone = this;

                    while (true) {
                        if (clone->children.size() != original->children.size()) {
                            const size_t child_index = clone->children.size();

                            tree *p = new tree(original->children[child_index]->val, clone);

                            try {
                                clone->children.push_back(p);
                            } catch (...) {
                                delete p;
                                throw;
                            }

                            original = original->children[child_index];
                            clone = clone->children[child_index];

                            clone->children.reserve(original->children.size());
                        } else {
                            if (original == &other)
                                break;

                            original = original->parent_;
                            clone = clone->parent_;
                        }
                    }
                }
            } catch (...) { // Exception thrown, so delete the existing children
                for (size_t i = 0; i < children.size(); ++i) {
                    delete children[i];
                }

                throw;
            }
        }
#if __cplusplus >= 201103L
        // Move from other node to this node
        tree(tree &&other) : children(std::move(other.children)), parent_(other.empty()? NULL: this), val(std::move(other.val)) {
            // Fix parent references for each child
            for (size_t i = 0; i < children.size(); ++i)
                children[i]->parent_ = this;
        }
        template<typename... Args>
        tree(Args&&... args) : parent_(this), val(std::forward<Args>(args)...) {}
#endif

        ~tree() {
            if (stack_recursion) {
                for (size_t i = 0; i < children.size(); ++i)
                    delete children[i];
            } else { // No use of stack recursion in deletion routine, purely iteration (necessary for extremely deep trees)
                tree *n = this;

                while (true) {
                    if (n->children.size()) {
                        tree *temp = n->children.back();
                        n->children.pop_back();
                        n = temp;
                    } else {
                        if (n == this)
                            break;

                        tree *temp = n;
                        n = n->parent_;

                        delete temp;
                    }
                }
            }
        }

        // WARNING! Assigning an empty tree to a child will NOT delete the child from its parent
        // It will instead just make the child a default-initialized node
        tree &operator=(const tree &other) {
            if (&other == this)
                return *this;

            erase_children();

            val = other.val;

            // Fix parent references for each child
            children.reserve(other.children.size());
            for (size_t i = 0; i < other.children.size(); ++i) {
                tree *p = new tree(*other.children[i]);
                p->parent_ = this;

                try {
                    children.push_back(p);
                } catch (...) {
                    delete p;
                    throw;
                }
            }

            // Fix own parent reference if necessary
            if (is_root())
                parent_ = other.empty()? NULL: this;

            return *this;
        }
#if __cplusplus >= 201103L
        tree &operator=(tree &&other) {
            if (&other == this)
                return *this;

            erase_children();

            val = std::move(other.val);
            children = std::move(other.children);

            // Fix parent references for each child
            for (size_t i = 0; i < children.size(); ++i)
                children[i]->parent_ = this;

            // Fix own parent reference if necessary
            if (is_root())
                parent_ = other.empty()? NULL: this;

            return *this;
        }
#endif

        size_t max_children() const { return children.max_size(); }
        size_t child_count() const { return children.size(); }

        bool is_leaf() const { return children.empty(); }
        bool has_child(size_t index) const { return child_count() > index; }

        bool empty() const { return parent_ == NULL; }

        // WARNING! If this function is called on a child object (is_child() == true), the child object will be removed from the parent and will be deleted
        // This makes any further references to the instantiated object invalid
        //
        // If called on the root, the tree is set to empty
        //
        // Calling this function on an already empty tree does nothing
        //
        // Invalidates iterators to any siblings of this tree
        void clear_to_empty() {
            if (parent_ == this) { // Root element, just clear all children and erase to empty
                erase_children();
                parent_ = NULL;
            } else if (parent_) { // If not empty, remove from parent and this object is now deleted
                for (size_t i = 0; i < children.size(); ++i) {
                    if (parent_->children[i] == this) {
                        parent_->erase_child(i);
                        return;
                    }
                }
            }
        }

        // Erase immediate children with specified value
        // Invalidates iterators to any children of this tree
        void erase(const T &value) {
            for (size_t i = children.size(); i; --i) {
                if (children[i-1]->val == value)
                    erase_child(i);
            }
        }
        // Erase immediate child with specific index, including its children
        // Invalidates iterators to any children of this tree
        void erase_child(size_t index) {
            delete children[index];
            children.erase(children.begin() + index);
        }
        // Erase all children of this tree
        // Invalidates iterators to any children of this tree
        void erase_children() {
            for (size_t i = 0; i < children.size(); ++i)
                delete children[i];

            children.clear();
        }

        // Return the value of the current node
        T &value() { upgrade(); return val; }
        T &operator*() { upgrade(); return val; }
        T *operator->() { upgrade(); return &val; }
        // An empty element if empty() is true
        const T &value() const { return val; }
        // An empty element if empty() is true
        const T &operator*() const { return val; }
        // An empty element if empty() is true
        const T &operator->() const { return &val; }

        // Returns true if this node is the root. Returns true for empty trees as well
        bool is_root() const { return parent_ == NULL || parent_ == this; }

        // Returns true if this node is a child of a larger tree
        bool is_child() const { return !is_root(); }

        // Returns the parent of this node
        // Undefined if empty() is true
        // If at the root, the root itself is returned
        tree &parent() { return *parent_; }
        const tree &parent() const { return *parent_; }

        // Always well defined, adds a child unless the new child tree is empty
        // No iterators are invalidated
        void append_child(tree value) {
            if (value.empty())
                return;

            tree *p = new tree();
            p->swap(value);
            p->parent_ = this;

            try {
                children.push_back(p);
            } catch (...) {
                delete p;
                throw;
            }
        }

        // Always well defined, creates a single-node child if it doesn't exist
        // No iterators are invalidated
        tree &child(size_t index) {
            while (children.size() <= index)
                append_child(T());

            return *children[index];
        }

        // Undefined if has_child(index) is false
        const tree &child(size_t index) const { return *children[index]; }

        // Defined only if iterator != end()
        // No iterators are invalidated
        // Undefined if iterator is invalid
        tree &subtree(iterator it) { return *it.n; }
        const tree &subtree(const_iterator it) const { return *it.n; }

        // Slices the subtree from the tree, using the specified node as the root, and returns it
        // Invalidates all iterators pointing to the iterator or its siblings, or any children of such iterators
        // Undefined if iterator is invalid
        tree slice(iterator it) {
            tree result;

            // Slice everything since root is disappearing
            if (it.n == this) {
                result.swap(*this);
                return result;
            }

            // Parent must be valid since it's not the root
            const size_t index = iterator::index_in_parent(it.n);

            it.n->parent_->children.erase(it.n->parent_->children.begin() + index);

            result.swap(*it.n);
            delete it.n;

            return result;
        }

        // Follows a path of values down the tree and returns an iterator to the element, or an invalid iterator if the path doesn't exist
        template<typename It>
        iterator follow_path(It begin, It end) const {
            iterator it = root();

            // Does begin match the root?
            if (it == this->end() || begin == end || !(*begin == *it))
                return iterator();

            // Check each child for the next path element
            for (++begin; begin != end; ++begin) {
                bool matched = false;

                for (size_t i = 0; i < children.size(); ++i) {
                    if (*begin == it.n->children[i]->val) {
                        matched = true;
                        it = it.child(i);
                        break;
                    }
                }

                if (!matched)
                    return iterator();
            }

            return it;
        }

        // Follows a path of values down the tree and returns an iterator to the element, or an invalid iterator if the path doesn't exist
        template<typename It>
        const_iterator follow_path(It begin, It end) const {
            const_iterator it = root();

            // Does begin match the root?
            if (it == this->end() || begin == end || !(*begin == *it))
                return const_iterator();

            // Check each child for the next path element
            for (++begin; begin != end; ++begin) {
                bool matched = false;

                for (size_t i = 0; i < children.size(); ++i) {
                    if (*begin == it.n->children[i]->val) {
                        matched = true;
                        it = it.child(i);
                        break;
                    }
                }

                if (!matched)
                    return const_iterator();
            }

            return it;
        }

        bool operator==(const tree &other) const {
            if (!empty() && !other.empty()) {
                const_preorder_iterator current = preorder_begin();
                const_preorder_iterator alternate = other.preorder_begin();

                for (; current != end() && alternate != other.end(); ++current, ++alternate) {
                    if (!(*current == *alternate) || !current.has_same_ancestor_heirarchy(alternate))
                        return false;
                }

                return current != end() || alternate != other.end();
            }
            else if (!empty() || !other.empty())
                return false;
            else
                return true;
        }
        bool operator!=(const tree &other) const { return !(*this == other); }

        // Returns the number of levels in the tree (empty = 0, just root = 1, etc.)
        size_t height() const {
            size_t height = 0;

            for (const_preorder_iterator it = preorder_begin(); it != end(); ++it) {
                using namespace std;

                height = max(height, it.depth() + 1);
            }

            return height;
        }
        // Returns the number of nodes in the tree
        size_t size() const { return std::distance(preorder_begin(), preorder_end()); }
        // Returns the number of leaf nodes in the tree
        size_t leaf_count() const {
            size_t leaves = 0;

            for (const_preorder_iterator it = preorder_begin(); it != end(); ++it) {
                leaves += subtree(it).is_leaf();
            }

            return leaves;
        }
        // Returns the number of non-leaf nodes in the tree
        size_t branch_count() const {
            size_t branches = 0;

            for (const_preorder_iterator it = preorder_begin(); it != end(); ++it) {
                branches += !subtree(it).is_leaf();
            }

            return branches;
        }

        // WARNING! Swapping an empty tree with a child will NOT delete the child from its parent
        // It will instead just make the child a default-initialized node
        void swap(tree &other) {
            std::swap(val, other.val);

            children.swap(other.children);

            // Fix parent references for each child
            for (size_t i = 0; i < children.size(); ++i)
                children[i]->parent_ = this;

            for (size_t i = 0; i < other.children.size(); ++i)
                other.children[i]->parent_ = &other;

            // Only change parents if empty or the root node
            if (is_root() && other.is_root()) {
                // Fix self reference if not empty tree
                if (parent_ == this)
                    parent_ = &other;

                // Then do the same for the other tree
                if (other.parent_ == &other)
                    other.parent_ = this;

                // Then do actual swap
                std::swap(parent_, other.parent_);
            }
        }
    };

#if __cplusplus >= 201103L
    template<typename T, bool stack_recursion = true>
    using binary_tree = tree<T, 2, stack_recursion>;

    template<typename T, bool stack_recursion = true>
    using tertiary_tree = tree<T, 3, stack_recursion>;

    template<typename T, bool stack_recursion = true>
    using quaternary_tree = tree<T, 4, stack_recursion>;

    template<typename T, bool stack_recursion = true>
    using dynamic_tree = tree<T, 0, stack_recursion>;
#endif
}

#endif // SKATE_TREE_H
